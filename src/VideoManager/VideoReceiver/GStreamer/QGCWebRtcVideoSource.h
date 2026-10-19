#pragma once

#include <atomic>
#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
typedef struct _GstPad GstPad;
typedef struct _GstPadProbeInfo GstPadProbeInfo;
class QGCVideoFrameContextStore;

struct QGCWebRtcVideoStatistics
{
    std::atomic<quint64> missingPacketContext{0};
    std::atomic<quint64> missingMetadata{0};
    std::atomic<quint64> rejectedDimensions{0};
    std::atomic<quint64> corruptedFrames{0};
    std::atomic<quint64> unsafeReferenceFrames{0};
    std::atomic<quint64> associatedFrames{0};
    std::atomic<quint64> metadataJoinTimeouts{0};
    std::atomic<quint64> keyframeRequests{0};
    std::atomic<quint64> lastRejectedEncodedSize{0};
    std::atomic<quint64> lastRejectedDecodedSize{0};

    QJsonObject snapshot() const;
};

/// Memory-only signaling credentials bound by the caller to one approved endpoint.
struct QGCWebRtcVideoOptions
{
    QByteArray cookie;
    QString origin;
    QJsonArray iceServers;
    QStringList codecPreferences{QStringLiteral("H264"), QStringLiteral("VP8")};
    std::shared_ptr<QGCVideoFrameContextStore> frameContexts;
    std::shared_ptr<QGCWebRtcVideoStatistics> statistics = std::make_shared<QGCWebRtcVideoStatistics>();
};

/// Authenticated signaling plus native RTP reception. Retry belongs to the receiver owner.
class QGCWebRtcVideoSource final
{
public:
    static GstElement* create(const QUrl& signalingUrl, std::shared_ptr<const QGCWebRtcVideoOptions> options);
    static bool available();
    static bool activate(GstElement* source, GstBus* bus);
    static void deactivate(GstElement* source);
    static void configureDecoder(GstElement* source, GstElement* decoder);
    static void associatePresentedBuffer(GstElement* source, GstPad* pad, GstPadProbeInfo* info);

    ~QGCWebRtcVideoSource();
    Q_DISABLE_COPY_MOVE(QGCWebRtcVideoSource)

private:
    class Impl;
    explicit QGCWebRtcVideoSource(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> _impl;
};
