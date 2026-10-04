#pragma once

#include "UnitTest.h"

class PixEagleCameraClientTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _stalePollCannotReplacePostStopState();
    void _automaticStopPreservesFailure();
    void _manualInputExpiry();
    void _manualLatestInputAndLongHold();
    void _manualReleaseDuringBegin();
    void _padHoldAndRelease();
    void _padFailureRequiresRelease();
    void _capabilityAndPermissionGates();
    void _boundedStepAndCapturedStop();
    void _invalidAndReplacedOwner();
    void _stopPreemptsPendingStep();
    void _endpointChangeStopsOriginalOwner();
    void _staleStatusKeepsCapturedStop();
};
