#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "PixEagleClient.h"

namespace {
constexpr int CONFIG_MAX_AGE_MS = 6000;
constexpr int CONFIG_READ_TIMEOUT_MS = 3000;
constexpr int CONFIG_ACTION_TIMEOUT_MS = 60000;
constexpr qint64 CONFIG_MAX_BYTES = 128 * 1024;

void boundReply(QNetworkReply* reply, int timeout)
{
    reply->setReadBufferSize(CONFIG_MAX_BYTES + 1);
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply]() {
        if (reply->bytesAvailable() > CONFIG_MAX_BYTES) {
            reply->abort();
        }
    });
    QTimer::singleShot(timeout, reply, [reply]() {
        if (!reply->isFinished()) {
            reply->abort();
        }
    });
}

QString tierLabel(const QString& tier)
{
    if (tier == "immediate") {
        return PixEagleClient::tr("Apply now");
    }
    if (tier == "tracker_restart") {
        return PixEagleClient::tr("Restart tracker");
    }
    if (tier == "follower_restart") {
        return PixEagleClient::tr("Restart follower");
    }
    return PixEagleClient::tr("Restart PixEagle");
}
}  // namespace

bool PixEagleClient::configAvailable() const
{
    // Backend configuration is an authenticated PixEagle operation. It must
    // remain available while a regular QGC client is waiting for PX4, because
    // restart and pending-setting recovery do not dispatch aircraft commands.
    return _sessionReady() && _context.value("capabilities").toArray().contains("config.operations.v1") &&
           _context.value("permissions").toObject().value("scopes").toArray().contains("config:read");
}

bool PixEagleClient::configFresh() const
{
    return configAvailable() && !_configState.isEmpty() &&
           _configState.value("instance_id") == _context.value("instance_id") &&
           _configState.value("runtime_id") == _context.value("runtime_id") && _configAge.isValid() &&
           _configAge.elapsed() < CONFIG_MAX_AGE_MS;
}

bool PixEagleClient::canSetOsd() const
{
    return configFresh() && !configBusy() && !backendRestarting() &&
           _context.value("permissions").toObject().value("scopes").toArray().contains("control:write") &&
           _configState.value("osd").toObject().value("can_set").toBool();
}

bool PixEagleClient::osdEnabled() const
{
    return configFresh() && _configState.value("osd").toObject().value("running_enabled").toBool();
}

QString PixEagleClient::osdStatusText() const
{
    if (!configFresh()) {
        return configBusy() ? tr("Applying overlay setting…") : tr("Waiting for current overlay state.");
    }
    const auto osd = _configState.value("osd").toObject();
    if (!_context.value("permissions").toObject().value("scopes").toArray().contains("control:write")) {
        return tr("Your account can view this setting. Control permission is required to change it.");
    }
    if (!osd.value("can_set").toBool()) {
        return tr("Overlay control is unavailable. Check PixEagle in the dashboard.");
    }
    if (osd.value("saved_enabled") != osd.value("running_enabled")) {
        return tr("The running overlay differs from the saved setting. Review Backend settings.");
    }
    return {};
}

bool PixEagleClient::canApplyConfig() const
{
    const auto apply = _configState.value("apply").toObject();
    const auto tier = apply.value("reload_tier").toString();
    return configFresh() && !configBusy() && !backendRestarting() && !targetMutationPending() &&
           !followingActionPending() &&
           _context.value("permissions").toObject().value("scopes").toArray().contains("config:write") &&
           apply.value("available").toBool() && (tier == "immediate" || tier == "tracker_restart");
}

QString PixEagleClient::configStatusText() const
{
    if (backendRestarting()) {
        return backendRestartStatus();
    }
    if (configBusy()) {
        return tr("Applying settings…");
    }
    if (!configFresh()) {
        return tr("Waiting for current PixEagle settings.");
    }
    if (!_configState.value("pending").toBool()) {
        return tr("Saved settings are running.");
    }
    const auto apply = _configState.value("apply").toObject();
    const auto tier = apply.value("reload_tier").toString();
    if (tier == "system_restart") {
        return tr(
            "Saved changes need a PixEagle restart. Stop tracking and camera movement, then restart on the ground.");
    }
    if (tier == "follower_restart") {
        return tr("Saved changes need a follower restart. Use the PixEagle dashboard.");
    }
    return apply.value("available").toBool() ? tr("Saved changes are waiting to be applied.")
                                             : tr("Saved changes are pending. Stop tracking, following and camera "
                                                  "movement before applying on the ground.");
}

