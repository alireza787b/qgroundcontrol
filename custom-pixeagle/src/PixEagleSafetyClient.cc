#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "PixEagleClient.h"

namespace {
constexpr int SAFETY_MAX_AGE_MS = 6000;
constexpr int SAFETY_READ_TIMEOUT_MS = 3000;
constexpr int SAFETY_ACTION_TIMEOUT_MS = 15000;
constexpr qint64 SAFETY_MAX_BYTES = 128 * 1024;

void boundSafetyReply(QNetworkReply* reply, int timeoutMs)
{
    reply->setReadBufferSize(SAFETY_MAX_BYTES + 1);
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply]() {
        if (reply->bytesAvailable() > SAFETY_MAX_BYTES) {
            reply->abort();
        }
    });
    QTimer::singleShot(timeoutMs, reply, [reply]() {
        if (!reply->isFinished()) {
            reply->abort();
        }
    });
}
}  // namespace

bool PixEagleClient::safetyAvailable() const
{
    return _readOnlyReady() && _context.value("capabilities").toArray().contains("safety.circuit_breaker.v1") &&
           _context.value("permissions").toObject().value("scopes").toArray().contains("safety:read");
}

bool PixEagleClient::safetyFresh() const
{
    return safetyAvailable() && _safetyAge.isValid() && _safetyAge.elapsed() < SAFETY_MAX_AGE_MS &&
           _safetyState.value("instance_id") == _context.value("instance_id") &&
           _safetyState.value("runtime_id") == _context.value("runtime_id") &&
           _safetyState.value("source").toString() == QStringLiteral("native_safety_status") &&
           _safetyState.value("schema_version").toInt() == 1 && _safetyState.value("active").isBool();
}

bool PixEagleClient::safetyActive() const
{
    return safetyFresh() && _safetyState.value("active").toBool();
}

bool PixEagleClient::safetyFollowerTest() const
{
    return safetyFresh() && _safetyState.value("follower_test").toBool();
}

bool PixEagleClient::canSetSafety() const
{
    return safetyFresh() && !safetyBusy() && _safetyState.value("can_set").toBool() &&
           _context.value("permissions").toObject().value("scopes").toArray().contains("safety:write") &&
           !_csrfHeader.isEmpty() && !_csrfToken.isEmpty();
}

QString PixEagleClient::safetyStatusText() const
{
    if (safetyBusy()) {
        return tr("Checking command block…");
    }
    if (!_safetyNotice.isEmpty()) {
        return _safetyNotice;
    }
    if (!safetyFresh()) {
        return safetyAvailable() ? tr("Command block state unknown; refresh PixEagle.")
                                 : tr("Command block control unavailable on this PixEagle backend.");
    }
    if (!_safetyState.value("available").toBool()) {
        return tr("Command block state unavailable; check PixEagle.");
    }
    if (safetyFollowerTest()) {
        return tr("Follower test; PixEagle flight commands remain blocked.");
    }
    return safetyActive() ? tr("Commands blocked") : tr("PixEagle flight commands permitted");
}

void PixEagleClient::_clearSafetyState()
{
    for (auto* pending : {&_safetyReply, &_safetyActionReply}) {
        if (*pending) {
            auto* reply = pending->data();
            *pending = nullptr;
            disconnect(reply, nullptr, this, nullptr);
            reply->abort();
            reply->deleteLater();
        }
    }
    _safetyState = {};
    _safetyNotice.clear();
    _safetyAge.invalidate();
    _safetyExpiryTimer.stop();
    emit safetyChanged();
}

