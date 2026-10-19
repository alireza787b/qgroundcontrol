#include "QGCWebRtcVideoSourceTest.h"

#include <gst/gst.h>

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QScopeGuard>
#include <QtGui/QImage>
#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/QVideoSink>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QSignalSpy>

#include "GStreamer.h"
#include "GstVideoReceiver.h"
#include "QGCQVideoSinkController.h"
#include "QGCVideoFrameContextStore.h"
#include "QGCWebRtcVideoSource.h"

#if defined(QGC_CUSTOM_BUILD)
#include "PixEagleVideoItem.h"
#endif

void QGCWebRtcVideoSourceTest::_nativeDecoderRecoveryPreservesStockPolicy()
{
    if (!gst_is_initialized()) {
        GStreamer::prepareEnvironment();
        gst_init(nullptr, nullptr);
    }
    GstElement* decoder = gst_element_factory_make("avdec_h264", nullptr);
    if (!decoder) {
        QSKIP("libav decoder is not installed on this platform");
    }
    const auto releaseDecoder = qScopeGuard([&] { gst_object_unref(decoder); });
    gboolean original = FALSE;
    g_object_get(decoder, "output-corrupt", &original, nullptr);
    GstElement* stockSource = gst_bin_new(nullptr);
    const auto releaseStock = qScopeGuard([&] { gst_object_unref(stockSource); });
    QGCWebRtcVideoSource::configureDecoder(stockSource, decoder);
    gboolean actual = FALSE;
    g_object_get(decoder, "output-corrupt", &actual, nullptr);
    QCOMPARE(actual, original);

    auto options = std::make_shared<QGCWebRtcVideoOptions>();
    options->frameContexts = std::make_shared<QGCVideoFrameContextStore>();
    options->origin = QStringLiteral("http://127.0.0.1:8197");
    GstElement* nativeSource = QGCWebRtcVideoSource::create(QUrl(QStringLiteral("ws://127.0.0.1:8197/ws")), options);
    QVERIFY(nativeSource);
    const auto releaseNative = qScopeGuard([&] { gst_object_unref(nativeSource); });
    GstElement* parser = gst_bin_get_by_name(GST_BIN(nativeSource), "webrtc-parser");
    GstElement* depayloader = gst_element_factory_make("rtph264depay", nullptr);
    QVERIFY(parser);
    QVERIFY(depayloader);
    const auto releaseParser = qScopeGuard([&] { gst_object_unref(parser); });
    const auto releaseDepayloader = qScopeGuard([&] { gst_object_unref(depayloader); });
    GstQuery* query = gst_query_new_caps(nullptr);
    const auto releaseQuery = qScopeGuard([&] { gst_query_unref(query); });
    gboolean handled = FALSE;
    g_signal_emit_by_name(parser, "autoplug-query", nullptr, depayloader, query, &handled);
    QVERIFY(handled);
    GstCaps* caps = nullptr;
    gst_query_parse_caps_result(query, &caps);
    QVERIFY(caps);
    QCOMPARE(QByteArray(gst_structure_get_string(gst_caps_get_structure(caps, 0), "alignment")), QByteArray("au"));
    QGCWebRtcVideoSource::configureDecoder(nativeSource, decoder);
    g_object_get(decoder, "output-corrupt", &actual, nullptr);
    QVERIFY(!actual);
    g_object_get(decoder, "discard-corrupted-frames", &actual, nullptr);
    QVERIFY(actual);
    g_object_get(decoder, "automatic-request-sync-points", &actual, nullptr);
    QVERIFY(actual);
}

void QGCWebRtcVideoSourceTest::_presentedFrameMatchesExactRtpContext_data()
{
    QTest::addColumn<QString>("codec");
    QTest::newRow("h264") << QStringLiteral("H264");
    QTest::newRow("vp8") << QStringLiteral("VP8");
}

