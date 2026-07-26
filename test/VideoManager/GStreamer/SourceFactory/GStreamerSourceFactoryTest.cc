#include "GStreamerTest.h"

#ifdef QGC_GST_STREAMING

#include <QtCore/QBuffer>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtGui/QImage>
#include <QtGui/QImageReader>
#include <QtGui/QImageWriter>
#include <QtNetwork/QHostAddress>
#include <QtTest/QSignalSpy>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>
#include <array>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include "GstSourceFactory.h"
#include "LocalHttpTestServer.h"
#include "QGCWebSocketVideoSource.h"

namespace {

GstSample* tryPullSampleOrPreroll(GstAppSink* sink)
{
    if (GstSample* sample = gst_app_sink_try_pull_sample(sink, 0)) {
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

}  // namespace

void GStreamerTest::_testSourceFactoryUdpRtpJitterBuffer()
{
    if (!gst_element_factory_find("udpsrc") || !gst_element_factory_find("rtpjitterbuffer")) {
        QSKIP("udpsrc/rtpjitterbuffer plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    config.jitterBuffer = GStreamer::SourceFactory::JitterBuffer::DropOnLatency;
    config.latencyMs = 80;
    config.doRetransmission = true;

    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("udp://127.0.0.1:5600"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    GstElement* jb = findChildByFactoryName(bin, "rtpjitterbuffer");
    QVERIFY2(jb, "static UDP-RTP path must insert an rtpjitterbuffer at NULL state");

    guint latency = 0;
    gboolean doLost = FALSE, doRtx = FALSE, dropOnLatency = FALSE;
    gint rtxDelay = 0, rtxMaxRetries = 0;
    g_object_get(jb, "latency", &latency, "do-lost", &doLost, "do-retransmission", &doRtx, "drop-on-latency",
                 &dropOnLatency, "rtx-delay", &rtxDelay, "rtx-max-retries", &rtxMaxRetries, nullptr);
    QCOMPARE(latency, 80u);
    QCOMPARE(doLost, TRUE);
    QCOMPARE(doRtx, TRUE);
    QCOMPARE(dropOnLatency, TRUE);
    QCOMPARE(rtxDelay, 25);
    QCOMPARE(rtxMaxRetries, 1);
}

void GStreamerTest::_testSourceFactoryJitterBufferNone()
{
    if (!gst_element_factory_find("udpsrc")) {
        QSKIP("udpsrc plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    config.jitterBuffer = GStreamer::SourceFactory::JitterBuffer::None;

    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("udp://127.0.0.1:5600"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    QVERIFY2(!findChildByFactoryName(bin, "rtpjitterbuffer"),
             "JitterBuffer::None must link the source straight to the parser");
}

void GStreamerTest::_testSourceFactoryNoRetransmission()
{
    if (!gst_element_factory_find("udpsrc") || !gst_element_factory_find("rtpjitterbuffer")) {
        QSKIP("udpsrc/rtpjitterbuffer plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    config.jitterBuffer = GStreamer::SourceFactory::JitterBuffer::Buffered;
    config.doRetransmission = false;

    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("udp://127.0.0.1:5600"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    GstElement* jb = findChildByFactoryName(bin, "rtpjitterbuffer");
    QVERIFY(jb);

    gboolean doRtx = TRUE, dropOnLatency = TRUE;
    g_object_get(jb, "do-retransmission", &doRtx, "drop-on-latency", &dropOnLatency, nullptr);
    QCOMPARE(doRtx, FALSE);
    QCOMPARE(dropOnLatency, FALSE);
}

void GStreamerTest::_testSourceFactoryRtspExcludesStaticJitterBuffer()
{
    if (!gst_element_factory_find("rtspsrc")) {
        QSKIP("rtspsrc plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    config.jitterBuffer = GStreamer::SourceFactory::JitterBuffer::DropOnLatency;

    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("rtsp://127.0.0.1:8554/test"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    QVERIFY2(!findChildByFactoryName(bin, "rtpjitterbuffer"),
             "rtspsrc owns its internal jitterbuffer; the factory must not add a second one");
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
        QStringLiteral("https://video.example.test:8443/camera.mjpg?quality=80"), config);
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
    gboolean automaticRedirect = TRUE;
    gboolean sslStrict = FALSE;
    gboolean singleStream = FALSE;
    gint retries = -1;
    guint timeout = 0;
    gint httpLogLevel = -1;
    g_object_get(source, "location", &location, "method", &method, "is-live", &isLive, "do-timestamp", &doTimestamp,
                 "keep-alive", &keepAlive, "automatic-redirect", &automaticRedirect, "retries", &retries, "timeout",
                 &timeout, "ssl-strict", &sslStrict, "http-log-level", &httpLogLevel, nullptr);
    const auto stringsCleanup = qScopeGuard([&] {
        g_free(location);
        g_free(method);
    });
    g_object_get(demux, "single-stream", &singleStream, nullptr);

    QCOMPARE(QString::fromUtf8(location), QStringLiteral("https://video.example.test:8443/camera.mjpg?quality=80"));
    QCOMPARE(QString::fromUtf8(method), QStringLiteral("GET"));
    QCOMPARE(isLive, TRUE);
    QCOMPARE(doTimestamp, TRUE);
    QCOMPARE(keepAlive, TRUE);
    QCOMPARE(automaticRedirect, FALSE);
    QCOMPARE(retries, 0);
    QCOMPARE(timeout, 9u);
    QCOMPARE(sslStrict, TRUE);
    QCOMPARE(httpLogLevel, 0);
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
    // A multipart video stream contains repeated images. Supplying two also
    // verifies delivery after multipartdemux exposes and links its dynamic pad.
    const QByteArray body = framePart + framePart + "--" + boundary + "--\r\n";
    const QByteArray response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=" +
        boundary +
        "\r\n"
        "Connection: close\r\n"
        "Content-Length: " +
        QByteArray::number(body.size()) + "\r\n\r\n" + body;

    TestFixtures::LocalHttpTestServer server;
    QVERIFY2(server.listen(), "Could not start local MJPEG test server");
    server.installRawResponder(response);

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
    QVERIFY2(gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE,
             "HTTP MJPEG delivery pipeline failed to start");

    GstSample* sample = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((sample = tryPullSampleOrPreroll(GST_APP_SINK(sink))) != nullptr, TestTimeout::mediumMs());
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

    const QByteArray excessivePixels = withJpegDimensions(jpeg, QGCWebSocketVideoSource::kMaximumJpegDimension, 4097);
    QVERIFY(!excessivePixels.isEmpty());
    QCOMPARE(jpegDimensions(excessivePixels), QSize(QGCWebSocketVideoSource::kMaximumJpegDimension, 4097));
    QVERIFY(!QGCWebSocketVideoSource::isCompleteJpeg(excessivePixels));

    QCOMPARE(QGCWebSocketVideoSource::normalizedOrigin(QStringLiteral("https://video.example.test/")),
             QStringLiteral("https://video.example.test"));
    QCOMPARE(QGCWebSocketVideoSource::normalizedOrigin(QStringLiteral(" http://127.0.0.1:3040 ")),
             QStringLiteral("http://127.0.0.1:3040"));
    QVERIFY(QGCWebSocketVideoSource::normalizedOrigin(QStringLiteral("ws://video.example.test")).isEmpty());
    QVERIFY(QGCWebSocketVideoSource::normalizedOrigin(QStringLiteral("https://operator@video.example.test")).isEmpty());
    QVERIFY(QGCWebSocketVideoSource::normalizedOrigin(QStringLiteral("https://video.example.test/path")).isEmpty());
    QVERIFY(
        QGCWebSocketVideoSource::normalizedOrigin(QStringLiteral("https://video.example.test?token=secret")).isEmpty());
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
    config.webSocketOrigin = QStringLiteral("https://operator.example.test/");
    const QString url = QStringLiteral("ws://127.0.0.1:%1/video").arg(server.serverPort());
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

    QVERIFY(GStreamer::SourceFactory::activate(sourceBin));
    QVERIFY(gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
    QVERIFY_SIGNAL_WAIT(connectionSpy, TestTimeout::mediumMs());
    QWebSocket* peer = server.nextPendingConnection();
    QVERIFY(peer);
    QCOMPARE(peer->origin(), QStringLiteral("https://operator.example.test"));

    const QByteArray jpeg = makeTestJpeg();
    QVERIFY(!jpeg.isEmpty());
    peer->setOutgoingFrameSize(64);
    (void) peer->sendTextMessage(QStringLiteral("metadata is ignored"));
    QCOMPARE(peer->sendBinaryMessage(jpeg), static_cast<qint64>(jpeg.size()));
    (void) peer->flush();

    GstSample* sample = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT((sample = gst_app_sink_try_pull_sample(GST_APP_SINK(binSink), 0)) != nullptr,
                             TestTimeout::mediumMs());
    QVERIFY(gst_buffer_get_size(gst_sample_get_buffer(sample)) > 0);
    gst_sample_unref(sample);

    QSignalSpy disconnectedSpy(peer, &QWebSocket::disconnected);
    GStreamer::SourceFactory::deactivate(sourceBin);
    (void) gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    pipeline = nullptr;
    QVERIFY_SIGNAL_WAIT(disconnectedSpy, TestTimeout::mediumMs());
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
        QVERIFY(GStreamer::SourceFactory::activate(source));
        GStreamer::SourceFactory::deactivate(source);
        gst_object_unref(source);
    }
}

void GStreamerTest::_testSourceFactoryRejectsUnsafeWebSocketJpegUrl()
{
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Invalid WebSocket JPEG URL")));
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("WebSocket JPEG credentials in URLs are not supported")));
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Invalid WebSocket Origin")));

    GStreamer::SourceFactory::Config config;
    QVERIFY(!GStreamer::SourceFactory::create(QStringLiteral("ws:///video"), config));
    QVERIFY(
        !GStreamer::SourceFactory::create(QStringLiteral("wss://operator:secret@video.example.test/video"), config));
    config.webSocketOrigin = QStringLiteral("https://video.example.test/not-an-origin");
    QVERIFY(!GStreamer::SourceFactory::create(QStringLiteral("wss://video.example.test/video"), config));
}

void GStreamerTest::_testSourceFactoryRejectsBadUri()
{
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtCriticalMsg,
                     QRegularExpression(QStringLiteral("URI is not specified|Invalid UDP port")));
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Unsupported URI scheme")));

    GStreamer::SourceFactory::Config config;
    QVERIFY(!GStreamer::SourceFactory::create(QString(), config));
    QVERIFY(!GStreamer::SourceFactory::create(QStringLiteral("ftp://127.0.0.1/x"), config));
    QVERIFY(!GStreamer::SourceFactory::create(QStringLiteral("udp://127.0.0.1:0"), config));
    QVERIFY(!GStreamer::SourceFactory::create(QStringLiteral("udp://127.0.0.1:99999"), config));
}

void GStreamerTest::_testSourceFactoryTcpMpegTs()
{
    if (!gst_element_factory_find("tcpclientsrc") || !gst_element_factory_find("tsdemux")) {
        QSKIP("tcpclientsrc/tsdemux plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("tcp://192.168.1.50:5600"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    GstElement* src = findChildByFactoryName(bin, "tcpclientsrc");
    QVERIFY2(src, "tcp:// must build a tcpclientsrc");
    gchar* host = nullptr;
    gint port = 0;
    g_object_get(src, "host", &host, "port", &port, nullptr);
    const auto hostCleanup = qScopeGuard([&] { g_free(host); });
    QCOMPARE(QString::fromUtf8(host), QStringLiteral("192.168.1.50"));
    QCOMPARE(port, 5600);

    QVERIFY2(findChildByFactoryName(bin, "tsdemux"), "MPEG-TS over TCP must insert tsdemux explicitly");
    QVERIFY2(!findChildByFactoryName(bin, "rtpjitterbuffer"), "raw MPEG-TS is not RTP; no jitterbuffer");
}

void GStreamerTest::_testSourceFactoryRejectsBadTcpUri()
{
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtCriticalMsg,
                     QRegularExpression(QStringLiteral("Invalid TCP port|Missing host in TCP URI")));

    GStreamer::SourceFactory::Config config;
    QVERIFY2(!GStreamer::SourceFactory::create(QStringLiteral("tcp://192.168.1.50"), config),
             "tcp:// without a port must be rejected");
    QVERIFY2(!GStreamer::SourceFactory::create(QStringLiteral("tcp://:5600"), config),
             "tcp:// without a host must be rejected");
    QVERIFY2(!GStreamer::SourceFactory::create(QStringLiteral("tcp://192.168.1.50:0"), config),
             "tcp:// with port 0 must be rejected");
}

void GStreamerTest::_testSourceFactoryUdp265Caps()
{
    if (!gst_element_factory_find("udpsrc") || !gst_element_factory_find("rtpjitterbuffer")) {
        QSKIP("udpsrc/rtpjitterbuffer plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("udp265://127.0.0.1:5600"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    GstElement* src = findChildByFactoryName(bin, "udpsrc");
    QVERIFY2(src, "udp265:// must build a udpsrc");

    GstCaps* caps = nullptr;
    g_object_get(src, "caps", &caps, nullptr);
    QVERIFY2(caps, "udp265:// must set H265 RTP caps on udpsrc");
    const GstStructure* st = gst_caps_get_structure(caps, 0);
    const gchar* encName = gst_structure_get_string(st, "encoding-name");
    QCOMPARE(QString::fromUtf8(encName), QStringLiteral("H265"));
    gst_caps_unref(caps);

    QVERIFY2(findChildByFactoryName(bin, "rtpjitterbuffer"), "udp265 RTP path must insert a jitterbuffer");
    QVERIFY2(!findChildByFactoryName(bin, "tsdemux"), "udp265 is RTP, not MPEG-TS; no tsdemux");
}

void GStreamerTest::_testSourceFactoryUdp265UsesExplicitDepayAndParser()
{
    if (!gst_element_factory_find("udpsrc") || !gst_element_factory_find("rtph265depay") ||
        !gst_element_factory_find("h265parse")) {
        QSKIP("udp265 RTP/H265 plugins unavailable");
    }

    GStreamer::SourceFactory::Config config;
    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("udp265://127.0.0.1:5600"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    QVERIFY2(findChildByFactoryName(bin, "rtph265depay"), "udp265:// must depayload H265 RTP explicitly");

    GstElement* parser = findChildByFactoryName(bin, "h265parse");
    QVERIFY2(parser, "udp265:// must parse H265 explicitly");

    gint configInterval = 0;
    g_object_get(parser, "config-interval", &configInterval, nullptr);
    QCOMPARE(configInterval, -1);
}

void GStreamerTest::_testSourceFactoryUdpH264Caps()
{
    if (!gst_element_factory_find("udpsrc")) {
        QSKIP("udpsrc plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("udp://127.0.0.1:5600"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    GstElement* src = findChildByFactoryName(bin, "udpsrc");
    QVERIFY(src);

    GstCaps* caps = nullptr;
    g_object_get(src, "caps", &caps, nullptr);
    QVERIFY2(caps, "udp:// must set H264 RTP caps");
    const GstStructure* st = gst_caps_get_structure(caps, 0);
    const gchar* media = gst_structure_get_string(st, "media");
    const gchar* encName = gst_structure_get_string(st, "encoding-name");
    QCOMPARE(QString::fromUtf8(media), QStringLiteral("video"));
    QCOMPARE(QString::fromUtf8(encName), QStringLiteral("H264"));
    gst_caps_unref(caps);
}

void GStreamerTest::_testSourceFactoryUdpMpegTs()
{
    if (!gst_element_factory_find("udpsrc") || !gst_element_factory_find("tsdemux")) {
        QSKIP("udpsrc/tsdemux plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("mpegts://127.0.0.1:5600"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    GstElement* src = findChildByFactoryName(bin, "udpsrc");
    QVERIFY2(src, "mpegts:// must build a udpsrc");

    GstCaps* caps = nullptr;
    g_object_get(src, "caps", &caps, nullptr);
    if (caps) {
        gst_caps_unref(caps);
    }
    QVERIFY2(!caps, "raw MPEG-TS over UDP must not force RTP caps on udpsrc");

    QVERIFY2(findChildByFactoryName(bin, "tsdemux"), "mpegts:// must insert tsdemux");
    QVERIFY2(!findChildByFactoryName(bin, "rtpjitterbuffer"), "raw MPEG-TS is not RTP; no jitterbuffer");
}

void GStreamerTest::_testSourceFactorySchemeCaseInsensitive()
{
    if (!gst_element_factory_find("udpsrc")) {
        QSKIP("udpsrc plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("UDP://127.0.0.1:5600"), config);
    QVERIFY2(bin, "uppercase scheme must be accepted (scheme is lower-cased before matching)");
    gst_object_unref(bin);
}

void GStreamerTest::_testSourceFactoryNegativeLatencyClamped()
{
    if (!gst_element_factory_find("udpsrc") || !gst_element_factory_find("rtpjitterbuffer")) {
        QSKIP("udpsrc/rtpjitterbuffer plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    config.latencyMs = -100;
    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("udp://127.0.0.1:5600"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    GstElement* jb = findChildByFactoryName(bin, "rtpjitterbuffer");
    QVERIFY(jb);
    guint latency = 99999;
    g_object_get(jb, "latency", &latency, nullptr);
    QCOMPARE(latency, 0u);
}

void GStreamerTest::_testSourceFactoryDynamicRtpLinkFailureCleansJitterBuffer()
{
    if (!gst_element_factory_find("udpsrc") || !gst_element_factory_find("tsdemux") ||
        !gst_element_factory_find("rtpjitterbuffer")) {
        QSKIP("udpsrc/tsdemux/rtpjitterbuffer plugin unavailable");
    }

    GStreamer::SourceFactory::Config config;
    config.jitterBuffer = GStreamer::SourceFactory::JitterBuffer::DropOnLatency;
    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("mpegts://127.0.0.1:5600"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    GstElement* tsdemux = findChildByFactoryName(bin, "tsdemux");
    QVERIFY(tsdemux);
    GstElement* parser = findChildByFactoryName(bin, "parsebin");
    QVERIFY(parser);

    GstPad* parserSink = gst_element_get_static_pad(parser, "sink");
    QVERIFY(parserSink);
    const auto parserSinkCleanup = qScopeGuard([&] { gst_object_unref(parserSink); });
    QVERIFY(!gst_pad_is_linked(parserSink));

    static GstStaticPadTemplate sRtpPadTemplate = GST_STATIC_PAD_TEMPLATE(
        "stray_rtp", GST_PAD_SRC, GST_PAD_SOMETIMES,
        GST_STATIC_CAPS("application/x-rtp, media=(string)video, clock-rate=(int)90000, encoding-name=(string)H264"));

    GstPad* strayPad = gst_pad_new_from_static_template(&sRtpPadTemplate, "stray_rtp");
    QVERIFY(strayPad);
    const auto padCleanup = qScopeGuard([&] { gst_object_unref(strayPad); });

    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("gst_element_link_pads\\(\\) failed")));
    g_signal_emit_by_name(tsdemux, "pad-added", strayPad);

    QVERIFY2(!gst_pad_is_linked(parserSink),
             "a failed dynamic RTP pad link must leave parsebin available for the next valid pad");
    QVERIFY2(!findChildByFactoryName(bin, "rtpjitterbuffer"),
             "a failed dynamic RTP pad link must remove its temporary jitterbuffer");
}

#endif
