#include "PixEagleVideoController.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>

#include "GStreamer.h"
#include "GstVideoReceiver.h"
#include "PixEagleManager.h"
#include "PixEagleSettings.h"
#include "QGCVideoFrameContextStore.h"
#include "QGCWebSocketVideoSource.h"
#include "VideoManager.h"

PixEagleVideoController::PixEagleVideoController(PixEagleManager* manager, PixEagleSettings* settings)
    : QObject(manager)
    , _manager(manager)
    , _targets(new PixEagleTargetController(this))
    , _settings(settings)
{
    _retryTimer.setSingleShot(true);
    connect(&_retryTimer, &QTimer::timeout, this, &PixEagleVideoController::_start);
    _contextRefreshTimer.setSingleShot(true);
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
}

void PixEagleVideoController::initialize()
{
    _bindClient();
}

PixEagleVideoController::~PixEagleVideoController()
{
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
        const auto context = _client->connectionContext();
        const auto video = context.value("video").toObject();
        if (!video.value("stream_id").toString().isEmpty() && !video.value("stream_epoch").toString().isEmpty()) {
            const QJsonArray identity{_client->endpoint(),
                                      QString::number(_client->sessionGeneration()),
                                      context.value("instance_id"),
                                      context.value("runtime_id"),
                                      context.value("command").toObject().value("connection_generation"),
                                      context.value("telemetry").toObject().value("connection_generation"),
                                      video.value("stream_id"),
                                      video.value("stream_epoch"),
                                      video.value("source_epoch"),
                                      video.value("variant"),
                                      video.value("width"),
                                      video.value("height")};
            key = QString::fromUtf8(QJsonDocument(identity).toJson(QJsonDocument::Compact));
        }
    }
    if (_desiredKey != key) {
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
    _sink = GStreamer::createVideoSink(config);
    if (!_sink) {
        return false;
    }
    _receiver = new GstVideoReceiver(this);
    _receiver->setName(QStringLiteral("pixeagleVideo"));
    _receiver->setSink(_sink);
    _receiver->setLowLatency(true);
    _receiver->setAutoReconnect(false);
    connect(_receiver, &VideoReceiver::onStartComplete, this, [this](VideoReceiver::STATUS status) {
        if (_state != State::Starting) {
            return;
        }
        if (status == VideoReceiver::STATUS_OK) {
            _state = State::Running;
            _receiver->startDecoding(_sink);
        } else {
            _state = State::Idle;
            _runningKey.clear();
            _error = tr("Video could not connect. Check streaming access and the allowed Origin in PixEagle.");
            if (_surface) {
                _surface->clearStream();
            }
            _retryTimer.start(_retryMs);
            _retryMs = qMin(_retryMs * 2, 15000);
        }
        emit changed();
    });
    connect(_receiver, &VideoReceiver::onStopComplete, this, [this](VideoReceiver::STATUS) {
        if (_state == State::Idle) {
            return;
        }
        _state = State::Idle;
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
    connect(_receiver, &VideoReceiver::onStartDecodingComplete, this, [this](VideoReceiver::STATUS status) {
        if (status != VideoReceiver::STATUS_OK && _state == State::Running) {
            _error = tr("Video could not be decoded. Check PixEagle's stream.");
            _stop();
        }
    });
    return true;
}

void PixEagleVideoController::_start()
{
    if (_state != State::Idle || _desiredKey.isEmpty() || !_client || !_client->mediaAvailable() || _suspended ||
        !_surfaceVisible() || !VideoManager::instance()->hasVideo()) {
        return;
    }
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
    auto options = std::make_shared<QGCWebSocketVideoOptions>();
    options->cookie = cookie;
    options->origin = _client->mediaOrigin();
    options->requireFrameMetadata = true;
    options->frameContexts = _contexts;
    _surface->setStream(_contexts, _client->connectionContext());
    _receiver->setWidget(_surface);
    if (!GStreamer::setupQVideoSinkElement(_sink, _surface->videoSink(), _receiver)) {
        _surface->clearStream();
        _error = tr("The video display could not be prepared.");
        emit changed();
        return;
    }
    _receiver->setWebSocketOptions(options);
    _receiver->setUri(_client->mediaUrl().toString(QUrl::FullyEncoded));
    _runningKey = _desiredKey;
    _state = State::Starting;
    _error.clear();
    _receiver->start(5);
    emit changed();
}

void PixEagleVideoController::_stop()
{
    _targets->cancelGesture();
    _retryTimer.stop();
    if (_surface) {
        _surface->clearStream();
    }
    VideoManager::instance()->setExternalVideoState(false, {});
    if (_state == State::Starting || _state == State::Running) {
        _state = State::Stopping;
        _receiver->stop();
    }
    emit changed();
}

void PixEagleVideoController::_presentationChanged()
{
    const bool known = _surface && _surface->presentationKnown();
    VideoManager::instance()->setExternalVideoState(known, known ? _surface->sourceSize() : QSize());
    if (known) {
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