void QGCWebRtcVideoSourceTest::_presentedFrameMatchesExactRtpContext()
{
    QFETCH(QString, codec);
    const QString endpoint = qEnvironmentVariable("QGC_WEBRTC_TEST_SIGNALING_URL");
    if (endpoint.isEmpty()) {
        QSKIP("Start the loopback numbered-frame qualification fixture and provide its signaling URL");
    }
    if (!gst_is_initialized()) {
        GStreamer::prepareEnvironment();
        gst_init(nullptr, nullptr);
    }
    QVERIFY(GStreamer::completeInit());
    ignoreLogMessage("default", QtWarningMsg,
                     QRegularExpression(QStringLiteral("No RHI backend\\. Using CPU conversion\\.")));
    if (qEnvironmentVariable("QGC_WEBRTC_TEST_PACKET_LOSS_PERCENT").toDouble() > 0) {
        ignoreLogMessage("Video.GStreamer.GStreamerAPI", QtWarningMsg,
                         QRegularExpression(QStringLiteral("^\\(NULL\\) Removing \\d+ expired NACKS$")));
    }
    QVERIFY(QGCWebRtcVideoSource::available());
    auto options = std::make_shared<QGCWebRtcVideoOptions>();
    options->codecPreferences = {codec};
    options->frameContexts = std::make_shared<QGCVideoFrameContextStore>();
    QUrl url(endpoint);
    QVERIFY(url.host() == QStringLiteral("127.0.0.1"));
    options->origin = QStringLiteral("http://127.0.0.1:%1").arg(url.port());
    url.setScheme(QStringLiteral("webrtc+ws"));

    GstVideoReceiver receiver;
    receiver.setAutoReconnect(false);
    receiver.setLowLatency(true);
    QSize expectedSize(160, 120);
    QVideoSink standaloneSink;
    QVideoSink* videoSink = &standaloneSink;
#if defined(QGC_CUSTOM_BUILD)
    QNetworkAccessManager network;
    QUrl contextUrl(endpoint);
    contextUrl.setScheme("http");
    contextUrl.setPath("/qualification/context");
    auto* contextReply = network.get(QNetworkRequest(contextUrl));
    QSignalSpy contextRead(contextReply, &QNetworkReply::finished);
    QTRY_VERIFY_WITH_TIMEOUT(!contextRead.isEmpty(), 3000);
    QCOMPARE(contextReply->error(), QNetworkReply::NoError);
    const auto expectedContext = QJsonDocument::fromJson(contextReply->readAll()).object();
    const auto expectedVideo = expectedContext.value("video").toObject();
    expectedSize = QSize(expectedVideo.value("width").toInt(), expectedVideo.value("height").toInt());
    contextReply->deleteLater();
    QQuickWindow window;
    window.resize(320, 240);
    PixEagleVideoItem widget(window.contentItem());
    widget.setSize(QSizeF(320, 240));
    widget.setStream(options->frameContexts, expectedContext);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window, TestTimeout::mediumMs()));
    videoSink = widget.videoSink();
#else
    QQuickItem widget;
#endif
    receiver.setWidget(&widget);
    GStreamer::VideoSinkConfig sinkConfig;
    sinkConfig.gpuZeroCopy = false;
    void* sink = GStreamer::createVideoSink(sinkConfig);
    QVERIFY(sink);
    const auto cleanup = qScopeGuard([&] { GStreamer::releaseVideoSink(sink); });
    QVERIFY(GStreamer::setupQVideoSinkElement(sink, videoSink, &receiver));
    for (auto* controller : QGCQVideoSinkController::controllersOf(&receiver)) {
        controller->setActive(true);
    }
    receiver.setWebRtcOptions(options);
    receiver.setUri(url.toString());
    int verified = 0;
    int missing = 0;
    QString failure;
    QElapsedTimer observation;
    observation.start();
    qint64 lastFrameMs = 0;
    qint64 firstFrameMs = -1;
    qint64 largestGapMs = 0;
    QSize smallestFrame;
    QSize largestFrame;
    connect(videoSink, &QVideoSink::videoFrameChanged, this, [&](const QVideoFrame& frame) {
        if (!frame.isValid()) {
            return;
        }
        const auto context = options->frameContexts->lookup(frame.startTime());
        if (!context) {
            ++missing;
            return;
        }
        const QImage image = frame.toImage();
        const auto provenance = context->metadata.value("provenance").toObject();
        const QSize encodedSize(provenance.value("encoded_width").toInt(), provenance.value("encoded_height").toInt());
        if (image.size() != encodedSize) {
            failure = QStringLiteral("Presented %1x%2 differs from context %3x%4 (capture %5)")
                          .arg(image.width())
                          .arg(image.height())
                          .arg(encodedSize.width())
                          .arg(encodedSize.height())
                          .arg(provenance.value("capture_id").toString());
            return;
        }
        int captureId = 0;
        for (int bit = 0; bit < 16; ++bit) {
            const int x = (bit * 8 + 4) * image.width() / expectedSize.width();
            const int y = 16 * image.height() / expectedSize.height();
            if (qGray(image.pixel(x, y)) > 128) {
                captureId |= 1 << bit;
            }
        }
        if (provenance.value("capture_id").toString() != QString::number(captureId)) {
            const QString expected = provenance.value("capture_id").toString();
            failure = QStringLiteral("Presented barcode %1 differs from context %2 at Qt timestamp %3")
                          .arg(captureId)
                          .arg(expected)
                          .arg(frame.startTime());
            const QString imagePrefix = qEnvironmentVariable("QGC_WEBRTC_FAILURE_IMAGE");
            if (!imagePrefix.isEmpty()) {
                image.save(imagePrefix + QLatin1Char('-') + codec.toLower() + QStringLiteral(".png"));
            }
            return;
        }
        ++verified;
        const qint64 now = observation.elapsed();
        if (firstFrameMs < 0) {
            firstFrameMs = now;
        }
        if (lastFrameMs) {
            largestGapMs = std::max(largestGapMs, now - lastFrameMs);
        }
        lastFrameMs = now;
        if (smallestFrame.isEmpty() || image.width() < smallestFrame.width()) {
            smallestFrame = image.size();
        }
        if (image.width() > largestFrame.width()) {
            largestFrame = image.size();
        }
    });
    QSignalSpy started(&receiver, &VideoReceiver::onStartComplete);
    QSignalSpy stopped(&receiver, &VideoReceiver::onStopComplete);
    QSignalSpy decoding(&receiver, &VideoReceiver::onStartDecodingComplete);
    receiver.start(10);
    QTRY_VERIFY_WITH_TIMEOUT(!started.isEmpty(), 15000);
    QCOMPARE(started.first().first().value<VideoReceiver::STATUS>(), VideoReceiver::STATUS_OK);
    receiver.startDecoding(sink);
    QTRY_VERIFY_WITH_TIMEOUT(!decoding.isEmpty(), 5000);
    QCOMPARE(decoding.first().first().value<VideoReceiver::STATUS>(), VideoReceiver::STATUS_OK);
    QTRY_VERIFY2_WITH_TIMEOUT(
        verified >= 12 || !failure.isEmpty() || !stopped.isEmpty(),
        qPrintable(
            QStringLiteral("%1 verified, %2 without metadata; %3")
                .arg(verified)
                .arg(missing)
                .arg(QString::fromUtf8(QJsonDocument(options->statistics->snapshot()).toJson(QJsonDocument::Compact)))),
        15000);
    QVERIFY2(failure.isEmpty(), qPrintable(failure));
    QVERIFY2(verified >= 12,
             qPrintable(QStringLiteral("Only %1 verified frames; %2 without metadata").arg(verified).arg(missing)));
    const int minimumDuration = std::clamp(qEnvironmentVariableIntValue("QGC_WEBRTC_TEST_DURATION_MS"), 0, 120000);
    QTRY_VERIFY_WITH_TIMEOUT(
        observation.elapsed() - firstFrameMs >= minimumDuration || !failure.isEmpty() || !stopped.isEmpty(),
        minimumDuration + 2000);
    QVERIFY2(failure.isEmpty(), qPrintable(failure));
    QVERIFY(stopped.isEmpty());
    QVERIFY2(
        observation.elapsed() - lastFrameMs < 1500,
        qPrintable(QString::fromUtf8(QJsonDocument(options->statistics->snapshot()).toJson(QJsonDocument::Compact))));
