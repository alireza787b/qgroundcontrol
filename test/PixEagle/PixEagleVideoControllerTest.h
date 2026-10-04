#pragma once

#include "UnitTest.h"

class PixEagleVideoControllerTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _preEnabledVideoPublishesControllerBeforeView();
    void _videoEnabledAfterInitialization();
    void _initializationIsIdempotent();
};
