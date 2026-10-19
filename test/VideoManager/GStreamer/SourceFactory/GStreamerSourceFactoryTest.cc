#include "GStreamerTest.h"

#ifdef QGC_GST_STREAMING
#include <array>
#include <atomic>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <QtConcurrent/QtConcurrent>
#include <QtCore/QBuffer>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtCore/QTimer>
#include <QtGui/QImage>
#include <QtGui/QImageReader>
#include <QtGui/QImageWriter>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QUdpSocket>
#include <QtTest/QSignalSpy>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

#include "GStreamerTestHelpers.h"
#include "GstScoped.h"
#include "GstSourceFactory.h"
#include "LocalHttpTestServer.h"
#include "QGCNetworkHelper.h"
#include "QGCRtpReferenceState.h"
#include "QGCVideoFrameContextStore.h"
#include "QGCWebSocketVideoSource.h"

void GStreamerTest::_testRtpReferenceLossRequiresCompleteSync()
{
    QGCRtpReferenceState references;
    const auto joined = references.observe(17, 65534, 100, true, true);
    QVERIFY(!references.observeAccessUnit(joined, true, false));
    const auto sync = references.observe(17, 65535, 200, true, false);
    QVERIFY(references.observeAccessUnit(sync, true, false));
    const auto wrapped = references.observe(17, 0, 300, true, false);
    QVERIFY(references.observeAccessUnit(wrapped, false, false));
    references.observe(17, 1, 400, false, false);
    // The missing middle fragment makes even an apparent sync frame unsafe.
    const auto damagedSync = references.observe(17, 3, 400, true, false);
    QVERIFY(!references.verified(sync));
    QVERIFY(!references.observeAccessUnit(damagedSync, true, false));
    const auto predicted = references.observe(17, 4, 500, true, false);
    QVERIFY(!references.observeAccessUnit(predicted, false, false));
    const auto recovered = references.observe(17, 5, 600, true, false);
    QVERIFY(references.observeAccessUnit(recovered, true, false));
    const auto corrupted = references.observe(17, 6, 700, true, false);
    QVERIFY(!references.observeAccessUnit(corrupted, false, true));
    QVERIFY(!references.verified(recovered));
    const auto otherSource = references.observe(18, 20, 800, true, false);
    QVERIFY(!references.observeAccessUnit(otherSource, true, false));
    const auto nextSync = references.observe(18, 21, 900, true, false);
    QVERIFY(references.observeAccessUnit(nextSync, true, false));
    const auto duplicate = references.observe(18, 21, 900, true, false);
    QVERIFY(!references.observeAccessUnit(duplicate, true, false));
    QVERIFY(!references.verified(nextSync));
    const auto afterDuplicate = references.observe(18, 22, 1000, true, false);
    QVERIFY(references.observeAccessUnit(afterDuplicate, true, false));
    // A wholly missing access unit must retire prediction references too.
    const auto afterMissingFrame = references.observe(18, 24, 1200, true, false);
    QVERIFY(!references.observeAccessUnit(afterMissingFrame, true, false));
    QVERIFY(!references.observeAccessUnit(afterDuplicate, true, false));
    const auto finalSync = references.observe(18, 25, 1300, true, false);
    QVERIFY(references.observeAccessUnit(finalSync, true, false));
    // Network continuity alone is insufficient: the display queue may shed a reference frame.
    references.observe(18, 26, 1400, true, false);
    const auto afterQueueDrop = references.observe(18, 27, 1500, true, false);
    QVERIFY(!references.observeAccessUnit(afterQueueDrop, false, false));
    const auto afterQueueSync = references.observe(18, 28, 1600, true, false);
    QVERIFY(references.observeAccessUnit(afterQueueSync, true, false));
}
namespace {

/// SourceFactory::create() hands out a floating bin; this sinks it.
GStreamer::GstElementPtr createSource(const QString& uri, const GStreamer::SourceFactory::Config& config)
{
    GstElement* const bin = GStreamer::SourceFactory::create(uri, config);
    return GStreamer::adoptElement(bin ? GST_ELEMENT(gst_object_ref_sink(bin)) : nullptr);
}

/// The first element inside @p bin made by @p factoryName, or null.
GStreamer::GstElementPtr child(GstElement* bin, const char* factoryName)
{
    return GstTest::findByFactory(GST_BIN(bin), factoryName);
}

/// RTP packets into rtph264depay and the units that reach h264parse after it.
struct DepayloaderOutput
{
    std::atomic<int> packetsIn{0};
    std::atomic<int> unitsOut{0};
    std::atomic<bool> firstUnitHasSps{false};
};

bool containsH264Sps(const guint8* data, gsize size)
{
    for (gsize i = 0; (i + 3) < size; ++i) {
        if ((data[i] == 0) && (data[i + 1] == 0) && (data[i + 2] == 1) && ((data[i + 3] & 0x1FU) == 7U)) {
            return true;
        }
    }
    return false;
}

GstPadProbeReturn recordDepayloaderUnit([[maybe_unused]] GstPad* pad, GstPadProbeInfo* info, gpointer data)
{
    auto* output = static_cast<DepayloaderOutput*>(data);
    GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    GstMapInfo map;
    if ((output->unitsOut.fetch_add(1) == 0) && buffer && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        output->firstUnitHasSps = containsH264Sps(map.data, map.size);
        gst_buffer_unmap(buffer, &map);
    }
    return GST_PAD_PROBE_OK;
}

// Units are recorded at h264parse's sink rather than the depayloader's src: the source bin sees
// deep-element-added before parsebin's own handlers, so a probe added here would run before the factory's.
void watchH264Depayloading([[maybe_unused]] GstBin* bin, [[maybe_unused]] GstBin* subBin, GstElement* element,
                           gpointer data)
{
    GstElementFactory* factory = gst_element_get_factory(element);
    const char* factoryName = factory ? gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory)) : nullptr;
    auto* output = static_cast<DepayloaderOutput*>(data);
    const GStreamer::GstPadPtr sinkPad = GStreamer::adoptPad(gst_element_get_static_pad(element, "sink"));
    if (g_strcmp0(factoryName, "rtph264depay") == 0) {
        (void) gst_pad_add_probe(sinkPad.get(), GST_PAD_PROBE_TYPE_BUFFER, GstTest::countBuffers, &output->packetsIn,
                                 nullptr);
    } else if (g_strcmp0(factoryName, "h264parse") == 0) {
        (void) gst_pad_add_probe(sinkPad.get(), GST_PAD_PROBE_TYPE_BUFFER, recordDepayloaderUnit, output, nullptr);
    }
}

/// What a bare test sink pad received: each buffer's bytes and PTS.
struct ReceivedBuffers
{
    QList<QByteArray> data;
    QList<GstClockTime> pts;
};

GstFlowReturn recordBuffer(GstPad* pad, [[maybe_unused]] GstObject* parent, GstBuffer* buffer)
{
    auto* const received = static_cast<ReceivedBuffers*>(gst_pad_get_element_private(pad));
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        received->data.append(QByteArray(reinterpret_cast<const char*>(map.data), static_cast<qsizetype>(map.size)));
        gst_buffer_unmap(buffer, &map);
    }
    received->pts.append(GST_BUFFER_PTS(buffer));
    gst_buffer_unref(buffer);
    return GST_FLOW_OK;
}

/// A src pad standing in for a depayloader's, linked to a sink pad recording into @p received, with @p caps sent.
struct GatedPads
{
    GatedPads(ReceivedBuffers* received, const char* caps)
        : src(GStreamer::adoptPad(GST_PAD(gst_object_ref_sink(gst_pad_new("src", GST_PAD_SRC)))))
        , sink(GStreamer::adoptPad(GST_PAD(gst_object_ref_sink(gst_pad_new("sink", GST_PAD_SINK)))))
    {
        gst_pad_set_element_private(sink.get(), received);
        gst_pad_set_chain_function(sink.get(), recordBuffer);
        gst_pad_set_event_function(sink.get(), [](GstPad*, GstObject*, GstEvent* event) -> gboolean {
            gst_event_unref(event);
            return TRUE;
        });
        (void) gst_pad_set_active(src.get(), TRUE);
        (void) gst_pad_set_active(sink.get(), TRUE);
        linked = gst_pad_link(src.get(), sink.get()) == GST_PAD_LINK_OK;
        (void) gst_pad_push_event(src.get(), gst_event_new_stream_start("parameter-set-gate"));
        const GStreamer::GstCapsPtr streamCaps = GStreamer::adoptCaps(gst_caps_from_string(caps));
        (void) gst_pad_push_event(src.get(), gst_event_new_caps(streamCaps.get()));
        GstSegment segment;
        gst_segment_init(&segment, GST_FORMAT_TIME);
        (void) gst_pad_push_event(src.get(), gst_event_new_segment(&segment));
    }

    ~GatedPads()
    {
        (void) gst_pad_set_active(src.get(), FALSE);
        (void) gst_pad_set_active(sink.get(), FALSE);
    }

    GatedPads(const GatedPads&) = delete;
    GatedPads& operator=(const GatedPads&) = delete;

    GstFlowReturn push(const QByteArray& bytes, GstClockTime pts) const
    {
        GstBuffer* const buffer = gst_buffer_new_memdup(bytes.constData(), static_cast<gsize>(bytes.size()));
        GST_BUFFER_PTS(buffer) = pts;
        return gst_pad_push(src.get(), buffer);
    }

    GStreamer::GstPadPtr src;
    GStreamer::GstPadPtr sink;
    bool linked = false;
};

void linkToSink([[maybe_unused]] GstElement* element, GstPad* pad, gpointer data)
{
    const GStreamer::GstPadPtr sinkPad = GStreamer::adoptPad(gst_element_get_static_pad(GST_ELEMENT(data), "sink"));
    if (sinkPad && !gst_pad_is_linked(sinkPad.get())) {
        (void) gst_pad_link(pad, sinkPad.get());
    }
}

GstStaticPadTemplate sH264RtpPadTemplate = GST_STATIC_PAD_TEMPLATE(
    "video_rtp", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264"));

/// An mpegts:// source bin, whose tsdemux the dynamic-pad tests hand RTP pads to.
struct MpegTsSource
{
    explicit MpegTsSource(const GStreamer::SourceFactory::Config& config)
        : bin(GStreamer::SourceFactory::create(QStringLiteral("mpegts://127.0.0.1:5600"), config))
        , demux(bin.get() ? child(bin.get(), "tsdemux") : nullptr)
        , parserSink(GStreamer::adoptPad(bin.get() ? parserSinkPad(bin.get()) : nullptr))
    {}

    /// Built, and no pad reaches the parser yet.
    bool isValid() const { return demux && parserSink && !gst_pad_is_linked(parserSink.get()); }

    GstTest::TestPipeline bin;
    GStreamer::GstElementPtr demux;
    GStreamer::GstPadPtr parserSink;

private:
    static GstPad* parserSinkPad(GstElement* bin)
    {
        const GStreamer::GstElementPtr parser = child(bin, "parsebin");
        return parser ? gst_element_get_static_pad(parser.get(), "sink") : nullptr;
    }
};

// Minimal RTSP responder for driving a real rtspsrc without gst-rtsp-server: answers OPTIONS/DESCRIBE with a
// fixed SDP and records each SETUP URL without answering it, so the client stops right after picking streams.
class FakeRtspServer
{
public:
    explicit FakeRtspServer(const QByteArray& sdp)
        : _sdp(sdp)
    {
        QObject::connect(&_server, &QTcpServer::newConnection, &_server, [this] {
            while (QTcpSocket* socket = _server.nextPendingConnection()) {
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [this, socket] { _serve(socket); });
            }
        });
    }

    bool listen() { return _server.listen(QHostAddress::LocalHost, 0); }

    QString url() const { return QStringLiteral("rtsp://127.0.0.1:%1/test").arg(_server.serverPort()); }

    const QList<QByteArray>& setupUrls() const { return _setupUrls; }

private:
    void _serve(QTcpSocket* socket)
    {
        QByteArray& pending = _pending[socket];
        pending += socket->readAll();
        for (qsizetype end = pending.indexOf("\r\n\r\n"); end >= 0; end = pending.indexOf("\r\n\r\n")) {
            const QList<QByteArray> lines = pending.left(end).split('\n');
            pending.remove(0, end + 4);

            const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
            const QByteArray method = requestLine.value(0);
            QByteArray cseq;
            for (const QByteArray& line : lines) {
                if (line.toLower().startsWith("cseq:")) {
                    cseq = line.mid(5).trimmed();
                }
            }

            if (method == "SETUP") {
                _setupUrls.append(requestLine.value(1));
                continue;
            }

            QByteArray reply = "RTSP/1.0 200 OK\r\nCSeq: " + cseq + "\r\n";
            if (method == "OPTIONS") {
                reply += "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n\r\n";
            } else if (method == "DESCRIBE") {
                reply += "Content-Base: " + url().toUtf8() +
                         "/\r\nContent-Type: application/sdp\r\nContent-Length: " + QByteArray::number(_sdp.size()) +
                         "\r\n\r\n" + _sdp;
            } else {
                reply += "\r\n";
            }
            (void) socket->write(reply);
        }
    }

    QTcpServer _server;
    QByteArray _sdp;
    QHash<QTcpSocket*, QByteArray> _pending;
    QList<QByteArray> _setupUrls;
};

}  // namespace

