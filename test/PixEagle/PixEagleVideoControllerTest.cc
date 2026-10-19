#include "PixEagleVideoControllerTest.h"

#include <gst/gst.h>

#include <QtCore/QJsonArray>
#include <QtCore/QScopeGuard>
#include <QtTest/QSignalSpy>

#include "GStreamer.h"
#include "GstVideoReceiver.h"
#include "PixEagleManager.h"
#include "PixEaglePlugin.h"
#include "PixEagleSettings.h"
#include "PixEagleTestServer.h"
#include "PixEagleVideoController.h"
#include "SettingsManager.h"
#include "VideoManager.h"
#include "VideoSettings.h"

namespace {
class VideoFixture
{
public:
    PixEaglePlugin* plugin = qobject_cast<PixEaglePlugin*>(QGCCorePlugin::instance());
    PixEagleManager* manager = plugin ? plugin->pixeagle() : nullptr;
    PixEagleSettings* settings = qobject_cast<PixEagleSettings*>(
        SettingsManager::instance()->value(QStringLiteral("pixEagleSettings")).value<QObject*>());
    VideoManager* videoManager = VideoManager::instance();

    VideoFixture()
    {
        if (valid()) {
            _integrationEnabled = settings->integrationEnabled()->rawValue();
            _videoEnabled = settings->videoEnabled()->rawValue();
            _endpoint = manager->activeClient()->endpoint();
        }
    }

    ~VideoFixture()
    {
        if (valid()) {
            settings->videoEnabled()->setRawValue(false);
            manager->activeClient()->setEndpoint(_endpoint);
            settings->integrationEnabled()->setRawValue(_integrationEnabled);
            settings->videoEnabled()->setRawValue(_videoEnabled);
        }
    }

    bool valid() const { return manager && settings && manager->activeClient()->companionOnly(); }

    void restorePreferences(bool videoEnabled)
    {
        // SettingsFact ignores saved values under unit tests. Restore startup preferences
        // on the real plugin's Facts; a second controller would compete for VideoManager.
        settings->videoEnabled()->setRawValue(false);
        settings->integrationEnabled()->setRawValue(true);
        manager->activeClient()->setEndpoint(QStringLiteral("http://127.0.0.1:8093"));
        settings->videoEnabled()->setRawValue(videoEnabled);
    }

private:
    QVariant _integrationEnabled;
    QVariant _videoEnabled;
    QString _endpoint;
};
}  // namespace

void PixEagleVideoControllerTest::_cleanupRetiresVideoBeforeApplicationDestruction()
{
    if (!gst_is_initialized()) {
        GStreamer::prepareEnvironment();
        gst_init(nullptr, nullptr);
    }
    QVERIFY(GStreamer::completeInit());
    VideoFixture test;
    QVERIFY(test.valid());
    PixEagleManager manager(test.settings);
    manager.initializeVideo();
    QPointer<PixEagleVideoController> video = manager.video();
    QVERIFY(video);
    QTRY_VERIFY_WITH_TIMEOUT(video->_createReceiver(), TestTimeout::mediumMs());
    QPointer<GstVideoReceiver> receiver = video->_receiver;
    QVERIFY(receiver);
    manager.cleanupVideo();
    QVERIFY(!video);
    QVERIFY(!receiver);
    manager.cleanupVideo();
    manager.initializeVideo();
    QVERIFY(!manager.video());
}

void PixEagleVideoControllerTest::_preEnabledVideoPublishesControllerBeforeView()
{
    VideoFixture test;
    QVERIFY(test.valid());
    QVERIFY(!test.manager->video());
    test.restorePreferences(true);
    QVERIFY(test.settings->integrationEnabled()->rawValue().toBool());
    QVERIFY(test.settings->videoEnabled()->rawValue().toBool());
    QSignalSpy published(test.manager, &PixEagleManager::videoChanged);
    PixEagleVideoItem surface;
    QPointer<PixEagleVideoController> attachedController;
    int activations = 0;
    bool publishedBeforeActivation = false;
    // A ready QML Loader instantiates its view synchronously when this source changes.
    connect(test.videoManager, &VideoManager::externalVideoSourceChanged, &surface, [&]() {
        if (!test.videoManager->externalVideoActive()) {
            return;
        }
        ++activations;
        attachedController = test.manager->video();
        publishedBeforeActivation = attachedController && published.count() == 1;
        if (attachedController) {
            attachedController->attachSurface(&surface);
        }
    });

    test.manager->initializeVideo();

    QCOMPARE(activations, 1);
    QVERIFY2(publishedBeforeActivation, "The initial video view must be able to attach during source activation");
    QCOMPARE(attachedController.data(), test.manager->video());
    QCOMPARE(test.videoManager->externalVideoSource(), QUrl(QStringLiteral("qrc:/qml/PixEagle/PixEagleVideoView.qml")));
    test.manager->video()->detachSurface(&surface);
}

