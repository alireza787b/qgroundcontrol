#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>

#include "PixEagleClient.h"
#include "PixEagleTargetController.h"
#include "PixEagleVideoItem.h"

class GstVideoReceiver;
class QNetworkAccessManager;
class QNetworkReply;
class PixEagleManager;
class PixEagleSettings;
class QGCVideoFrameContextStore;

/// One displayed companion stream; each vehicle retains its own authenticated client.
class PixEagleVideoController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by PixEagleManager")
    Q_PROPERTY(QString ownerText READ ownerText NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString compactText READ compactText NOTIFY changed)
    Q_PROPERTY(QString runtimeStatusText READ runtimeStatusText NOTIFY changed)
    Q_PROPERTY(bool live READ live NOTIFY changed)
    Q_PROPERTY(PixEagleTargetController* targets READ targets CONSTANT)

public:
    PixEagleVideoController(PixEagleManager* manager, PixEagleSettings* settings);
    ~PixEagleVideoController() override;
    void initialize();

    QString ownerText() const;
    QString statusText() const;
    QString compactText() const;
    QString runtimeStatusText() const;
    bool live() const;

    PixEagleTargetController* targets() const { return _targets; }

    Q_INVOKABLE void attachSurface(PixEagleVideoItem* surface);
    Q_INVOKABLE void detachSurface(PixEagleVideoItem* surface);

signals:
    void changed();

private:
    friend class PixEagleVideoControllerTest;
    enum class State
    {
        Idle,
        Starting,
        Running,
        Stopping
    };
    void _bindClient();
    void _updateDesired();
    void _start();
    void _stop();
    void _presentationChanged();
    bool _surfaceVisible() const;
    bool _createReceiver();
    void _retireReceiver();
    void _requestMediaConfig();
    void _cancelMediaConfig();
    void _failVideo(const QString& stage, const QString& message);
    static bool _nativeConfigEligible(const QJsonObject& config);
    static bool _nativeTransportAllowed(const QJsonObject& config, bool runtimeAvailable, bool previousFailure);

    PixEagleManager* _manager = nullptr;
    PixEagleTargetController* _targets = nullptr;
    PixEagleSettings* _settings = nullptr;
    QPointer<PixEagleClient> _client;
    QPointer<PixEagleVideoItem> _surface;
    GstVideoReceiver* _receiver = nullptr;
    void* _sink = nullptr;
    std::shared_ptr<QGCVideoFrameContextStore> _contexts;
    QTimer _retryTimer;
    QTimer _contextRefreshTimer;
    QTimer _presentationWatchdog;
    QString _desiredKey;
    QString _runningKey;
    QNetworkAccessManager* _mediaNetwork = nullptr;
    QPointer<QNetworkReply> _mediaConfigReply;
    QJsonObject _mediaConfig;
    QString _mediaConfigKey;
    bool _usingWebRtc = false;
    bool _webRtcFailed = false;
    QString _error;
    State _state = State::Idle;
    int _retryMs = 1000;
    quint64 _attemptGeneration = 0;
    bool _selected = false;
    bool _suspended = false;
    bool _updating = false;
};
