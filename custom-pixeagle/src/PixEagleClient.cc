#include "PixEagleClient.h"

#include <utility>

#include <QtCore/QCryptographicHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtCore/QUuid>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkCookie>
#include <QtNetwork/QNetworkCookieJar>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <qtkeychain/keychain.h>

namespace {
constexpr qint64 MAX_RESPONSE_BYTES = 128 * 1024;
constexpr int TARGET_READ_TIMEOUT_MS = 3000;
constexpr int TARGET_ACTION_TIMEOUT_MS = 5000;
constexpr int CAMERA_SELECT_ACTION_TIMEOUT_MS = 8000;
constexpr int MODEL_ACTION_TIMEOUT_MS = 60000;
constexpr int TARGET_STATE_MAX_AGE_MS = 6000;

bool validUid(const QString& uid)
{
    bool ok = false;
    const quint64 value = uid.toULongLong(&ok);
    return ok && value != 0 && QString::number(value) == uid;
}
}  // namespace

PixEagleClient::PixEagleClient(QObject* parent, bool companionOnly)
    : QObject(parent)
    , _companionOnly(companionOnly)
{
    _pollTimer.setSingleShot(true);
    _pollTimer.setInterval(2000);
    _expiryTimer.setSingleShot(true);
    _expiryTimer.setInterval(6000);
    _statusExpiryTimer.setSingleShot(true);
    _statusExpiryTimer.setInterval(6000);
    _targetExpiryTimer.setSingleShot(true);
    _modelExpiryTimer.setSingleShot(true);
    _followingExpiryTimer.setSingleShot(true);
    _configExpiryTimer.setSingleShot(true);
    _restartRecoveryTimer.setInterval(1000);
    connect(&_restartRecoveryTimer, &QTimer::timeout, this, &PixEagleClient::_recoverBackendRestart);
    _safetyExpiryTimer.setSingleShot(true);
    connect(&_safetyExpiryTimer, &QTimer::timeout, this, &PixEagleClient::safetyChanged);
    connect(&_configExpiryTimer, &QTimer::timeout, this, &PixEagleClient::configChanged);
    connect(this, &PixEagleClient::changed, this, &PixEagleClient::configChanged);
    connect(&_modelExpiryTimer, &QTimer::timeout, this, &PixEagleClient::modelsChanged);
    connect(&_targetExpiryTimer, &QTimer::timeout, this, &PixEagleClient::targetChanged);
    connect(&_followingExpiryTimer, &QTimer::timeout, this, &PixEagleClient::followingChanged);
    connect(&_statusExpiryTimer, &QTimer::timeout, this, [this]() {
        _runtimeStatus = {};
        emit changed();
    });
    connect(&_pollTimer, &QTimer::timeout, this, &PixEagleClient::refresh);
    connect(&_expiryTimer, &QTimer::timeout, this, [this]() {
        _clearContext();
        _error = _companionOnly ? tr("Connection status is out of date. Retry the connection in PixEagle settings.")
                                : tr("Connection status is out of date. Verify the vehicle again.");
        _verificationReason = _error;
        emit changed();
    });
    _initializeCameraState();
    _resetSession();
}

PixEagleClient::~PixEagleClient()
{
    for (const auto& reply : {_targetStateReply, _targetCatalogReply, _targetActionReply, _modelsReply, _followingReply,
                              _followingActionReply, _configReply, _configActionReply, _cameraReply, _cameraActionReply,
                              _cameraStopReply}) {
        if (reply) {
            disconnect(reply, nullptr, this, nullptr);
            reply->abort();
        }
    }
    for (const auto& reply : {_safetyReply, _safetyActionReply}) {
        if (reply) {
            disconnect(reply, nullptr, this, nullptr);
            reply->abort();
        }
    }
    if (_statusReply) {
        disconnect(_statusReply, nullptr, this, nullptr);
        _statusReply->abort();
    }
    if (_reply) {
        disconnect(_reply, nullptr, this, nullptr);
        _reply->abort();
    }
}

bool PixEagleClient::validateEndpoint(const QString& text, QUrl* result)
{
    QUrl url(text.trimmed(), QUrl::StrictMode);
    const QHostAddress address(url.host());
    const bool loopback = address.isLoopback() || url.host().compare("localhost", Qt::CaseInsensitive) == 0;
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment() ||
        (url.scheme() != "https" && !(url.scheme() == "http" && loopback)) || url.port() == 0 ||
        url.path().contains('\\')) {
        return false;
    }
    const QStringList segments = url.path().split('/');
    if (segments.contains("..") || segments.contains(".")) {
        return false;
    }
    QString path = url.path();
    while (path.endsWith('/')) {
        path.chop(1);
    }
    url.setPath(path);
    if (result) {
        *result = url;
    }
    return true;
}

void PixEagleClient::setEndpoint(const QString& text)
{
    QUrl url;
    if (!validateEndpoint(text.trimmed().isEmpty() ? defaultEndpoint() : text, &url)) {
        _error = tr("Enter an HTTPS address, or HTTP on localhost, without credentials, query or fragment.");
        emit changed();
        return;
    }
    if (_endpoint == url) {
        return;
    }
    _restartAge.invalidate();
    _restartRecoveryTimer.stop();
    _restartResult.clear();
    _resetSession();
    _endpoint = url;
    _credentialStatus.clear();
    emit endpointChanged();
    emit changed();
    _loadRememberedSignIn();
}

void PixEagleClient::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }
    _enabled = enabled;
    if (!enabled) {
        _restartAge.invalidate();
        _restartRecoveryTimer.stop();
        _restartResult.clear();
        _resetSession();
    } else {
        _loadRememberedSignIn();
    }
    emit changed();
}

bool PixEagleClient::rememberSignIn() const
{
    return QSettings().value(QStringLiteral("PixEagle/RememberSignIn/") + _credentialKey(), true).toBool();
}

