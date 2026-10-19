#include "PixEagleVideoController.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QGuiApplication>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtQuick/QQuickWindow>

#include "GStreamer.h"
#include "GstVideoReceiver.h"
#include "PixEagleManager.h"
#include "PixEagleSettings.h"
#include "QGCLoggingCategory.h"
#include "QGCVideoFrameContextStore.h"
#include "QGCWebSocketVideoSource.h"
#include "VideoManager.h"

#if defined(QGC_GST_WEBRTC)
#include "QGCWebRtcVideoSource.h"
#endif

QGC_LOGGING_CATEGORY(PixEagleVideoLog, "Custom.PixEagle.Video")

PixEagleVideoController::PixEagleVideoController(PixEagleManager* manager, PixEagleSettings* settings)
    : QObject(manager)
    , _manager(manager)
    , _targets(new PixEagleTargetController(this))
    , _settings(settings)
{
    _mediaNetwork = new QNetworkAccessManager(this);
    _retryTimer.setSingleShot(true);
    connect(&_retryTimer, &QTimer::timeout, this, &PixEagleVideoController::_start);
    _contextRefreshTimer.setSingleShot(true);
    _presentationWatchdog.setSingleShot(true);
    _presentationWatchdog.setInterval(10000);
    connect(_manager, &PixEagleManager::activeClientChanged, this, &PixEagleVideoController::_bindClient);
    connect(_settings->integrationEnabled(), &Fact::rawValueChanged, this, &PixEagleVideoController::_updateDesired);
    connect(_settings->videoEnabled(), &Fact::rawValueChanged, this, &PixEagleVideoController::_updateDesired);
    connect(VideoManager::instance(), &VideoManager::externalVideoStartRequested, this, [this]() {
        _suspended = false;
        _updateDesired();
    });
    connect(VideoManager::instance(), &VideoManager::externalVideoStopRequested, this, [this]() {
        _suspended = true;
        _stop();
    });
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, &PixEagleVideoController::_updateDesired);
    connect(&_presentationWatchdog, &QTimer::timeout, this, [this]() {
        if ((_state != State::Running && _state != State::Starting) || (_surface && _surface->frameFresh())) {
            return;
        }
        _failVideo(QStringLiteral("presentation"), tr("No fresh video frame was presented. Retrying."));
    });
}

void PixEagleVideoController::initialize()
{
    _bindClient();
}

PixEagleVideoController::~PixEagleVideoController()
{
    _cancelMediaConfig();
    if (_surface) {
        _surface->clearStream();
    }
    delete _receiver;
    if (_sink) {
        GStreamer::releaseVideoSink(_sink);
    }
}

void PixEagleVideoController::_bindClient()
{
    if (_client) {
        disconnect(_client, nullptr, this, nullptr);
    }
    _client = _manager->activeClient();
    _targets->setClient(_client);
    if (_client) {
        connect(_client, &PixEagleClient::changed, this, &PixEagleVideoController::_updateDesired);
        connect(_client, &PixEagleClient::endpointChanged, this, &PixEagleVideoController::_updateDesired);
    }
    _updateDesired();
}

bool PixEagleVideoController::_surfaceVisible() const
{
    const auto applicationState = qGuiApp->applicationState();
    return _surface && _surface->isVisible() && _surface->window() && _surface->window()->isVisible() &&
           applicationState != Qt::ApplicationSuspended && applicationState != Qt::ApplicationHidden;
}

void PixEagleVideoController::_updateDesired()
{
    if (_updating) {
        return;
    }
    QScopedValueRollback updating(_updating, true);
    _selected = _settings->integrationEnabled()->rawValue().toBool() &&
                _settings->videoEnabled()->rawValue().toBool() && _client && !_client->endpoint().isEmpty();
    VideoManager::instance()->setExternalVideoSource(
        _selected ? QUrl(QStringLiteral("qrc:/qml/PixEagle/PixEagleVideoView.qml")) : QUrl());

    QString key;
    if (_selected && _client->mediaAvailable()) {
        const auto context = _client->mediaContext();
        const auto video = context.value("video").toObject();
        if (!video.value("stream_id").toString().isEmpty() && !video.value("stream_epoch").toString().isEmpty()) {
            const QJsonArray identity{_client->endpoint(),
                                      QString::number(_client->mediaSessionGeneration()),
                                      context.value("instance_id"),
                                      context.value("runtime_id"),
                                      video.value("stream_id"),
                                      video.value("stream_epoch"),
                                      video.value("source_epoch"),
                                      video.value("variant"),
                                      video.value("width"),
                                      video.value("height"),
                                      video.value("delivery_scaling_version")};
            key = QString::fromUtf8(QJsonDocument(identity).toJson(QJsonDocument::Compact));
        }
    }
    if (_desiredKey != key) {
        _cancelMediaConfig();
        _mediaConfig = {};
        _mediaConfigKey.clear();
        _webRtcFailed = false;
        _desiredKey = key;
        _error.clear();
        _retryMs = 1000;
        _stop();
    }
    if (!_surfaceVisible() || _suspended || !VideoManager::instance()->hasVideo()) {
        if (_state != State::Idle) {
            _stop();
        }
    } else if (_state == State::Idle && !_retryTimer.isActive()) {
        _start();
    }
    emit changed();
}

