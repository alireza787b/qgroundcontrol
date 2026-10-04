#pragma once

#include "PixEagleManager.h"
#include "QGCCorePlugin.h"

/// Custom-build entry point retaining stock QGC options and flight controls.
class PixEaglePlugin : public QGCCorePlugin
{
    Q_OBJECT
    Q_PROPERTY(PixEagleManager* pixeagle READ pixeagle NOTIFY pixeagleChanged)

public:
    explicit PixEaglePlugin(QObject* parent = nullptr);

    static QGCCorePlugin* instance();
    void registerCustomSettings(SettingsManager* settingsManager) override;
    const QVariantList& toolBarIndicators() override;

    PixEagleManager* pixeagle() const { return _pixeagle; }

signals:
    void pixeagleChanged();

private:
    PixEagleManager* _pixeagle = nullptr;
    QVariantList _toolBarIndicators;
};