void GStreamerTest::_testSourceFactoryUdpRtpJitterBuffer()
{
    const QString missing = GstTest::firstMissingFactory({"udpsrc", "rtpjitterbuffer"});
    if (!missing.isEmpty()) {
        QSKIP(qPrintable(QStringLiteral("%1 plugin unavailable").arg(missing)));
    }

    GStreamer::SourceFactory::Config config;
    config.jitterBuffer = GStreamer::SourceFactory::JitterBuffer::DropOnLatency;
    config.latencyMs = 80;
    config.doRetransmission = true;

    const GStreamer::GstElementPtr bin = createSource(QStringLiteral("udp://127.0.0.1:5600"), config);
    QVERIFY(bin);

    const GStreamer::GstElementPtr jb = child(bin.get(), "rtpjitterbuffer");
    QVERIFY2(jb, "static UDP-RTP path must insert an rtpjitterbuffer at NULL state");

    guint latency = 0;
    gboolean doLost = FALSE, doRtx = TRUE, dropOnLatency = FALSE;
    g_object_get(jb.get(), "latency", &latency, "do-lost", &doLost, "do-retransmission", &doRtx, "drop-on-latency",
                 &dropOnLatency, nullptr);
    QCOMPARE(latency, 80u);
    QCOMPARE(doLost, TRUE);
    QVERIFY2(!doRtx, "plain UDP has no RTCP back-channel, so retransmission requests could never reach the sender");
    QCOMPARE(dropOnLatency, TRUE);
}

