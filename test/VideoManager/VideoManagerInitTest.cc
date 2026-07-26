#include "VideoManagerInitTest.h"

#ifdef QGC_GST_STREAMING

#include "VideoManager.h"
#include "VideoReceiver.h"

#include <QtCore/QRegularExpression>
#include <QtTest/QSignalSpy>
#include <QtQuick/QQuickWindow>

namespace {

class TestVideoReceiver final : public VideoReceiver
{
public:
    explicit TestVideoReceiver(QObject *parent = nullptr)
        : VideoReceiver(parent)
    {}

    void start(uint32_t timeout) override
    {
        ++startCount;
        lastTimeout = timeout;
        emit onStartComplete(STATUS_OK);
    }

    void stop() override
    {
        ++stopCount;
    }

    void startDecoding(void *sink) override { Q_UNUSED(sink); }
    void stopDecoding() override {}
    void startRecording(const QString &videoFile, FILE_FORMAT format) override
    {
        Q_UNUSED(videoFile);
        Q_UNUSED(format);
    }
    void stopRecording() override {}
    void takeScreenshot(const QString &imageFile) override { Q_UNUSED(imageFile); }

    int startCount = 0;
    int stopCount = 0;
    uint32_t lastTimeout = 0;
};

}  // namespace

void VideoManagerInitTest::init()
{
    UnitTest::init();

    static const QRegularExpression sGStreamerCriticalRe(
        QStringLiteral("cannot register existing type|"
                       "g_type_add_interface_static.*G_TYPE_IS_INSTANTIATABLE|"
                       "g_once_init_leave.*result != 0|"
                       "GStreamer initialization failed"));
    // These backend startup diagnostics are platform/runtime dependent and not
    // tied to a single deterministic call site in this fixture.
    ignoreLogMessage("Video.GStreamer.GStreamerLogging", QtCriticalMsg, sGStreamerCriticalRe);
}

void VideoManagerInitTest::_testQmlReadyBeforeBackendReady()
{
    VideoManager videoManager;
    QQuickWindow mainWindow;
    videoManager._mainWindow = &mainWindow;

    int createReceiversCount = 0;
    videoManager._createVideoReceiversForTest = [&createReceiversCount]() {
        ++createReceiversCount;
    };

    videoManager._initState = VideoManager::InitState::Pending;

    videoManager._initAfterQmlIsReady();
    QCOMPARE(videoManager._initState, VideoManager::InitState::QmlReady);
    QCOMPARE(createReceiversCount, 0);

    videoManager._onBackendInitComplete(true);
    QCOMPARE(videoManager._initState, VideoManager::InitState::Running);
    QCOMPARE(createReceiversCount, 1);

    expectLogMessage("Video.VideoManager", QtWarningMsg, QRegularExpression(QStringLiteral("_onBackendInitComplete: unexpected state")));
    videoManager._onBackendInitComplete(true);
    verifyExpectedLogMessage();
    QCOMPARE(createReceiversCount, 1);
}

void VideoManagerInitTest::_testBackendReadyBeforeQmlReady()
{
    VideoManager videoManager;
    QQuickWindow mainWindow;
    videoManager._mainWindow = &mainWindow;

    int createReceiversCount = 0;
    videoManager._createVideoReceiversForTest = [&createReceiversCount]() {
        ++createReceiversCount;
    };

    videoManager._initState = VideoManager::InitState::Pending;

    videoManager._onBackendInitComplete(true);
    QCOMPARE(videoManager._initState, VideoManager::InitState::BackendReady);
    QCOMPARE(createReceiversCount, 0);

    videoManager._initAfterQmlIsReady();
    QCOMPARE(videoManager._initState, VideoManager::InitState::Running);
    QCOMPARE(createReceiversCount, 1);

    expectLogMessage("Video.VideoManager", QtWarningMsg, QRegularExpression(QStringLiteral("_initAfterQmlIsReady: unexpected state")));
    videoManager._initAfterQmlIsReady();
    verifyExpectedLogMessage();
    QCOMPARE(createReceiversCount, 1);
}

void VideoManagerInitTest::_testBackendInitFailure()
{
    VideoManager videoManager;
    QQuickWindow mainWindow;
    videoManager._mainWindow = &mainWindow;

    int createReceiversCount = 0;
    videoManager._createVideoReceiversForTest = [&createReceiversCount]() {
        ++createReceiversCount;
    };

    videoManager._initState = VideoManager::InitState::Pending;

    expectLogMessage("Video.VideoManager", QtCriticalMsg, QRegularExpression(QStringLiteral("video initialization failed")));
    videoManager._onBackendInitComplete(false);
    verifyExpectedLogMessage();
    QCOMPARE(videoManager._initState, VideoManager::InitState::Failed);
    QCOMPARE(createReceiversCount, 0);

    expectLogMessage("Video.VideoManager", QtWarningMsg, QRegularExpression(QStringLiteral("QML ready but video init failed")));
    videoManager._initAfterQmlIsReady();
    verifyExpectedLogMessage();
    QCOMPARE(videoManager._initState, VideoManager::InitState::Failed);
    QCOMPARE(createReceiversCount, 0);
}

void VideoManagerInitTest::_testManagerRequestedRestart()
{
    VideoManager videoManager;
    TestVideoReceiver receiver;
    receiver.setName(QStringLiteral("testVideo"));
    receiver.setUri(QStringLiteral("udp://0.0.0.0:5600"));
    receiver.setStarted(true);

    videoManager._restartVideo(&receiver);
    videoManager._restartVideo(&receiver);
    QCOMPARE(receiver.stopCount, 1);
    QVERIFY(videoManager._receiverLifecycle.value(&receiver).restartAfterStop);

    QSignalSpy startSpy(&receiver, &VideoReceiver::onStartComplete);
    videoManager._handleReceiverStopComplete(&receiver, VideoReceiver::STATUS_OK);

    QVERIFY(startSpy.wait(1500));
    QCOMPARE(receiver.startCount, 1);
    QVERIFY(receiver.lastTimeout > 0);
    QVERIFY(!videoManager._receiverLifecycle.value(&receiver).restartAfterStop);
    QVERIFY(!videoManager._receiverLifecycle.value(&receiver).retryScheduled);
}

