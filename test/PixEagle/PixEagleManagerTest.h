#pragma once

#include "BaseClasses/VehicleTestManualConnect.h"

class PixEagleManagerTest : public VehicleTestManualConnect
{
    Q_OBJECT

private slots:
    void _vehicleEndpointDefaultsAndRestores_data();
    void _vehicleEndpointDefaultsAndRestores();
    void _dashboardDefaults_data();
    void _dashboardDefaults();
    void _dashboardOverridePolicy_data();
    void _dashboardOverridePolicy();
    void _dashboardOverridesFollowEndpoint();
    void _rawIdentityConflictAndVehicleRemoval_data();
    void _rawIdentityConflictAndVehicleRemoval();
};