void PixEagleVideoController::attachSurface(PixEagleVideoItem* surface)
{
    if (!surface || _surface == surface) {
        return;
    }
    if (_surface) {
        detachSurface(_surface);
    }
    _surface = surface;
    _targets->setSurface(surface);
    connect(surface, &PixEagleVideoItem::presentationChanged, this, &PixEagleVideoController::_presentationChanged);
    connect(surface, &PixEagleVideoItem::frameContextRejected, this, [this]() {
        if (_client && !_contextRefreshTimer.isActive()) {
            _client->refresh();
            _contextRefreshTimer.start(1000);
        }
    });
    connect(surface, &QQuickItem::visibleChanged, this, &PixEagleVideoController::_updateDesired);
    connect(surface, &QQuickItem::windowChanged, this, [this](QQuickWindow* window) {
        _stop();
        if (window) {
            connect(window, &QWindow::visibleChanged, this, &PixEagleVideoController::_updateDesired,
                    Qt::UniqueConnection);
        }
        _updateDesired();
    });
    connect(surface, &QObject::destroyed, this, [this]() {
        _surface = nullptr;
        _stop();
    });
    if (surface->window()) {
        connect(surface->window(), &QWindow::visibleChanged, this, &PixEagleVideoController::_updateDesired,
                Qt::UniqueConnection);
    }
    _updateDesired();
}

void PixEagleVideoController::detachSurface(PixEagleVideoItem* surface)
{
    if (!surface || surface != _surface) {
        return;
    }
    _stop();
    disconnect(surface, nullptr, this, nullptr);
    _targets->setSurface(nullptr);
    _surface = nullptr;
}

bool PixEagleVideoController::_createReceiver()
{
    if (_receiver) {
        return true;
    }
    if (!VideoManager::instance()->waitForVideoBackendReady(std::chrono::milliseconds(0))) {
        return false;
    }
    GStreamer::VideoSinkConfig config;
    config.gpuZeroCopy = false;
    if (!_sink) {
        _sink = GStreamer::createVideoSink(config);
    }
    if (!_sink) {
        return false;
    }
    _receiver = new GstVideoReceiver(this);
    const auto attempt = ++_attemptGeneration;
    _receiver->setName(QStringLiteral("pixeagleVideo"));
    _receiver->setSink(_sink);
    _receiver->setLowLatency(true);
    _receiver->setAutoReconnect(false);
    connect(_receiver, &VideoReceiver::onStartComplete, this, [this, attempt](VideoReceiver::STATUS status) {
        if (attempt != _attemptGeneration || _state != State::Starting) {
            return;
        }
        if (status == VideoReceiver::STATUS_OK) {
            _state = State::Running;
            _receiver->startDecoding(_sink);
            _presentationWatchdog.start();
        } else {
            _failVideo(QStringLiteral("startup"), tr("Video could not connect. Check streaming access in PixEagle."));
        }
        emit changed();
    });
    connect(_receiver, &VideoReceiver::onStopComplete, this, [this, attempt](VideoReceiver::STATUS) {
        if (attempt != _attemptGeneration || _state == State::Idle) {
            return;
        }
        if (_state != State::Stopping && _usingWebRtc) {
            _webRtcFailed = true;
            qCDebug(PixEagleVideoLog) << "WebRTC transport ended; using JPEG fallback; attempt" << attempt;
        }
        _state = State::Idle;
        _usingWebRtc = false;
        qCDebug(PixEagleVideoLog) << "Receiver stopped; attempt" << attempt;
        _retireReceiver();
        _presentationWatchdog.stop();
        _runningKey.clear();
        if (_surface) {
            _surface->clearStream();
        }
        VideoManager::instance()->setExternalVideoState(false, {});
        if (!_desiredKey.isEmpty() && !_suspended && _surfaceVisible()) {
            _retryTimer.start(_retryMs);
            _retryMs = qMin(_retryMs * 2, 15000);
        }
        emit changed();
    });
    connect(_receiver, &VideoReceiver::onStartDecodingComplete, this, [this, attempt](VideoReceiver::STATUS status) {
        if (attempt == _attemptGeneration && status != VideoReceiver::STATUS_OK && _state == State::Running) {
            _failVideo(QStringLiteral("decode"), tr("Video could not be decoded. Retrying."));
        }
    });
    return true;
}