void VideoManagerInitTest::_testTransportStopDoesNotRestart()
{
    VideoManager videoManager;
    TestVideoReceiver receiver;
    receiver.setName(QStringLiteral("testVideo"));
    receiver.setUri(QStringLiteral("udp://0.0.0.0:5600"));
    receiver.setStarted(true);

    QSignalSpy startSpy(&receiver, &VideoReceiver::onStartComplete);
    videoManager._handleReceiverStopComplete(&receiver, VideoReceiver::STATUS_OK);

    QVERIFY(!receiver.started());
    QVERIFY(!startSpy.wait(1100));
    QCOMPARE(receiver.startCount, 0);
}

void VideoManagerInitTest::_testFailedStopDoesNotRestart()
{
    VideoManager videoManager;
    TestVideoReceiver receiver;
    receiver.setName(QStringLiteral("testVideo"));
    receiver.setUri(QStringLiteral("udp://0.0.0.0:5600"));
    receiver.setStarted(true);

    videoManager._restartVideo(&receiver);
    QSignalSpy startSpy(&receiver, &VideoReceiver::onStartComplete);
    expectLogMessage(
        "Video.VideoManager",
        QtWarningMsg,
        QRegularExpression(QStringLiteral("Video receiver stop failed; manager restart cancelled")));
    videoManager._handleReceiverStopComplete(&receiver, VideoReceiver::STATUS_FAIL);
    verifyExpectedLogMessage();

    QVERIFY(!startSpy.wait(1100));
    QCOMPARE(receiver.startCount, 0);
    QVERIFY(!videoManager._receiverLifecycle.value(&receiver).restartAfterStop);
    QVERIFY(!videoManager._receiverLifecycle.value(&receiver).retryScheduled);
}

void VideoManagerInitTest::_testOperatorStopCancelsPendingRestart()
{
    VideoManager videoManager;
    TestVideoReceiver receiver;
    receiver.setName(QStringLiteral("testVideo"));
    receiver.setUri(QStringLiteral("udp://0.0.0.0:5600"));
    receiver.setStarted(true);
    videoManager._videoReceivers.append(&receiver);

    videoManager._restartVideo(&receiver);
    videoManager._handleReceiverStopComplete(&receiver, VideoReceiver::STATUS_OK);
    QSignalSpy startSpy(&receiver, &VideoReceiver::onStartComplete);
    videoManager.stopVideo();

    QVERIFY(!startSpy.wait(1100));
    QCOMPARE(receiver.startCount, 0);
    QCOMPARE(receiver.stopCount, 2);
    QVERIFY(!videoManager._receiverLifecycle.value(&receiver).runRequested);
    QVERIFY(!videoManager._receiverLifecycle.value(&receiver).restartAfterStop);
    QVERIFY(!videoManager._receiverLifecycle.value(&receiver).retryScheduled);

    videoManager._videoReceivers.removeOne(&receiver);
}

void VideoManagerInitTest::_testOperatorStopCancelsStartFailureRetry()
{
    VideoManager videoManager;
    TestVideoReceiver receiver;
    receiver.setName(QStringLiteral("testVideo"));
    receiver.setUri(QStringLiteral("udp://0.0.0.0:5600"));
    videoManager._videoReceivers.append(&receiver);

    videoManager._startReceiver(&receiver);
    QCOMPARE(receiver.startCount, 1);

    QSignalSpy startSpy(&receiver, &VideoReceiver::onStartComplete);
    videoManager._handleReceiverStartComplete(&receiver, VideoReceiver::STATUS_FAIL);
    QVERIFY(videoManager._receiverLifecycle.value(&receiver).retryScheduled);

    videoManager.stopVideo();

    QVERIFY(!startSpy.wait(1100));
    QCOMPARE(receiver.startCount, 1);
    QCOMPARE(receiver.stopCount, 1);
    QVERIFY(!videoManager._receiverLifecycle.value(&receiver).runRequested);
    QVERIFY(!videoManager._receiverLifecycle.value(&receiver).retryScheduled);

    videoManager._videoReceivers.removeOne(&receiver);
}

#else

void VideoManagerInitTest::init() { UnitTest::init(); QSKIP("GStreamer not enabled"); }
void VideoManagerInitTest::_testQmlReadyBeforeBackendReady() { QSKIP("GStreamer not enabled"); }
void VideoManagerInitTest::_testBackendReadyBeforeQmlReady() { QSKIP("GStreamer not enabled"); }
void VideoManagerInitTest::_testBackendInitFailure() { QSKIP("GStreamer not enabled"); }
void VideoManagerInitTest::_testManagerRequestedRestart() { QSKIP("GStreamer not enabled"); }
void VideoManagerInitTest::_testTransportStopDoesNotRestart() { QSKIP("GStreamer not enabled"); }
void VideoManagerInitTest::_testFailedStopDoesNotRestart() { QSKIP("GStreamer not enabled"); }
void VideoManagerInitTest::_testOperatorStopCancelsPendingRestart() { QSKIP("GStreamer not enabled"); }
void VideoManagerInitTest::_testOperatorStopCancelsStartFailureRetry() { QSKIP("GStreamer not enabled"); }

#endif

UT_REGISTER_TEST(VideoManagerInitTest, TestLabel::Unit)
