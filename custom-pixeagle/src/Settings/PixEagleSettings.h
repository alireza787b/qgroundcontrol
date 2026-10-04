#pragma once

#include <QtQmlIntegration/QtQmlIntegration>

#include "SettingsGroup.h"

class PixEagleSettings : public SettingsGroup
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Registered by the PixEagle plugin")

public:
    explicit PixEagleSettings(QObject* parent = nullptr);

    DEFINE_SETTING_NAME_GROUP()
    DEFINE_SETTINGFACT(integrationEnabled)
    DEFINE_SETTINGFACT(videoEnabled)
    DEFINE_SETTINGFACT(tapToTarget)
};