QString PixEagleClient::_credentialKey() const
{
    const QByteArray digest =
        QCryptographicHash::hash(_endpoint.toString().toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(digest);
}

void PixEagleClient::_loadRememberedSignIn()
{
    if (!_enabled || _authenticated || busy() || !rememberSignIn() || _endpoint.isEmpty() ||
        QSettings().value(QStringLiteral("PixEagle/AutoSignInSuppressed/") + _credentialKey(), false).toBool()) {
        return;
    }
    const quint64 generation = _generation;
    const QString key = _credentialKey();
    auto* job = new QKeychain::ReadPasswordJob(QStringLiteral("org.pixeagle.qgc"), this);
    job->setKey(key);
    connect(job, &QKeychain::Job::finished, this, [this, generation, key](QKeychain::Job* completed) {
        if (generation != _generation || key != _credentialKey() || !_enabled || _authenticated || busy() ||
            !rememberSignIn()) {
            return;
        }
        if (completed->error() == QKeychain::EntryNotFound) {
            return;
        }
        if (completed->error() != QKeychain::NoError) {
            _credentialStatus = tr("System password store unavailable. Sign in manually this time.");
            emit changed();
            return;
        }
        const QJsonDocument saved =
            QJsonDocument::fromJson(static_cast<QKeychain::ReadPasswordJob*>(completed)->binaryData());
        const QJsonObject account = saved.object();
        const QString username = account.value(QStringLiteral("username")).toString();
        const QString password = account.value(QStringLiteral("password")).toString();
        if (!username.isEmpty() && !password.isEmpty()) {
            signIn(username, password);
        }
    });
    job->start();
}

void PixEagleClient::_saveRememberedSignIn()
{
    const QString username = std::exchange(_pendingCredentialUsername, QString{});
    const QString password = std::exchange(_pendingCredentialPassword, QString{});
    if (!rememberSignIn() || username.isEmpty() || password.isEmpty() || !_authenticated) {
        return;
    }
    const QByteArray data =
        QJsonDocument(QJsonObject{{QStringLiteral("username"), username}, {QStringLiteral("password"), password}})
            .toJson(QJsonDocument::Compact);
    const QString key = _credentialKey();
    const quint64 generation = _generation;
    const quint64 credentialDecisionGeneration = _credentialDecisionGeneration;
    auto* job = new QKeychain::WritePasswordJob(QStringLiteral("org.pixeagle.qgc"), this);
    job->setKey(key);
    job->setBinaryData(data);
    connect(job, &QKeychain::Job::finished, this,
            [this, generation, key, credentialDecisionGeneration](QKeychain::Job* completed) {
                if (generation != _generation || key != _credentialKey() ||
                    credentialDecisionGeneration != _credentialDecisionGeneration) {
                    if (!QSettings().value(QStringLiteral("PixEagle/RememberSignIn/") + key, true).toBool()) {
                        _deleteRememberedSignIn(key);
                    }
                    return;
                }
                QSettings().setValue(QStringLiteral("PixEagle/AutoSignInSuppressed/") + key,
                                     completed->error() != QKeychain::NoError);
                _credentialStatus = completed->error() == QKeychain::NoError
                                        ? tr("Sign-in saved in the system password store.")
                                        : tr("System password store unavailable. Sign in again next time.");
                emit changed();
            });
    job->start();
}

void PixEagleClient::_deleteRememberedSignIn(const QString& key)
{
    auto* job = new QKeychain::DeletePasswordJob(QStringLiteral("org.pixeagle.qgc"), this);
    job->setKey(key);
    job->start();
}

void PixEagleClient::setRememberSignIn(bool remember)
{
    QSettings().setValue(QStringLiteral("PixEagle/RememberSignIn/") + _credentialKey(), remember);
    if (!remember) {
        ++_credentialDecisionGeneration;
        _deleteRememberedSignIn(_credentialKey());
        QSettings().remove(QStringLiteral("PixEagle/AutoSignInSuppressed/") + _credentialKey());
        _credentialStatus.clear();
    } else if (_authenticated) {
        _credentialStatus = tr("Sign in again to save this account.");
    } else {
        _loadRememberedSignIn();
    }
    emit changed();
}

void PixEagleClient::setVehicleIdentity(int systemId, const QString& aircraftUid, bool online)
{
    if (_companionOnly) {
        return;
    }
    if (_systemId == systemId && _aircraftUid == aircraftUid && _online == online) {
        return;
    }
    const bool identityChanged = _systemId != systemId || _aircraftUid != aircraftUid;
    if (!_aircraftUid.isEmpty() && _aircraftUid != aircraftUid) {
        _identityConflict = true;
    }
    if (identityChanged) {
        _resetSession();
    }
    _systemId = systemId;
    _aircraftUid = aircraftUid;
    _online = online;
    observeAircraftUid(aircraftUid);
    _clearContext();
    // A reply captured for a previous aircraft identity must not establish the new binding.
    ++_generation;
    if (_reply) {
        _reply->abort();
        _reply = nullptr;
    }
    if (_authenticated && _enabled && online) {
        _pollTimer.start();
    }
    emit changed();
}

void PixEagleClient::observeAircraftUid(const QString& aircraftUid)
{
    if (!validUid(aircraftUid)) {
        if (!_observedUid.isEmpty()) {
            _identityConflict = true;
            _verifiedKey.clear();
            _clearTargetState();
            emit changed();
        }
        return;
    }
    if (!_observedUid.isEmpty() && _observedUid != aircraftUid) {
        _identityConflict = true;
        _verifiedKey.clear();
        _clearTargetState();
    }
    _observedUid = aircraftUid;
    emit changed();
}

void PixEagleClient::setDuplicateAssociation(bool duplicate, const QString& otherVehicle)
{
    if (_duplicateAssociation != duplicate || _conflictingVehicle != otherVehicle) {
        _duplicateAssociation = duplicate;
        _conflictingVehicle = otherVehicle;
        if (duplicate) {
            if (!_verifiedKey.isEmpty()) {
                _verificationReason = tr("A duplicate connection invalidated verification. Verify this vehicle again.");
            }
            _verifiedKey.clear();
            _clearTargetState();
        }
        emit changed();
    }
}

void PixEagleClient::setAutoVerifySingleVehicle(bool enabled)
{
    const bool next = enabled && !_companionOnly;
    if (_autoVerifySingleVehicle == next) {
        return;
    }
    _autoVerifySingleVehicle = next;
    if (!_autoVerifySingleVehicle) {
        _autoVerificationPending = false;
        _autoVerificationAttemptedKey.clear();
    } else {
        // The manager can learn that this is the only vehicle after sign-in and
        // context discovery have already completed. Re-evaluate the current
        // context instead of waiting for another identity event.
        _maybeAutoVerify();
    }
    emit changed();
}

void PixEagleClient::_clearContext()
{
    _clearConfigState();
    _clearSafetyState();
    if (cameraCanStop()) {
        stopCamera(_cameraCapturedContext);
    }
    _cameraAge.invalidate();
    _cameraExpiryTimer.stop();
    emit cameraChanged();
    if (_followingReply) {
        disconnect(_followingReply, nullptr, this, nullptr);
        _followingReply->abort();
        _followingReply->deleteLater();
        _followingReply = nullptr;
    }
    _followingAge.invalidate();
    _followingExpiryTimer.stop();
    emit followingChanged();
    _clearTargetState();
    _context = {};
    _runtimeStatus = {};
    _statusExpiryTimer.stop();
    if (_statusReply) {
        disconnect(_statusReply, nullptr, this, nullptr);
        _statusReply->abort();
        _statusReply->deleteLater();
        _statusReply = nullptr;
    }
    if (!_verifiedKey.isEmpty()) {
        _verificationReason = tr("The connection was interrupted. Verify this vehicle again.");
    }
    _verifiedKey.clear();
    _pendingVerificationKey.clear();
    _expiryTimer.stop();
    emit contextChanged();
}

void PixEagleClient::_resetSession()
{
    _pendingCredentialUsername.clear();
    _pendingCredentialPassword.clear();
    const bool cameraStopOwnsNetwork = _stopCameraBeforeReset();
    _clearCameraState(cameraStopOwnsNetwork);
    ++_generation;
    _pollTimer.stop();
    _clearFollowingState();
    if (_reply) {
        _reply->abort();
        _reply = nullptr;
    }
    // Replacing the owner also discards late Set-Cookie writes from cancelled requests.
    if (_network && !cameraStopOwnsNetwork) {
        _network->deleteLater();
    }
    _network = new QNetworkAccessManager(this);
    _authenticated = false;
    _signedInAs.clear();
    _csrfHeader.clear();
    _csrfToken.clear();
    _error.clear();
    _clearContext();
    _verificationReason.clear();
    _autoVerificationAttemptedKey.clear();
    _autoVerificationPending = false;
}

void PixEagleClient::signIn(const QString& username, const QString& password)
{
    if (!_enabled || busy() || _endpoint.isEmpty()) {
        return;
    }
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        _error = tr("Enter your PixEagle username and password.");
        emit changed();
        return;
    }
    _resetSession();
    _request(Request::Login, {{"username", username.trimmed()}, {"password", password}});
}

void PixEagleClient::signInAt(const QString& endpoint, const QString& username, const QString& password)
{
    if (!_enabled || busy()) {
        return;
    }
    const QString destination = endpoint.trimmed().isEmpty() ? defaultEndpoint() : endpoint;
    if (!validateEndpoint(destination)) {
        _error = tr("Enter an HTTPS address, or HTTP on localhost, without credentials, query or fragment.");
        emit changed();
        return;
    }
    setEndpoint(destination);
    signIn(username, password);
}

void PixEagleClient::signInAtRemembered(const QString& endpoint, const QString& username, const QString& password)
{
    const QString destination = endpoint.trimmed().isEmpty() ? defaultEndpoint() : endpoint;
    const bool admitted =
        _enabled && !busy() && validateEndpoint(destination) && !username.trimmed().isEmpty() && !password.isEmpty();
    signInAt(endpoint, username, password);
    if (admitted && busy() && rememberSignIn()) {
        _pendingCredentialUsername = username.trimmed();
        _pendingCredentialPassword = password;
    }
}

void PixEagleClient::signOut()
{
    if (!_enabled || busy()) {
        return;
    }
    _restartAge.invalidate();
    _restartRecoveryTimer.stop();
    _restartResult.clear();
    ++_credentialDecisionGeneration;
    QSettings().setValue(QStringLiteral("PixEagle/AutoSignInSuppressed/") + _credentialKey(), true);
    if (_authenticated) {
        _pollTimer.stop();
        _clearContext();
        _request(Request::Logout);
    } else {
        _resetSession();
        emit changed();
    }
}

void PixEagleClient::verifyVehicle()
{
    if (canVerify()) {
        _clearContext();
        _request(Request::Verify);
    }
}

bool PixEagleClient::canVerify() const
{
    return _enabled && _authenticated && !busy() && _online && validUid(_aircraftUid) && !_identityConflict &&
           !_duplicateAssociation;
}

void PixEagleClient::refresh()
{
    if (_enabled && _authenticated && !busy() && (_online || _companionOnly)) {
        _request(Request::Context);
    }
}

void PixEagleClient::_request(Request kind, const QJsonObject& body)
{
    _error.clear();
    QString path;
    switch (kind) {
        case Request::Login:
            path = "/api/v1/auth/login";
            break;
        case Request::Logout:
            path = "/api/v1/auth/logout";
            break;
        case Request::Context:
        case Request::Confirm:
            path = "/api/v1/integration/context";
            break;
        case Request::Verify:
            path = "/api/v1/integration/connection";
            break;
    }
    QUrl url = _endpoint;
    url.setPath(url.path() + path);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(20000);
    request.setRawHeader("Accept", "application/json");
    const bool readContext = kind == Request::Context || kind == Request::Confirm;
    if (!readContext) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        if (kind != Request::Login) {
            request.setRawHeader(_csrfHeader, _csrfToken);
        }
    }
    _requestTimer.start();
    QNetworkReply* reply = readContext ? _network->get(request)
                                       : _network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    _reply = reply;
    reply->setReadBufferSize(MAX_RESPONSE_BYTES + 1);
    connect(reply, &QNetworkReply::readyRead, this, [reply]() {
        if (reply->bytesAvailable() > MAX_RESPONSE_BYTES) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, kind, generation = _generation]() { _finished(reply, kind, generation); });
    emit changed();
}

void PixEagleClient::_finished(QNetworkReply* reply, Request kind, quint64 generation)
{
    reply->deleteLater();
    if (generation != _generation || !_enabled) {
        return;
    }
    _reply = nullptr;
    const bool readContext = kind == Request::Context || kind == Request::Confirm;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (kind == Request::Logout) {
        const bool success = reply->error() == QNetworkReply::NoError && status == 200;
        _resetSession();
        if (!success) {
            _error = tr("Signed out here. PixEagle could not confirm session revocation.");
        }
        emit changed();
        return;
    }
    if (reply->error() != QNetworkReply::NoError || status != 200) {
        if (kind == Request::Login) {
            _pendingCredentialUsername.clear();
            _pendingCredentialPassword.clear();
            if (backendRestarting() && (status == 0 || status >= 500) &&
                reply->error() != QNetworkReply::SslHandshakeFailedError) {
                _restartSignInAttempted = false;
                _restartNextSignInMs = _restartAge.elapsed() + 3000;
            }
        }
        _clearContext();
        _pollTimer.stop();
        if (status == 401) {
            _resetSession();
            _error = kind == Request::Login
                         ? tr("Sign-in was not accepted. Check the username and re-enter the password.")
                         : tr("Your session expired. Sign in again.");
        } else if (status == 403) {
            _error = tr("Access was denied. Check this account's permissions and PixEagle access policy.");
        } else if (status >= 300 && status < 400) {
            _error = tr("This address redirects. Enter PixEagle's final address and sign in again.");
        } else if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
            _error = tr("The TLS certificate could not be verified. Check the address and certificate.");
        } else if (status == 429) {
            _error = tr("Too many attempts. Wait before signing in again.");
        } else if (status == 404) {
            _error = tr("This PixEagle version does not provide the native connection API.");
        } else {
            _error = _authenticated && _companionOnly ? tr("PixEagle is unreachable. Check it is running, then retry "
                                                           "the connection in PixEagle settings.")
                     : _authenticated
                         ? tr("PixEagle is unreachable. Check it is running, then verify again. Sign out to change its "
                              "address.")
                         : tr("Could not connect to PixEagle. Check its address and whether it is running.");
        }
        emit changed();
        return;
    }
    if (readContext && _requestTimer.elapsed() > 3000) {
        _clearContext();
        _error = _companionOnly
                     ? tr("Connection status arrived too late. Retry the connection in PixEagle settings.")
                     : tr("Connection status arrived too late. Check the connection, then verify the vehicle again.");
        emit changed();
        return;
    }
    QJsonParseError parseError;
    const QByteArray bytes = reply->read(MAX_RESPONSE_BYTES + 1);
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    const bool valid = bytes.size() <= MAX_RESPONSE_BYTES && parseError.error == QJsonParseError::NoError &&
                       document.isObject() &&
                       (kind == Request::Login ? _acceptSession(document.object()) : _acceptContext(document.object()));
    if (!valid) {
        _resetSession();
        _error = tr("PixEagle returned an unsupported connection response. Check its version and sign in again.");
    } else if (kind == Request::Login) {
        _saveRememberedSignIn();
        refresh();
    } else if (kind == Request::Verify) {
        // Discovery may take several seconds. Confirm its identity with a separate, age-bounded read.
        _pendingVerificationKey = _contextKey();
        _request(Request::Confirm);
    } else {
        if (kind == Request::Confirm) {
            if (_pendingVerificationKey == _contextKey() && _identitiesMatch()) {
                _verifiedKey = _contextKey();
                _verificationReason.clear();
            }
            _pendingVerificationKey.clear();
        }
        _expiryTimer.start(6000 - static_cast<int>(_requestTimer.elapsed()));
        _pollTimer.start();
    }
    emit changed();
    if (_readOnlyReady()) {
        _refreshRuntimeStatus();
        refreshTargetState();
        refreshFollowing();
        refreshSafety();
        refreshCamera();
        if (_modelsRequested) {
            refreshModels();
        }
        if (_configRequested) {
            refreshConfig();
        }
    }
    if (kind == Request::Context) {
        _maybeAutoVerify();
    }
}

