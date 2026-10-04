#pragma once

#include "UnitTest.h"

class OnScreenCameraTrackingControllerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _stockCoordinates_data();
    void _stockCoordinates();
    void _externalOwner_data();
    void _externalOwner();
    void _cancelAndRearm();
    void _replacingPointerPreservesSelection();
    void _cancelSelectionWithoutPointer_data();
    void _cancelSelectionWithoutPointer();
    void _ownerChangeCancels();
};
