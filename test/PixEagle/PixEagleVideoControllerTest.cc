#include "PixEagleVideoControllerTest.h"

#include <QtTest/QSignalSpy>

#include "PixEagleManager.h"
#include "PixEaglePlugin.h"
#include "PixEagleSettings.h"
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

UT_REGISTER_TEST(PixEagleVideoControllerTest, TestLabel::Integration)
