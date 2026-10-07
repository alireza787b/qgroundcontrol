#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QVariantList>
#include <QtQmlIntegration/QtQmlIntegration>

#include "PixEagleClient.h"

class PixEagleSettings;
class PixEagleVideoController;
Q_MOC_INCLUDE("PixEagleVideoController.h")
class Vehicle;

/// Retains separate sessions when the operator changes the active QGC vehicle.
class PixEagleManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by the PixEagle plugin")
    Q_PROPERTY(PixEagleClient* activeClient READ activeClient NOTIFY activeClientChanged)
    Q_PROPERTY(QStringList vehicleLabels READ vehicleLabels NOTIFY vehiclesChanged)
    Q_PROPERTY(QVariantList followingVehicles READ followingVehicles NOTIFY followingVehiclesChanged)
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeClientChanged)
    Q_PROPERTY(PixEagleVideoController* video READ video NOTIFY videoChanged)
    Q_PROPERTY(QUrl dashboardUrl READ dashboardUrl NOTIFY dashboardUrlChanged)
    Q_PROPERTY(QString dashboardUrlOverride READ dashboardUrlOverride NOTIFY dashboardUrlChanged)

public:
    explicit PixEagleManager(PixEagleSettings* settings, QObject* parent = nullptr);
    PixEagleClient* activeClient() const;
    QStringList vehicleLabels() const;
    QVariantList followingVehicles() const;
    int activeIndex() const;
    QUrl dashboardUrl() const;
    QString dashboardUrlOverride() const;
    void initializeVideo();

    PixEagleVideoController* video() const { return _video; }

    Q_INVOKABLE void selectVehicle(int index);
    Q_INVOKABLE bool stopFollowingForVehicle(int index, const QString& token);
    Q_INVOKABLE bool setDashboardUrlOverride(const QString& address);

signals:
    void activeClientChanged();
    void vehiclesChanged();
    void followingVehiclesChanged();
    void videoChanged();
    void dashboardUrlChanged();

private:
    void _addVehicle(Vehicle* vehicle);
    void _updateIdentity(Vehicle* vehicle);
    void _updateDuplicates();
    void _updateEnabled();
    QString _dashboardSettingsKey() const;

    PixEagleSettings* _settings = nullptr;
    QHash<Vehicle*, PixEagleClient*> _clients;
    QHash<Vehicle*, QString> _provisionalEndpoints;
    PixEagleClient* _companion = nullptr;
    PixEagleVideoController* _video = nullptr;
};