void PixEagleVideoControllerTest::_videoEnabledAfterInitialization()
{
    VideoFixture test;
    QVERIFY(test.valid());
    test.restorePreferences(false);
    test.manager->initializeVideo();
    auto* controller = test.manager->video();
    QVERIFY(controller);
    QVERIFY(!test.videoManager->externalVideoActive());
    QSignalSpy published(test.manager, &PixEagleManager::videoChanged);
    QSignalSpy sourceChanged(test.videoManager, &VideoManager::externalVideoSourceChanged);

    test.settings->videoEnabled()->setRawValue(true);
    QVERIFY(test.videoManager->externalVideoActive());
    QCOMPARE(sourceChanged.count(), 1);
    test.settings->videoEnabled()->setRawValue(false);
    QVERIFY(!test.videoManager->externalVideoActive());
    test.settings->videoEnabled()->setRawValue(true);
    QVERIFY(test.videoManager->externalVideoActive());
    QCOMPARE(sourceChanged.count(), 3);
    QCOMPARE(test.manager->video(), controller);
    QCOMPARE(published.count(), 0);
}

void PixEagleVideoControllerTest::_initializationIsIdempotent()
{
    VideoFixture test;
    QVERIFY(test.valid());
    test.restorePreferences(true);
    test.manager->initializeVideo();
    auto* controller = test.manager->video();
    QVERIFY(controller);
    QSignalSpy published(test.manager, &PixEagleManager::videoChanged);
    QSignalSpy sourceChanged(test.videoManager, &VideoManager::externalVideoSourceChanged);

    test.manager->initializeVideo();
    test.manager->initializeVideo();

    QCOMPARE(test.manager->video(), controller);
    QCOMPARE(published.count(), 0);
    QCOMPARE(sourceChanged.count(), 0);
    QVERIFY(test.videoManager->externalVideoActive());
}

void PixEagleVideoControllerTest::_externalVideoDoesNotRequireStockStreamSetting()
{
    VideoFixture test;
    QVERIFY(test.valid());
    auto* stockSettings = SettingsManager::instance()->videoSettings();
    const QVariant stockEnabled = stockSettings->streamEnabled()->rawValue();
    stockSettings->streamEnabled()->setRawValue(false);
    test.restorePreferences(true);
    test.manager->initializeVideo();
    QVERIFY(test.videoManager->externalVideoActive());
    QVERIFY(test.videoManager->hasVideo());
    test.manager->video()->detachSurface(nullptr);
    stockSettings->streamEnabled()->setRawValue(stockEnabled);
}

void PixEagleVideoControllerTest::_nativeCapabilityRequiresEveryPrerequisite()
{
    const QJsonObject native{{"available", true},
                             {"runtime_ready", true},
                             {"provider", "gstreamer"},
                             {"frame_association", "verified"},
                             {"missing_components", QJsonArray()},
                             {"signaling_path", "/ws/webrtc_signaling"},
                             {"fallback", "websocket_jpeg"}};
    const QJsonObject qualified{
        {"schema_version", 1},        {"source", "streaming_client_config"},         {"streaming_enabled", true},
        {"default_protocol", "auto"}, {"transports", QJsonObject{{"webrtc", true}}}, {"native_webrtc", native}};
    QVERIFY(PixEagleVideoController::_nativeConfigEligible(qualified));
    QVERIFY(PixEagleVideoController::_nativeTransportAllowed(qualified, true, false));
    QVERIFY(!PixEagleVideoController::_nativeTransportAllowed(qualified, false, false));
    QVERIFY(!PixEagleVideoController::_nativeTransportAllowed(qualified, true, true));
    for (auto it = native.begin(); it != native.end(); ++it) {
        auto incomplete = native;
        incomplete.remove(it.key());
        auto config = qualified;
        config.insert("native_webrtc", incomplete);
        QVERIFY2(!PixEagleVideoController::_nativeConfigEligible(config), qPrintable(it.key()));
    }
    for (const auto* field : {"available", "runtime_ready"}) {
        auto invalid = native;
        invalid.insert(field, "true");
        auto config = qualified;
        config.insert("native_webrtc", invalid);
        QVERIFY(!PixEagleVideoController::_nativeConfigEligible(config));
    }
    for (const auto& invalid :
         {QJsonObject{{"provider", "aiortc"}}, QJsonObject{{"missing_components", QJsonArray{"nicesrc"}}},
          QJsonObject{{"frame_association", "unknown"}},
          QJsonObject{{"signaling_path", "https://other-host/signaling"}}}) {
        auto unavailable = native;
        for (auto it = invalid.begin(); it != invalid.end(); ++it) {
            unavailable.insert(it.key(), it.value());
        }
        auto config = qualified;
        config.insert("native_webrtc", unavailable);
        QVERIFY(!PixEagleVideoController::_nativeConfigEligible(config));
    }
    for (const auto* protocol : {"websocket", "http", "unknown"}) {
        auto config = qualified;
        config.insert("default_protocol", protocol);
        QVERIFY(!PixEagleVideoController::_nativeConfigEligible(config));
    }
}