void GStreamerTest::_testSourceFactoryJitterBufferNone()
{
    if (!GstTest::firstMissingFactory({"udpsrc"}).isEmpty()) {
        QSKIP("udpsrc plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    config.jitterBuffer = GStreamer::SourceFactory::JitterBuffer::None;

    const GStreamer::GstElementPtr bin = createSource(QStringLiteral("udp://127.0.0.1:5600"), config);
    QVERIFY(bin);

    QVERIFY2(!child(bin.get(), "rtpjitterbuffer"), "JitterBuffer::None must link the source straight to the parser");
}

void GStreamerTest::_testSourceFactoryNoRetransmission()
{
    if (!GstTest::firstMissingFactory({"rtspsrc"}).isEmpty()) {
        QSKIP("rtspsrc plugin unavailable");
    }

    // RTSP is the only source with a back-channel for retransmission requests, so it's the one the option reaches.
    GStreamer::SourceFactory::Config config;
    config.doRetransmission = false;

    const GStreamer::GstElementPtr bin = createSource(QStringLiteral("rtsp://127.0.0.1:8554/test"), config);
    QVERIFY(bin);

    const GStreamer::GstElementPtr source = child(bin.get(), "rtspsrc");
    QVERIFY(source);

    gboolean doRtx = TRUE;
    g_object_get(source.get(), "do-retransmission", &doRtx, nullptr);
    QCOMPARE(doRtx, FALSE);
}

void GStreamerTest::_testSourceFactoryRtspJitterBufferPolicy_data()
{
    QTest::addColumn<int>("jitterBufferPolicy");
    QTest::addColumn<guint>("expectedLatencyMs");
    QTest::addColumn<bool>("expectedRetransmission");
    QTest::addColumn<bool>("expectedDropOnLatency");
    QTest::addColumn<int>("expectedRtxDelay");
    QTest::addColumn<int>("expectedRtxMaxRetries");

    using JitterBuffer = GStreamer::SourceFactory::JitterBuffer;
    // -1 is rtpjitterbuffer's "auto" default, i.e. the factory left the internal jitterbuffer untouched.
    QTest::newRow("low-latency") << static_cast<int>(JitterBuffer::None) << 0u << false << true << -1 << -1;
    QTest::newRow("drop-on-latency") << static_cast<int>(JitterBuffer::DropOnLatency) << 80u << true << true << 25 << 1;
}

void GStreamerTest::_testSourceFactoryRtspJitterBufferPolicy()
{
    if (!GstTest::firstMissingFactory({"rtspsrc"}).isEmpty()) {
        QSKIP("rtspsrc plugin unavailable");
    }

    QFETCH(int, jitterBufferPolicy);
    QFETCH(guint, expectedLatencyMs);
    QFETCH(bool, expectedRetransmission);
    QFETCH(bool, expectedDropOnLatency);
    QFETCH(int, expectedRtxDelay);
    QFETCH(int, expectedRtxMaxRetries);

    GStreamer::SourceFactory::Config config;
    config.jitterBuffer = static_cast<GStreamer::SourceFactory::JitterBuffer>(jitterBufferPolicy);
    config.latencyMs = 80;
    config.doRetransmission = true;

    const GStreamer::GstElementPtr bin = createSource(QStringLiteral("rtsp://127.0.0.1:8554/test"), config);
    QVERIFY(bin);

    const GStreamer::GstElementPtr source = child(bin.get(), "rtspsrc");
    QVERIFY(source);

    guint latencyMs = 0;
    gboolean doRetransmission = FALSE;
    gboolean dropOnLatency = FALSE;
    g_object_get(source.get(), "latency", &latencyMs, "do-retransmission", &doRetransmission, "drop-on-latency",
                 &dropOnLatency, nullptr);
    QCOMPARE(latencyMs, expectedLatencyMs);
    QCOMPARE(doRetransmission, expectedRetransmission ? TRUE : FALSE);
    QCOMPARE(dropOnLatency, expectedDropOnLatency ? TRUE : FALSE);

    QVERIFY2(!child(bin.get(), "rtpjitterbuffer"),
             "rtspsrc owns its internal jitterbuffer; the factory must not add a second one");

    // rtspsrc only creates its rtpbin during SETUP; drive the same signals it emits so the
    // per-stream internal jitterbuffer policy can be checked without a server.
    GStreamer::GstElementPtr manager = GStreamer::adoptElement(gst_element_factory_make("rtpbin", nullptr));
    GStreamer::GstElementPtr internalBuffer =
        GStreamer::adoptElement(gst_element_factory_make("rtpjitterbuffer", nullptr));
    QVERIFY(manager);
    QVERIFY(internalBuffer);
    g_signal_emit_by_name(source.get(), "new-manager", manager.get());
    g_signal_emit_by_name(manager.get(), "new-jitterbuffer", internalBuffer.get(), 0u, 0u);

    gint rtxDelay = 0;
    gint rtxMaxRetries = 0;
    g_object_get(internalBuffer.get(), "rtx-delay", &rtxDelay, "rtx-max-retries", &rtxMaxRetries, nullptr);
    QCOMPARE(rtxDelay, expectedRtxDelay);
    QCOMPARE(rtxMaxRetries, expectedRtxMaxRetries);
}

void GStreamerTest::_testSourceFactoryRtspSelectsOnlyVideoStreams_data()
{
    QTest::addColumn<QString>("capsName");
    QTest::addColumn<QString>("media");
    QTest::addColumn<bool>("expectedSelected");

    // Before the first SETUP rtspsrc passes SDP-derived caps that are still named application/x-unknown.
    const QString sdpCaps = QStringLiteral("application/x-unknown");
    QTest::newRow("video") << sdpCaps << QStringLiteral("video") << true;
    QTest::newRow("audio") << sdpCaps << QStringLiteral("audio") << false;
    QTest::newRow("metadata") << sdpCaps << QStringLiteral("application") << false;
    // On reconnect the stream caps already carry the transport media type.
    QTest::newRow("reconnect-video") << QStringLiteral("application/x-rtp") << QStringLiteral("video") << true;
}

void GStreamerTest::_testSourceFactoryRtspSelectsOnlyVideoStreams()
{
    if (!GstTest::firstMissingFactory({"rtspsrc"}).isEmpty()) {
        QSKIP("rtspsrc plugin unavailable");
    }

    QFETCH(QString, capsName);
    QFETCH(QString, media);
    QFETCH(bool, expectedSelected);

    GStreamer::SourceFactory::Config config;
    const GStreamer::GstElementPtr bin = createSource(QStringLiteral("rtsp://127.0.0.1:8554/test"), config);
    QVERIFY(bin);

    const GStreamer::GstElementPtr source = child(bin.get(), "rtspsrc");
    QVERIFY(source);

    const QByteArray capsNameUtf8 = capsName.toUtf8();
    const QByteArray mediaUtf8 = media.toUtf8();
    const GStreamer::GstCapsPtr caps = GStreamer::adoptCaps(gst_caps_new_simple(
        capsNameUtf8.constData(), "media", G_TYPE_STRING, mediaUtf8.constData(), "payload", G_TYPE_INT, 96,
        "clock-rate", G_TYPE_INT, 90000, "encoding-name", G_TYPE_STRING, "H264", nullptr));
    QVERIFY(caps);

    gboolean selected = expectedSelected ? FALSE : TRUE;
    g_signal_emit_by_name(source.get(), "select-stream", 0u, caps.get(), &selected);
    QCOMPARE(selected, expectedSelected ? TRUE : FALSE);
}

void GStreamerTest::_testSourceFactoryRtspSetsUpOnlyVideoStream()
{
    if (!GstTest::firstMissingFactory({"rtspsrc"}).isEmpty()) {
        QSKIP("rtspsrc plugin unavailable");
    }

    ignoreLogMessage("Video.GStreamer.GStreamerLogging", QtWarningMsg, GstTest::libproxyIgnoreMaskWarning());

    // Audio listed first, as on many IP cameras; rtspsrc sets streams up in SDP order.
    FakeRtspServer server(
        QByteArrayLiteral("v=0\r\n"
                          "o=- 0 0 IN IP4 127.0.0.1\r\n"
                          "s=QGC select-stream test\r\n"
                          "c=IN IP4 127.0.0.1\r\n"
                          "t=0 0\r\n"
                          "m=audio 0 RTP/AVP 97\r\n"
                          "a=rtpmap:97 OPUS/48000/2\r\n"
                          "a=control:trackID=0\r\n"
                          "m=video 0 RTP/AVP 96\r\n"
                          "a=rtpmap:96 H264/90000\r\n"
                          "a=control:trackID=1\r\n"));
    QVERIFY(server.listen());

    GstTest::TestPipeline pipeline(gst_pipeline_new("rtsp-select-stream-test"));
    QVERIFY(pipeline.get());

    GStreamer::SourceFactory::Config config;
    const GStreamer::GstElementPtr bin = createSource(server.url(), config);
    QVERIFY(bin);
    QVERIFY(gst_bin_add(pipeline.bin(), bin.get()));
    QVERIFY(gst_element_set_state(pipeline.get(), GST_STATE_PAUSED) != GST_STATE_CHANGE_FAILURE);

    QVERIFY_TRUE_WAIT(!server.setupUrls().isEmpty(), TestTimeout::mediumMs());
    QVERIFY2(server.setupUrls().first().endsWith("/trackID=1"),
             qPrintable(QStringLiteral("first SETUP must target the video track, got %1")
                            .arg(QString::fromUtf8(server.setupUrls().first()))));
}

void GStreamerTest::_testSourceFactoryRejectsBadUri()
{
    ignoreLogMessage(
        "Video.GStreamer.GstSourceFactory", QtWarningMsg,
        QRegularExpression(QStringLiteral("URI is not specified|Unsupported URI scheme|Invalid UDP port")));

    GStreamer::SourceFactory::Config config;
    QVERIFY(!createSource(QString(), config));
    QVERIFY(!createSource(QStringLiteral("ftp://127.0.0.1/x"), config));
    QVERIFY(!createSource(QStringLiteral("udp://127.0.0.1:0"), config));
    QVERIFY(!createSource(QStringLiteral("udp://127.0.0.1:99999"), config));
}

void GStreamerTest::_testSourceFactoryTcpMpegTs()
{
    const QString missing = GstTest::firstMissingFactory({"tcpclientsrc", "tsdemux"});
    if (!missing.isEmpty()) {
        QSKIP(qPrintable(QStringLiteral("%1 plugin unavailable").arg(missing)));
    }

    GStreamer::SourceFactory::Config config;
    const GStreamer::GstElementPtr bin = createSource(QStringLiteral("tcp://192.168.1.50:5600"), config);
    QVERIFY(bin);

    const GStreamer::GstElementPtr src = child(bin.get(), "tcpclientsrc");
    QVERIFY2(src, "tcp:// must build a tcpclientsrc");
    gchar* rawHost = nullptr;
    gint port = 0;
    g_object_get(src.get(), "host", &rawHost, "port", &port, nullptr);
    const GStreamer::GCharPtr host = GStreamer::adoptGChar(rawHost);
    QCOMPARE(QString::fromUtf8(host.get()), QStringLiteral("192.168.1.50"));
    QCOMPARE(port, 5600);

    QVERIFY2(child(bin.get(), "tsdemux"), "MPEG-TS over TCP must insert tsdemux explicitly");
    QVERIFY2(!child(bin.get(), "rtpjitterbuffer"), "raw MPEG-TS is not RTP; no jitterbuffer");
}

void GStreamerTest::_testSourceFactoryRejectsBadTcpUri()
{
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Invalid TCP port|Missing host in TCP URI")));

    GStreamer::SourceFactory::Config config;
    QVERIFY2(!createSource(QStringLiteral("tcp://192.168.1.50"), config), "tcp:// without a port must be rejected");
    QVERIFY2(!createSource(QStringLiteral("tcp://:5600"), config), "tcp:// without a host must be rejected");
    QVERIFY2(!createSource(QStringLiteral("tcp://192.168.1.50:0"), config), "tcp:// with port 0 must be rejected");
}

void GStreamerTest::_testSourceFactoryUdp265()
{
    const QString missing = GstTest::firstMissingFactory({"udpsrc", "rtpjitterbuffer", "rtph265depay", "h265parse"});
    if (!missing.isEmpty()) {
        QSKIP(qPrintable(QStringLiteral("%1 plugin unavailable").arg(missing)));
    }

    GStreamer::SourceFactory::Config config;
    const GStreamer::GstElementPtr bin = createSource(QStringLiteral("udp265://127.0.0.1:5600"), config);
    QVERIFY(bin);

    const GStreamer::GstElementPtr src = child(bin.get(), "udpsrc");
    QVERIFY2(src, "udp265:// must build a udpsrc");
    GstCaps* rawCaps = nullptr;
    g_object_get(src.get(), "caps", &rawCaps, nullptr);
    const GStreamer::GstCapsPtr caps = GStreamer::adoptCaps(rawCaps);
    QVERIFY2(caps, "udp265:// must set H265 RTP caps on udpsrc");
    QCOMPARE(QString::fromUtf8(gst_structure_get_string(gst_caps_get_structure(caps.get(), 0), "encoding-name")),
             QStringLiteral("H265"));

    QVERIFY2(child(bin.get(), "rtpjitterbuffer"), "udp265 RTP path must insert a jitterbuffer");
    QVERIFY2(!child(bin.get(), "tsdemux"), "udp265 is RTP, not MPEG-TS; no tsdemux");
    QVERIFY2(child(bin.get(), "rtph265depay"), "udp265:// must depayload H265 RTP explicitly");

    const GStreamer::GstElementPtr parser = child(bin.get(), "h265parse");
    QVERIFY2(parser, "udp265:// must parse H265 explicitly");
    gint configInterval = 0;
    g_object_get(parser.get(), "config-interval", &configInterval, nullptr);
    QCOMPARE(configInterval, -1);
}

void GStreamerTest::_testSourceFactoryUdpH264Caps()
{
    if (!GstTest::firstMissingFactory({"udpsrc"}).isEmpty()) {
        QSKIP("udpsrc plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    const GStreamer::GstElementPtr bin = createSource(QStringLiteral("udp://127.0.0.1:5600"), config);
    QVERIFY(bin);

    const GStreamer::GstElementPtr src = child(bin.get(), "udpsrc");
    QVERIFY(src);
    GstCaps* rawCaps = nullptr;
    g_object_get(src.get(), "caps", &rawCaps, nullptr);
    const GStreamer::GstCapsPtr caps = GStreamer::adoptCaps(rawCaps);
    QVERIFY2(caps, "udp:// must set H264 RTP caps");
    const GstStructure* st = gst_caps_get_structure(caps.get(), 0);
    QCOMPARE(QString::fromUtf8(gst_structure_get_string(st, "media")), QStringLiteral("video"));
    QCOMPARE(QString::fromUtf8(gst_structure_get_string(st, "encoding-name")), QStringLiteral("H264"));
}

void GStreamerTest::_testSourceFactoryDropsRtpUntilParameterSets()
{
    const QString missing = GstTest::firstMissingFactory(
        {"videotestsrc", "x264enc", "rtph264pay", "udpsink", "udpsrc", "parsebin", "rtph264depay", "fakesink"});
    if (!missing.isEmpty()) {
        QSKIP(qPrintable(QStringLiteral("%1 plugin unavailable").arg(missing)));
    }

    QUdpSocket portProbe;
    QVERIFY(portProbe.bind(QHostAddress::LocalHost, 0));
    const quint16 port = portProbe.localPort();
    portProbe.close();

    DepayloaderOutput output;
    std::atomic<int> sentFrames{0};

    // A GOP far longer than the test: the receiver joins after the only keyframe until the test forces one.
    GstTest::TestPipeline sender;
    QVERIFY2(
        sender.launch(QStringLiteral("videotestsrc is-live=true ! video/x-raw,width=160,height=120,framerate=30/1 ! "
                                     "x264enc name=encoder tune=zerolatency speed-preset=ultrafast "
                                     "key-int-max=100000 ! rtph264pay config-interval=-1 pt=96 ! "
                                     "udpsink host=127.0.0.1 port=%1 sync=false async=false")
                          .arg(port)),
        qPrintable(sender.error()));
    const GStreamer::GstElementPtr encoder = sender.element("encoder");
    QVERIFY(encoder);
    const GStreamer::GstPadPtr encoderSrc = GStreamer::adoptPad(gst_element_get_static_pad(encoder.get(), "src"));
    (void) gst_pad_add_probe(encoderSrc.get(), GST_PAD_PROBE_TYPE_BUFFER, GstTest::countBuffers, &sentFrames, nullptr);

    GStreamer::SourceFactory::Config config;
    const GStreamer::GstElementPtr source = createSource(QStringLiteral("udp://127.0.0.1:%1").arg(port), config);
    QVERIFY(source);
    (void) g_signal_connect(source.get(), "deep-element-added", G_CALLBACK(watchH264Depayloading), &output);

    GstTest::TestPipeline receiver(gst_pipeline_new("join-mid-gop"));
    GstElement* const sink = gst_element_factory_make("fakesink", nullptr);
    QVERIFY(sink);
    g_object_set(sink, "sync", FALSE, "async", FALSE, nullptr);
    QVERIFY(gst_bin_add(receiver.bin(), source.get()));
    QVERIFY(gst_bin_add(receiver.bin(), sink));
    (void) g_signal_connect(source.get(), "pad-added", G_CALLBACK(linkToSink), sink);

    QVERIFY(sender.play());
    QVERIFY_TRUE_WAIT(sentFrames.load() >= 5, TestTimeout::mediumMs());
    QVERIFY(receiver.play());

    QVERIFY_TRUE_WAIT(output.packetsIn.load() >= 20, TestTimeout::mediumMs());
    QCOMPARE(output.unitsOut.load(), 0);

    QVERIFY(gst_pad_send_event(encoderSrc.get(),
                               gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE, TRUE, 0)));
    QVERIFY_TRUE_WAIT(output.unitsOut.load() > 0, TestTimeout::mediumMs());
    QVERIFY2(output.firstUnitHasSps.load(), "the first unit reaching h264parse must carry the SPS");
}

void GStreamerTest::_testParameterSetGateHoldsUntilCompleteSet()
{
    const QByteArray aud = QByteArray::fromHex("0000000109f0");
    const QByteArray sps = QByteArray::fromHex("0000000167640014acd9");
    const QByteArray pps = QByteArray::fromHex("0000000168ebe3cb");
    const QByteArray idr = QByteArray::fromHex("0000000165888400");
    const QByteArray delta = QByteArray::fromHex("00000001419a2400");

    ReceivedBuffers received;
    const GatedPads pads(&received, "video/x-h264, stream-format=(string)byte-stream, alignment=(string)nal");
    QVERIFY(pads.linked);
    GStreamer::SourceFactory::gateOnParameterSets(pads.src.get(), false);

    // A late join whose first keyframe lost its PPS (and IDR): nothing may pass on an SPS alone.
    for (const QByteArray& nal : {aud, sps, delta, delta, aud, sps}) {
        QCOMPARE(pads.push(nal, GST_SECOND), GST_FLOW_OK);
    }
    QVERIFY(received.data.isEmpty());

    QCOMPARE(pads.push(pps, 2 * GST_SECOND), GST_FLOW_OK);
    QVERIFY(received.data.isEmpty());
    QCOMPARE(pads.push(idr, 3 * GST_SECOND), GST_FLOW_OK);
    QCOMPARE(pads.push(delta, 4 * GST_SECOND), GST_FLOW_OK);

    QCOMPARE(received.data.size(), 2);
    QCOMPARE(received.data.at(0), sps + pps + idr);
    QCOMPARE(received.pts.at(0), 3 * GST_SECOND);
    QCOMPARE(received.data.at(1), delta);
}

void GStreamerTest::_testParameterSetGatePassesLengthPrefixedOutput()
{
    ReceivedBuffers received;
    const GatedPads pads(&received, "video/x-h264, stream-format=(string)avc, alignment=(string)au");
    QVERIFY(pads.linked);
    GStreamer::SourceFactory::gateOnParameterSets(pads.src.get(), false);

    // avc carries its parameter sets in codec_data, so its first slice passes as is.
    const QByteArray lengthPrefixedSlice = QByteArray::fromHex("000000044188a5f0");
    QCOMPARE(pads.push(lengthPrefixedSlice, 0), GST_FLOW_OK);
    QCOMPARE(received.data, QList<QByteArray>{lengthPrefixedSlice});
}

void GStreamerTest::_testSourceFactoryUdpMpegTs()
{
    const QString missing = GstTest::firstMissingFactory({"udpsrc", "tsdemux"});
    if (!missing.isEmpty()) {
        QSKIP(qPrintable(QStringLiteral("%1 plugin unavailable").arg(missing)));
    }

    GStreamer::SourceFactory::Config config;
    const GStreamer::GstElementPtr bin = createSource(QStringLiteral("mpegts://127.0.0.1:5600"), config);
    QVERIFY(bin);

    const GStreamer::GstElementPtr src = child(bin.get(), "udpsrc");
    QVERIFY2(src, "mpegts:// must build a udpsrc");
    GstCaps* rawCaps = nullptr;
    g_object_get(src.get(), "caps", &rawCaps, nullptr);
    QVERIFY2(!GStreamer::adoptCaps(rawCaps), "raw MPEG-TS over UDP must not force RTP caps on udpsrc");

    QVERIFY2(child(bin.get(), "tsdemux"), "mpegts:// must insert tsdemux");
    QVERIFY2(!child(bin.get(), "rtpjitterbuffer"), "raw MPEG-TS is not RTP; no jitterbuffer");
}

void GStreamerTest::_testSourceFactorySchemeCaseInsensitive()
{
    if (!GstTest::firstMissingFactory({"udpsrc"}).isEmpty()) {
        QSKIP("udpsrc plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    QVERIFY2(createSource(QStringLiteral("UDP://127.0.0.1:5600"), config),
             "uppercase scheme must be accepted (scheme is lower-cased before matching)");
}

void GStreamerTest::_testSourceFactoryNegativeLatencyClamped()
{
    const QString missing = GstTest::firstMissingFactory({"udpsrc", "rtpjitterbuffer"});
    if (!missing.isEmpty()) {
        QSKIP(qPrintable(QStringLiteral("%1 plugin unavailable").arg(missing)));
    }

    GStreamer::SourceFactory::Config config;
    config.latencyMs = -100;
    const GStreamer::GstElementPtr bin = createSource(QStringLiteral("udp://127.0.0.1:5600"), config);
    QVERIFY(bin);

    const GStreamer::GstElementPtr jb = child(bin.get(), "rtpjitterbuffer");
    QVERIFY(jb);
    guint latency = 99999;
    g_object_get(jb.get(), "latency", &latency, nullptr);
    QCOMPARE(latency, 0u);
}

void GStreamerTest::_testSourceFactoryDynamicRtpLinkFailureCleansJitterBuffer()
{
    const QString missing = GstTest::firstMissingFactory({"udpsrc", "tsdemux", "rtpjitterbuffer"});
    if (!missing.isEmpty()) {
        QSKIP(qPrintable(QStringLiteral("%1 plugin unavailable").arg(missing)));
    }

    GStreamer::SourceFactory::Config config;
    config.jitterBuffer = GStreamer::SourceFactory::JitterBuffer::DropOnLatency;
    const MpegTsSource source(config);
    QVERIFY(source.isValid());

    // A running bin syncs the temporary jitterbuffer out of NULL, so removing it without a NULL
    // transition raises GStreamer's "disposing element in READY/PLAYING" critical. The source stays
    // locked in NULL so the test doesn't bind the UDP port.
    const GStreamer::GstElementPtr udpsrc = child(source.bin.get(), "udpsrc");
    QVERIFY(udpsrc);
    (void) gst_element_set_locked_state(udpsrc.get(), TRUE);
    QCOMPARE(gst_element_set_state(source.bin.get(), GST_STATE_READY), GST_STATE_CHANGE_SUCCESS);

    // Not added to tsdemux, so linking it to the jitterbuffer fails.
    const GStreamer::GstPadPtr strayPad = GStreamer::adoptPad(
        GST_PAD(gst_object_ref_sink(gst_pad_new_from_static_template(&sH264RtpPadTemplate, "stray_rtp"))));
    QVERIFY(strayPad);

    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("gst_element_link_pads\\(\\) failed")));
    g_signal_emit_by_name(source.demux.get(), "pad-added", strayPad.get());

    QVERIFY2(!gst_pad_is_linked(source.parserSink.get()),
             "a failed dynamic RTP pad link must leave parsebin available for the next valid pad");
    QVERIFY2(!child(source.bin.get(), "rtpjitterbuffer"),
             "a failed dynamic RTP pad link must remove its temporary jitterbuffer");
}

void GStreamerTest::_testSourceFactoryDynamicLinkSkipsNonVideoRtp()
{
    const QString missing = GstTest::firstMissingFactory({"udpsrc", "tsdemux", "rtpjitterbuffer"});
    if (!missing.isEmpty()) {
        QSKIP(qPrintable(QStringLiteral("%1 plugin unavailable").arg(missing)));
    }

    GStreamer::SourceFactory::Config config;
    const MpegTsSource source(config);
    QVERIFY(source.isValid());

    // Mirrors an RTSP SDP listing audio before video: the first RTP pad must not claim the parser.
    static GstStaticPadTemplate sAudioTemplate = GST_STATIC_PAD_TEMPLATE(
        "audio_rtp", GST_PAD_SRC, GST_PAD_SOMETIMES,
        GST_STATIC_CAPS("application/x-rtp, media=(string)audio, clock-rate=(int)48000, encoding-name=(string)OPUS"));

    // gst_element_add_pad emits pad-added and consumes the floating pad.
    QVERIFY(gst_element_add_pad(source.demux.get(), gst_pad_new_from_static_template(&sAudioTemplate, "audio_rtp")));
    QVERIFY2(!gst_pad_is_linked(source.parserSink.get()), "an audio RTP pad must not be linked to the video parser");
    QVERIFY2(!child(source.bin.get(), "rtpjitterbuffer"), "an audio RTP pad must not get a jitterbuffer");

    QVERIFY(
        gst_element_add_pad(source.demux.get(), gst_pad_new_from_static_template(&sH264RtpPadTemplate, "video_rtp")));
    QVERIFY2(gst_pad_is_linked(source.parserSink.get()), "the video RTP pad that follows must still reach the parser");
}

namespace {

GstSample* tryPullSampleOrPreroll(GstAppSink* sink, GstClockTime timeout = 0)
{
    if (GstSample* sample = gst_app_sink_try_pull_sample(sink, timeout)) {
        return sample;
    }
    return gst_app_sink_try_pull_preroll(sink, 0);
}

// Borrowed (bin-owned) first child whose element-factory name matches, or nullptr.
GstElement* findChildByFactoryName(GstElement* bin, const char* factoryName)
{
    GstIterator* it = gst_bin_iterate_elements(GST_BIN(bin));
    GValue item = G_VALUE_INIT;
    GstElement* match = nullptr;
    bool done = false;
    while (!done) {
        switch (gst_iterator_next(it, &item)) {
            case GST_ITERATOR_OK: {
                GstElement* child = GST_ELEMENT(g_value_get_object(&item));
                GstElementFactory* f = gst_element_get_factory(child);
                if (f && (g_strcmp0(GST_OBJECT_NAME(f), factoryName) == 0)) {
                    match = child;
                    done = true;
                }
                g_value_reset(&item);
                break;
            }
            case GST_ITERATOR_RESYNC:
                gst_iterator_resync(it);
                break;
            default:
                done = true;
                break;
        }
    }
    g_value_unset(&item);
    gst_iterator_free(it);
    return match;
}

QByteArray makeTestJpeg(int width = 4, int height = 4, bool progressive = false)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::green);

    QByteArray jpeg;
    QBuffer buffer(&jpeg);
    if (!buffer.open(QIODevice::WriteOnly)) {
        return {};
    }

    QImageWriter writer(&buffer, "JPEG");
    writer.setProgressiveScanWrite(progressive);
    if (!writer.write(image)) {
        return {};
    }
    return jpeg;
}

QByteArray withJpegDimensions(QByteArray jpeg, quint16 width, quint16 height)
{
    constexpr std::array<unsigned char, 9> kStartOfFrameMarkers = {
        0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA,
    };

    for (const unsigned char markerCode : kStartOfFrameMarkers) {
        QByteArray marker;
        marker.append(static_cast<char>(0xFF));
        marker.append(static_cast<char>(markerCode));
        const qsizetype markerOffset = jpeg.indexOf(marker);
        if ((markerOffset < 0) || ((markerOffset + 8) >= jpeg.size())) {
            continue;
        }

        jpeg[markerOffset + 5] = static_cast<char>((height >> 8) & 0xFF);
        jpeg[markerOffset + 6] = static_cast<char>(height & 0xFF);
        jpeg[markerOffset + 7] = static_cast<char>((width >> 8) & 0xFF);
        jpeg[markerOffset + 8] = static_cast<char>(width & 0xFF);
        return jpeg;
    }

    return {};
}

QSize jpegDimensions(const QByteArray& jpeg)
{
    QBuffer buffer;
    buffer.setData(jpeg);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return {};
    }

    QImageReader reader(&buffer, "JPEG");
    return reader.size();
}

