#include <cmath>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSet>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "PixEagleClient.h"
#include "QGCLoggingCategory.h"

QGC_LOGGING_CATEGORY(PixEagleCameraLog, "qgc.pixeagle.camera")

namespace {
constexpr int CAMERA_TIMEOUT_MS = 4000;
constexpr int CAMERA_FRESH_MS = 6000;
constexpr qint64 CAMERA_MAX_BYTES = 64 * 1024;

QNetworkRequest cameraRequest(QUrl url, const QString& path)
{
    url.setPath(url.path() + path);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(CAMERA_TIMEOUT_MS);
    request.setRawHeader("Accept", "application/json");
    return request;
}

void boundCameraReply(QNetworkReply* reply)
{
    reply->setReadBufferSize(CAMERA_MAX_BYTES + 1);
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply]() {
        if (reply->bytesAvailable() > CAMERA_MAX_BYTES) {
            reply->abort();
        }
    });
    QTimer::singleShot(CAMERA_TIMEOUT_MS, reply, [reply]() {
        if (!reply->isFinished()) {
            reply->abort();
        }
    });
}
}  // namespace

void PixEagleClient::_initializeCameraState()
{
    _cameraClientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    _cameraExpiryTimer.setSingleShot(true);
    _cameraRenewTimer.setInterval(100);
    connect(&_cameraRenewTimer, &QTimer::timeout, this, &PixEagleClient::_renewCameraManual);
    connect(this, &PixEagleClient::changed, this, [this]() {
        if (!cameraFresh() && cameraCanStop()) {
            stopCamera(_cameraCapturedContext);
        }
        emit cameraChanged();
    });
    connect(&_cameraExpiryTimer, &QTimer::timeout, this, [this]() {
        if (cameraCanStop()) {
            stopCamera(_cameraCapturedContext);
        }
        emit cameraChanged();
    });
}

bool PixEagleClient::cameraAvailable() const
{
    // Camera ownership is a companion operation. It does not require a
    // verified PX4 association; aircraft following remains separately gated.
    return _sessionReady() && _context.value("capabilities").toArray().contains("camera.control.v1") &&
           _context.value("permissions").toObject().value("scopes").toArray().contains("control:read");
}

bool PixEagleClient::cameraFresh() const
{
    const auto video = _context.value("video").toObject();
    return cameraAvailable() && !_cameraStatus.isEmpty() && _cameraAge.isValid() &&
           _cameraAge.elapsed() < CAMERA_FRESH_MS &&
           _cameraStatus.value("instance_id") == _context.value("instance_id") &&
           _cameraStatus.value("runtime_id") == _context.value("runtime_id") &&
           (!video.contains("source_epoch") || _cameraStatus.value("source_epoch") == video.value("source_epoch"));
}

QVariantMap PixEagleClient::cameraStatus() const
{
    if (!cameraFresh()) {
        return {};
    }
    auto status = _cameraStatus.toVariantMap();
    auto telemetry = status.value("telemetry").toMap();
    if (!telemetry.isEmpty()) {
        const double age = telemetry.value("angles_age_ms").toDouble() + _cameraAge.elapsed();
        const double limit = telemetry.value("angles_max_age_ms").toDouble();
        telemetry.insert("angles_age_ms", age);
        telemetry.insert("angles_fresh", telemetry.value("angles_fresh").toBool() && limit > 0 && age <= limit);
        status.insert("telemetry", telemetry);
    }
    return status;
}

bool PixEagleClient::cameraActionPending() const
{
    return _cameraActionReply || _cameraStopReply;
}

bool PixEagleClient::cameraCanStop() const
{
    return _authenticated && _network && !_cameraCapturedGuard.isEmpty() && !_cameraCapturedContext.isEmpty() &&
           !_csrfHeader.isEmpty() && !_csrfToken.isEmpty() && !_cameraStopReply;
}

void PixEagleClient::setCameraRequested(bool requested)
{
    if (_cameraRequested == requested) {
        return;
    }
    _cameraRequested = requested;
    if (requested) {
        refreshCamera();
    } else if (cameraCanStop()) {
        stopCamera(_cameraCapturedContext);
    }
}

