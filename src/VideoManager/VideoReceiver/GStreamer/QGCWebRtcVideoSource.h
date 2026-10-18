#pragma once

#include <memory>

#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
typedef struct _GstPad GstPad;
typedef struct _GstPadProbeInfo GstPadProbeInfo;
class QGCVideoFrameContextStore;

/// Memory-only signaling credentials bound by the caller to one approved endpoint.
struct QGCWebRtcVideoOptions
{
    QByteArray cookie;
    QString origin;
    QJsonArray iceServers;
    QStringList codecPreferences{QStringLiteral("H264"), QStringLiteral("VP8")};
    std::shared_ptr<QGCVideoFrameContextStore> frameContexts;
};

/// Authenticated signaling plus native RTP reception. Retry belongs to the receiver owner.
class QGCWebRtcVideoSource final
{
public:
    static GstElement* create(const QUrl& signalingUrl, std::shared_ptr<const QGCWebRtcVideoOptions> options);
    static bool available();
    static bool activate(GstElement* source, GstBus* bus);
    static void deactivate(GstElement* source);
    static void associatePresentedBuffer(GstElement* source, GstPad* pad, GstPadProbeInfo* info);

    ~QGCWebRtcVideoSource();
    Q_DISABLE_COPY_MOVE(QGCWebRtcVideoSource)

private:
    class Impl;
    explicit QGCWebRtcVideoSource(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> _impl;
};