void PixEagleVideoControllerTest::_mediaFallbackResetsOnlyWithMediaIdentity()
{
    VideoFixture test;
    QVERIFY(test.valid());
    test.restorePreferences(true);
    test.manager->initializeVideo();
    auto* controller = test.manager->video();
    QVERIFY(controller);
    controller->_usingWebRtc = true;
    controller->_failVideo(QStringLiteral("decode"), QStringLiteral("fixture decode failure"));
    QVERIFY(controller->_webRtcFailed);
    controller->_updateDesired();
    QVERIFY(controller->_webRtcFailed);
    controller->_desiredKey = QStringLiteral("retired media identity");
    controller->_updateDesired();
    QVERIFY(!controller->_webRtcFailed);
    QVERIFY(controller->_desiredKey.isEmpty());
    controller->_usingWebRtc = false;
}

void PixEagleVideoControllerTest::_mediaDiscoveryDoesNotBlockVerificationOrSurviveEndpointChange()
{
    VideoFixture test;
    QVERIFY(test.valid());
    test.restorePreferences(true);
    test.manager->initializeVideo();
    auto* controller = test.manager->video();
    QVERIFY(controller);
    PixEagleTest::CompanionServer server;
    QVERIFY(server.start());
    PixEagleTest::advertiseTargets(server.context);
    PixEagleClient client;
    PixEagleTest::configure(client, server);
    QVERIFY(PixEagleTest::signIn(client));
    QVERIFY(client.mediaAvailable());
    controller->_client = &client;
    const auto restore = qScopeGuard([controller]() { controller->_bindClient(); });
    controller->_updateDesired();
    QVERIFY(!controller->_desiredKey.isEmpty());
    const QByteArray path = "/api/v1/streams/client-config";
    server.deferredPath = path;
    controller->_requestMediaConfig();
    QTRY_VERIFY_WITH_TIMEOUT(server.lastRequestIndex(path) >= 0, TestTimeout::mediumMs());
    const auto request = server.lastRequest(path);
    QCOMPARE(request.headers.value("cookie"), QByteArray("pixeagle_session=session-first"));
    QCOMPARE(request.headers.value("origin"), client.mediaOrigin().toUtf8());
    QVERIFY(controller->_mediaConfigReply);
    QVERIFY(!client.busy());
    QVERIFY(PixEagleTest::verify(client));
    QVERIFY(controller->_mediaConfigReply);
    const QJsonObject config{{"schema_version", 1}, {"source", "streaming_client_config"}};
    QVERIFY(server.respond(server.lastRequestIndex(path), {200, config, {}}));
    QTRY_VERIFY_WITH_TIMEOUT(!controller->_mediaConfigReply, TestTimeout::mediumMs());
    QCOMPARE(controller->_mediaConfig, config);
    QCOMPARE(controller->_mediaConfigKey, controller->_desiredKey);

    PixEagleTest::CompanionServer otherEndpoint;
    QVERIFY(otherEndpoint.start());
    server.deferredPath.clear();
    server.overrides.insert(path, {302, {}, {{"Location", otherEndpoint.endpoint().toUtf8() + path}}});
    controller->_mediaConfigKey.clear();
    controller->_requestMediaConfig();
    QTRY_VERIFY_WITH_TIMEOUT(!controller->_mediaConfigReply, TestTimeout::mediumMs());
    QVERIFY(controller->_mediaConfig.isEmpty());
    QVERIFY(otherEndpoint.requests.isEmpty());

    server.deferredPath = path;
    controller->_mediaConfigKey.clear();
    controller->_requestMediaConfig();
    QVERIFY(controller->_mediaConfigReply);
    client.setEndpoint(QStringLiteral("http://127.0.0.1:8094"));
    controller->_updateDesired();
    QVERIFY(!controller->_mediaConfigReply);
    QVERIFY(controller->_mediaConfig.isEmpty());
    QVERIFY(controller->_desiredKey.isEmpty());
}

UT_REGISTER_TEST(PixEagleVideoControllerTest, TestLabel::Integration)