bool PixEagleClient::_acceptCameraStatus(const QJsonObject& data)
{
    const auto guard = data.value("guard").toObject();
    if (data.value("instance_id") != _context.value("instance_id") ||
        data.value("runtime_id") != _context.value("runtime_id") || !data.value("enabled").isBool() ||
        !data.value("available").isBool() || !data.value("connected").isBool() ||
        !data.value("motion_active").isBool() || !data.value("capabilities").isArray() ||
        data.value("capabilities").toArray().size() > 32 || !data.value("selection_modes").isArray() ||
        data.value("selection_modes").toArray().size() > 16 || !data.value("following_active").isBool()) {
        return false;
    }
    if (data.value("enabled").toBool() &&
        (data.value("camera_id").toString().isEmpty() || data.value("camera_generation").toString().isEmpty() ||
         guard.value("camera_id") != data.value("camera_id") ||
         guard.value("camera_generation") != data.value("camera_generation") ||
         guard.value("source_epoch") != data.value("source_epoch"))) {
        return false;
    }
    QSet<QString> modes;
    for (const auto& value : data.value("selection_modes").toArray()) {
        const auto mode = value.toObject();
        const QString id = mode.value("id").toString();
        if (id.isEmpty() || modes.contains(id) || mode.value("label").toString().isEmpty() ||
            !mode.value("point").isBool() || !mode.value("rectangle").isBool()) {
            return false;
        }
        modes.insert(id);
    }
    for (const auto& value : data.value("capabilities").toArray()) {
        if (!value.isString() || value.toString().isEmpty()) {
            return false;
        }
    }
    if (!_cameraCapturedGuard.isEmpty() && guard != _cameraCapturedGuard && cameraCanStop()) {
        stopCamera(_cameraCapturedContext);
    }
    _cameraStatus = data;
    _cameraAge.start();
    _cameraExpiryTimer.start(CAMERA_FRESH_MS);
    return true;
}

void PixEagleClient::refreshCamera()
{
    if (!_cameraRequested || !cameraAvailable() || _cameraReply) {
        return;
    }
    auto* reply = _network->get(cameraRequest(_endpoint, QStringLiteral("/api/v1/gimbal/control")));
    _cameraReply = reply;
    boundCameraReply(reply);
    QElapsedTimer age;
    age.start();
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, generation = _generation, commandGeneration = _cameraCommandGeneration, age]() {
                reply->deleteLater();
                if (generation != _generation || _cameraReply != reply) {
                    return;
                }
                _cameraReply = nullptr;
                if (commandGeneration != _cameraCommandGeneration) {
                    refreshCamera();
                    return;
                }
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (status == 401) {
                    _resetSession();
                    _error = tr("Your session expired. Sign in again.");
                    emit changed();
                    return;
                }
                const auto bytes = reply->isOpen() ? reply->read(CAMERA_MAX_BYTES + 1) : QByteArray{};
                if (reply->error() == QNetworkReply::NoError && status == 200 && bytes.size() <= CAMERA_MAX_BYTES &&
                    age.elapsed() < CAMERA_TIMEOUT_MS && _acceptCameraStatus(QJsonDocument::fromJson(bytes).object())) {
                    _cameraAge = age;
                    _cameraExpiryTimer.start(CAMERA_FRESH_MS - static_cast<int>(age.elapsed()));
                    if (_cameraManualState == QStringLiteral("refreshing")) {
                        _cameraManualState.clear();
                    }
                } else {
                    _cameraAge.invalidate();
                    _cameraExpiryTimer.stop();
                    if (cameraCanStop()) {
                        stopCamera(_cameraCapturedContext);
                    }
                }
                emit cameraChanged();
            });
}

QString PixEagleClient::captureCameraContext() const
{
    if (!cameraFresh()) {
        return {};
    }
    return QString::number(_generation) + "|" + _endpoint.toString() + "|" +
           QString::fromUtf8(QJsonDocument(_cameraStatus.value("guard").toObject()).toJson(QJsonDocument::Compact));
}