void PixEagleClient::_maybeAutoVerify()
{
    if (!_autoVerifySingleVehicle || _companionOnly || _autoVerificationPending || associationVerified() ||
        !_identitiesReadyForVerification() || !canVerify()) {
        return;
    }

    const QString key = _contextKey();
    if (key.isEmpty() || _autoVerificationAttemptedKey == key) {
        return;
    }

    const quint64 contextGeneration = _generation;
    _autoVerificationPending = true;
    emit changed();
    QTimer::singleShot(0, this, [this, contextGeneration, key]() {
        _autoVerificationPending = false;
        if (_generation == contextGeneration && _contextKey() == key && _autoVerifySingleVehicle && canVerify() &&
            !associationVerified()) {
            // Record an attempt only when the request is actually admitted.
            // A context or identity race before dispatch must remain retryable.
            _autoVerificationAttemptedKey = key;
            verifyVehicle();
        }
        emit changed();
    });
}

bool PixEagleClient::_acceptSession(const QJsonObject& data)
{
    const QJsonObject principal = data.value("principal").toObject();
    const QString header = data.value("csrf_header_name").toString();
    const QString token = data.value("csrf_token").toString();
    static const QRegularExpression headerPattern("^[A-Za-z0-9!#$%&'*+.^_`|~-]{1,128}$");
    const QStringList reservedHeaders{"host",
                                      "cookie",
                                      "authorization",
                                      "origin",
                                      "referer",
                                      "content-type",
                                      "content-length",
                                      "connection",
                                      "transfer-encoding",
                                      "proxy-authorization"};
    if (!data.value("authenticated").toBool() || data.value("auth_mode").toString() != "browser_session" ||
        principal.value("kind").toString() != "session" || !data.value("csrf_required").toBool() ||
        !headerPattern.match(header).hasMatch() || reservedHeaders.contains(header.toLower()) || token.isEmpty() ||
        token.size() > 4096 || token.contains('\r') || token.contains('\n')) {
        return false;
    }
    _csrfHeader = header.toLatin1();
    _csrfToken = token.toUtf8();
    _signedInAs = principal.value("subject").toString().left(120);
    _authenticated = true;
    return true;
}

bool PixEagleClient::_acceptContext(const QJsonObject& data)
{
    const auto command = data.value("command").toObject();
    const auto telemetry = data.value("telemetry").toObject();
    const auto permissions = data.value("permissions").toObject();
    if (data.value("contract_version").toString() != "1" || data.value("instance_id").toString().isEmpty() ||
        data.value("runtime_id").toString().isEmpty() || command.value("connection_generation").toString().isEmpty() ||
        telemetry.value("connection_generation").toString().isEmpty() || !data.value("association").isObject() ||
        !data.value("readiness").isObject() || permissions.value("principal_kind").toString() != "session") {
        return false;
    }
    const QString oldTargetKey = _targetContextKey();
    _context = data;
    if (!_followingStatus.isEmpty() && (_followingStatus.value("instance_id") != data.value("instance_id") ||
                                        _followingStatus.value("runtime_id") != data.value("runtime_id"))) {
        _clearFollowingState();
    }
    if (oldTargetKey != _targetContextKey()) {
        _clearTargetState();
    }
    if (_verifiedKey != _contextKey() || !_identitiesMatch()) {
        if (!_verifiedKey.isEmpty()) {
            _verificationReason = tr("PixEagle's aircraft connection changed. Verify this vehicle again.");
        }
        _verifiedKey.clear();
    }
    emit contextChanged();
    return true;
}

QString PixEagleClient::_contextKey() const
{
    const auto command = _context.value("command").toObject();
    const auto telemetry = _context.value("telemetry").toObject();
    const QJsonArray key{_context.value("instance_id"),
                         _context.value("runtime_id"),
                         command.value("connection_generation"),
                         telemetry.value("connection_generation"),
                         command.value("autopilot_uid"),
                         telemetry.value("autopilot_uid"),
                         telemetry.value("system_id")};
    return QString::fromUtf8(QJsonDocument(key).toJson(QJsonDocument::Compact));
}

bool PixEagleClient::_identitiesMatch() const
{
    const auto command = _context.value("command").toObject();
    const auto telemetry = _context.value("telemetry").toObject();
    const QString commandUid = command.value("autopilot_uid").toString();
    return _enabled && _authenticated && _online && !_identityConflict && !_duplicateAssociation &&
           validUid(_aircraftUid) && validUid(commandUid) && commandUid == _aircraftUid &&
           telemetry.value("autopilot_uid").toString() == _aircraftUid && command.value("connected").toBool() &&
           telemetry.value("connected").toBool() && telemetry.value("fresh").toBool() &&
           telemetry.value("system_id").toInt(-1) == _systemId &&
           (command.value("system_id").isNull() || command.value("system_id").toInt(-1) == _systemId) &&
           _context.value("association").toObject().value("verified").toBool() &&
           _context.value("readiness").toObject().value("connection_ready").toBool();
}

bool PixEagleClient::_identitiesReadyForVerification() const
{
    const auto command = _context.value("command").toObject();
    const auto telemetry = _context.value("telemetry").toObject();
    const QString commandUid = command.value("autopilot_uid").toString();
    return _enabled && _authenticated && _online && !_identityConflict && !_duplicateAssociation &&
           validUid(_aircraftUid) && validUid(commandUid) && commandUid == _aircraftUid &&
           telemetry.value("autopilot_uid").toString() == _aircraftUid && command.value("connected").toBool() &&
           telemetry.value("connected").toBool() && telemetry.value("fresh").toBool() &&
           telemetry.value("system_id").toInt(-1) == _systemId &&
           (command.value("system_id").isNull() || command.value("system_id").toInt(-1) == _systemId);
}

bool PixEagleClient::associationVerified() const
{
    return !_verifiedKey.isEmpty() && _verifiedKey == _contextKey() && _identitiesMatch();
}

QString PixEagleClient::vehicleLabel() const
{
    return _companionOnly ? tr("Companion only")
                          : (_systemId ? tr("Vehicle %1").arg(_systemId) : tr("No vehicle selected"));
}

QString PixEagleClient::statusText() const
{
    if (!_enabled) {
        return tr("PixEagle is off.");
    }
    if (_identityConflict) {
        return tr("QGC received conflicting aircraft identities for %1. Disconnect the duplicate vehicle links, then "
                  "reconnect.")
            .arg(vehicleLabel());
    }
    if (_duplicateAssociation) {
        return tr("This aircraft or companion is also assigned to %1. Sign out and check the PixEagle address.")
            .arg(_conflictingVehicle.isEmpty() ? tr("another vehicle") : _conflictingVehicle);
    }
    if (!_error.isEmpty()) {
        return _error;
    }
    if (busy() && !_authenticated) {
        return tr("Signing in…");
    }
    if (!_authenticated) {
        return _endpoint.isEmpty() ? tr("Enter the PixEagle address.") : tr("Sign in to PixEagle.");
    }
    if (_companionOnly) {
        return _context.isEmpty() ? tr("Checking PixEagle…") : tr("PixEagle connected. No aircraft in QGC.");
    }
    if (!_online) {
        return tr("Waiting for this vehicle's connection to QGC.");
    }
    if (!validUid(_aircraftUid)) {
        return tr("QGC has not received this aircraft's unique identity. Association cannot be verified yet.");
    }
    if (associationVerified()) {
        return tr("Aircraft association verified. Connection is monitored.");
    }
    if (busy()) {
        return tr("Checking aircraft association…");
    }
    if (_autoVerificationPending) {
        return tr("Checking aircraft association…");
    }
    if (_autoVerifySingleVehicle && _autoVerificationAttemptedKey == _contextKey()) {
        return tr("Automatic vehicle association check did not complete. Verify again.");
    }
    const auto command = _context.value("command").toObject();
    const auto telemetry = _context.value("telemetry").toObject();
    if (validUid(command.value("autopilot_uid").toString()) &&
        command.value("autopilot_uid").toString() != _aircraftUid) {
        return tr(
            "This PixEagle is connected to a different aircraft. Check the address or select the intended vehicle.");
    }
    if (!command.value("connected").toBool()) {
        return tr("Signed in. Verify the vehicle to check PixEagle's aircraft connection.");
    }
    if (!telemetry.value("fresh").toBool() || !_context.value("association").toObject().value("verified").toBool()) {
        return tr(
            "PixEagle's aircraft and telemetry identities are not verified. Check its connections, then verify again.");
    }
    if (!_verificationReason.isEmpty()) {
        return _verificationReason;
    }
    return tr("Signed in. Verify that PixEagle is connected to this vehicle.");
}

