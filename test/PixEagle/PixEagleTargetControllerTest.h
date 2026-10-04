#pragma once

#include "UnitTest.h"

class PixEagleTargetControllerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _cameraModesAndEngineCatalog();
    void _selectionUsesPressedFrame_data();
    void _selectionUsesPressedFrame();
    void _tapRetargetUsesFreshContext_data();
    void _tapRetargetUsesFreshContext();
    void _tapOutcomeNeverReplaysGesture_data();
    void _tapOutcomeNeverReplaysGesture();
    void _manualSelectionIsOneShot_data();
    void _manualSelectionIsOneShot();
    void _contextChangesCancelSelection_data();
    void _contextChangesCancelSelection();
    void _smartRectangleIsRejected();
    void _staleFrameRevisionIsRejected();
    void _cancelUsesTrackingOwnerWithoutVideo_data();
    void _cancelUsesTrackingOwnerWithoutVideo();
    void _lostTargetCanBeCancelledWithoutVideo_data();
    void _lostTargetCanBeCancelledWithoutVideo();
    void _trackingStateExpiresAndRecovers();
    void _modeAndTrackerRequests_data();
    void _modeAndTrackerRequests();
    void _trackerCatalogAliases_data();
    void _trackerCatalogAliases();
    void _viewOnlyAndFollowingBlockControls_data();
    void _viewOnlyAndFollowingBlockControls();
    void _unknownOutcomeRemainsVisible();
    void _controlIntentCannotCrossContext_data();
    void _controlIntentCannotCrossContext();
    void _modelControlOwnership_data();
    void _modelControlOwnership();
};