bool PixEagleClient::cameraStep(const QString& operation, int direction, const QString& context)
{
    static const QSet<QString> operations{"pan", "tilt", "roll", "zoom", "home"};
    const auto scopes = _context.value("permissions").toObject().value("scopes").toArray();
    if (context.isEmpty() || context != captureCameraContext() || cameraActionPending() || !cameraFresh() ||
        !_cameraStatus.value("available").toBool() || !_cameraStatus.value("enabled").toBool() ||
        !_cameraStatus.value("connected").toBool() || _cameraStatus.value("following_active").toBool() ||
        !scopes.contains("actions:execute") || !operations.contains(operation) ||
        !_cameraStatus.value("capabilities").toArray().contains(operation) ||
        (operation != "home" && direction != -1 && direction != 1)) {
        _cameraError = tr("Camera changed or is unavailable. Review its current status before moving it.");
        emit cameraChanged();
        return false;
    }
    _cameraStopGestureId.clear();
    _cameraCapturedGuard = _cameraStatus.value("guard").toObject();
    _cameraCapturedContext = context;
    QJsonObject body{{"operation", operation},
                     {"camera_context", QJsonObject{{"guard", _cameraCapturedGuard}, {"client_id", _cameraClientId}}}};
    if (operation != "home") {
        body.insert("direction", direction);
    }
    return _postCameraAction(body, false);
}

bool PixEagleClient::_validCameraIntent(const QString& operation, double value) const
{
    static const QSet<QString> axes{"pan", "tilt", "roll", "zoom"};
    return axes.contains(operation) && std::isfinite(value) && value >= -1 && value <= 1 &&
           _cameraStatus.value("capabilities").toArray().contains(operation);
}

bool PixEagleClient::beginCameraManual(const QString& operation, double value, const QString& context)
{
    const auto capabilities = _cameraStatus.value("capabilities").toArray();
    const auto scopes = _context.value("permissions").toObject().value("scopes").toArray();
    if (!_validCameraIntent(operation, value) || context.isEmpty() || context != captureCameraContext() ||
        cameraActionPending() || !cameraFresh() || !_cameraGestureId.isEmpty() ||
        !_cameraStatus.value("available").toBool() || !_cameraStatus.value("enabled").toBool() ||
        !_cameraStatus.value("connected").toBool() || _cameraStatus.value("following_active").toBool() ||
        !capabilities.contains("manual_begin") || !capabilities.contains("manual_update") ||
        !scopes.contains("actions:execute")) {
        return false;
    }
    _cameraCapturedGuard = _cameraStatus.value("guard").toObject();
    _cameraCapturedContext = context;
    _cameraGestureId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    _cameraSequence = 0;
    _cameraStopGestureId.clear();
    _cameraBeginAccepted = false;
    _cameraManualState = QStringLiteral("preparing");
    _cameraIntent = {{"axis", operation}, {"value", value}};
    _cameraInputAge.start();
    ++_cameraCommandGeneration;
    const bool sent = _postCameraAction(
        {{"operation", "manual_begin"},
         {"gesture_id", _cameraGestureId},
         {"sequence", _cameraSequence},
         {"intent", _cameraIntent},
         {"camera_context", QJsonObject{{"guard", _cameraCapturedGuard}, {"client_id", _cameraClientId}}}},
        false);
    if (sent) {
        _cameraRenewTimer.start();
    } else {
        _cameraGestureId.clear();
        _cameraManualState.clear();
    }
    return sent;
}

bool PixEagleClient::updateCameraManual(const QString& operation, double value, const QString& context)
{
    if (_cameraGestureId.isEmpty() || context != _cameraCapturedContext || !_validCameraIntent(operation, value) ||
        !_cameraError.isEmpty()) {
        return false;
    }
    _cameraIntent = {{"axis", operation}, {"value", value}};
    _cameraInputAge.start();
    return true;
}