QString PixEagleClient::diagnostics() const
{
    const auto command = _context.value("command").toObject();
    const auto telemetry = _context.value("telemetry").toObject();
    return tr("QGC aircraft ID: %1\nPixEagle aircraft ID: %2\nTelemetry aircraft ID: %3\nInstance: %4\nRuntime: %5")
        .arg(_aircraftUid, command.value("autopilot_uid").toString(), telemetry.value("autopilot_uid").toString(),
             instanceId(), _context.value("runtime_id").toString());
}

bool PixEagleClient::mediaAvailable() const
{
    const auto video = _context.value("video").toObject();
    return _readOnlyReady() && _context.value("capabilities").toArray().contains("video.frame_provenance.v1") &&
           _context.value("permissions").toObject().value("scopes").toArray().contains("media:read") &&
           video.value("provenance_version").toString() == "1" && video.value("ws_path").toString() == "/ws/video_feed";
}

QUrl PixEagleClient::mediaUrl() const
{
    if (!mediaAvailable()) {
        return {};
    }
    QUrl url = _endpoint;
    url.setPath(url.path() + "/ws/video_feed");
    url.setScheme(url.scheme() == "https" ? "wss" : "ws");
    return url;
}

QByteArray PixEagleClient::mediaCookie() const
{
    if (!mediaAvailable()) {
        return {};
    }
    QUrl url = mediaUrl();
    url.setScheme(url.scheme() == "wss" ? "https" : "http");
    QByteArray result;
    const auto cookies = _network->cookieJar()->cookiesForUrl(url);
    for (const auto& cookie : cookies) {
        if (!result.isEmpty()) {
            result += "; ";
        }
        result += cookie.toRawForm(QNetworkCookie::NameAndValueOnly);
    }
    return result;
}

QString PixEagleClient::mediaOrigin() const
{
    // A localhost URL can be forwarded through a container bridge; its peer need not be loopback.
    return _endpoint.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
}

bool PixEagleClient::_readOnlyReady() const
{
    return _enabled && _authenticated && !_context.isEmpty() && (_companionOnly || associationVerified());
}

void PixEagleClient::_refreshRuntimeStatus()
{
    if (_statusReply || !_readOnlyReady() ||
        !_context.value("permissions").toObject().value("scopes").toArray().contains("telemetry:read") ||
        !_context.value("capabilities").toArray().contains("status.tracker_runtime.v1")) {
        return;
    }
    QUrl url = _endpoint;
    url.setPath(url.path() + "/api/v1/tracking/runtime-status");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(3000);
    request.setRawHeader("Accept", "application/json");
    auto* reply = _network->get(request);
    _statusReply = reply;
    reply->setReadBufferSize(MAX_RESPONSE_BYTES + 1);
    connect(reply, &QNetworkReply::readyRead, this, [reply]() {
        if (reply->bytesAvailable() > MAX_RESPONSE_BYTES) {
            reply->abort();
        }
    });
    QElapsedTimer age;
    age.start();
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation = _generation, key = _contextKey(), age]() {
        reply->deleteLater();
        if (generation != _generation || !_enabled) {
            return;
        }
        _statusReply = nullptr;
        _runtimeStatus = {};
        _statusExpiryTimer.stop();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401) {
            _resetSession();
            _error = tr("Your session expired. Sign in again.");
        } else if (reply->error() == QNetworkReply::NoError && status == 200 && age.elapsed() <= 3000 &&
                   key == _contextKey() && _readOnlyReady()) {
            const auto bytes = reply->read(MAX_RESPONSE_BYTES + 1);
            const auto data = QJsonDocument::fromJson(bytes).object();
            if (bytes.size() <= MAX_RESPONSE_BYTES && data.value("schema_version").toInt() == 1 &&
                data.value("source").toString() == "tracker_runtime" && data.value("active_tracking").isBool() &&
                data.value("following_active").isBool()) {
                _runtimeStatus = data;
                _statusExpiryTimer.start(6000 - static_cast<int>(age.elapsed()));
            }
        }
        emit changed();
    });
}

QString PixEagleClient::runtimeStatusText() const
{
    if (!_readOnlyReady() || _runtimeStatus.isEmpty()) {
        return tr("Tracking and following status unavailable");
    }
    if (_runtimeStatus.value("following_active").toBool()) {
        return tr("Following active in PixEagle. Use PixEagle to stop.");
    }
    if (_runtimeStatus.value("status").toString() == "unavailable") {
        return tr("Tracking status unavailable. Following inactive.");
    }
    if (_runtimeStatus.value("active_tracking").toBool()) {
        return _runtimeStatus.value("data_is_stale").toBool() ? tr("Tracking data stale. Following inactive.")
                                                              : tr("Tracking active. Following inactive.");
    }
    return tr("Tracking inactive. Following inactive.");
}

QVariantList PixEagleClient::followerChoices() const
{
    QVariantList result;
    for (const auto& value : _followingStatus.value("profiles").toArray()) {
        const auto row = value.toObject();
        if (row.value("compatible").toBool()) {
            result.append(
                QVariantMap{{"mode", row.value("mode").toString()}, {"label", row.value("display_name").toString()}});
        }
    }
    return result;
}

QString PixEagleClient::selectedFollower() const
{
    return _followingStatus.value("configured_mode").toString();
}

bool PixEagleClient::followingActive() const
{
    return _followingStatus.value("following_active").toBool();
}

bool PixEagleClient::followerTestActive() const
{
    return _followingFresh() && followingActive() &&
           _followingStatus.value("execution_mode").toString() == QStringLiteral("COMMAND_PREVIEW");
}

QString PixEagleClient::followingState() const
{
    if (followingActionPending()) {
        return _followingActionName == "native_follow_start" ? QStringLiteral("starting") : QStringLiteral("updating");
    }
    if (!_followingFresh()) {
        return QStringLiteral("unknown");
    }
    if (followingActive()) {
        const QString authority = _followingStatus.value("continuity_authority_state").toString();
        if (authority == "HANDOFF_PENDING") {
            return QStringLiteral("stopping");
        }
        if (_followingStatus.value("continuity_target_transition_pending").toBool()) {
            return QStringLiteral("retargeting");
        }
        if (authority == "COASTING") {
            return QStringLiteral("coasting");
        }
        if (authority == "REACQUIRING") {
            return QStringLiteral("reacquiring");
        }
        if (_followingStatus.value("target_status").toString() == "lost") {
            return QStringLiteral("target_lost");
        }
        return followerTestActive() ? QStringLiteral("preview") : QStringLiteral("active");
    }
    if (!_followingStatus.value("pending_start_id").toString().isEmpty()) {
        return QStringLiteral("starting");
    }
    if (_lastFollowingHandoff().value("result").toString() == "pending") {
        return QStringLiteral("stopping");
    }
    return canStartFollowing() ? QStringLiteral("ready") : QStringLiteral("idle");
}

QJsonObject PixEagleClient::_lastFollowingHandoff() const
{
    const QJsonObject handoff = _followingStatus.value("last_handoff").toObject();
    const QString result = handoff.value("result").toString();
    const QString execution = handoff.value("execution_mode").toString();
    const QString aircraftUid = handoff.value("aircraft_uid").toString();
    const bool ownerMatches = _companionOnly
                                  ? execution == "COMMAND_PREVIEW" && aircraftUid.isEmpty()
                                  : associationVerified() && !_aircraftUid.isEmpty() && aircraftUid == _aircraftUid;
    if (!_followingFresh() || followingActive() || !_followingStatus.value("pending_start_id").toString().isEmpty() ||
        handoff.value("follow_session_id").toString().isEmpty() || !ownerMatches ||
        (execution != "PX4" && execution != "COMMAND_PREVIEW") ||
        (result != "pending" && result != "confirmed_hold" && result != "stopped" && result != "failed") ||
        (result == "confirmed_hold" && execution != "PX4")) {
        return {};
    }
    return handoff;
}

QString PixEagleClient::followingSummaryText() const
{
    const QString state = followingState();
    const bool preview = followerTestActive();
    if (state == "retargeting") {
        return preview ? tr("Test: changing target") : tr("Changing target");
    }
    if (state == "coasting" || state == "target_lost") {
        return preview ? tr("Test: target lost") : tr("Target lost");
    }
    if (state == "reacquiring") {
        return preview ? tr("Test: reacquiring") : tr("Reacquiring");
    }
    if (state == "stopping") {
        return preview || _lastFollowingHandoff().value("execution_mode").toString() == "COMMAND_PREVIEW"
                   ? tr("Stopping test")
                   : tr("Stopping");
    }
    if (state == "preview") {
        return tr("Follower test");
    }
    if (state == "active") {
        return _followingStatus.value("sih_replay_authorized").toBool() ? tr("SIH following") : tr("Following");
    }
    if (state == "starting") {
        return tr("Starting…");
    }
    if (state == "updating") {
        return tr("Updating…");
    }
    if (state == "unknown") {
        return tr("Unknown");
    }
    const QJsonObject handoff = _lastFollowingHandoff();
    const QString result = handoff.value("result").toString();
    if (result == "confirmed_hold") {
        return tr("Hold");
    }
    if (result == "failed") {
        return tr("Stop unconfirmed");
    }
    if (result == "stopped" && handoff.value("execution_mode").toString() == "COMMAND_PREVIEW") {
        return tr("Test stopped");
    }
    if (_companionOnly) {
        return tr("Disabled");
    }
    if (state == "ready" && _followingStatus.value("sih_replay_authorized").toBool()) {
        return tr("SIH ready");
    }
    return state == "ready" ? tr("Ready") : tr("Stopped");
}