void PixEagleClient::refreshSafety()
{
    if (!safetyAvailable() || _safetyReply || safetyBusy()) {
        return;
    }
    QUrl url = _endpoint;
    url.setPath(url.path() + "/api/v1/integration/safety");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(SAFETY_READ_TIMEOUT_MS);
    request.setRawHeader("Accept", "application/json");
    auto* reply = _network->get(request);
    _safetyReply = reply;
    boundSafetyReply(reply, SAFETY_READ_TIMEOUT_MS);
    QElapsedTimer age;
    age.start();
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation = _generation, age]() {
        reply->deleteLater();
        if (generation != _generation || _safetyReply != reply) {
            return;
        }
        _safetyReply = nullptr;
        _safetyState = {};
        _safetyAge.invalidate();
        _safetyExpiryTimer.stop();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && status == 200 && age.elapsed() < SAFETY_READ_TIMEOUT_MS &&
            safetyAvailable()) {
            const QByteArray bytes = reply->read(SAFETY_MAX_BYTES + 1);
            const QJsonObject data = QJsonDocument::fromJson(bytes).object();
            if (bytes.size() <= SAFETY_MAX_BYTES && data.value("schema_version").toInt() == 1 &&
                data.value("source").toString() == QStringLiteral("native_safety_status") &&
                data.value("instance_id") == _context.value("instance_id") &&
                data.value("runtime_id") == _context.value("runtime_id") && data.value("active").isBool() &&
                data.value("state_generation").toString().size() == 64) {
                _safetyState = data;
                _safetyAge.start();
                _safetyExpiryTimer.start(SAFETY_MAX_AGE_MS);
                _safetyNotice.clear();
            }
        }
        emit safetyChanged();
    });
}

QString PixEagleClient::captureSafetyContext() const
{
    if (!canSetSafety()) {
        return {};
    }
    const QJsonObject context{{"instance_id", _safetyState.value("instance_id")},
                              {"runtime_id", _safetyState.value("runtime_id")},
                              {"state_generation", _safetyState.value("state_generation")},
                              {"expected_active", _safetyState.value("active")}};
    return QString::fromUtf8(QJsonDocument(context).toJson(QJsonDocument::Compact));
}

bool PixEagleClient::setSafetyActive(bool enabled, const QString& context)
{
    if (!canSetSafety() || enabled == safetyActive() || (!enabled && safetyFollowerTest()) ||
        QJsonDocument::fromJson(context.toUtf8()).object() !=
            QJsonDocument::fromJson(captureSafetyContext().toUtf8()).object()) {
        return false;
    }
    const QJsonObject captured = QJsonDocument::fromJson(context.toUtf8()).object();
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QJsonObject body{{"source", "qgroundcontrol"}, {"confirm", true},    {"dry_run", false},
                           {"idempotency_key", id},      {"enabled", enabled}, {"native_safety_context", captured}};
    QUrl url = _endpoint;
    url.setPath(url.path() + "/api/v1/actions/circuit-breaker-set");
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setTransferTimeout(SAFETY_ACTION_TIMEOUT_MS);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader(_csrfHeader, _csrfToken);
    auto* reply = _network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    _safetyActionReply = reply;
    _safetyAge.invalidate();
    _safetyExpiryTimer.stop();
    _safetyNotice.clear();
    boundSafetyReply(reply, SAFETY_ACTION_TIMEOUT_MS);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation = _generation, id]() {
        reply->deleteLater();
        if (generation != _generation || _safetyActionReply != reply) {
            return;
        }
        _safetyActionReply = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->isOpen() ? reply->read(SAFETY_MAX_BYTES + 1) : QByteArray{};
        const QJsonObject data = QJsonDocument::fromJson(bytes).object();
        if (reply->error() == QNetworkReply::NoError && (status == 200 || status == 202) &&
            bytes.size() <= SAFETY_MAX_BYTES && data.value("action_type").toString() == "circuit_breaker_set" &&
            data.value("idempotency_key").toString() == id && data.value("status").toString() == "success" &&
            data.value("executed").toBool()) {
            _safetyNotice = tr("Checking current command block state…");
        } else {
            _safetyNotice = status == 409 ? tr("Safety state changed. Refresh and review it before trying again.")
                                          : tr("Command block outcome unknown. Check PixEagle before continuing.");
        }
        emit safetyChanged();
        refreshSafety();
    });
    emit safetyChanged();
    return true;
}
