#include "QGCWebRtcVideoSourceTest.h"

#include <gst/gst.h>

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
        if (image.size() != expectedSize) {
            failure = QStringLiteral("Unexpected presented dimensions");
            return;
        }
        int captureId = 0;
        for (int bit = 0; bit < 16; ++bit) {
            if (qGray(image.pixel(bit * 8 + 4, 16)) > 128) {
                captureId |= 1 << bit;
            }
        }
        if (context->metadata.value("provenance").toObject().value("capture_id").toString() !=
            QString::number(captureId)) {
            failure = QStringLiteral("Presented barcode differs from its exact frame context");
            return;
        }
        ++verified;
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
    QTRY_VERIFY2_WITH_TIMEOUT(verified >= 12 || !failure.isEmpty() || !stopped.isEmpty(),
                              qPrintable(QStringLiteral("%1 verified, %2 without metadata").arg(verified).arg(missing)),
                              15000);
    QVERIFY2(failure.isEmpty(), qPrintable(failure));
    QVERIFY2(verified >= 12,
             qPrintable(QStringLiteral("Only %1 verified frames; %2 without metadata").arg(verified).arg(missing)));
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
}

UT_REGISTER_TEST_STANDALONE(QGCWebRtcVideoSourceTest, TestLabel::Integration, TestLabel::Network)