QString PixEagleClient::followingStatusText() const
{
    if (!_followingNotice.isEmpty()) {
        return _followingNotice;
    }
    if (followingActionPending()) {
        return _followingActionName == "native_follow_start" ? tr("Starting following; Stop can cancel")
                                                             : tr("Checking following action…");
    }
    if (followingActive()) {
        if (!_followingFresh()) {
            return tr("Following state is out of date; checking status");
        }
        const QString authority = _followingStatus.value("continuity_authority_state").toString();
        if (authority == "HANDOFF_PENDING") {
            return followerTestActive() ? tr("Stopping follower test; no aircraft commands")
                                        : tr("Stopping following; waiting for aircraft confirmation");
        }
        if (_followingStatus.value("continuity_target_transition_pending").toBool()) {
            return followerTestActive() ? tr("Follower test: changing target; no aircraft commands")
                                        : tr("Changing target; bounded guidance active");
        }
        if (authority == "COASTING") {
            return followerTestActive() ? tr("Follower test: target lost; no aircraft commands")
                                        : tr("Target lost; slowing while looking for it again");
        }
        if (authority == "REACQUIRING") {
            return followerTestActive() ? tr("Follower test: reacquiring; no aircraft commands")
                                        : tr("Reacquiring; confirming target and restoring follower commands");
        }
        if (_followingStatus.value("target_status").toString() == "lost") {
            return tr("Target lost; checking follower status");
        }
        if (_followingStatus.value("altitude_limited_reason").toString() == "descent_limited") {
            return tr("Following; descent limited by altitude setting");
        }
        if (_followingStatus.value("altitude_limited_reason").toString() == "climb_limited") {
            return tr("Following; climb limited by altitude setting");
        }
        if (_followingStatus.value("sih_replay_authorized").toBool()) {
            return tr("SIH simulation: following recorded targets");
        }
        return followerTestActive() ? tr("Follower test active; no aircraft commands") : tr("Following active");
    }
    if (!_followingFresh()) {
        return tr("Following status unavailable");
    }
    const QJsonObject handoff = _lastFollowingHandoff();
    if (!handoff.isEmpty()) {
        const QString result = handoff.value("result").toString();
        const bool preview = handoff.value("execution_mode").toString() == "COMMAND_PREVIEW";
        if (result == "pending") {
            return preview ? tr("Stopping follower test; no aircraft commands")
                           : tr("Stopping following; waiting for aircraft confirmation");
        }
        if (result == "failed") {
            return preview ? tr("Follower test stop unconfirmed; check PixEagle")
                           : tr("Aircraft Hold unconfirmed; check flight mode and PixEagle");
        }
        const QString reason = handoff.value("reason_code").toString();
        const QString outcome = result == "confirmed_hold" ? tr("Aircraft Hold confirmed")
                                : preview                  ? tr("Follower test stopped; no aircraft commands")
                                                           : tr("Following stopped");
        if (reason == "maximum_coast_time_reached" || reason == "maximum_coast_distance_reached" ||
            reason == "maximum_retarget_time_reached") {
            return tr("%1; target recovery budget exhausted").arg(outcome);
        }
        return outcome;
    }
    if (_followingStatus.value("start_allowed").toBool()) {
        return tr("Ready to follow");
    }
    const auto reasons = _followingStatus.value("start_reason_codes").toArray();
    if (reasons.contains("camera_control_active")) {
        return tr("Release camera controls before following");
    }
    if (reasons.contains("command_preview_not_aircraft_following")) {
        return tr("Bench: following disabled");
    }
    if (_companionOnly) {
        return tr("Connect aircraft to follow");
    }
    if (reasons.contains("aircraft_not_verified")) {
        return tr("Verify the aircraft before following");
    }
    if (reasons.contains("target_not_tracking")) {
        return tr("Select a target before following");
    }
    if (reasons.contains("vehicle_not_armed") || reasons.contains("vehicle_not_airborne")) {
        return tr("Take off before following");
    }
    if (reasons.contains("vehicle_flight_state_unavailable")) {
        return tr("Aircraft flight state unavailable; check PixEagle");
    }
    if (reasons.contains("following_altitude_below_start_margin")) {
        return tr("Climb above the configured follower altitude margin before following");
    }
    if (reasons.contains("following_altitude_above_start_margin")) {
        return tr("Descend below the configured follower altitude margin before following");
    }
    if (reasons.contains("following_altitude_unavailable")) {
        return tr("Aircraft altitude unavailable; wait for fresh telemetry");
    }
    if (reasons.contains("ACTION_OFFBOARD_REPLAY_NOT_AUTHORIZED")) {
        return tr("Recorded video cannot start aircraft following");
    }
    if (reasons.contains("ACTION_OFFBOARD_COMMAND_INHIBIT_ACTIVE")) {
        return tr("Aircraft commands are inhibited in PixEagle");
    }
    return tr("Following is not ready; check PixEagle status");
}

bool PixEagleClient::_followingReadReady() const
{
    const auto scopes = _context.value("permissions").toObject().value("scopes").toArray();
    return _readOnlyReady() && _context.value("capabilities").toArray().contains("following.operations.v1") &&
           scopes.contains("status:read") && scopes.contains("telemetry:read");
}

bool PixEagleClient::_followingFresh() const
{
    return _followingReadReady() && !_followingStatus.isEmpty() && _followingAge.isValid() &&
           _followingAge.elapsed() < TARGET_STATE_MAX_AGE_MS;
}

bool PixEagleClient::canSelectFollower() const
{
    const auto scopes = _context.value("permissions").toObject().value("scopes").toArray();
    return (_companionOnly || associationVerified()) && _followingFresh() && targetStateFresh() &&
           !followingActionPending() && !followingActive() && scopes.contains("actions:execute") &&
           scopes.contains("media:read") &&
           _followingStatus.value("guard").toObject() == _targetState.value("guard").toObject();
}

bool PixEagleClient::canStartFollowing() const
{
    return !_companionOnly && associationVerified() && canSelectFollower() &&
           _followingStatus.value("start_allowed").toBool();
}

bool PixEagleClient::canStopFollowing() const
{
    const QString sessionId = followingActive() ? _followingStatus.value("follow_session_id").toString()
                                                : _followingStatus.value("follow_session_id").toString().isEmpty()
                                                      ? _followingStatus.value("pending_start_id").toString()
                                                      : _followingStatus.value("follow_session_id").toString();
    const QString uid = followingActive() ? _followingStatus.value("follow_aircraft_uid").toString()
                                          : _followingStatus.value("follow_aircraft_uid").toString().isEmpty()
                                                ? _followingStatus.value("pending_aircraft_uid").toString()
                                                : _followingStatus.value("follow_aircraft_uid").toString();
    const bool pendingStart = !followingActive() && _followingStatus.value("follow_session_id").toString().isEmpty();
    return _enabled && _authenticated && !_companionOnly && !_identityConflict && !_duplicateAssociation &&
           _followingStatus.value("stop_allowed").toBool() &&
           (!followingActionPending() || (pendingStart && _followingActionName == "native_follow_start")) &&
           !sessionId.isEmpty() && uid == _aircraftUid;
}

void PixEagleClient::_clearFollowingState()
{
    for (auto* pending : {&_followingReply, &_followingActionReply}) {
        if (*pending) {
            auto* reply = pending->data();
            *pending = nullptr;
            disconnect(reply, nullptr, this, nullptr);
            reply->abort();
            reply->deleteLater();
        }
    }
    _followingStatus = {};
    _followingActionId.clear();
    _followingActionName.clear();
    _followingNotice.clear();
    _followingActionError.clear();
    _followingAge.invalidate();
    _followingExpiryTimer.stop();
    emit followingChanged();
}

bool PixEagleClient::_acceptFollowing(const QJsonObject& data)
{
    static const QRegularExpression digest("^[0-9a-f]{64}$");
    const auto guard = data.value("guard").toObject();
    const auto command = _context.value("command").toObject();
    const auto telemetry = _context.value("telemetry").toObject();
    const auto video = _context.value("video").toObject();
    const auto profiles = data.value("profiles").toArray();
    if (data.value("schema_version").toInt() != 1 || data.value("source").toString() != "native_following_status" ||
        data.value("instance_id") != _context.value("instance_id") ||
        data.value("runtime_id") != _context.value("runtime_id") ||
        !digest.match(data.value("profile_generation").toString()).hasMatch() ||
        !data.value("start_allowed").isBool() || !data.value("stop_allowed").isBool() ||
        !data.value("following_active").isBool() || !data.value("start_reason_codes").isArray() ||
        !data.value("configured_mode").isString() || !data.value("runtime_mode").isString() ||
        !data.value("profiles").isArray() || profiles.size() > 32 ||
        guard.value("instance_id") != data.value("instance_id") ||
        guard.value("runtime_id") != data.value("runtime_id") ||
        guard.value("command_generation") != command.value("connection_generation") ||
        guard.value("telemetry_generation") != telemetry.value("connection_generation") ||
        guard.value("aircraft_uid") != command.value("autopilot_uid") ||
        guard.value("system_id") != telemetry.value("system_id")) {
        return false;
    }
    for (const auto* field : {"stream_id", "stream_epoch", "source_epoch"}) {
        if (guard.value(field) != video.value(field)) {
            return false;
        }
    }
    for (const auto& value : profiles) {
        const auto row = value.toObject();
        if (!value.isObject() || row.value("mode").toString().isEmpty() ||
            row.value("display_name").toString().isEmpty() || !row.value("compatible").isBool()) {
            return false;
        }
    }
    _followingStatus = data;
    _followingAge.start();
    _followingExpiryTimer.start(TARGET_STATE_MAX_AGE_MS);
    _followingNotice.clear();
    return true;
}