// Throwaway localhost-only test material. It is intentionally not stored as a PEM credential.
QSslCertificate testTlsCertificate()
{
    static constexpr auto kCertificateDer =
        "MIIBmzCCAUGgAwIBAgIUFbcEH9kmwztc5b9Ha3cY4x9FDO8wCgYIKoZIzj0EAwIwFDESMBAGA1UEAwwJbG9jYWxob3N0MCAX"
        "DTI2MDcyODA2MjMwOFoYDzIxMjYwNzA0MDYyMzA4WjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwWTATBgcqhkjOPQIBBggqhkjO"
        "PQMBBwNCAAR4OSsMTUhwl3a5KP18gdm/QNswKq5NFg0JqeUNF7BgFzAa10WVKA7/c6TEoaKsSn32iXesOUYI7Fs1WH7zFsjA"
        "o28wbTAdBgNVHQ4EFgQUvOEXyJbTXCPncAQ5EWRp6XuZ1WgwHwYDVR0jBBgwFoAUvOEXyJbTXCPncAQ5EWRp6XuZ1WgwDwYD"
        "VR0TAQH/BAUwAwEB/zAaBgNVHREEEzARgglsb2NhbGhvc3SHBH8AAAEwCgYIKoZIzj0EAwIDSAAwRQIhAP8NQO0HrtST24NQ"
        "2BiGZOoyNmjgnlJ7U9vkDcwNXjYKAiBkOpNock+KqNb9qooUWnkxz0VUBbkKBcs9EFtS3EQndw==";
    return QSslCertificate(QByteArray::fromBase64(QByteArray(kCertificateDer)), QSsl::Der);
}

QSslKey testTlsPrivateKey()
{
    static constexpr auto kPrivateKeyDer =
        "MHcCAQEEIEdHEf9aQn1QFwpXUAWdVOLDXr8LbWlidiIpShmGvco5oAoGCCqGSM49AwEHoUQDQgAEeDkrDE1IcJd2uSj9fIHZ"
        "v0DbMCquTRYNCanlDRewYBcwGtdFlSgO/3OkxKGirEp99ol3rDlGCOxbNVh+8xbIwA==";
    return QSslKey(QByteArray::fromBase64(QByteArray(kPrivateKeyDer)), QSsl::Ec, QSsl::Der, QSsl::PrivateKey);
}

bool configureSecureTestServer(QWebSocketServer& server, const QSslCertificate& certificate, const QSslKey& privateKey)
{
    if (certificate.isNull() || privateKey.isNull()) {
        return false;
    }

    QSslConfiguration configuration = server.sslConfiguration();
    configuration.setLocalCertificate(certificate);
    configuration.setPrivateKey(privateKey);
    server.setSslConfiguration(configuration);
    return true;
}

std::shared_ptr<QGCWebSocketVideoOptions> nativeVideoOptions()
{
    auto options = std::make_shared<QGCWebSocketVideoOptions>();
    options->cookie = "session=local-test-session";
    options->origin = QStringLiteral("https://operator.example.test");
    options->requireFrameMetadata = true;
    options->frameContexts = std::make_shared<QGCVideoFrameContextStore>();
    return options;
}

QJsonObject nativeFrameMetadata(qint64 id, const QByteArray& jpeg)
{
    const QSize dimensions = jpegDimensions(jpeg);
    return {{"type", "frame"},
            {"timestamp", 1234.5},
            {"quality", 85},
            {"size", jpeg.size()},
            {"frame_id", id},
            {"frame_age_ms", 2.0},
            {"provenance", QJsonObject{{"version", "1"},
                                       {"instance_id", "instance-a"},
                                       {"runtime_id", "runtime-a"},
                                       {"stream_id", "front"},
                                       {"stream_epoch", "stream-1"},
                                       {"source_epoch", "source-1"},
                                       {"frame_id", QString::number(id)},
                                       {"capture_id", QString::number(id)},
                                       {"capture_state", "fresh"},
                                       {"capture_age_ms", 2.0},
                                       {"publication_age_ms", 1.0},
                                       {"encoded_width", dimensions.width()},
                                       {"encoded_height", dimensions.height()},
                                       {"variant", "processed_osd"},
                                       {"geometry_verified", false}}}};
}

void sendMetadata(QWebSocket* peer, const QJsonObject& metadata)
{
    QJsonObject wireMetadata = metadata;
    if (wireMetadata.value("type").toString() == QLatin1String("frame") && !wireMetadata.contains("delivery_token")) {
        wireMetadata.insert("delivery_token", peer->property("deliveryToken").toString());
    }
    peer->sendTextMessage(QString::fromUtf8(QJsonDocument(wireMetadata).toJson(QJsonDocument::Compact)));
}

class NativeWebSocketPeer
{
public:
    explicit NativeWebSocketPeer(QWebSocketServer& server)
    {
        _connection = QObject::connect(&server, &QWebSocketServer::newConnection, &server, [this, &server] {
            peer = server.nextPendingConnection();
            if (peer) {
                messages = std::make_unique<QSignalSpy>(peer, &QWebSocket::textMessageReceived);
                QObject::connect(peer, &QWebSocket::textMessageReceived, peer, [socket = peer](const QString& text) {
                    const QJsonObject message = QJsonDocument::fromJson(text.toUtf8()).object();
                    if (message.value("delivery_token").isString()) {
                        socket->setProperty("deliveryToken", message.value("delivery_token").toString());
                        socket->setProperty("deliveryReceivedMs", QGCVideoFrameContextStore::monotonicMs());
                    }
                });
            }
        });
    }

    ~NativeWebSocketPeer() { QObject::disconnect(_connection); }

    QPointer<QWebSocket> peer;
    std::unique_ptr<QSignalSpy> messages;

private:
    QMetaObject::Connection _connection;
};

class NativeVideoPipeline
{
public:
    ~NativeVideoPipeline()
    {
        if (_pipeline) {
            GStreamer::SourceFactory::deactivate(source);
            gst_element_set_state(_pipeline, GST_STATE_NULL);
            gst_object_unref(_pipeline);
        }
    }

