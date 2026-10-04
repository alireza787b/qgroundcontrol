#pragma once

#include "UnitTest.h"

class PixEagleConfigClientTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _readAndApply();
    void _restartBackendIdentityAndRecovery();
    void _restartRefusalAndInstanceChange();
    void _invalidState_data();
    void _invalidState();
    void _permissionAndCapabilityGates();
    void _conflictAndStaleConfirmation();
    void _lateReplyCannotCrossEndpoint();
    void _stateExpiresWithoutDemand();
    void _runtimeRestartInvalidatesState();
    void _restartRequirementIsReadOnly();
};
