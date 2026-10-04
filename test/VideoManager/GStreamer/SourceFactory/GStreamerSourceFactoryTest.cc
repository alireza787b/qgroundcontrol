#include "GStreamerTest.h"

#ifdef QGC_GST_STREAMING

#include <array>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include <QtConcurrent/QtConcurrent>
#include <QtCore/QBuffer>
#include <QtCore/QDeadlineTimer>
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
#include <QtTest/QSignalSpy>
#include <QtWebSockets/QWebSocket>
#include <QtWebSockets/QWebSocketServer>

#include "GstSourceFactory.h"
#include "LocalHttpTestServer.h"
#include "QGCNetworkHelper.h"
#include "QGCVideoFrameContextStore.h"
#include "QGCWebSocketVideoSource.h"

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
    const qint64 second = store.insert(firstEpoch, {{"frame_id", "2"}});
    const qint64 earlierArrival = QGCVideoFrameContextStore::monotonicMs() - 37;
    const qint64 third = store.insert(firstEpoch, {{"frame_id", "3"}}, earlierArrival);
    QVERIFY(first >= 0 && first < second && second < third);
    QVERIFY(!store.lookup(first));
    QVERIFY(store.lookup(second));
    QCOMPARE(store.lookup(third)->metadata.value("frame_id").toString(), QStringLiteral("3"));
    QCOMPARE(store.lookup(third)->receivedMonotonicMs, earlierArrival);

    const quint64 secondEpoch = store.beginEpoch();
    QVERIFY(!store.lookup(third));
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

void GStreamerTest::_testSourceFactoryRtspJitterBufferPolicy_data()
{
    QTest::addColumn<int>("jitterBufferPolicy");
    QTest::addColumn<guint>("expectedLatencyMs");
    QTest::addColumn<bool>("expectedRetransmission");
    QTest::addColumn<bool>("expectedDropOnLatency");

    using JitterBuffer = GStreamer::SourceFactory::JitterBuffer;
    QTest::newRow("low-latency") << static_cast<int>(JitterBuffer::None) << 0u << false << true;
    QTest::newRow("drop-on-latency") << static_cast<int>(JitterBuffer::DropOnLatency) << 80u << true << true;
    QTest::newRow("buffered") << static_cast<int>(JitterBuffer::Buffered) << 80u << true << false;
}

void GStreamerTest::_testSourceFactoryRtspJitterBufferPolicy()
{
    if (!gst_element_factory_find("rtspsrc")) {
        QSKIP("rtspsrc plugin unavailable");
    }

    QFETCH(int, jitterBufferPolicy);
    QFETCH(guint, expectedLatencyMs);
    QFETCH(bool, expectedRetransmission);
    QFETCH(bool, expectedDropOnLatency);

    GStreamer::SourceFactory::Config config;
    config.jitterBuffer = static_cast<GStreamer::SourceFactory::JitterBuffer>(jitterBufferPolicy);
    config.latencyMs = 80;
    config.doRetransmission = true;

    GstElement* bin = GStreamer::SourceFactory::create(QStringLiteral("rtsp://127.0.0.1:8554/test"), config);
    QVERIFY(bin);
    const auto cleanup = qScopeGuard([&] { gst_object_unref(bin); });

    GstElement* source = findChildByFactoryName(bin, "rtspsrc");
    QVERIFY(source);

    guint latencyMs = 0;
    gboolean doRetransmission = FALSE;
    gboolean dropOnLatency = FALSE;
    g_object_get(source, "latency", &latencyMs, "do-retransmission", &doRetransmission, "drop-on-latency",
                 &dropOnLatency, nullptr);
    QCOMPARE(latencyMs, expectedLatencyMs);
    QCOMPARE(doRetransmission, expectedRetransmission ? TRUE : FALSE);
    QCOMPARE(dropOnLatency, expectedDropOnLatency ? TRUE : FALSE);

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

void GStreamerTest::_testSourceFactoryRejectsBadUri()
{
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtCriticalMsg,
                     QRegularExpression(QStringLiteral("URI is not specified")));
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
                     QRegularExpression(QStringLiteral("Unsupported URI scheme|Invalid UDP port")));

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
    ignoreLogMessage("Video.GStreamer.GstSourceFactory", QtWarningMsg,
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