void PixEagleVideoController::_retireReceiver()
{
    if (_receiver) {
        disconnect(_receiver, nullptr, this, nullptr);
        _receiver->deleteLater();
        _receiver = nullptr;
    }
}

bool PixEagleVideoController::_nativeTransportAllowed(const QJsonObject& config, bool runtimeAvailable,
                                                      bool previousFailure)
{
    return runtimeAvailable && !previousFailure && _nativeConfigEligible(config);
}

bool PixEagleVideoController::_nativeConfigEligible(const QJsonObject& config)
{
    const auto native = config.value("native_webrtc").toObject();
    const auto protocol = config.value("default_protocol").toString();
    return config.value("schema_version") == QJsonValue(1) &&
           config.value("source") == QJsonValue("streaming_client_config") &&
           config.value("streaming_enabled") == QJsonValue(true) && (protocol == "auto" || protocol == "webrtc") &&
           config.value("transports").toObject().value("webrtc") == QJsonValue(true) &&
           native.value("available") == QJsonValue(true) && native.value("runtime_ready") == QJsonValue(true) &&
           native.value("provider") == QJsonValue("gstreamer") &&
           native.value("frame_association") == QJsonValue("verified") &&
           native.value("missing_components").isArray() && native.value("missing_components").toArray().isEmpty() &&
           native.value("signaling_path") == QJsonValue("/ws/webrtc_signaling") &&
           native.value("fallback") == QJsonValue("websocket_jpeg");
}

void PixEagleVideoController::_cancelMediaConfig()
{
    if (_mediaConfigReply) {
        disconnect(_mediaConfigReply, nullptr, this, nullptr);
        _mediaConfigReply->abort();
        _mediaConfigReply->deleteLater();
        _mediaConfigReply = nullptr;
    }
}

void PixEagleVideoController::_requestMediaConfig()
{
    if (_mediaConfigReply || !_client || !_client->mediaAvailable() || _desiredKey.isEmpty()) {
        return;
    }
    QUrl url(_client->endpoint());
    url.setPath(url.path() + "/api/v1/streams/client-config");
    QNetworkRequest request(url);
    request.setTransferTimeout(3000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setRawHeader("Cookie", _client->mediaCookie());
    request.setRawHeader("Origin", _client->mediaOrigin().toUtf8());
    request.setRawHeader("Accept", "application/json");
    auto* reply = _mediaNetwork->get(request);
    _mediaConfigReply = reply;
    constexpr qint64 responseLimit = 128 * 1024;
    reply->setReadBufferSize(responseLimit + 1);
    connect(reply, &QNetworkReply::readyRead, this, [reply]() {
        if (reply->bytesAvailable() > responseLimit) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, key = _desiredKey]() {
        reply->deleteLater();
        if (_mediaConfigReply != reply) {
            return;
        }
        _mediaConfigReply = nullptr;
        if (key != _desiredKey || !_client || !_client->mediaAvailable()) {
            return;
        }
        _mediaConfigKey = key;
        _mediaConfig = {};
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && status == 200) {
            const auto bytes = reply->read(responseLimit + 1);
            if (bytes.size() <= responseLimit) {
                _mediaConfig = QJsonDocument::fromJson(bytes).object();
            }
        } else if (status == 401) {
            // The independent media request never mutates authentication/verification sequencing.
            _client->refresh();
        }
        _updateDesired();
    });
}

void PixEagleVideoController::_failVideo(const QString& stage, const QString& message)
{
    _error = message;
    if (_usingWebRtc) {
        _webRtcFailed = true;
    }
    qCDebug(PixEagleVideoLog) << "Receiver failure; stage" << stage << "attempt" << _attemptGeneration << "WebRTC"
                              << _usingWebRtc;
    _stop();
}