QVariantList PixEagleClient::pendingConfigChanges() const
{
    QVariantList rows;
    if (!configFresh()) {
        return rows;
    }
    for (const auto& value : _configState.value("pending_changes").toArray()) {
        const auto row = value.toObject();
        rows.append(QVariantMap{{"name", row.value("path").toString()},
                                {"apply", tierLabel(row.value("reload_tier").toString())}});
    }
    return rows;
}

void PixEagleClient::_clearConfigState()
{
    for (auto* pending : {&_configReply, &_configActionReply}) {
        if (*pending) {
            disconnect(*pending, nullptr, this, nullptr);
            (*pending)->abort();
            (*pending)->deleteLater();
            *pending = nullptr;
        }
    }
    _configState = {};
    _configAge.invalidate();
    _configExpiryTimer.stop();
    _configActionError.clear();
    emit configChanged();
}

void PixEagleClient::setConfigRequested(bool requested)
{
    _configRequested = requested;
    if (requested) {
        refreshConfig();
    }
}

bool PixEagleClient::_acceptConfigState(const QJsonObject& data)
{
    static const QRegularExpression generation("^[0-9a-f]{64}$");
    const auto osd = data.value("osd").toObject();
    const auto apply = data.value("apply").toObject();
    const auto restart = data.value("system_restart").toObject();
    const QStringList tiers{"immediate", "tracker_restart", "follower_restart", "system_restart"};
    const auto nullableBool = [](const QJsonValue& value) { return value.isBool() || value.isNull(); };
    const auto reason = [](const QJsonValue& value) {
        return value.isNull() || (value.isString() && value.toString().size() <= 2048);
    };
    if (data.value("schema_version").toInt() != 1 || data.value("source").toString() != "native_config_state" ||
        data.value("instance_id") != _context.value("instance_id") ||
        data.value("runtime_id") != _context.value("runtime_id") ||
        !generation.match(data.value("config_generation").toString()).hasMatch() || !data.value("pending").isBool() ||
        !data.value("pending_changes").isArray() || data.value("pending_changes").toArray().size() > 256 ||
        !osd.value("available").isBool() || !osd.value("can_set").isBool() ||
        !nullableBool(osd.value("saved_enabled")) || !nullableBool(osd.value("running_enabled")) ||
        !reason(osd.value("unavailable_reason")) || osd.value("scope").toString() != "backend_overlay" ||
        !apply.value("available").isBool() || !reason(apply.value("reason")) ||
        !(apply.value("reload_tier").isNull() || tiers.contains(apply.value("reload_tier").toString())) ||
        !restart.value("available").isBool() || !reason(restart.value("reason"))) {
        return false;
    }
    if (osd.value("can_set").toBool() && (!osd.value("available").toBool() || !osd.value("saved_enabled").isBool() ||
                                          !osd.value("running_enabled").isBool())) {
        return false;
    }
    for (const auto& value : data.value("pending_changes").toArray()) {
        const auto row = value.toObject();
        if (!row.value("path").isString() || row.value("path").toString().isEmpty() ||
            row.value("path").toString().size() > 256 || !tiers.contains(row.value("reload_tier").toString())) {
            return false;
        }
    }
    if ((!data.value("pending").toBool() && !data.value("pending_changes").toArray().isEmpty()) ||
        (apply.value("available").toBool() &&
         (!data.value("pending").toBool() || apply.value("reload_tier").isNull()))) {
        return false;
    }
    _configState = data;
    _configAge.start();
    _configExpiryTimer.start(CONFIG_MAX_AGE_MS);
    return true;
}

void PixEagleClient::refreshConfig()
{
    if (!configAvailable() || _configReply || configBusy()) {
        return;
    }
    auto url = _endpoint;
    url.setPath(url.path() + "/api/v1/integration/config");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(CONFIG_READ_TIMEOUT_MS);
    auto* reply = _network->get(request);
    _configReply = reply;
    boundReply(reply, CONFIG_READ_TIMEOUT_MS);
    QElapsedTimer age;
    age.start();
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, generation = _generation, key = _targetContextKey(), age]() {
                reply->deleteLater();
                if (_configReply != reply) {
                    return;
                }
                _configReply = nullptr;
                if (generation != _generation || key != _targetContextKey() || !configAvailable()) {
                    return;
                }
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (status == 401) {
                    _sessionExpired();
                    emit changed();
                    return;
                }
                const auto bytes = reply->isOpen() ? reply->read(CONFIG_MAX_BYTES + 1) : QByteArray{};
                if (status == 200 && reply->error() == QNetworkReply::NoError && bytes.size() <= CONFIG_MAX_BYTES &&
                    age.elapsed() < CONFIG_READ_TIMEOUT_MS &&
                    _acceptConfigState(QJsonDocument::fromJson(bytes).object())) {
                    _configAge = age;
                    _configExpiryTimer.start(CONFIG_MAX_AGE_MS - static_cast<int>(age.elapsed()));
                } else {
                    _configState = {};
                    _configAge.invalidate();
                    _configExpiryTimer.stop();
                }
                emit configChanged();
            });
}