void PixEagleClient::refreshFollowing()
{
    if (!_followingReadReady() || _followingReply) {
        return;
    }
    QUrl url = _endpoint;
    url.setPath(url.path() + "/api/v1/integration/following");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(TARGET_READ_TIMEOUT_MS);
    request.setRawHeader("Accept", "application/json");
    auto* reply = _network->get(request);
    _followingReply = reply;
    reply->setReadBufferSize(MAX_RESPONSE_BYTES + 1);
    QTimer::singleShot(TARGET_READ_TIMEOUT_MS, reply, [reply]() {
        if (!reply->isFinished()) {
            reply->abort();
        }
    });
    QElapsedTimer age;
    age.start();
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, generation = _generation, key = _targetContextKey(), age]() {
                reply->deleteLater();
                if (generation != _generation || _followingReply != reply || key != _targetContextKey()) {
                    return;
                }
                _followingReply = nullptr;
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (status == 401) {
                    _resetSession();
                    _error = tr("Your session expired. Sign in again.");
                    emit changed();
                    return;
                }
                const QByteArray bytes = reply->isOpen() ? reply->read(MAX_RESPONSE_BYTES + 1) : QByteArray{};
                const auto data = QJsonDocument::fromJson(bytes).object();
                if (reply->error() == QNetworkReply::NoError && status == 200 && bytes.size() <= MAX_RESPONSE_BYTES &&
                    age.elapsed() <= TARGET_READ_TIMEOUT_MS && _acceptFollowing(data)) {
                    _followingAge = age;
                    _followingExpiryTimer.start(TARGET_STATE_MAX_AGE_MS - static_cast<int>(age.elapsed()));
                } else {
                    _followingAge.invalidate();
                    _followingExpiryTimer.stop();
                }
                emit followingChanged();
            });
}

QString PixEagleClient::captureFollowingContext() const
{
    if (!_followingFresh()) {
        return {};
    }
    return QString::number(_generation) + "|" + _targetContextKey() + "|" +
           _followingStatus.value("profile_generation").toString() + "|" +
           QString::fromUtf8(QJsonDocument(_followingStatus.value("guard").toObject()).toJson(QJsonDocument::Compact));
}

QString PixEagleClient::captureFollowingStop() const
{
    if (!canStopFollowing()) {
        return {};
    }
    const QString sessionId = !_followingStatus.value("follow_session_id").toString().isEmpty()
                                  ? _followingStatus.value("follow_session_id").toString()
                                  : _followingStatus.value("pending_start_id").toString();
    return QString::number(_generation) + "|" + _followingStatus.value("instance_id").toString() + "|" +
           _followingStatus.value("runtime_id").toString() + "|" + sessionId + "|" + _aircraftUid;
}

bool PixEagleClient::selectFollower(const QString& mode, const QString& context)
{
    if (context.isEmpty() || context != captureFollowingContext() || !canSelectFollower()) {
        return false;
    }
    bool compatible = false;
    for (const auto& value : _followingStatus.value("profiles").toArray()) {
        const auto row = value.toObject();
        if (row.value("mode").toString() == mode && row.value("compatible").toBool()) {
            compatible = true;
            break;
        }
    }
    if (!compatible) {
        return false;
    }
    return _postFollowingAction(
        QStringLiteral("native_follower_select"),
        {{"native_context", QJsonObject{{"guard", _followingStatus.value("guard").toObject()},
                                        {"binding_mode", _companionOnly ? "companion_only" : "vehicle"}}},
         {"profile_mode", mode},
         {"profile_generation", _followingStatus.value("profile_generation")}},
        10000);
}

bool PixEagleClient::startFollowing(const QString& context)
{
    if (context.isEmpty() || context != captureFollowingContext() || !canStartFollowing()) {
        _followingActionError =
            tr("Following was not started. Review the current target, follower and aircraft, then try again.");
        emit followingChanged();
        return false;
    }
    return _postFollowingAction(QStringLiteral("native_follow_start"),
                                {{"native_context", QJsonObject{{"guard", _followingStatus.value("guard").toObject()},
                                                                {"binding_mode", "vehicle"}}},
                                 {"start_attempt_id", QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-')},
                                 {"profile_mode", selectedFollower()},
                                 {"profile_generation", _followingStatus.value("profile_generation")}},
                                30000);
}

bool PixEagleClient::stopFollowing(const QString& context)
{
    if (context.isEmpty() || context != captureFollowingStop() || !canStopFollowing()) {
        _followingActionError =
            tr("Stop was not sent. Review the current aircraft and following session, then try again.");
        emit followingChanged();
        return false;
    }
    return _postFollowingAction(QStringLiteral("native_follow_stop"),
                                {{"instance_id", _followingStatus.value("instance_id")},
                                 {"runtime_id", _followingStatus.value("runtime_id")},
                                 {"follow_session_id", !_followingStatus.value("follow_session_id").toString().isEmpty()
                                                              ? _followingStatus.value("follow_session_id")
                                                              : _followingStatus.value("pending_start_id")},
                                 {"aircraft_uid", !_followingStatus.value("follow_aircraft_uid").toString().isEmpty()
                                                         ? _followingStatus.value("follow_aircraft_uid")
                                                         : _followingStatus.value("pending_aircraft_uid")}},
                                15000);
}

bool PixEagleClient::_postFollowingAction(const QString& action, QJsonObject body, int timeoutMs)
{
    if (_followingActionReply && action == "native_follow_stop" && _followingActionName == "native_follow_start") {
        auto* startReply = _followingActionReply.data();
        _followingActionReply = nullptr;
        disconnect(startReply, nullptr, this, nullptr);
        startReply->abort();
        startReply->deleteLater();
    }
    if (_followingActionReply || _csrfHeader.isEmpty() || _csrfToken.isEmpty()) {
        return false;
    }
    QString resource = action;
    resource.replace('_', '-');
    QUrl url = _endpoint;
    url.setPath(url.path() + "/api/v1/actions/" + resource);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(timeoutMs);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader(_csrfHeader, _csrfToken);
    _followingActionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    _followingActionName = action;
    body.insert("source", "qgroundcontrol");
    body.insert("confirm", true);
    body.insert("dry_run", false);
    body.insert("idempotency_key", _followingActionId);
    _followingNotice.clear();
    _followingActionError.clear();
    auto* reply = _network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    _followingActionReply = reply;
    reply->setReadBufferSize(MAX_RESPONSE_BYTES + 1);
    connect(reply, &QNetworkReply::readyRead, this, [reply]() {
        if (reply->bytesAvailable() > MAX_RESPONSE_BYTES) {
            reply->abort();
        }
    });
    QTimer::singleShot(timeoutMs, reply, [reply]() {
        if (!reply->isFinished()) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, action, generation = _generation, id = _followingActionId]() {
                reply->deleteLater();
                if (generation != _generation || _followingActionReply != reply || id != _followingActionId) {
                    return;
                }
                _followingActionReply = nullptr;
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray bytes = reply->isOpen() ? reply->read(MAX_RESPONSE_BYTES + 1) : QByteArray{};
                const auto data = QJsonDocument::fromJson(bytes).object();
                if (status == 401) {
                    _resetSession();
                    _error = tr("Your session expired. Sign in again.");
                    emit changed();
                    return;
                }
                if (reply->error() == QNetworkReply::NoError && (status == 200 || status == 202) &&
                    bytes.size() <= MAX_RESPONSE_BYTES && data.value("action_type").toString() == action &&
                    data.value("idempotency_key").toString() == id && data.value("status").toString() == "success" &&
                    data.value("executed").toBool()) {
                    _followingNotice = action == "native_follower_select"
                                           ? tr("Follower choice saved; checking current status")
                                           : tr("Following request accepted; checking current status");
                    _followingActionError.clear();
                } else if (status == 409) {
                    _followingNotice = tr("Target, follower or aircraft changed. Review status and try again.");
                    _followingActionError = _followingNotice;
                } else if (status == 403 || status == 422) {
                    _followingNotice = tr("PixEagle refused this following action. Check permissions and readiness.");
                    _followingActionError = _followingNotice;
                } else {
                    _followingNotice = tr("Following outcome is unknown. Check PixEagle before trying again.");
                    _followingActionError = _followingNotice;
                }
                _followingActionId.clear();
                _followingActionName.clear();
                _followingAge.invalidate();
                _followingExpiryTimer.stop();
                emit followingChanged();
                refreshFollowing();
                refreshTargetState();
            });
    emit followingChanged();
    return true;
}

bool PixEagleClient::_targetReadReady() const
{
    const auto scopes = _context.value("permissions").toObject().value("scopes").toArray();
    return _readOnlyReady() && _context.value("capabilities").toArray().contains("target.operations.v1") &&
           scopes.contains("status:read") && scopes.contains("telemetry:read");
}

QString PixEagleClient::_targetContextKey() const
{
    const auto video = _context.value("video").toObject();
    const QJsonArray key{_contextKey(),
                         _context.value("command").toObject().value("connected"),
                         _context.value("telemetry").toObject().value("connected"),
                         _context.value("permissions"),
                         _context.value("capabilities"),
                         video.value("stream_id"),
                         video.value("stream_epoch"),
                         video.value("source_epoch"),
                         video.value("variant"),
                         video.value("width"),
                         video.value("height")};
    return QString::fromUtf8(QJsonDocument(key).toJson(QJsonDocument::Compact));
}

bool PixEagleClient::targetStateFresh() const
{
    return _targetReadReady() && !_targetState.isEmpty() && _targetStateAge.isValid() &&
           _targetStateAge.elapsed() < TARGET_STATE_MAX_AGE_MS;
}

QJsonObject PixEagleClient::targetGuard() const
{
    if (!targetStateFresh()) {
        return {};
    }
    auto guard = _targetState.value("guard").toObject();
    guard.insert("_client_generation", QString::number(_generation));
    guard.insert("_client_context", _targetContextKey());
    return guard;
}

bool PixEagleClient::targetWriteAllowed() const
{
    const auto scopes = _context.value("permissions").toObject().value("scopes").toArray();
    return _mutationDestinationReady() && scopes.contains("actions:execute") && scopes.contains("media:read");
}

bool PixEagleClient::_mutationDestinationReady() const
{
    if (!targetStateFresh() || targetMutationPending()) {
        return false;
    }
    if (_companionOnly) {
        const auto command = _context.value("command").toObject().value("connected");
        const auto telemetry = _context.value("telemetry").toObject().value("connected");
        return command.isBool() && telemetry.isBool() &&
               ((!command.toBool() && !telemetry.toBool()) ||
                _context.value("capabilities").toArray().contains("target.unbound_tracking.v1"));
    }
    return associationVerified();
}

void PixEagleClient::_clearTargetState()
{
    _clearModelState();
    _targetState = {};
    _targetCatalog = {};
    _targetStateAge.invalidate();
    _targetExpiryTimer.stop();
    for (auto* pending : {&_targetStateReply, &_targetCatalogReply, &_targetActionReply}) {
        if (*pending) {
            auto* reply = pending->data();
            *pending = nullptr;
            disconnect(reply, nullptr, this, nullptr);
            reply->abort();
            reply->deleteLater();
        }
    }
    if (!_targetActionName.isEmpty()) {
        const auto action = std::exchange(_targetActionName, {});
        _targetActionId.clear();
        _targetActionModelId.clear();
        emit targetActionFinished(action, QStringLiteral("unknown"),
                                  tr("The connection changed. The action outcome is unknown; check PixEagle."));
    }
    emit targetChanged();
}

void PixEagleClient::refreshTargetState()
{
    if (!_targetReadReady() || targetMutationPending()) {
        return;
    }
    if (!_targetStateReply) {
        _readTargetResource(false);
    }
    if (!_targetCatalogReply) {
        _readTargetResource(true);
    }
}

void PixEagleClient::_readTargetResource(bool catalog)
{
    QUrl url = _endpoint;
    url.setPath(url.path() + (catalog ? "/api/v1/tracking/catalog" : "/api/v1/integration/target-state"));
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(TARGET_READ_TIMEOUT_MS);
    request.setRawHeader("Accept", "application/json");
    auto* reply = _network->get(request);
    (catalog ? _targetCatalogReply : _targetStateReply) = reply;
    reply->setReadBufferSize(MAX_RESPONSE_BYTES + 1);
    connect(reply, &QNetworkReply::readyRead, this, [reply]() {
        if (reply->bytesAvailable() > MAX_RESPONSE_BYTES) {
            reply->abort();
        }
    });
    QTimer::singleShot(TARGET_READ_TIMEOUT_MS, reply, [reply]() {
        if (!reply->isFinished()) {
            reply->abort();
        }
    });
    QElapsedTimer age;
    age.start();
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, catalog, generation = _generation, key = _targetContextKey(), age]() {
                reply->deleteLater();
                if (generation != _generation || key != _targetContextKey() || !_targetReadReady()) {
                    return;
                }
                auto& pending = catalog ? _targetCatalogReply : _targetStateReply;
                if (pending != reply) {
                    return;
                }
                pending = nullptr;
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (status == 401) {
                    _resetSession();
                    _error = tr("Your session expired. Sign in again.");
                    emit changed();
                    return;
                }
                const auto bytes = reply->isOpen() ? reply->read(MAX_RESPONSE_BYTES + 1) : QByteArray{};
                const auto data = QJsonDocument::fromJson(bytes).object();
                const bool valid = reply->error() == QNetworkReply::NoError && status == 200 &&
                                   bytes.size() <= MAX_RESPONSE_BYTES && age.elapsed() <= TARGET_READ_TIMEOUT_MS;
                if (catalog) {
                    _targetCatalog = valid && data.value("schema_version").toInt() == 1 &&
                                             data.value("source").toString() == "tracking_catalog" &&
                                             data.value("ui_trackers").isArray() &&
                                             data.value("tracker_types").isObject()
                                         ? data
                                         : QJsonObject{};
                } else if (valid && _acceptTargetState(data)) {
                    _targetStateAge = age;
                    _targetExpiryTimer.start(TARGET_STATE_MAX_AGE_MS - static_cast<int>(age.elapsed()));
                } else {
                    _targetState = {};
                    _targetStateAge.invalidate();
                    _targetExpiryTimer.stop();
                }
                emit targetChanged();
            });
}