void PixEagleVideoController::_start()
{
    if (_state != State::Idle || _desiredKey.isEmpty() || !_client || !_client->mediaAvailable() || _suspended ||
        !_surfaceVisible() || !VideoManager::instance()->hasVideo()) {
        return;
    }
#if defined(QGC_GST_WEBRTC)
    if (_mediaConfigKey != _desiredKey && QGCWebRtcVideoSource::available()) {
        _requestMediaConfig();
        return;
    }
#endif
    if (!_createReceiver()) {
        _error = tr("The video backend is not ready.");
        _retryTimer.start(2000);
        emit changed();
        return;
    }
    const auto cookie = _client->mediaCookie();
    if (cookie.isEmpty()) {
        _error = tr("Video needs a current PixEagle session. Sign in again.");
        emit changed();
        return;
    }
    _contexts = std::make_shared<QGCVideoFrameContextStore>();
    _surface->setStream(_contexts, _client->mediaContext());
    _receiver->setWidget(_surface);
    if (!GStreamer::setupQVideoSinkElement(_sink, _surface->videoSink(), _receiver)) {
        _surface->clearStream();
        _error = tr("The video display could not be prepared.");
        emit changed();
        return;
    }
    _usingWebRtc = false;
#if defined(QGC_GST_WEBRTC)
    _usingWebRtc = _nativeTransportAllowed(_mediaConfig, QGCWebRtcVideoSource::available(), _webRtcFailed);
    if (_usingWebRtc) {
        auto options = std::make_shared<QGCWebRtcVideoOptions>();
        options->cookie = cookie;
        options->origin = _client->mediaOrigin();
        options->iceServers = _mediaConfig.value("ice_servers").toArray();
        options->frameContexts = _contexts;
        _receiver->setWebRtcOptions(options);
        QUrl url(_client->endpoint());
        url.setScheme(url.scheme() == "https" ? "webrtc+wss" : "webrtc+ws");
        url.setPath(url.path() + "/ws/webrtc_signaling");
        _receiver->setUri(url.toString(QUrl::FullyEncoded));
    } else
#endif
    {
        auto options = std::make_shared<QGCWebSocketVideoOptions>();
        options->cookie = cookie;
        options->origin = _client->mediaOrigin();
        options->requireFrameMetadata = true;
        options->frameContexts = _contexts;
        _receiver->setWebSocketOptions(options);
        _receiver->setUri(_client->mediaUrl().toString(QUrl::FullyEncoded));
    }
    _runningKey = _desiredKey;
    _state = State::Starting;
    _error.clear();
    qCDebug(PixEagleVideoLog) << "Starting receiver; attempt" << _attemptGeneration;
    _presentationWatchdog.start();
    _receiver->start(5);
    emit changed();
}

void PixEagleVideoController::_stop()
{
    _targets->cancelGesture();
    _cancelMediaConfig();
    _retryTimer.stop();
    _presentationWatchdog.stop();
    if (_surface) {
        _surface->clearStream();
    }
    VideoManager::instance()->setExternalVideoState(false, {});
    if (_state == State::Starting || _state == State::Running) {
        _state = State::Stopping;
        qCDebug(PixEagleVideoLog) << "Stopping receiver; attempt" << _attemptGeneration;
        _receiver->stop();
    }
    emit changed();
}

void PixEagleVideoController::_presentationChanged()
{
    const bool known = _surface && _surface->presentationKnown();
    const bool fresh = known && _surface->frameFresh();
    if (fresh) {
        _presentationWatchdog.stop();
    } else if (_state == State::Running && !_presentationWatchdog.isActive()) {
        _presentationWatchdog.start();
    }
    VideoManager::instance()->setExternalVideoState(known, known ? _surface->sourceSize() : QSize());
    if (fresh) {
        _retryMs = 1000;
        _error.clear();
    }
    emit changed();
}

QString PixEagleVideoController::ownerText() const
{
    if (_client && _client->companionOnly()) {
        return tr("PixEagle · No aircraft");
    }
    return _client ? tr("%1 · PixEagle").arg(_client->vehicleLabel()) : tr("PixEagle");
}

QString PixEagleVideoController::statusText() const
{
    if (!_client || !_client->authenticated()) {
        return _client ? _client->statusText() : tr("Connect a vehicle to QGC.");
    }
    if (!_client->mediaAvailable()) {
        return tr("Video is unavailable. Check PixEagle's streaming setup and this account's access.");
    }
    if (!_error.isEmpty()) {
        return _error;
    }
    if (_surface && _surface->presentationKnown()) {
        return _surface->frameFresh() ? tr("Video active") : tr("Video delayed or paused. Check the PixEagle source.");
    }
    return tr("Waiting for PixEagle video");
}

QString PixEagleVideoController::runtimeStatusText() const
{
    return _client ? _client->runtimeStatusText() : tr("Tracking and following status unavailable");
}

QString PixEagleVideoController::compactText() const
{
    const QString owner = _client && !_client->companionOnly() ? _client->vehicleLabel() : tr("PixEagle");
    const QString status =
        !_client || !_client->authenticated()
            ? tr("Sign in")
            : (_surface && _surface->presentationKnown() ? (_surface->frameFresh() ? tr("Active") : tr("Delayed"))
                                                         : tr("No video"));
    return tr("%1 · %2").arg(owner, status);
}

bool PixEagleVideoController::live() const
{
    return _client && _client->mediaAvailable() && _surface && _surface->frameFresh();
}
