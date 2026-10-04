#pragma once

#include "UnitTest.h"

class PixEagleModelClientTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _inventoryDiscoveryAndPermissions_data();
    void _inventoryDiscoveryAndPermissions();
    void _invalidInventory_data();
    void _invalidInventory();
    void _inventoryExpiresWithoutDemand();
    void _inventoryDoesNotRewindTargetRevision();
    void _selectionEnvelopeAndSerialization();
    void _selectionGates_data();
    void _selectionGates();
    void _selectionOutcomes_data();
    void _selectionOutcomes();
    void _longSelectionKeepsConnectionFresh();
    void _lateReplyCannotCrossContext_data();
    void _lateReplyCannotCrossContext();
};