bool PixEagleClient::_acceptTargetState(const QJsonObject& data)
{
    const auto guard = data.value("guard").toObject();
    const auto command = _context.value("command").toObject();
    const auto telemetry = _context.value("telemetry").toObject();
    const auto video = _context.value("video").toObject();
    const QString mode = data.value("mode").toString();
    static const QRegularExpression revisionPattern("^(0|[1-9][0-9]{0,63})$");
    if (data.value("schema_version").toInt() != 1 || data.value("source").toString() != "native_target_state" ||
        data.value("instance_id") != _context.value("instance_id") ||
        data.value("runtime_id") != _context.value("runtime_id") ||
        !revisionPattern.match(data.value("target_revision").toString()).hasMatch() ||
        (mode != "classic" && mode != "smart" && mode != "external") || !data.value("tracking_active").isBool() ||
        !data.value("following_active").isBool() || !data.value("allowed_actions").isArray() ||
        !data.value("reason_codes").isArray() || guard.value("version").toString() != "1" ||
        guard.value("instance_id") != data.value("instance_id") ||
        guard.value("runtime_id") != data.value("runtime_id") ||
        guard.value("target_revision") != data.value("target_revision") || guard.value("mode") != data.value("mode") ||
        guard.value("tracker_type") != data.value("tracker_type") ||
        guard.value("command_generation") != command.value("connection_generation") ||
        guard.value("telemetry_generation") != telemetry.value("connection_generation")) {
        return false;
    }
    for (const auto* field : {"stream_id", "stream_epoch", "source_epoch"}) {
        if (guard.value(field) != video.value(field)) {
            return false;
        }
    }
    if (!_companionOnly &&
        (guard.value("aircraft_uid").toString() != _aircraftUid || guard.value("system_id").toInt(-1) != _systemId ||
         guard.value("component_id") != telemetry.value("component_id"))) {
        return false;
    }
    _targetState = data;
    _targetStateAge.start();
    _targetExpiryTimer.start(TARGET_STATE_MAX_AGE_MS);
    return true;
}

bool PixEagleClient::_modelReadReady() const
{
    return _readOnlyReady() && _context.value("capabilities").toArray().contains("models.operations.v1") &&
           _context.value("permissions").toObject().value("scopes").toArray().contains("models:read");
}

bool PixEagleClient::modelStateFresh() const
{
    return _modelReadReady() && !_modelInventory.isEmpty() && _modelStateAge.isValid() &&
           _modelStateAge.elapsed() < TARGET_STATE_MAX_AGE_MS;
}

bool PixEagleClient::modelSelectionAllowed() const
{
    return modelStateFresh() && _modelInventory.value("available").toBool() && _mutationDestinationReady() &&
           !_targetState.value("following_active").toBool(true) && !_runtimeStatus.value("following_active").toBool() &&
           !(_targetState.value("mode").toString() == "smart" && _targetState.value("tracking_active").toBool()) &&
           _context.value("permissions").toObject().value("scopes").toArray().contains("models:select");
}

void PixEagleClient::_clearModelState()
{
    if (_modelsReply) {
        disconnect(_modelsReply, nullptr, this, nullptr);
        _modelsReply->abort();
        _modelsReply->deleteLater();
        _modelsReply = nullptr;
    }
    _modelInventory = {};
    _modelStateAge.invalidate();
    _modelExpiryTimer.stop();
    emit modelsChanged();
}

void PixEagleClient::setModelsRequested(bool requested)
{
    _modelsRequested = requested;
    if (requested) {
        refreshModels();
    } else if (_modelsReply) {
        disconnect(_modelsReply, nullptr, this, nullptr);
        _modelsReply->abort();
        _modelsReply->deleteLater();
        _modelsReply = nullptr;
    }
}

bool PixEagleClient::_acceptModelInventory(const QJsonObject& data)
{
    static const QRegularExpression generationPattern("^[0-9a-f]{64}$");
    static const QRegularExpression idPattern("^[A-Za-z0-9][A-Za-z0-9_.-]{0,127}$");
    const auto nullableString = [](const QJsonValue& value, int limit) {
        return value.isNull() || (value.isString() && value.toString().size() <= limit);
    };
    const auto nullableId = [](const QJsonValue& value) {
        return value.isNull() || (value.isString() && idPattern.match(value.toString()).hasMatch());
    };
    const auto runtime = data.value("runtime").toObject();
    if (data.value("schema_version").toInt() != 1 || data.value("source").toString() != "native_model_inventory" ||
        data.value("instance_id") != _context.value("instance_id") ||
        data.value("runtime_id") != _context.value("runtime_id") ||
        !generationPattern.match(data.value("model_generation").toString()).hasMatch() ||
        !data.value("available").isBool() || !nullableString(data.value("unavailable_reason"), 2048) ||
        !nullableId(data.value("configured_model_id")) || !nullableId(data.value("active_model_id")) ||
        !nullableString(runtime.value("backend"), 128) || !nullableString(runtime.value("device"), 128) ||
        !runtime.value("fallback_occurred").isBool() || !nullableString(runtime.value("fallback_reason"), 2048) ||
        !data.value("models").isArray() || data.value("models").toArray().size() > 256 ||
        (!data.value("target_state").isNull() && !data.value("target_state").isObject())) {
        return false;
    }
    QStringList ids;
    for (const auto& value : data.value("models").toArray()) {
        const auto row = value.toObject();
        const auto id = row.value("model_id").toString();
        const auto labels = row.value("labels").toArray();
        const auto size = row.value("size_mb");
        const int total = row.value("total_labels").toInt(-1);
        if (!idPattern.match(id).hasMatch() || ids.contains(id) || !row.value("display_name").isString() ||
            row.value("display_name").toString().size() > 256 || !row.value("task").isString() ||
            row.value("task").toString().size() > 64 || !row.value("available").isBool() ||
            !nullableString(row.value("unavailable_reason"), 2048) || !row.value("labels").isArray() ||
            labels.size() > 32 || total < labels.size() || !row.value("has_more_labels").isBool() ||
            row.value("has_more_labels").toBool() != (total > labels.size()) ||
            (!size.isNull() && (!size.isDouble() || size.toDouble() < 0))) {
            return false;
        }
        for (const auto& label : labels) {
            if (!label.isString() || label.toString().size() > 256) {
                return false;
            }
        }
        ids.append(id);
    }
    for (const auto* field : {"configured_model_id", "active_model_id"}) {
        if (!data.value(field).isNull() && !ids.contains(data.value(field).toString())) {
            return false;
        }
    }
    _modelInventory = data;
    _modelStateAge.start();
    _modelExpiryTimer.start(TARGET_STATE_MAX_AGE_MS);
    return true;
}

