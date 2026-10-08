#pragma once

#include "UnitTest.h"

class PixEagleVideoItemTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _renderedPixelsMatchContextAfterCoalescing();
    void _streamAndStoreEpochsDiscardOldFrames();
    void _invalidMetadataBlanks_data();
    void _invalidMetadataBlanks();
    void _negotiatedDeliverySize_data();
    void _negotiatedDeliverySize();
    void _freshness_data();
    void _freshness();
    void _freshnessExpiresWithoutChangingIdentity();
    void _surfaceChangesInvalidatePresentation_data();
    void _surfaceChangesInvalidatePresentation();
    void _textureFailureCannotPublishNewContext();
    void _rotationAndMirroring_data();
    void _rotationAndMirroring();
    void _nativeJpegDecodeRetainsPresentedIdentity();
    void _selectionRejectsUnverifiedGeometry_data();
    void _selectionRejectsUnverifiedGeometry();
    void _selectionPinsFrameAndMapsLetterbox();
    void _selectionRejectsUnswappedLayout();
    void _selectionCancelledByChanges_data();
    void _selectionCancelledByChanges();
    void _selectionAgeDoesNotFollowNewFrames();
};

class PixEagleLiveVideoTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _authenticatedBackendReachesPresentedFrame();
};