void PixEagleClient::_renewCameraManual()
{
    if (_cameraGestureId.isEmpty()) {
        _cameraRenewTimer.stop();
        return;
    }
    if (!_cameraInputAge.isValid() || _cameraInputAge.elapsed() > 150 || !cameraFresh() ||
        captureCameraContext() != _cameraCapturedContext || !_cameraError.isEmpty()) {
        stopCamera(_cameraCapturedContext);
        return;
    }
    if (!_cameraBeginAccepted || cameraActionPending()) {
        return;
    }
    _postCameraAction(
        {{"operation", "manual_update"},
         {"gesture_id", _cameraGestureId},
         {"sequence", ++_cameraSequence},
         {"intent", _cameraIntent},
         {"camera_context", QJsonObject{{"guard", _cameraCapturedGuard}, {"client_id", _cameraClientId}}}},
        false);
}

bool PixEagleClient::stopCamera(const QString& context, bool acknowledgeError)
{
    if (!cameraCanStop() || context != _cameraCapturedContext) {
        return false;
    }
    if (acknowledgeError) {
        _cameraError.clear();
    }
    QJsonObject body{{"operation", "stop"},
                     {"camera_context", QJsonObject{{"guard", _cameraCapturedGuard}, {"client_id", _cameraClientId}}}};
    if (!_cameraGestureId.isEmpty()) {
        _cameraStopGestureId = _cameraGestureId;
    }
    if (!_cameraStopGestureId.isEmpty()) {
        body.insert("gesture_id", _cameraStopGestureId);
        body.insert("sequence", ++_cameraSequence);
    }
    if (_cameraActionReply) {
        auto* reply = _cameraActionReply.data();
        _cameraActionReply = nullptr;
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
    _cameraRenewTimer.stop();
    _cameraBeginAccepted = false;
    _cameraGestureId.clear();
    _cameraManualState.clear();
    ++_cameraCommandGeneration;
    return _postCameraAction(body, true);
}

bool PixEagleClient::_postCameraAction(QJsonObject body, bool stop)
{
    if (_csrfHeader.isEmpty() || _csrfToken.isEmpty() || (stop ? !!_cameraStopReply : cameraActionPending())) {
        return false;
    }
    auto request = cameraRequest(_endpoint, QStringLiteral("/api/v1/actions/gimbal-control"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(_csrfHeader, _csrfToken);
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    body.insert("source", "qgroundcontrol");
    body.insert("confirm", true);
    body.insert("dry_run", false);
    body.insert("idempotency_key", id);
    if (!stop) {
        _cameraError.clear();
    }
    auto* reply = _network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    (stop ? _cameraStopReply : _cameraActionReply) = reply;
    boundCameraReply(reply);
    const QString operation = body.value("operation").toString();
    const QString gesture = body.value("gesture_id").toString();
    const auto sequence = body.value("sequence");
    const auto commandGeneration = _cameraCommandGeneration;
    QElapsedTimer elapsed;
    elapsed.start();
    qCDebug(PixEagleCameraLog) << "Dispatch" << operation << gesture << sequence.toInteger();
    connect(
        reply, &QNetworkReply::finished, this,
        [this, reply, generation = _generation, id, stop, operation, gesture, sequence, commandGeneration, elapsed]() {
            reply->deleteLater();
            auto& pending = stop ? _cameraStopReply : _cameraActionReply;
            if (generation != _generation || pending != reply) {
                return;
            }
            pending = nullptr;
            qCDebug(PixEagleCameraLog) << "Response" << operation << gesture << sequence.toInteger()
                                       << elapsed.elapsed();
            if (commandGeneration != _cameraCommandGeneration) {
                emit cameraChanged();
                return;
            }
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto bytes = reply->isOpen() ? reply->read(CAMERA_MAX_BYTES + 1) : QByteArray{};
            const auto data = QJsonDocument::fromJson(bytes).object();
            if (status == 401) {
                _resetSession();
                _error = tr("Your session expired. Sign in again.");
                emit changed();
                return;
            }
            const bool manual = operation.startsWith("manual_");
            const auto result = data.value("result").toObject();
            const auto manualResult = result.value("manual").toObject();
            const bool envelopeValid = reply->error() == QNetworkReply::NoError && (status == 200 || status == 202) &&
                                       bytes.size() <= CAMERA_MAX_BYTES &&
                                       data.value("action_type") == "gimbal_control" &&
                                       data.value("idempotency_key") == id && data.value("executed").toBool();
            const bool cancelled = envelopeValid && result.value("reason") == "camera_control_interrupted";
            const QString errorCode = data.value("code").toString();
            const QString errorDetail = data.value("detail").toString();
            const bool staleContext = !stop && status == 409 && bytes.size() <= CAMERA_MAX_BYTES &&
                                      (errorCode == QStringLiteral("camera_context_conflict") ||
                                       errorDetail.startsWith(QStringLiteral("Camera, source or target changed.")) ||
                                       errorDetail.startsWith(QStringLiteral("Camera owner changed.")));
            if (staleContext) {
                // A Stop or source transition can retire a context between the status poll and
                // the next input. The backend rejects that input before transmission; discard
                // the gesture and refresh the authoritative guard instead of alarming the pilot.
                qCDebug(PixEagleCameraLog) << "Camera context retired; refreshing before accepting input";
                _cameraError.clear();
                _cameraRenewTimer.stop();
                _cameraGestureId.clear();
                _cameraBeginAccepted = false;
                _cameraManualState = QStringLiteral("refreshing");
                _cameraInputAge.invalidate();
                _cameraCapturedGuard = {};
                _cameraCapturedContext.clear();
                ++_cameraCommandGeneration;
                _cameraAge.invalidate();
                _cameraExpiryTimer.stop();
                emit cameraChanged();
                refreshCamera();
                return;
            }
            if (!cancelled && (!envelopeValid || data.value("status") != "success" ||
                               (manual && (manualResult.value("gesture_id") != gesture ||
                                           manualResult.value("sequence") != sequence)))) {
                _cameraError = status == 409 ? tr("Camera control changed. Release and try again.")
                               : status == 403
                                   ? tr("Camera control is not permitted for this account.")
                                   : tr("Camera command outcome is unknown. Check the camera before retrying.");
            }
            if (manual && _cameraError.isEmpty() && gesture == _cameraGestureId) {
                _cameraBeginAccepted = true;
                _cameraManualState = manualResult.value("state").toString();
                if (_cameraManualState == "failed" || _cameraManualState == "expired" ||
                    _cameraManualState == "stopped") {
                    _cameraError = tr("Camera control stopped. Release and try again.");
                }
            }
            if (stop && envelopeValid && data.value("status") == "success") {
                _cameraCapturedGuard = {};
                _cameraCapturedContext.clear();
            }
            if (!manual) {
                _cameraAge.invalidate();
                _cameraExpiryTimer.stop();
            }
            if (manual && !_cameraError.isEmpty()) {
                stopCamera(_cameraCapturedContext);
            }
            emit cameraChanged();
            if (!manual) {
                refreshCamera();
            }
        });
    emit cameraChanged();
    return true;
}

bool PixEagleClient::_stopCameraBeforeReset()
{
    if (cameraCanStop()) {
        stopCamera(_cameraCapturedContext);
    }
    if (!_cameraStopReply || !_network) {
        return false;
    }
    // Preserve only the original session transport long enough for its bounded Stop.
    auto* network = _network;
    network->setParent(nullptr);
    connect(_cameraStopReply, &QNetworkReply::finished, network, &QObject::deleteLater);
    QTimer::singleShot(CAMERA_TIMEOUT_MS + 100, network, &QObject::deleteLater);
    return true;
}

void PixEagleClient::_clearCameraState(bool preserveStop)
{
    _cameraRenewTimer.stop();
    _cameraGestureId.clear();
    _cameraStopGestureId.clear();
    _cameraManualState.clear();
    _cameraInputAge.invalidate();
    _cameraBeginAccepted = false;
    ++_cameraCommandGeneration;
    for (auto* pending : {&_cameraReply, &_cameraActionReply, &_cameraStopReply}) {
        if (*pending) {
            auto* reply = pending->data();
            disconnect(reply, nullptr, this, nullptr);
            if (!(preserveStop && pending == &_cameraStopReply)) {
                reply->abort();
                reply->deleteLater();
            }
            *pending = nullptr;
        }
    }
    _cameraStatus = {};
    _cameraCapturedGuard = {};
    _cameraCapturedContext.clear();
    _cameraError.clear();
    _cameraAge.invalidate();
    _cameraExpiryTimer.stop();
    emit cameraChanged();
}