#if defined(QGC_CUSTOM_BUILD)
    QTRY_VERIFY2_WITH_TIMEOUT(widget.selectionReady(), qPrintable(widget.selectionError()), 5000);
    const auto presented = widget.capturePresentedContext();
    const auto token = widget.beginSelection(&widget, QPointF(160, 120));
    QVERIFY2(!token.isEmpty(), qPrintable(widget.selectionError()));
    const auto selection = widget.finishSelection(token, QPointF(160, 120), false);
    QVERIFY2(selection.value("valid").toBool(), qPrintable(widget.selectionError()));
    QCOMPARE(selection.value("context").toMap().value("capture_id"), presented.value("capture_id"));
    const auto geometry = selection.value("selection_geometry").toMap();
    contextUrl.setPath("/qualification/selection/" + geometry.value("token").toString());
    auto* selectionReply = network.get(QNetworkRequest(contextUrl));
    QSignalSpy selectionRead(selectionReply, &QNetworkReply::finished);
    QTRY_VERIFY_WITH_TIMEOUT(!selectionRead.isEmpty(), 3000);
    QCOMPARE(selectionReply->error(), QNetworkReply::NoError);
    const auto retained = QJsonDocument::fromJson(selectionReply->readAll()).object();
    selectionReply->deleteLater();
    QVERIFY(retained.value("available").toBool());
    QCOMPARE(retained.value("capture_id").toVariant(), presented.value("capture_id"));
    QCOMPARE(retained.value("geometry").toObject().toVariantMap(), geometry);
#endif
    receiver.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!stopped.isEmpty(), 10000);
    QVERIFY(!options->frameContexts->lookup(videoSink->videoFrame().startTime()));
    QVERIFY2(failure.isEmpty(), qPrintable(failure));
    expectLogMessage("default", QtInfoMsg, QRegularExpression(QStringLiteral(".*verified frames.*")));
    qInfo() << codec << "verified frames" << verified << "missing context" << missing << "observation ms"
            << observation.elapsed() << "largest verified-frame gap ms" << largestGapMs << "smallest frame"
            << smallestFrame << "largest frame" << largestFrame << "association" << options->statistics->snapshot();
    verifyExpectedLogMessage();
}

UT_REGISTER_TEST_STANDALONE(QGCWebRtcVideoSourceTest, TestLabel::Integration, TestLabel::Network)