    bool start(const QString& url, const std::shared_ptr<const QGCWebSocketVideoOptions>& options)
    {
        GStreamer::SourceFactory::Config config;
        config.webSocketOptions = options;
        source = GStreamer::SourceFactory::create(url, config);
        GstElement* decoder = gst_element_factory_make("jpegdec", nullptr);
        sink = gst_element_factory_make("appsink", nullptr);
        _pipeline = gst_pipeline_new(nullptr);
        if (!source || !decoder || !sink || !_pipeline) {
            gst_clear_object(&source);
            gst_clear_object(&decoder);
            gst_clear_object(&sink);
            return false;
        }
        g_object_set(sink, "sync", FALSE, "max-buffers", 16u, "drop", TRUE, nullptr);
        gst_bin_add_many(GST_BIN(_pipeline), source, decoder, sink, nullptr);
        return gst_element_link_many(source, decoder, sink, nullptr) &&
               gst_element_set_state(_pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE &&
               GStreamer::SourceFactory::activate(source);
    }

    GstMessage* error() const
    {
        GstBus* bus = gst_element_get_bus(_pipeline);
        GstMessage* message = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR);
        gst_object_unref(bus);
        return message;
    }

    GstElement* source = nullptr;
    GstElement* sink = nullptr;

private:
    GstElement* _pipeline = nullptr;
};

}  // namespace

void GStreamerTest::_testWebSocketFrameContextEvictionAndEpochs()
{
    QGCVideoFrameContextStore store(2);
    const quint64 firstEpoch = store.beginEpoch();
    const qint64 first = store.insert(firstEpoch, {{"frame_id", "1"}});
    const auto firstReceipt = store.lookup(first);
    QVERIFY(firstReceipt);
    const qint64 second = store.insert(firstEpoch, {{"frame_id", "2"}});
    const qint64 earlierArrival = QGCVideoFrameContextStore::monotonicMs() - 37;
    const qint64 third = store.insert(
        firstEpoch, {{"frame_id", "3"}, {"provenance", QJsonObject{{"capture_age_ms", 25}}}}, earlierArrival);
    QVERIFY(first >= 0 && first < second && second < third);
    QVERIFY(!store.lookup(first));
    QVERIFY(store.lookup(second));
    QCOMPARE(store.lookup(third)->metadata.value("frame_id").toString(), QStringLiteral("3"));
    QCOMPARE(store.lookup(third)->receivedMonotonicMs, earlierArrival);
    QCOMPARE(store.presentationStatistics().value("verified_frames").toInteger(), qint64(3));
    QCOMPARE(store.presentationStatistics().value("presented_frames").toInteger(), qint64(0));
    store.markPresented(first, *firstReceipt);
    store.markPresented(first, *firstReceipt);
    QCOMPARE(store.presentationStatistics().value("presented_frames").toInteger(), qint64(1));
    store.markPresented(third, *store.lookup(third));
    store.markPresented(second, *store.lookup(second));
    QCOMPARE(store.presentationStatistics().value("presented_frames").toInteger(), qint64(2));
    QVERIFY(store.presentationStatistics().value("last_presented_age_bound_ms").toDouble() >= 62);

    const quint64 secondEpoch = store.beginEpoch();
    QVERIFY(!store.lookup(third));
    store.markPresented(first, *firstReceipt);
    QCOMPARE(store.presentationStatistics().value("presented_frames").toInteger(), qint64(0));
    QCOMPARE(store.presentationStatistics().value("verified_frames").toInteger(), qint64(0));
    QCOMPARE(store.insert(firstEpoch, {{"frame_id", "late"}}), qint64(-1));
    const qint64 restarted = store.insert(secondEpoch, {{"frame_id", "1"}});
    store.endEpoch(firstEpoch);
    QVERIFY(store.lookup(restarted));
    QVERIFY(restarted > third);
    QGCVideoFrameContextStore otherStore;
    const qint64 other = otherStore.insert(otherStore.beginEpoch(), {});
    QVERIFY(other > restarted);
    QVERIFY(!store.lookup(other));
    store.endEpoch(secondEpoch);
    QVERIFY(!store.lookup(restarted));
    QVERIFY(store.presentationStatistics().isEmpty());
}

void GStreamerTest::_testWebSocketNativeFramesSurviveDrops()
{
    QWebSocketServer server(QStringLiteral("Native JPEG"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    auto options = nativeVideoOptions();
    const auto store = options->frameContexts;
    NativeWebSocketPeer connection(server);
    NativeVideoPipeline pipeline;
    QVERIFY(pipeline.start(QStringLiteral("ws://127.0.0.1:%1/prefix/ws/video_feed").arg(server.serverPort()), options));
    QTRY_VERIFY_WITH_TIMEOUT(connection.peer, TestTimeout::mediumMs());
    QWebSocket* peer = connection.peer;
    QVERIFY(peer);
    QCOMPARE(peer->request().rawHeader("Cookie"), options->cookie);
    QCOMPARE(peer->origin(), options->origin);
    QCOMPARE(peer->requestUrl().path(), QStringLiteral("/prefix/ws/video_feed"));

    QSignalSpy& messages = *connection.messages;
    QTRY_VERIFY_WITH_TIMEOUT(!messages.isEmpty(), TestTimeout::mediumMs());
    const QJsonObject capabilities = QJsonDocument::fromJson(messages.takeFirst().at(0).toString().toUtf8()).object();
    QCOMPARE(capabilities.value("type").toString(), QStringLiteral("stream_capabilities"));
    QVERIFY(capabilities.value("latest_frame_ack").toBool());
    QVERIFY(!capabilities.value("delivery_token").toString().isEmpty());

    GstElement* appsrc = findChildByFactoryName(pipeline.source, "appsrc");
    QVERIFY(appsrc);
    GstPad* pad = gst_element_get_static_pad(appsrc, "src");
    QVERIFY(pad);
    const gulong block = gst_pad_add_probe(
        pad, static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM | GST_PAD_PROBE_TYPE_BUFFER),
        [](GstPad*, GstPadProbeInfo*, gpointer) { return GST_PAD_PROBE_OK; }, nullptr, nullptr);
    QVERIFY(block);
    bool released = false;
    const auto unblock = qScopeGuard([&] {
        if (!released) {
            gst_pad_remove_probe(pad, block);
        }
        gst_object_unref(pad);
    });
    QMap<qint64, qint64> latestTokenReceipt;
    QMap<qint64, QJsonObject> expectedMetadata;
    for (qint64 id = 1; id <= 8; ++id) {
        const QByteArray jpeg = makeTestJpeg(static_cast<int>(id) + 4, 4);
        QJsonObject metadata = nativeFrameMetadata(id, jpeg);
        metadata.insert("delivery_token", peer->property("deliveryToken").toString());
        latestTokenReceipt.insert(id, peer->property("deliveryReceivedMs").toLongLong());
        expectedMetadata.insert(id, metadata);
        sendMetadata(peer, metadata);
        sendMetadata(peer, {{"type", "pong"}, {"timestamp", 4567.0}});
        peer->sendBinaryMessage(jpeg);
        QTRY_VERIFY_WITH_TIMEOUT(!messages.isEmpty(), TestTimeout::mediumMs());
        const QJsonObject ack = QJsonDocument::fromJson(messages.takeFirst().at(0).toString().toUtf8()).object();
        QCOMPARE(ack.value("type").toString(), QStringLiteral("frame_ack"));
        QCOMPARE(ack.value("frame_id").toInteger(), id);
    }
    guint64 queued = 0;
    g_object_get(appsrc, "current-level-buffers", &queued, nullptr);
    QCOMPARE(queued, quint64(2));
    gst_pad_remove_probe(pad, block);
    released = true;

    QList<qint64> received;
    bool contextsMatch = true;
    auto drain = [&]() {
        while (GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(pipeline.sink), 0)) {
            const auto release = qScopeGuard([&] { gst_sample_unref(sample); });
            GstBuffer* buffer = gst_sample_get_buffer(sample);
            const qint64 ptsUs = static_cast<qint64>(GST_BUFFER_PTS(buffer) / GST_USECOND);
            const auto entry = store->lookup(ptsUs);
            if (!entry) {
                contextsMatch = false;
                return false;
            }
            int width = 0;
            const qint64 id = entry->metadata.value("frame_id").toInteger();
            if (!gst_structure_get_int(gst_caps_get_structure(gst_sample_get_caps(sample), 0), "width", &width) ||
                width != id + 4 || entry->metadata != expectedMetadata.value(id) ||
                entry->receivedMonotonicMs > latestTokenReceipt.value(id)) {
                contextsMatch = false;
                return false;
            }
            received.append(entry->metadata.value("frame_id").toInteger());
        }
        return true;
    };
    QTRY_VERIFY_WITH_TIMEOUT((drain() && received.contains(8)), TestTimeout::mediumMs());
    QVERIFY(contextsMatch);
    QVERIFY(received.size() < 8);
}

void GStreamerTest::_testWebSocketNativeRejectsAmbiguousFrames_data()
{
    QTest::addColumn<QString>("fault");
    for (const char* fault : {"missing", "duplicate-metadata", "wrong-size", "wrong-dimensions", "wrong-id",
                              "duplicate-frame", "late-metadata", "malformed-json", "fractional-id", "wrong-token",
                              "replayed-token", "missing-negotiated-token"}) {
        QTest::newRow(fault) << QString::fromLatin1(fault);
    }
}

