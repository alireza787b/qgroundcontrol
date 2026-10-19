#pragma once

#include "UnitTest.h"

class QGCWebRtcVideoSourceTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _nativeDecoderRecoveryPreservesStockPolicy();
    void _presentedFrameMatchesExactRtpContext_data();
    void _presentedFrameMatchesExactRtpContext();
};