QString PixEagleClient::captureConfigContext() const
{
    if (!configFresh() || configBusy()) {
        return {};
    }
    return QString::number(_generation) + "|" + _targetContextKey() + "|" +
           _configState.value("config_generation").toString();
}

bool PixEagleClient::setOsdEnabled(bool enabled, const QString& context)
{
    return canSetOsd() && _postConfigAction("osd-set", {{"enabled", enabled}}, context);
}

bool PixEagleClient::applyConfig(const QString& context)
{
    return canApplyConfig() &&
           _postConfigAction("config-apply",
                             {{"reload_tier", _configState.value("apply").toObject().value("reload_tier")}}, context);
}

bool PixEagleClient::_postConfigAction(const QString& action, QJsonObject body, const QString& context)
{
    if (context.isEmpty() || context != captureConfigContext() || _csrfHeader.isEmpty() || _csrfToken.isEmpty()) {
        return false;
    }
    body.insert("instance_id", _context.value("instance_id"));
    body.insert("runtime_id", _context.value("runtime_id"));
    body.insert("config_generation", _configState.value("config_generation"));
    body.insert("confirm", true);
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    body.insert("idempotency_key", id);
    auto url = _endpoint;
    url.setPath(url.path() + "/api/v1/actions/" + action);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(_csrfHeader, _csrfToken);
    request.setTransferTimeout(CONFIG_ACTION_TIMEOUT_MS);
    auto* reply = _network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    _configActionReply = reply;
    if (_configReply) {
        disconnect(_configReply, nullptr, this, nullptr);
        _configReply->abort();
        _configReply->deleteLater();
        _configReply = nullptr;
    }
    _configActionError.clear();
    _configAge.invalidate();
    _configExpiryTimer.stop();
    boundReply(reply, CONFIG_ACTION_TIMEOUT_MS);
    emit configChanged();
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, generation = _generation, key = _targetContextKey(), action, id]() {
                reply->deleteLater();
                if (_configActionReply != reply) {
                    return;
                }
                _configActionReply = nullptr;
                if (generation != _generation || key != _targetContextKey() || !configAvailable()) {
                    return;
                }
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const auto bytes = reply->isOpen() ? reply->read(CONFIG_MAX_BYTES + 1) : QByteArray{};
                const auto data = QJsonDocument::fromJson(bytes).object();
                // Reconcile through a new read; an acknowledgement alone is not running-state evidence.
                if (status == 401) {
                    _sessionExpired();
                    emit changed();
                    return;
                }
                if (status == 409) {
                    _configActionError =
                        tr("Settings or runtime changed. Review current settings before trying again.");
                } else if (status == 403) {
                    _configActionError = tr("You do not have permission to change these settings.");
                } else if ((status != 200 && status != 202) || reply->error() != QNetworkReply::NoError ||
                           bytes.size() > CONFIG_MAX_BYTES || data.value("status").toString() != "success" ||
                           data.value("idempotency_key").toString() != id ||
                           data.value("action_type").toString() != (action == "osd-set" ? "osd_set" : "config_apply") ||
                           data.value("executed") != QJsonValue(true)) {
                    _configActionError =
                        tr("The settings change was not confirmed. Refresh and check its state before trying again.");
                }
                emit configChanged();
                refreshConfig();
                refreshTargetState();
                if (_modelsRequested) {
                    refreshModels();
                }
            });
    return true;
}

bool PixEagleClient::canRestartBackend() const
{
    return configFresh() && !configBusy() && !backendRestarting() && !targetMutationPending() &&
           !followingActionPending() && _context.value("capabilities").toArray().contains("config.system_restart.v1") &&
           _context.value("permissions").toObject().value("scopes").toArray().contains("system:admin") &&
           _configState.value("system_restart").toObject().value("available").toBool();
}