void GStreamerTest::_testWebSocketNativeRejectsAmbiguousFrames()
{
    QFETCH(QString, fault);
    QWebSocketServer server(QStringLiteral("Invalid native JPEG"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    auto options = nativeVideoOptions();
    NativeWebSocketPeer connection(server);
    NativeVideoPipeline pipeline;
    QVERIFY(pipeline.start(QStringLiteral("ws://127.0.0.1:%1/video").arg(server.serverPort()), options));
    QTRY_VERIFY_WITH_TIMEOUT(connection.peer, TestTimeout::mediumMs());
    QWebSocket* peer = connection.peer;
    QVERIFY(peer);
    QSignalSpy& messages = *connection.messages;
    QTRY_VERIFY_WITH_TIMEOUT(!messages.isEmpty(), TestTimeout::mediumMs());
    messages.clear();

    const QByteArray jpeg = makeTestJpeg();
    QJsonObject metadata = nativeFrameMetadata(1, jpeg);
    if (fault == QLatin1String("duplicate-frame") || fault == QLatin1String("replayed-token") ||
        fault == QLatin1String("missing-negotiated-token")) {
        const QString previousToken = peer->property("deliveryToken").toString();
        sendMetadata(peer, metadata);
        peer->sendBinaryMessage(jpeg);
        QTRY_VERIFY_WITH_TIMEOUT(!messages.isEmpty(), TestTimeout::mediumMs());
        messages.clear();
        if (fault != QLatin1String("duplicate-frame")) {
            metadata = nativeFrameMetadata(2, jpeg);
            metadata.insert("delivery_token", fault == QLatin1String("replayed-token") ? QJsonValue(previousToken)
                                                                                       : QJsonValue(QJsonValue::Null));
        }
    } else if (fault == QLatin1String("wrong-size")) {
        metadata.insert("size", jpeg.size() + 1);
    } else if (fault == QLatin1String("wrong-dimensions") || fault == QLatin1String("wrong-id")) {
        QJsonObject provenance = metadata.value("provenance").toObject();
        provenance.insert(fault == QLatin1String("wrong-id") ? "frame_id" : "encoded_width",
                          fault == QLatin1String("wrong-id") ? QJsonValue("2") : QJsonValue(5));
        metadata.insert("provenance", provenance);
    } else if (fault == QLatin1String("fractional-id")) {
        metadata.insert("frame_id", 1.5);
    } else if (fault == QLatin1String("wrong-token")) {
        metadata.insert("delivery_token", "unrequested-token");
    }
    if (fault == QLatin1String("malformed-json")) {
        peer->sendTextMessage(QStringLiteral("{invalid"));
    } else if (fault != QLatin1String("missing")) {
        sendMetadata(peer, metadata);
    }
    if (fault == QLatin1String("duplicate-metadata")) {
        sendMetadata(peer, metadata);
    }
    if (fault == QLatin1String("late-metadata")) {
        QTimer::singleShot(3100, Qt::PreciseTimer, peer, [peer, jpeg] { peer->sendBinaryMessage(jpeg); });
    } else {
        peer->sendBinaryMessage(jpeg);
    }
    GstMessage* error = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(error || (error = pipeline.error()), TestTimeout::longMs());
    const auto release = qScopeGuard([&] { gst_message_unref(error); });
    GError* detail = nullptr;
    gst_message_parse_error(error, &detail, nullptr);
    QVERIFY(detail);
    QCOMPARE(QString::fromUtf8(detail->message), QStringLiteral("WebSocket frame context was rejected"));
    g_clear_error(&detail);
    QVERIFY(messages.isEmpty());
    QVERIFY(!options->frameContexts->isCurrentEpoch(1));
}

void GStreamerTest::_testWebSocketNativeSessionIsolation()
{
    QWebSocketServer firstServer(QStringLiteral("First companion"), QWebSocketServer::NonSecureMode);
    QWebSocketServer secondServer(QStringLiteral("Second companion"), QWebSocketServer::NonSecureMode);
    QVERIFY(firstServer.listen(QHostAddress::LocalHost, 0));
    QVERIFY(secondServer.listen(QHostAddress::LocalHost, 0));
    auto firstOptions = nativeVideoOptions();
    auto secondOptions = nativeVideoOptions();
    secondOptions->cookie = "session=other-session";
    secondOptions->frameContexts = firstOptions->frameContexts;
    qint64 previousPts = -1;
    const QByteArray jpeg = makeTestJpeg();
    for (int run = 0; run != 2; ++run) {
        QWebSocketServer& server = run ? secondServer : firstServer;
        const auto options = run ? secondOptions : firstOptions;
        NativeWebSocketPeer connection(server);
        NativeVideoPipeline pipeline;
        const QByteArray expectedCookie = options->cookie;
        QVERIFY(pipeline.start(QStringLiteral("ws://127.0.0.1:%1/video").arg(server.serverPort()), options));
        options->cookie = "session=changed-after-source-construction";
        QTRY_VERIFY_WITH_TIMEOUT(connection.peer, TestTimeout::mediumMs());
        QWebSocket* peer = connection.peer;
        QVERIFY(peer);
        QCOMPARE(peer->request().rawHeader("Cookie"), expectedCookie);
        QSignalSpy& messages = *connection.messages;
        QTRY_VERIFY_WITH_TIMEOUT(!messages.isEmpty(), TestTimeout::mediumMs());
        const QString initialToken = peer->property("deliveryToken").toString();
        const qint64 tokenReceipt = peer->property("deliveryReceivedMs").toLongLong();
        messages.clear();
        // This pair was emitted before the server processed capability negotiation.
        peer->sendTextMessage(QString::fromUtf8(QJsonDocument(nativeFrameMetadata(1, jpeg)).toJson()));
        peer->sendBinaryMessage(jpeg);
        QTRY_VERIFY_WITH_TIMEOUT(!messages.isEmpty(), TestTimeout::mediumMs());
        QCOMPARE(peer->property("deliveryToken").toString(), initialToken);
        QVERIFY(!gst_app_sink_try_pull_sample(GST_APP_SINK(pipeline.sink), 0));
        constexpr qint64 frameId = 9007199254740993LL;
        QTimer::singleShot(run ? 0 : 250, peer, [peer, jpeg] {
            sendMetadata(peer, nativeFrameMetadata(frameId, jpeg));
            peer->sendBinaryMessage(jpeg);
        });
        GstSample* sample = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT(sample || (sample = gst_app_sink_try_pull_sample(GST_APP_SINK(pipeline.sink), 0)),
                                 TestTimeout::mediumMs());
        const qint64 pts = static_cast<qint64>(GST_BUFFER_PTS(gst_sample_get_buffer(sample)) / GST_USECOND);
        gst_sample_unref(sample);
        QVERIFY(pts > previousPts);
        QVERIFY(options->frameContexts->lookup(pts));
        QCOMPARE(options->frameContexts->lookup(pts)->metadata.value("frame_id").toInteger(), frameId);
        QVERIFY(options->frameContexts->lookup(pts)->receivedMonotonicMs <= tokenReceipt);
        if (!run) {
            QVERIFY(QGCVideoFrameContextStore::monotonicMs() -
                        options->frameContexts->lookup(pts)->receivedMonotonicMs >=
                    200);
        }
        QVERIFY(!options->frameContexts->lookup(previousPts));
        previousPts = pts;
        GStreamer::SourceFactory::deactivate(pipeline.source);
        QVERIFY(!options->frameContexts->lookup(pts));
    }
}

void GStreamerTest::_testWebSocketNativeRedirectRejected()
{
    TestFixtures::LocalHttpTestServer redirect;
    QWebSocketServer destination(QStringLiteral("Must not receive credentials"), QWebSocketServer::NonSecureMode);
    QVERIFY(redirect.listen());
    QVERIFY(destination.listen(QHostAddress::LocalHost, 0));
    redirect.installRawResponder(
        "HTTP/1.1 302 Found\r\nLocation: ws://127.0.0.1:" + QByteArray::number(destination.serverPort()) +
        "/video\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    expectLogMessage("Video.GStreamer.WebSocketVideoSource", QtWarningMsg,
                     QRegularExpression(QStringLiteral("WebSocket transport error code")));
    NativeVideoPipeline pipeline;
    auto options = nativeVideoOptions();
    QVERIFY(pipeline.start(QStringLiteral("ws://127.0.0.1:%1/video").arg(redirect.port()), options));
    GstMessage* error = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(error || (error = pipeline.error()), TestTimeout::mediumMs());
    gst_message_unref(error);
    verifyExpectedLogMessage();
    QVERIFY(!destination.hasPendingConnections());
    QVERIFY(!options->frameContexts->isCurrentEpoch(1));
}

void GStreamerTest::_testSourceFactoryHttpMjpeg()
{
    if (!gst_element_factory_find("souphttpsrc") || !gst_element_factory_find("multipartdemux") ||
        !gst_element_factory_find("jpegparse")) {
        QSKIP("souphttpsrc/multipartdemux/jpegparse plugins unavailable");
    }

    GStreamer::SourceFactory::Config config;
    config.timeoutS = 9;
    GstElement* bin = GStreamer::SourceFactory::create(
        QStringLiteral("https://video.example.test:8443/camera.mjpg?quality=80#local-view"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    GstElement* source = findChildByFactoryName(bin, "souphttpsrc");
    GstElement* demux = findChildByFactoryName(bin, "multipartdemux");
    GstElement* parser = findChildByFactoryName(bin, "jpegparse");
    QVERIFY(source);
    QVERIFY(demux);
    QVERIFY(parser);

    gchar* location = nullptr;
    gchar* method = nullptr;
    gboolean isLive = FALSE;
    gboolean doTimestamp = FALSE;
    gboolean keepAlive = FALSE;
    gboolean compress = TRUE;
    gboolean iradioMode = TRUE;
    gboolean automaticRedirect = TRUE;
    gboolean sslStrict = FALSE;
    gboolean singleStream = FALSE;
    gint retries = -1;
    guint timeout = 0;
    gint httpLogLevel = -1;
    gchar* userAgent = nullptr;
    g_object_get(source, "location", &location, "method", &method, "is-live", &isLive, "do-timestamp", &doTimestamp,
                 "keep-alive", &keepAlive, "compress", &compress, "iradio-mode", &iradioMode, "automatic-redirect",
                 &automaticRedirect, "retries", &retries, "timeout", &timeout, "ssl-strict", &sslStrict,
                 "http-log-level", &httpLogLevel, "user-agent", &userAgent, nullptr);
    const auto stringsCleanup = qScopeGuard([&] {
        g_free(location);
        g_free(method);
        g_free(userAgent);
    });
    g_object_get(demux, "single-stream", &singleStream, nullptr);

    QCOMPARE(QString::fromUtf8(location), QStringLiteral("https://video.example.test:8443/camera.mjpg?quality=80"));
    QCOMPARE(QString::fromUtf8(method), QStringLiteral("GET"));
    QCOMPARE(isLive, TRUE);
    QCOMPARE(doTimestamp, TRUE);
    QCOMPARE(keepAlive, TRUE);
    QCOMPARE(compress, FALSE);
    QCOMPARE(iradioMode, FALSE);
    QCOMPARE(automaticRedirect, FALSE);
    QCOMPARE(retries, 0);
    QCOMPARE(timeout, 9u);
    QCOMPARE(sslStrict, TRUE);
    QCOMPARE(httpLogLevel, 0);
    QCOMPARE(QString::fromUtf8(userAgent), QGCNetworkHelper::defaultUserAgent());
    QCOMPARE(singleStream, TRUE);

    static GstStaticPadTemplate jpegPadTemplate =
        GST_STATIC_PAD_TEMPLATE("src_%u", GST_PAD_SRC, GST_PAD_SOMETIMES, GST_STATIC_CAPS("image/jpeg"));
    GstPad* jpegPad = gst_pad_new_from_static_template(&jpegPadTemplate, "src_0");
    QVERIFY(jpegPad);
    QVERIFY2(gst_element_add_pad(demux, jpegPad), "multipartdemux test pad must be accepted");

    GstPad* parserSink = gst_element_get_static_pad(parser, "sink");
    QVERIFY(parserSink);
    const auto parserSinkCleanup = qScopeGuard([&] { gst_object_unref(parserSink); });
    QVERIFY2(gst_pad_is_linked(parserSink), "multipart JPEG pad-added must link to jpegparse");
    GstPad* peer = gst_pad_get_peer(parserSink);
    QVERIFY(peer);
    QCOMPARE(peer, jpegPad);
    gst_object_unref(peer);
    QVERIFY(gst_element_remove_pad(demux, jpegPad));

    GstPad* srcPad = gst_element_get_static_pad(bin, "src");
    QVERIFY2(srcPad, "HTTP MJPEG source bin must expose a static parsed-JPEG source pad");
    gst_object_unref(srcPad);
}

void GStreamerTest::_testSourceFactoryHttpMjpegDelivery()
{
    if (!gst_element_factory_find("souphttpsrc") || !gst_element_factory_find("multipartdemux") ||
        !gst_element_factory_find("jpegparse") || !gst_element_factory_find("appsink")) {
        QSKIP("souphttpsrc/multipartdemux/jpegparse/appsink plugins unavailable");
    }

    QByteArray jpeg;
    QBuffer jpegBuffer(&jpeg);
    QVERIFY(jpegBuffer.open(QIODevice::WriteOnly));
    QImage image(16, 16, QImage::Format_RGB32);
    image.fill(Qt::green);
    QVERIFY2(image.save(&jpegBuffer, "JPEG"), "Qt JPEG encoder unavailable");

    const QByteArray boundary("qgc-test-boundary");
    const QByteArray framePart = "--" + boundary +
                                 "\r\n"
                                 "Content-Type: image/jpeg\r\n"
                                 "Content-Length: " +
                                 QByteArray::number(jpeg.size()) + "\r\n\r\n" + jpeg + "\r\n";
    // Keep the multipart response open like a camera stream so delivery does
    // not depend on the timing of a finite response reaching EOS.
    const QByteArray response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=" +
        boundary +
        "\r\n"
        "Connection: keep-alive\r\n\r\n" +
        framePart + framePart;

    TestFixtures::LocalHttpTestServer server;
    QVERIFY2(server.listen(), "Could not start local MJPEG test server");

    GStreamer::SourceFactory::Config config;
    config.timeoutS = 5;
    GstElement* source = GStreamer::SourceFactory::create(server.url(QStringLiteral("/video_feed")), config);
    GstElement* sink = gst_element_factory_make("appsink", "sink");
    GstElement* pipeline = gst_pipeline_new("http-mjpeg-delivery-test");
    if (!source || !sink || !pipeline) {
        gst_clear_object(&source);
        gst_clear_object(&sink);
        gst_clear_object(&pipeline);
        QFAIL("Could not create HTTP MJPEG delivery pipeline");
    }
    const auto pipelineCleanup = qScopeGuard([&] {
        (void) gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    });

    g_object_set(sink, "sync", FALSE, "max-buffers", 1u, "drop", TRUE, nullptr);
    gst_bin_add_many(GST_BIN(pipeline), source, sink, nullptr);
    QVERIFY2(gst_element_link(source, sink), "Could not link HTTP MJPEG source to appsink");

    // souphttpsrc can wait for the HTTP response while changing state. Run that
    // transition off-thread so this thread can accept and serve the request.
    const QFuture<GstStateChangeReturn> stateFuture =
        QtConcurrent::run([pipeline]() { return gst_element_set_state(pipeline, GST_STATE_PLAYING); });

    QTcpSocket* client = server.waitForConnection(TestTimeout::mediumMs());
    QVERIFY2(client, "HTTP MJPEG source did not connect to the local test server");
    const auto clientCleanup = qScopeGuard([&] {
        client->disconnectFromHost();
        client->deleteLater();
    });

    QByteArray request;
    QDeadlineTimer requestDeadline(TestTimeout::mediumDuration());
    while (!request.contains(QByteArrayLiteral("\r\n\r\n")) && !requestDeadline.hasExpired()) {
        request.append(client->readAll());
        if (!request.contains(QByteArrayLiteral("\r\n\r\n"))) {
            (void) client->waitForReadyRead(static_cast<int>(requestDeadline.remainingTime()));
        }
    }
    QVERIFY2(request.startsWith(QByteArrayLiteral("GET /video_feed HTTP/1.1\r\n")),
             "HTTP MJPEG source sent an unexpected request");

    QCOMPARE(client->write(response), static_cast<qint64>(response.size()));
    QDeadlineTimer responseDeadline(TestTimeout::mediumDuration());
    while ((client->bytesToWrite() > 0) && !responseDeadline.hasExpired()) {
        (void) client->waitForBytesWritten(static_cast<int>(responseDeadline.remainingTime()));
    }
    QCOMPARE(client->bytesToWrite(), 0);

    QTRY_VERIFY_WITH_TIMEOUT(stateFuture.isFinished(), TestTimeout::mediumMs());
    QVERIFY2(stateFuture.result() != GST_STATE_CHANGE_FAILURE, "HTTP MJPEG delivery pipeline failed to start");

    GstSample* sample = tryPullSampleOrPreroll(GST_APP_SINK(sink), TestTimeout::mediumDuration().count() * GST_MSECOND);
    QVERIFY2(sample, "HTTP MJPEG stream did not deliver a JPEG sample before timeout");
    const auto sampleCleanup = qScopeGuard([&] { gst_sample_unref(sample); });
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    QVERIFY(buffer);
    QVERIFY(gst_buffer_get_size(buffer) > 0);
    GstCaps* caps = gst_sample_get_caps(sample);
    QVERIFY(caps);
    QCOMPARE(QString::fromUtf8(gst_structure_get_name(gst_caps_get_structure(caps, 0))), QStringLiteral("image/jpeg"));
}

void GStreamerTest::_testSourceFactoryRejectsUnsafeHttpMjpegUrl()
{
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Invalid HTTP MJPEG URL")));
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("HTTP MJPEG credentials in URLs are not supported")));

    GStreamer::SourceFactory::Config config;
    QVERIFY(!GStreamer::SourceFactory::create(QStringLiteral("http:///camera.mjpg"), config));
    QVERIFY(!GStreamer::SourceFactory::create(QStringLiteral("http://video.example.test:0/camera.mjpg"), config));
    QVERIFY(!GStreamer::SourceFactory::create(QStringLiteral("https://operator:secret@video.example.test/camera.mjpg"),
                                              config));
}

void GStreamerTest::_testSourceFactoryWebSocketJpeg()
{
    if (!gst_element_factory_find("appsrc") || !gst_element_factory_find("jpegparse")) {
        QSKIP("appsrc/jpegparse plugins unavailable");
    }

    GStreamer::SourceFactory::Config config;
    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("ws://127.0.0.1:9/video"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    GstElement* appsrc = findChildByFactoryName(bin, "appsrc");
    GstElement* parser = findChildByFactoryName(bin, "jpegparse");
    QVERIFY(appsrc);
    QVERIFY(parser);

    gboolean isLive = FALSE;
    gboolean doTimestamp = FALSE;
    gboolean block = TRUE;
    gboolean emitSignals = TRUE;
    GstFormat format = GST_FORMAT_UNDEFINED;
    guint64 maxBuffers = 0;
    guint64 maxBytes = 0;
    GstAppLeakyType leakyType = GST_APP_LEAKY_TYPE_NONE;
    g_object_get(appsrc, "is-live", &isLive, "do-timestamp", &doTimestamp, "format", &format, "block", &block,
                 "max-buffers", &maxBuffers, "max-bytes", &maxBytes, "leaky-type", &leakyType, "emit-signals",
                 &emitSignals, nullptr);
    QCOMPARE(isLive, TRUE);
    QCOMPARE(doTimestamp, TRUE);
    QCOMPARE(format, GST_FORMAT_TIME);
    QCOMPARE(block, FALSE);
    QCOMPARE(maxBuffers, 2u);
    QCOMPARE(maxBytes, static_cast<guint64>(QGCWebSocketVideoSource::kMaximumJpegBytes * 2));
    QCOMPARE(leakyType, GST_APP_LEAKY_TYPE_DOWNSTREAM);
    QCOMPARE(emitSignals, FALSE);

    GstPad* srcPad = gst_element_get_static_pad(bin, "src");
    QVERIFY2(srcPad, "WebSocket JPEG source bin must expose a static parsed-JPEG source pad");
    gst_object_unref(srcPad);
}

void GStreamerTest::_testWebSocketJpegValidation()
{
    const QByteArray jpeg = makeTestJpeg();
    QVERIFY(!jpeg.isEmpty());
    QVERIFY(QGCWebSocketVideoSource::isCompleteJpeg(jpeg));
    QVERIFY(!QGCWebSocketVideoSource::isCompleteJpeg(QByteArrayLiteral("not-a-jpeg")));
    QVERIFY(!QGCWebSocketVideoSource::isCompleteJpeg(jpeg + jpeg));

    QByteArray jpegWithTrailingByte = jpeg;
    jpegWithTrailingByte.append('\0');
    QVERIFY(!QGCWebSocketVideoSource::isCompleteJpeg(jpegWithTrailingByte));

    QByteArray missingEndOfImage = jpeg;
    missingEndOfImage.chop(2);
    QVERIFY(!QGCWebSocketVideoSource::isCompleteJpeg(missingEndOfImage));
    QVERIFY(!QGCWebSocketVideoSource::isCompleteJpeg(QByteArray::fromHex("ffd8ffd9")));
    QVERIFY(!QGCWebSocketVideoSource::isCompleteJpeg(QByteArray::fromHex("ffd8ffe10001ffd9")));
    QVERIFY(!QGCWebSocketVideoSource::isCompleteJpeg(QByteArray::fromHex("ffd8ffe10010abcdffd9")));

    QByteArray jpegWithEmbeddedEndMarker = jpeg;
    jpegWithEmbeddedEndMarker.insert(2, QByteArray::fromHex("ffe1000678ffd979"));
    QVERIFY(QGCWebSocketVideoSource::isCompleteJpeg(jpegWithEmbeddedEndMarker));

    const QByteArray progressiveJpeg = makeTestJpeg(4, 4, true);
    QVERIFY(!progressiveJpeg.isEmpty());
    QVERIFY(QGCWebSocketVideoSource::isCompleteJpeg(progressiveJpeg));

    const QByteArray oversized(QGCWebSocketVideoSource::kMaximumJpegBytes + 1, '\0');
    QVERIFY(!QGCWebSocketVideoSource::isCompleteJpeg(oversized));

    const QByteArray excessiveWidth = withJpegDimensions(jpeg, QGCWebSocketVideoSource::kMaximumJpegDimension + 1, 1);
    QVERIFY(!excessiveWidth.isEmpty());
    QCOMPARE(jpegDimensions(excessiveWidth), QSize(QGCWebSocketVideoSource::kMaximumJpegDimension + 1, 1));
    QVERIFY(!QGCWebSocketVideoSource::isCompleteJpeg(excessiveWidth));

    const QByteArray uhd = withJpegDimensions(jpeg, 3840, 2160);
    QVERIFY(!uhd.isEmpty());
    QVERIFY(QGCWebSocketVideoSource::isCompleteJpeg(uhd));

    const quint16 excessiveHeight = static_cast<quint16>(
        (QGCWebSocketVideoSource::kMaximumDecodedPixels / QGCWebSocketVideoSource::kMaximumJpegDimension) + 1);
    const QByteArray excessivePixels =
        withJpegDimensions(jpeg, QGCWebSocketVideoSource::kMaximumJpegDimension, excessiveHeight);
    QVERIFY(!excessivePixels.isEmpty());
    QCOMPARE(jpegDimensions(excessivePixels), QSize(QGCWebSocketVideoSource::kMaximumJpegDimension, excessiveHeight));
    QVERIFY(!QGCWebSocketVideoSource::isCompleteJpeg(excessivePixels));
}

void GStreamerTest::_testSourceFactoryWebSocketJpegDelivery()
{
    if (!gst_element_factory_find("appsrc") || !gst_element_factory_find("jpegparse") ||
        !gst_element_factory_find("appsink")) {
        QSKIP("appsrc/jpegparse/appsink plugins unavailable");
    }

    QWebSocketServer server(QStringLiteral("QGC WebSocket JPEG test"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QSignalSpy connectionSpy(&server, &QWebSocketServer::newConnection);

    GStreamer::SourceFactory::Config config;
    const QString url = QStringLiteral("ws://127.0.0.1:%1/video?camera=front#local-fragment").arg(server.serverPort());
    GstElement* bin = GStreamer::SourceFactory::create(url, config);
    QVERIFY(bin);

    GstElement* pipeline = gst_pipeline_new("websocket-jpeg-test");
    GstElement* sink = gst_element_factory_make("appsink", "sink");
    QVERIFY(pipeline);
    QVERIFY(sink);
    const auto pipelineCleanup = qScopeGuard([&] {
        if (pipeline) {
            (void) gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
        }
        gst_clear_object(&sink);
        gst_clear_object(&bin);
    });

    g_object_set(sink, "sync", FALSE, "emit-signals", FALSE, "max-buffers", 1u, "drop", TRUE, nullptr);
    GstElement* sourceBin = bin;
    QVERIFY(gst_bin_add(GST_BIN(pipeline), bin));
    bin = nullptr;
    GstElement* binSink = sink;
    QVERIFY(gst_bin_add(GST_BIN(pipeline), sink));
    sink = nullptr;
    QVERIFY(gst_element_link(sourceBin, binSink));

    QVERIFY(gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    QVERIFY(GStreamer::SourceFactory::activate(sourceBin));
    QVERIFY_SIGNAL_WAIT(connectionSpy, TestTimeout::mediumMs());
    QWebSocket* peer = server.nextPendingConnection();
    QVERIFY(peer);
    QCOMPARE(peer->requestUrl().path(), QStringLiteral("/video"));
    QCOMPARE(peer->requestUrl().query(), QStringLiteral("camera=front"));
    QVERIFY(peer->requestUrl().fragment().isEmpty());

    const QByteArray jpeg = makeTestJpeg();
    QVERIFY(!jpeg.isEmpty());
    peer->setOutgoingFrameSize(64);
    (void) peer->sendTextMessage(QStringLiteral("metadata is ignored"));
    QCOMPARE(peer->sendBinaryMessage(jpeg), static_cast<qint64>(jpeg.size()));
    (void) peer->flush();
    QTRY_COMPARE_WITH_TIMEOUT(peer->bytesToWrite(), 0, TestTimeout::mediumMs());

    GstSample* sample =
        tryPullSampleOrPreroll(GST_APP_SINK(binSink), TestTimeout::mediumDuration().count() * GST_MSECOND);
    QVERIFY2(sample, "WebSocket JPEG stream did not deliver a sample before timeout");
    QVERIFY(gst_buffer_get_size(gst_sample_get_buffer(sample)) > 0);
    gst_sample_unref(sample);

    QSignalSpy disconnectedSpy(peer, &QWebSocket::disconnected);
    GStreamer::SourceFactory::deactivate(sourceBin);
    (void) gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    pipeline = nullptr;
    QVERIFY_SIGNAL_WAIT(disconnectedSpy, TestTimeout::mediumMs());
}

void GStreamerTest::_testSourceFactoryWebSocketJpegRejectsMalformedMessage()
{
    if (!gst_element_factory_find("appsrc") || !gst_element_factory_find("jpegparse")) {
        QSKIP("appsrc/jpegparse plugins unavailable");
    }

    expectLogMessage("Video.GStreamer.WebSocketVideoSource", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Rejecting WebSocket message")));

    QWebSocketServer server(QStringLiteral("QGC malformed WebSocket JPEG test"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QSignalSpy connectionSpy(&server, &QWebSocketServer::newConnection);

    GStreamer::SourceFactory::Config config;
    GstElement* source =
        GStreamer::SourceFactory::create(QStringLiteral("ws://127.0.0.1:%1/video").arg(server.serverPort()), config);
    GstElement* pipeline = gst_pipeline_new("websocket-jpeg-malformed-message-test");
    QVERIFY(source);
    QVERIFY(pipeline);
    const auto cleanup = qScopeGuard([&] {
        GStreamer::SourceFactory::deactivate(source);
        (void) gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    });

    QVERIFY(gst_bin_add(GST_BIN(pipeline), source));
    QVERIFY(gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    QVERIFY(GStreamer::SourceFactory::activate(source));
    QVERIFY_SIGNAL_WAIT(connectionSpy, TestTimeout::mediumMs());

    QWebSocket* peer = server.nextPendingConnection();
    QVERIFY(peer);
    QSignalSpy disconnectedSpy(peer, &QWebSocket::disconnected);
    const QByteArray malformed = QByteArrayLiteral("not-a-jpeg");
    QCOMPARE(peer->sendBinaryMessage(malformed), static_cast<qint64>(malformed.size()));
    (void) peer->flush();

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    QVERIFY(bus);
    const auto busCleanup = qScopeGuard([&] { gst_object_unref(bus); });
    GstMessage* message = gst_bus_timed_pop_filtered(
        bus, static_cast<GstClockTime>(TestTimeout::mediumMs()) * GST_MSECOND, GST_MESSAGE_ERROR);
    QVERIFY2(message, "A malformed WebSocket JPEG message must surface as a GStreamer bus error");
    gst_message_unref(message);
    verifyExpectedLogMessage();

    QVERIFY_SIGNAL_WAIT(disconnectedSpy, TestTimeout::mediumMs());
    QCOMPARE(peer->closeCode(), QWebSocketProtocol::CloseCodeProtocolError);
}

void GStreamerTest::_testSourceFactoryWebSocketJpegWssTrusted_data()
{
    QTest::addColumn<bool>("nativeMode");
    QTest::newRow("stock") << false;
    QTest::newRow("native") << true;
}

void GStreamerTest::_testSourceFactoryWebSocketJpegWssTrusted()
{
    QFETCH(bool, nativeMode);
    if (!QSslSocket::supportsSsl()) {
        QSKIP("TLS backend unavailable");
    }
    if (!gst_element_factory_find("appsrc") || !gst_element_factory_find("jpegparse")) {
        QSKIP("appsrc/jpegparse plugins unavailable");
    }

    const QSslCertificate certificate = testTlsCertificate();
    const QSslKey privateKey = testTlsPrivateKey();
    QVERIFY(!certificate.isNull());
    QVERIFY(!privateKey.isNull());

    const QSslConfiguration previousDefault = QSslConfiguration::defaultConfiguration();
    const auto restoreDefault = qScopeGuard([&] { QSslConfiguration::setDefaultConfiguration(previousDefault); });
    QSslConfiguration trustedDefault = previousDefault;
    QList<QSslCertificate> authorities = trustedDefault.caCertificates();
    authorities.append(certificate);
    trustedDefault.setCaCertificates(authorities);
    QSslConfiguration::setDefaultConfiguration(trustedDefault);

    QWebSocketServer server(QStringLiteral("QGC trusted WSS JPEG test"), QWebSocketServer::SecureMode);
    QVERIFY(configureSecureTestServer(server, certificate, privateKey));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QSignalSpy connectionSpy(&server, &QWebSocketServer::newConnection);

    GStreamer::SourceFactory::Config config;
    if (nativeMode) {
        config.webSocketOptions = nativeVideoOptions();
    }
    GstElement* source =
        GStreamer::SourceFactory::create(QStringLiteral("wss://127.0.0.1:%1/video").arg(server.serverPort()), config);
    GstElement* pipeline = gst_pipeline_new("websocket-jpeg-trusted-wss-test");
    QVERIFY(source);
    QVERIFY(pipeline);
    const auto cleanup = qScopeGuard([&] {
        GStreamer::SourceFactory::deactivate(source);
        (void) gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    });

    QVERIFY(gst_bin_add(GST_BIN(pipeline), source));
    QVERIFY(gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    QVERIFY(GStreamer::SourceFactory::activate(source));
    QVERIFY_SIGNAL_WAIT(connectionSpy, TestTimeout::mediumMs());
}

void GStreamerTest::_testSourceFactoryWebSocketJpegWssRejectsUntrusted_data()
{
    _testSourceFactoryWebSocketJpegWssTrusted_data();
}

void GStreamerTest::_testSourceFactoryWebSocketJpegWssRejectsUntrusted()
{
    QFETCH(bool, nativeMode);
    if (!QSslSocket::supportsSsl()) {
        QSKIP("TLS backend unavailable");
    }
    if (!gst_element_factory_find("appsrc") || !gst_element_factory_find("jpegparse")) {
        QSKIP("appsrc/jpegparse plugins unavailable");
    }

    expectLogMessage("Video.GStreamer.WebSocketVideoSource", QtWarningMsg,
                     QRegularExpression(QStringLiteral("WebSocket TLS verification failed")));
    expectLogMessage("Video.GStreamer.WebSocketVideoSource", QtWarningMsg,
                     QRegularExpression(QStringLiteral("WebSocket transport error code")));

    const QSslCertificate certificate = testTlsCertificate();
    const QSslKey privateKey = testTlsPrivateKey();
    QVERIFY(!certificate.isNull());
    QVERIFY(!privateKey.isNull());

    QWebSocketServer server(QStringLiteral("QGC untrusted WSS JPEG test"), QWebSocketServer::SecureMode);
    QVERIFY(configureSecureTestServer(server, certificate, privateKey));
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));

    GStreamer::SourceFactory::Config config;
    if (nativeMode) {
        config.webSocketOptions = nativeVideoOptions();
    }
    GstElement* source =
        GStreamer::SourceFactory::create(QStringLiteral("wss://127.0.0.1:%1/video").arg(server.serverPort()), config);
    GstElement* pipeline = gst_pipeline_new("websocket-jpeg-untrusted-wss-test");
    QVERIFY(source);
    QVERIFY(pipeline);
    const auto cleanup = qScopeGuard([&] {
        GStreamer::SourceFactory::deactivate(source);
        (void) gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    });

    QVERIFY(gst_bin_add(GST_BIN(pipeline), source));
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    QVERIFY(bus);
    const auto busCleanup = qScopeGuard([&] { gst_object_unref(bus); });
    QVERIFY(gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    QVERIFY(GStreamer::SourceFactory::activate(source));

    GstMessage* message = nullptr;
    // QTRY re-evaluates its expression after the wait loop. Retain the first popped message so
    // the final verification does not consume the bus a second time.
    QTRY_VERIFY_WITH_TIMEOUT(
        (message != nullptr) ||
            ((message = gst_bus_pop_filtered(bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR))) != nullptr),
        TestTimeout::mediumMs());
    gst_message_unref(message);
    verifyExpectedLogMessage();
    verifyExpectedLogMessage();
}

void GStreamerTest::_testSourceFactoryWebSocketJpegFailedHandshake()
{
    if (!gst_element_factory_find("appsrc") || !gst_element_factory_find("jpegparse")) {
        QSKIP("appsrc/jpegparse plugins unavailable");
    }

    QWebSocketServer portReservation(QStringLiteral("QGC WebSocket closed-port test"), QWebSocketServer::NonSecureMode);
    QVERIFY(portReservation.listen(QHostAddress::LocalHost, 0));
    const quint16 closedPort = portReservation.serverPort();
    portReservation.close();

    GStreamer::SourceFactory::Config config;
    GstElement* source =
        GStreamer::SourceFactory::create(QStringLiteral("ws://127.0.0.1:%1/video").arg(closedPort), config);
    GstElement* pipeline = gst_pipeline_new("websocket-jpeg-failed-handshake-test");
    QVERIFY(source);
    QVERIFY(pipeline);
    const auto cleanup = qScopeGuard([&] {
        GStreamer::SourceFactory::deactivate(source);
        (void) gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    });

    QVERIFY(gst_bin_add(GST_BIN(pipeline), source));
    QVERIFY(gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    QVERIFY(GStreamer::SourceFactory::activate(source));

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    QVERIFY(bus);
    const auto busCleanup = qScopeGuard([&] { gst_object_unref(bus); });
    GstMessage* message = gst_bus_timed_pop_filtered(
        bus, static_cast<GstClockTime>(TestTimeout::mediumMs()) * GST_MSECOND, GST_MESSAGE_ERROR);
    QVERIFY2(message, "A failed WebSocket handshake must surface as a GStreamer bus error");
    gst_message_unref(message);
}

void GStreamerTest::_testSourceFactoryWebSocketJpegRemoteDisconnect()
{
    if (!gst_element_factory_find("appsrc") || !gst_element_factory_find("jpegparse")) {
        QSKIP("appsrc/jpegparse plugins unavailable");
    }

    ignoreLogMessage("Video.GStreamer.WebSocketVideoSource", QtWarningMsg,
                     QRegularExpression(QStringLiteral("WebSocket transport error code")));

    QWebSocketServer server(QStringLiteral("QGC WebSocket disconnect test"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QSignalSpy connectionSpy(&server, &QWebSocketServer::newConnection);

    GStreamer::SourceFactory::Config config;
    GstElement* source =
        GStreamer::SourceFactory::create(QStringLiteral("ws://127.0.0.1:%1/video").arg(server.serverPort()), config);
    GstElement* pipeline = gst_pipeline_new("websocket-jpeg-remote-disconnect-test");
    QVERIFY(source);
    QVERIFY(pipeline);
    const auto cleanup = qScopeGuard([&] {
        GStreamer::SourceFactory::deactivate(source);
        (void) gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    });

    QVERIFY(gst_bin_add(GST_BIN(pipeline), source));
    QVERIFY(gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    QVERIFY(GStreamer::SourceFactory::activate(source));
    QVERIFY_SIGNAL_WAIT(connectionSpy, TestTimeout::mediumMs());

    QWebSocket* peer = server.nextPendingConnection();
    QVERIFY(peer);
    peer->abort();

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    QVERIFY(bus);
    const auto busCleanup = qScopeGuard([&] { gst_object_unref(bus); });
    GstMessage* message = gst_bus_timed_pop_filtered(
        bus, static_cast<GstClockTime>(TestTimeout::mediumMs()) * GST_MSECOND, GST_MESSAGE_ERROR);
    QVERIFY2(message, "A remote WebSocket disconnect must surface as a GStreamer bus error");
    gst_message_unref(message);
}

void GStreamerTest::_testSourceFactoryWebSocketJpegImmediateStop()
{
    if (!gst_element_factory_find("appsrc") || !gst_element_factory_find("jpegparse")) {
        QSKIP("appsrc/jpegparse plugins unavailable");
    }

    QWebSocketServer server(QStringLiteral("QGC WebSocket lifecycle test"), QWebSocketServer::NonSecureMode);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    const QString url = QStringLiteral("ws://127.0.0.1:%1/video").arg(server.serverPort());

    for (int iteration = 0; iteration < 8; ++iteration) {
        GStreamer::SourceFactory::Config config;
        GstElement* source = GStreamer::SourceFactory::create(url, config);
        QVERIFY(source);
        auto sourceCleanup = qScopeGuard([&] { gst_object_unref(source); });

        GstElement* pipeline = gst_pipeline_new("websocket-jpeg-lifecycle-test");
        QVERIFY(pipeline);
        const auto pipelineCleanup = qScopeGuard([&] {
            GStreamer::SourceFactory::deactivate(source);
            (void) gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
        });

        QVERIFY(gst_bin_add(GST_BIN(pipeline), source));
        sourceCleanup.dismiss();
        QVERIFY(gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
        QVERIFY(GStreamer::SourceFactory::activate(source));
    }
}

void GStreamerTest::_testSourceFactoryRejectsUnsafeWebSocketJpegUrl()
{
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Invalid WebSocket JPEG URL")));
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("WebSocket JPEG credentials in URLs are not supported")));

    GStreamer::SourceFactory::Config config;
    QVERIFY(!GStreamer::SourceFactory::create(QStringLiteral("ws:///video"), config));
    QVERIFY(!GStreamer::SourceFactory::create(QStringLiteral("ws://video.example.test:0/video"), config));
    QVERIFY(
        !GStreamer::SourceFactory::create(QStringLiteral("wss://operator:secret@video.example.test/video"), config));

    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Invalid WebSocket session options")));
    const QString localUrl = QStringLiteral("ws://127.0.0.1/video");
    auto options = nativeVideoOptions();
    config.webSocketOptions = options;
    options->cookie.append("\r\nX-Injected: value");
    QVERIFY(!GStreamer::SourceFactory::create(localUrl, config));
    options = nativeVideoOptions();
    config.webSocketOptions = options;
    options->origin.append("\r\nX-Injected: value");
    QVERIFY(!GStreamer::SourceFactory::create(localUrl, config));
    options = nativeVideoOptions();
    config.webSocketOptions = options;
    options->frameContexts.reset();
    QVERIFY(!GStreamer::SourceFactory::create(localUrl, config));
}

#endif
