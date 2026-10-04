#include "PixEaglePlugin.h"

#include <QtCore/QApplicationStatic>
#include <QtCore/QUrl>

#include "PixEagleSettings.h"
#include "SettingsManager.h"

Q_APPLICATION_STATIC(PixEaglePlugin, _pixEaglePluginInstance);

PixEaglePlugin::PixEaglePlugin(QObject* parent)
    : QGCCorePlugin(parent)
{}

QGCCorePlugin* PixEaglePlugin::instance()
{
    return _pixEaglePluginInstance();
}

const QVariantList& PixEaglePlugin::toolBarIndicators()
{
    if (_toolBarIndicators.isEmpty()) {
        _toolBarIndicators = QGCCorePlugin::toolBarIndicators();
        _toolBarIndicators.append(QUrl(QStringLiteral("qrc:/qml/PixEagle/PixEagleIndicator.qml")));
    }
    return _toolBarIndicators;
}

void PixEaglePlugin::registerCustomSettings(SettingsManager* settingsManager)
{
    auto* settings = new PixEagleSettings();
    settingsManager->registerCustomSettingsGroup(QStringLiteral("pixEagleSettings"), settings);
    _pixeagle = new PixEagleManager(settings, this);
    QTimer::singleShot(0, _pixeagle, &PixEagleManager::initializeVideo);
    emit pixeagleChanged();
}