QString PixEagleClient::backendRestartStatus() const
{
    if (backendRestarting()) {
        return tr("Restarting PixEagle… Waiting for the backend to return.");
    }
    if (!_restartResult.isEmpty()) {
        return _restartResult;
    }
    if (!configFresh()) {
        return tr("Waiting for current restart availability.");
    }
    const auto reason = _configState.value("system_restart").toObject().value("reason").toString();
    if (reason == "supervisor_not_verified") {
        return tr("This launcher cannot restart PixEagle remotely. Use its service or launcher controls.");
    }
    if (reason == "restart_policy_denied" || reason == "system_admin_principal_required") {
        return tr("Restart requires administrator permission and an allowed connection policy.");
    }
    return canRestartBackend() ? QString{}
                               : tr("Stop tracking, detection and camera movement; land and disarm before restarting.");
}

bool PixEagleClient::restartBackend(const QString& context)
{
    if (!canRestartBackend() || context.isEmpty() || context != captureConfigContext() || _csrfHeader.isEmpty() ||
        _csrfToken.isEmpty()) {
        return false;
    }
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject guard{{"instance_id", _context.value("instance_id")},
                            {"runtime_id", _context.value("runtime_id")},
                            {"config_generation", _configState.value("config_generation")}};
    const QJsonObject body{{"confirm", true}, {"idempotency_key", id}, {"restart_context", guard}};
    auto url = _endpoint;
    url.setPath(url.path() + "/api/v1/actions/system-restart");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(_csrfHeader, _csrfToken);
    request.setTransferTimeout(CONFIG_ACTION_TIMEOUT_MS);
    auto* reply = _network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    _configActionReply = reply;
    _restartEndpoint = _endpoint;
    _restartInstance = guard.value("instance_id").toString();
    _restartRuntime = guard.value("runtime_id").toString();
    _restartResult.clear();
    _restartSignInAttempted = false;
    _restartSignInAttempts = 0;
    _restartNextSignInMs = 0;
    _restartAge.start();
    _restartRecoveryTimer.start();
    boundReply(reply, CONFIG_ACTION_TIMEOUT_MS);
    emit configChanged();
    connect(reply, &QNetworkReply::finished, this, [this, reply, id, generation = _generation]() {
        reply->deleteLater();
        if (_configActionReply != reply || generation != _generation) {
            return;
        }
        _configActionReply = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes = reply->isOpen() ? reply->read(CONFIG_MAX_BYTES + 1) : QByteArray{};
        const auto data = QJsonDocument::fromJson(bytes).object();
        const bool accepted =
            status == 202 && reply->error() == QNetworkReply::NoError && bytes.size() <= CONFIG_MAX_BYTES &&
            data.value("status").toString() == "success" && data.value("action_type").toString() == "system_restart" &&
            data.value("idempotency_key").toString() == id && data.value("executed") == QJsonValue(true);
        if (!accepted && status >= 400 && status < 500) {
            _restartAge.invalidate();
            _restartRecoveryTimer.stop();
            _restartResult = tr("Restart was refused. Refresh settings and review the backend state.");
            refreshConfig();
        }
        // A lost acknowledgement may follow an accepted exit; reconcile without replaying it.
        emit configChanged();
    });
    return true;
}

void PixEagleClient::_recoverBackendRestart()
{
    if (!backendRestarting()) {
        _restartRecoveryTimer.stop();
        return;
    }
    if (_endpoint != _restartEndpoint || !_enabled || _restartAge.elapsed() >= 60000) {
        _restartAge.invalidate();
        _restartRecoveryTimer.stop();
        _restartResult =
            tr("PixEagle restart was not confirmed. Check the backend and reconnect; no restart was repeated.");
        emit configChanged();
        return;
    }
    if (!_context.isEmpty() && _context.value("instance_id").toString() != _restartInstance) {
        _restartAge.invalidate();
        _restartRecoveryTimer.stop();
        _restartResult = tr("A different PixEagle instance answered. Review the backend address before reconnecting.");
        _resetSession();
        emit configChanged();
        return;
    }
    if (_authenticated && _sessionReady() && _context.value("runtime_id").toString() != _restartRuntime) {
        if (configFresh()) {
            _restartAge.invalidate();
            _restartRecoveryTimer.stop();
            _restartResult = tr("PixEagle restarted.");
            emit configChanged();
            return;
        }
        refreshConfig();
    }
    if (!busy()) {
        if (_authenticated) {
            refresh();
        } else if (!_restartSignInAttempted && _restartSignInAttempts < 6 &&
                   _restartAge.elapsed() >= _restartNextSignInMs) {
            _restartSignInAttempted = true;
            ++_restartSignInAttempts;
            _loadRememberedSignIn();
        }
    }
}