void PixEagleClient::refreshModels()
{
    if (!_modelReadReady() || _modelsReply || targetMutationPending()) {
        return;
    }
    QUrl url = _endpoint;
    url.setPath(url.path() + "/api/v1/integration/models");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(TARGET_READ_TIMEOUT_MS);
    request.setRawHeader("Accept", "application/json");
    auto* reply = _network->get(request);
    _modelsReply = reply;
    reply->setReadBufferSize(MAX_RESPONSE_BYTES + 1);
    connect(reply, &QNetworkReply::readyRead, this, [reply]() {
        if (reply->bytesAvailable() > MAX_RESPONSE_BYTES) {
            reply->abort();
        }
    });
    QTimer::singleShot(TARGET_READ_TIMEOUT_MS, reply, [reply]() {
        if (!reply->isFinished()) {
            reply->abort();
        }
    });
    QElapsedTimer age;
    age.start();
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, generation = _generation, key = _targetContextKey(), age]() {
                reply->deleteLater();
                if (generation != _generation || key != _targetContextKey() || _modelsReply != reply ||
                    !_modelReadReady()) {
                    return;
                }
                _modelsReply = nullptr;
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (status == 401) {
                    _resetSession();
                    _error = tr("Your session expired. Sign in again.");
                    emit changed();
                    return;
                }
                const auto bytes = reply->isOpen() ? reply->read(MAX_RESPONSE_BYTES + 1) : QByteArray{};
                const auto data = QJsonDocument::fromJson(bytes).object();
                if (reply->error() == QNetworkReply::NoError && status == 200 && bytes.size() <= MAX_RESPONSE_BYTES &&
                    age.elapsed() <= TARGET_READ_TIMEOUT_MS && _acceptModelInventory(data)) {
                    _modelStateAge = age;
                    _modelExpiryTimer.start(TARGET_STATE_MAX_AGE_MS - static_cast<int>(age.elapsed()));
                    const auto target = data.value("target_state").toObject();
                    const auto revision = target.value("target_revision").toString();
                    const auto previous = _targetState.value("target_revision").toString();
                    if (_targetReadReady() &&
                        (revision.size() > previous.size() ||
                         (revision.size() == previous.size() && revision >= previous)) &&
                        _acceptTargetState(target)) {
                        _targetStateAge = age;
                        _targetExpiryTimer.start(TARGET_STATE_MAX_AGE_MS - static_cast<int>(age.elapsed()));
                        emit targetChanged();
                    }
                } else {
                    _clearModelState();
                }
                emit modelsChanged();
            });
}

bool PixEagleClient::submitModelSelection(const QString& modelId, const QJsonObject& capturedTargetGuard,
                                          const QString& modelGeneration)
{
    if (!modelSelectionAllowed() || capturedTargetGuard.isEmpty() || capturedTargetGuard != targetGuard() ||
        modelGeneration != _modelInventory.value("model_generation").toString()) {
        return false;
    }
    bool available = false;
    for (const auto& value : _modelInventory.value("models").toArray()) {
        const auto row = value.toObject();
        if (row.value("model_id").toString() == modelId && row.value("available").toBool()) {
            available = true;
            break;
        }
    }
    if (!available) {
        return false;
    }
    _targetActionModelId = modelId;
    return _submitAction(QStringLiteral("model_select"),
                         {{"model_id", modelId}, {"model_generation", modelGeneration}, {"device", "auto"}},
                         capturedTargetGuard, MODEL_ACTION_TIMEOUT_MS);
}

bool PixEagleClient::submitTargetAction(const QString& action, const QJsonObject& payload,
                                        const QJsonObject& capturedGuard)
{
    static const QStringList actions{"tracking_start",    "smart_click",    "tracking_stop",
                                     "smart_mode_toggle", "tracker_switch", "gimbal_control"};
    if (!targetWriteAllowed() || capturedGuard.isEmpty() || capturedGuard != targetGuard() ||
        !actions.contains(action) || !_targetState.value("allowed_actions").toArray().contains(action)) {
        return false;
    }
    if (action == "gimbal_control") {
        const auto operation = payload.value("operation").toString();
        if (operation != "select" && operation != "cancel" && operation != "set_mode") {
            return false;
        }
    }
    const bool selection = action == "tracking_start" || action == "smart_click" ||
                           (action == "gimbal_control" && payload.value("operation").toString() == "select");
    if ((_targetState.value("following_active").toBool() || _runtimeStatus.value("following_active").toBool()) &&
        !selection) {
        return false;
    }
    if (selection && !payload.value("frame").isObject()) {
        return false;
    }
    const int timeoutMs = action == "gimbal_control" && payload.value("operation").toString() == "select"
                              ? CAMERA_SELECT_ACTION_TIMEOUT_MS
                              : TARGET_ACTION_TIMEOUT_MS;
    return _submitAction(action, payload, capturedGuard, timeoutMs);
}

bool PixEagleClient::_submitAction(const QString& action, QJsonObject body, const QJsonObject& capturedGuard,
                                   int timeoutMs)
{
    QJsonObject guard = capturedGuard;
    guard.remove("_client_generation");
    guard.remove("_client_context");
    QJsonObject nativeContext{{"guard", guard}, {"binding_mode", _companionOnly ? "companion_only" : "vehicle"}};
    if (body.contains("frame")) {
        nativeContext.insert("frame", body.take("frame"));
    }
    _targetActionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    body.insert("source", "qgroundcontrol");
    body.insert("confirm", true);
    body.insert("dry_run", false);
    body.insert("idempotency_key", _targetActionId);
    body.insert("native_context", nativeContext);
    if (action == "tracker_switch") {
        body.insert("persist", false);
    }
    QString resource = action;
    resource.replace('_', '-');
    QUrl url = _endpoint;
    url.setPath(url.path() + "/api/v1/actions/" + resource);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(timeoutMs);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader(_csrfHeader, _csrfToken);
    _clearModelState();
    if (_targetStateReply) {
        disconnect(_targetStateReply, nullptr, this, nullptr);
        _targetStateReply->abort();
        _targetStateReply->deleteLater();
        _targetStateReply = nullptr;
    }
    _targetStateAge.invalidate();
    _targetExpiryTimer.stop();
    _targetActionName = action;
    auto* reply = _network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    _targetActionReply = reply;
    reply->setReadBufferSize(MAX_RESPONSE_BYTES + 1);
    connect(reply, &QNetworkReply::readyRead, this, [reply]() {
        if (reply->bytesAvailable() > MAX_RESPONSE_BYTES) {
            reply->abort();
        }
    });
    QTimer::singleShot(timeoutMs, reply, [reply]() {
        if (!reply->isFinished()) {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, action, generation = _generation, key = _targetContextKey()]() {
                _finishTargetAction(reply, action, generation, key);
            });
    emit targetChanged();
    return true;
}

void PixEagleClient::_finishTargetAction(QNetworkReply* reply, const QString& action, quint64 generation,
                                         const QString& contextKey)
{
    reply->deleteLater();
    if (generation != _generation || contextKey != _targetContextKey() || _targetActionReply != reply) {
        return;
    }
    _targetActionReply = nullptr;
    _targetActionName.clear();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto bytes = reply->isOpen() ? reply->read(MAX_RESPONSE_BYTES + 1) : QByteArray{};
    const auto data = QJsonDocument::fromJson(bytes).object();
    QString outcome = QStringLiteral("unknown");
    const bool modelSelection = action == "model_select";
    QString message = modelSelection
                          ? tr("The model change outcome is unknown. Refresh models before trying again.")
                          : tr("The action outcome is unknown. Check the current target before trying again.");
    if (status == 401) {
        _resetSession();
        _error = tr("Your session expired. Sign in again.");
        outcome = QStringLiteral("rejected");
        message = _error;
        emit changed();
    } else if (status == 409) {
        outcome = QStringLiteral("conflict");
        const auto code = data.value("code").toString();
        if (!modelSelection && (code == "frame_evicted" || code == "frame_expired")) {
            message = tr("The displayed frame expired before selection. Tap the live video again.");
        } else {
            message = modelSelection
                          ? tr("The model, target or connection changed. Review models and select again.")
                          : tr("The target or connection changed. Review the current image and select again.");
        }
    } else if (status == 403 || status == 400 || status == 422) {
        outcome = QStringLiteral("rejected");
        message = modelSelection
                      ? tr("PixEagle did not allow this model change. Check model permissions and availability.")
                      : tr("PixEagle did not allow this target action. Check permissions and target availability.");
    } else if (reply->error() == QNetworkReply::NoError && (status == 200 || status == 202) &&
               bytes.size() <= MAX_RESPONSE_BYTES && data.value("action_type").toString() == action &&
               data.value("idempotency_key").toString() == _targetActionId && data.value("accepted").isBool() &&
               data.value("executed").isBool() && data.value("dry_run").isBool() && !data.value("dry_run").toBool() &&
               data.value("confirmed").toBool()) {
        if (data.value("status").toString() == "success" && data.value("accepted").toBool() &&
            data.value("executed").toBool()) {
            const auto result = data.value("result").toObject();
            const auto selectionStatus = result.value("selection_status").toString();
            const auto inventory = result.value("model_inventory").toObject();
            if (!modelSelection ||
                (result.value("model_id").toString() == _targetActionModelId &&
                 (selectionStatus == "configured" || selectionStatus == "active") &&
                 inventory.value("configured_model_id").toString() == _targetActionModelId &&
                 (selectionStatus != "active" ||
                  inventory.value("active_model_id").toString() == _targetActionModelId) &&
                 inventory.value("target_state") == result.value("target_state") &&
                 result.value("native_target_revision") ==
                     result.value("target_state").toObject().value("target_revision") &&
                 _acceptTargetState(result.value("target_state").toObject()) && _acceptModelInventory(inventory))) {
                outcome = QStringLiteral("accepted");
                message = modelSelection ? tr("PixEagle confirmed the model change.")
                                         : tr("PixEagle confirmed the target action.");
            }
        } else if (data.value("status").toString() == "failure") {
            outcome = QStringLiteral("rejected");
            message = modelSelection ? tr("PixEagle could not change the model. Review the current model and runtime.")
                                     : tr("PixEagle could not complete the target action. Check the current target.");
        }
        _acceptTargetState(data.value("result").toObject().value("target_state").toObject());
    }
    _targetActionId.clear();
    _targetActionModelId.clear();
    emit targetChanged();
    emit modelsChanged();
    emit targetActionFinished(action, outcome, message);
    refreshTargetState();
    if (modelSelection || _modelsRequested) {
        refreshModels();
    }
}
