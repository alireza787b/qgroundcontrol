#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QSharedPointer>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>

#include "PixEagleClient.h"
#include "UnitTest.h"

namespace PixEagleTest {
inline const QString AIRCRAFT_UID = QStringLiteral("18446744073709551614");
inline const QByteArray LOGIN_PATH = "/api/v1/auth/login";
inline const QByteArray CONTEXT_PATH = "/api/v1/integration/context";
inline const QByteArray VERIFY_PATH = "/api/v1/integration/connection";
inline const QByteArray LOGOUT_PATH = "/api/v1/auth/logout";
inline const QByteArray STATUS_PATH = "/api/v1/tracking/runtime-status";
inline const QByteArray TARGET_STATE_PATH = "/api/v1/integration/target-state";
inline const QByteArray TARGET_CATALOG_PATH = "/api/v1/tracking/catalog";
inline const QByteArray MODEL_INVENTORY_PATH = "/api/v1/integration/models";
inline const QByteArray MODEL_SELECT_PATH = "/api/v1/actions/model-select";
inline const QByteArray FOLLOWING_PATH = "/api/v1/integration/following";
inline const QByteArray FOLLOW_START_PATH = "/api/v1/actions/native-follow-start";
inline const QByteArray FOLLOW_STOP_PATH = "/api/v1/actions/native-follow-stop";
inline const QByteArray SAFETY_PATH = "/api/v1/integration/safety";
inline const QByteArray SAFETY_SET_PATH = "/api/v1/actions/circuit-breaker-set";

inline void advertiseMedia(QJsonObject& context)
{
    context.insert("capabilities", QJsonArray{"video.frame_provenance.v1", "status.tracker_runtime.v1"});
    context.insert("video", QJsonObject{{"provenance_version", "1"}, {"ws_path", "/ws/video_feed"}});
    context.insert("permissions", QJsonObject{{"principal_kind", "session"},
                                              {"scopes", QJsonArray{"status:read", "telemetry:read", "media:read"}}});
}

inline QJsonObject aircraftContext(const QString& uid = AIRCRAFT_UID, int systemId = 7)
{
    return {{"contract_version", "1"},
            {"instance_id", "companion-instance"},
            {"runtime_id", "runtime-a"},
            {"command", QJsonObject{{"connected", true},
                                    {"connection_generation", "3"},
                                    {"autopilot_uid", uid},
                                    {"system_id", QJsonValue::Null},
                                    {"component_id", QJsonValue::Null}}},
            {"telemetry", QJsonObject{{"connected", true},
                                      {"connection_generation", "5"},
                                      {"autopilot_uid", uid},
                                      {"system_id", systemId},
                                      {"component_id", 1},
                                      {"fresh", true}}},
            {"association", QJsonObject{{"verified", true}}},
            {"readiness", QJsonObject{{"connection_ready", true}, {"following_allowed", false}}},
            {"permissions",
             QJsonObject{{"principal_kind", "session"}, {"scopes", QJsonArray{"status:read", "telemetry:read"}}}}};
}

inline void advertiseTargets(QJsonObject& context, bool companionOnly = false)
{
    advertiseMedia(context);
    auto capabilities = context.value("capabilities").toArray();
    capabilities.append("target.operations.v1");
    context.insert("capabilities", capabilities);
    auto permissions = context.value("permissions").toObject();
    auto scopes = permissions.value("scopes").toArray();
    scopes.append("actions:execute");
    permissions.insert("scopes", scopes);
    context.insert("permissions", permissions);
    auto video = context.value("video").toObject();
    video.insert("stream_id", "video-feed");
    video.insert("stream_epoch", "stream-a");
    video.insert("source_epoch", "source-a");
    context.insert("video", video);
    if (companionOnly) {
        for (const auto* field : {"command", "telemetry"}) {
            auto identity = context.value(field).toObject();
            identity.insert("connected", false);
            context.insert(field, identity);
        }
    }
}

inline QJsonObject nativeTargetState(const QJsonObject& context, const QString& revision = QStringLiteral("1"))
{
    const auto command = context.value("command").toObject();
    const auto telemetry = context.value("telemetry").toObject();
    const auto video = context.value("video").toObject();
    const QJsonObject guard{{"version", "1"},
                            {"instance_id", context.value("instance_id")},
                            {"runtime_id", context.value("runtime_id")},
                            {"target_revision", revision},
                            {"mode", "classic"},
                            {"tracker_type", "CSRT"},
                            {"command_generation", command.value("connection_generation")},
                            {"telemetry_generation", telemetry.value("connection_generation")},
                            {"aircraft_uid", command.value("autopilot_uid")},
                            {"system_id", telemetry.value("system_id")},
                            {"component_id", telemetry.value("component_id")},
                            {"stream_id", video.value("stream_id")},
                            {"stream_epoch", video.value("stream_epoch")},
                            {"source_epoch", video.value("source_epoch")}};
    return {{"schema_version", 1},
            {"source", "native_target_state"},
            {"instance_id", context.value("instance_id")},
            {"runtime_id", context.value("runtime_id")},
            {"target_revision", revision},
            {"mode", "classic"},
            {"tracker_type", "CSRT"},
            {"external_selection_mode", QJsonValue::Null},
            {"tracking_active", false},
            {"following_active", false},
            {"target_status", "inactive"},
            {"mode_availability", QJsonObject{{"classic", QJsonObject{{"available", true}}},
                                              {"smart", QJsonObject{{"available", true}}},
                                              {"external", QJsonObject{{"available", true}}}}},
            {"allowed_actions", QJsonArray{"tracking_start", "smart_click", "tracking_stop", "smart_mode_toggle",
                                           "tracker_switch", "gimbal_control"}},
            {"reason_codes", QJsonArray{}},
            {"guard", guard}};
}

inline void advertiseModels(QJsonObject& context)
{
    auto capabilities = context.value("capabilities").toArray();
    capabilities.append("models.operations.v1");
    context.insert("capabilities", capabilities);
    auto permissions = context.value("permissions").toObject();
    auto scopes = permissions.value("scopes").toArray();
    scopes.append("models:read");
    scopes.append("models:select");
    permissions.insert("scopes", scopes);
    context.insert("permissions", permissions);
}

inline QJsonObject nativeModelInventory(const QJsonObject& context, const QString& revision = QStringLiteral("1"))
{
    const QJsonObject row{{"model_id", "detector-a"},
                          {"display_name", "Detector A"},
                          {"task", "detect"},
                          {"available", true},
                          {"unavailable_reason", QJsonValue::Null},
                          {"labels", QJsonArray{"person", "car"}},
                          {"total_labels", 2},
                          {"has_more_labels", false},
                          {"size_mb", 25.0}};
    return {{"schema_version", 1},
            {"source", "native_model_inventory"},
            {"instance_id", context.value("instance_id")},
            {"runtime_id", context.value("runtime_id")},
            {"model_generation", QString(64, 'a')},
            {"available", true},
            {"unavailable_reason", QJsonValue::Null},
            {"configured_model_id", QJsonValue::Null},
            {"active_model_id", QJsonValue::Null},
            {"runtime", QJsonObject{{"backend", QJsonValue::Null},
                                    {"device", QJsonValue::Null},
                                    {"fallback_occurred", false},
                                    {"fallback_reason", QJsonValue::Null}}},
            {"models", QJsonArray{row}},
            {"target_state", nativeTargetState(context, revision)}};
}

inline void advertiseFollowing(QJsonObject& context)
{
    auto capabilities = context.value("capabilities").toArray();
    capabilities.append("following.operations.v1");
    context.insert("capabilities", capabilities);
}

inline void advertiseSafety(QJsonObject& context)
{
    auto capabilities = context.value("capabilities").toArray();
    capabilities.append("safety.circuit_breaker.v1");
    context.insert("capabilities", capabilities);
    auto permissions = context.value("permissions").toObject();
    auto scopes = permissions.value("scopes").toArray();
    scopes.append("safety:read");
    scopes.append("safety:write");
    permissions.insert("scopes", scopes);
    context.insert("permissions", permissions);
}

inline QJsonObject nativeSafetyStatus(const QJsonObject& context, bool active = true)
{
    return {{"schema_version", 1},
            {"source", "native_safety_status"},
            {"instance_id", context.value("instance_id")},
            {"runtime_id", context.value("runtime_id")},
            {"state_generation", QString(64, active ? 'a' : 'b')},
            {"available", true},
            {"active", active},
            {"persisted_active", active},
            {"follower_test", false},
            {"following_active", false},
            {"can_set", true},
            {"reason_code", QJsonValue::Null}};
}

inline QJsonObject nativeFollowingStatus(const QJsonObject& context, bool ready = false, bool active = false)
{
    const auto guard = nativeTargetState(context).value("guard");
    const QJsonObject profile{{"mode", "mc_velocity_position"},
                              {"display_name", "Position follow"},
                              {"control_type", "velocity"},
                              {"airframe_phase", "multicopter"},
                              {"compatible", true},
                              {"reason_code", QJsonValue::Null}};
    return {
        {"schema_version", 1},
        {"source", "native_following_status"},
        {"instance_id", context.value("instance_id")},
        {"runtime_id", context.value("runtime_id")},
        {"profile_generation", QString(64, 'a')},
        {"configured_mode", "mc_velocity_position"},
        {"runtime_mode", "mc_velocity_position"},
        {"current_mode", active ? QJsonValue("mc_velocity_position") : QJsonValue::Null},
        {"activation_pending", false},
        {"execution_mode", "PX4"},
        {"following_active", active},
        {"following_status", active ? "active" : "inactive"},
        {"target_mode", "classic"},
        {"target_status", ready ? "tracking" : "idle"},
        {"profiles", QJsonArray{profile}},
        {"start_allowed", ready && !active},
        {"start_reason_codes", ready ? QJsonArray{} : QJsonArray{"target_not_tracking"}},
        {"stop_allowed", active},
        {"follow_session_id", active ? QJsonValue(QString(32, 'a')) : QJsonValue::Null},
        {"follow_aircraft_uid", active ? context.value("command").toObject().value("autopilot_uid") : QJsonValue::Null},
        {"pending_start_id", QJsonValue::Null},
        {"pending_aircraft_uid", QJsonValue::Null},
        {"guard", guard}};
}

struct HttpRequest
{
    QByteArray method;
    QByteArray target;
    QMap<QByteArray, QByteArray> headers;
    QByteArray body;
    QPointer<QTcpSocket> socket;
};

struct HttpResponse
{
    int status = 200;
    QJsonObject body;
    QMap<QByteArray, QByteArray> headers;
};

// This fixture records complete requests, including POST bodies, before responding.
class CompanionServer : public QTcpServer
{
public:
    explicit CompanionServer(const QByteArray& name = "first")
        : tag(name)
        , context(aircraftContext())
    {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (hasPendingConnections()) {
                QTcpSocket* socket = nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                const auto buffer = QSharedPointer<QByteArray>::create();
                const auto processed = QSharedPointer<bool>::create(false);
                const auto readRequest = [this, socket, buffer, processed]() {
                    if (*processed) {
                        return;
                    }
                    buffer->append(socket->readAll());
                    const qsizetype headerEnd = buffer->indexOf("\r\n\r\n");
                    if (headerEnd < 0) {
                        return;
                    }
                    const QList<QByteArray> lines = buffer->left(headerEnd).split('\n');
                    const QList<QByteArray> start = lines.first().trimmed().split(' ');
                    if (start.size() < 2) {
                        socket->abort();
                        return;
                    }
                    HttpRequest request;
                    request.method = start[0];
                    request.target = start[1];
                    request.socket = socket;
                    for (qsizetype i = 1; i < lines.size(); ++i) {
                        const qsizetype colon = lines[i].indexOf(':');
                        if (colon > 0) {
                            request.headers.insert(lines[i].left(colon).trimmed().toLower(),
                                                   lines[i].mid(colon + 1).trimmed());
                        }
                    }
                    const qsizetype length = request.headers.value("content-length").toLongLong();
                    if (buffer->size() < headerEnd + 4 + length) {
                        return;
                    }
                    request.body = buffer->mid(headerEnd + 4, length);
                    *processed = true;
                    requests.append(request);
                    if (deferredPath.isEmpty() || !request.target.endsWith(deferredPath)) {
                        respond(requests.size() - 1, responseFor(request));
                    }
                };
                connect(socket, &QTcpSocket::readyRead, socket, readRequest);
                readRequest();
            }
        });
    }

    bool start() { return listen(QHostAddress::LocalHost, 0); }

    QString endpoint(const QString& prefix = {}) const
    {
        return QStringLiteral("http://127.0.0.1:%1%2").arg(serverPort()).arg(prefix);
    }

    HttpRequest lastRequest(const QByteArray& path) const
    {
        for (auto it = requests.crbegin(); it != requests.crend(); ++it) {
            if (it->target.endsWith(path)) {
                return *it;
            }
        }
        return {};
    }

    qsizetype lastRequestIndex(const QByteArray& path) const
    {
        for (qsizetype index = requests.size(); index > 0; --index) {
            if (requests[index - 1].target.endsWith(path)) {
                return index - 1;
            }
        }
        return -1;
    }

    HttpResponse responseFor(const HttpRequest& request) const
    {
        if (overrides.contains(request.target)) {
            return overrides.value(request.target);
        }
        if (request.target.endsWith(LOGIN_PATH)) {
            return {200,
                    {{"authenticated", true},
                     {"auth_mode", "browser_session"},
                     {"principal", QJsonObject{{"kind", "session"}, {"subject", "pilot"}, {"role", "operator"}}},
                     {"csrf_required", true},
                     {"csrf_header_name", "x-test-csrf"},
                     {"csrf_token", QString::fromLatin1("csrf-" + tag)}},
                    {{"Set-Cookie", "pixeagle_session=session-" + tag + "; Path=/; HttpOnly; SameSite=Lax"}}};
        }
        if (request.target.endsWith(CONTEXT_PATH) || request.target.endsWith(VERIFY_PATH)) {
            return {200, context, {}};
        }
        if (request.target.endsWith(LOGOUT_PATH)) {
            return {200, {{"authenticated", false}, {"revoked", true}}, {}};
        }
        if (request.target.endsWith(STATUS_PATH)) {
            return {200,
                    {{"schema_version", 1},
                     {"source", "tracker_runtime"},
                     {"status", "inactive"},
                     {"active_tracking", false},
                     {"following_active", false},
                     {"data_is_stale", false}},
                    {}};
        }
        if (request.target.endsWith(TARGET_STATE_PATH)) {
            return {200, targetSnapshot.isEmpty() ? nativeTargetState(context) : targetSnapshot, {}};
        }
        if (request.target.endsWith(FOLLOWING_PATH)) {
            return {200, followingSnapshot.isEmpty() ? nativeFollowingStatus(context) : followingSnapshot, {}};
        }
        if (request.target.endsWith(TARGET_CATALOG_PATH)) {
            return {200,
                    {{"schema_version", 1},
                     {"source", "tracking_catalog"},
                     {"ui_trackers", QJsonArray{}},
                     {"tracker_types", QJsonObject{}}},
                    {}};
        }
        if (request.target.endsWith(MODEL_INVENTORY_PATH)) {
            return {200, modelSnapshot.isEmpty() ? nativeModelInventory(context) : modelSnapshot, {}};
        }
        if (request.target.contains("/api/v1/actions/")) {
            const auto body = QJsonDocument::fromJson(request.body).object();
            auto action = QString::fromLatin1(request.target.mid(request.target.lastIndexOf('/') + 1));
            action.replace('-', '_');
            QJsonObject result{{"target_state", nativeTargetState(context, "2")}};
            if (action == "gimbal_control" && body.value("operation").toString().startsWith("manual_")) {
                result.insert("manual", QJsonObject{{"gesture_id", body.value("gesture_id")},
                                                    {"sequence", body.value("sequence")},
                                                    {"state", "moving"},
                                                    {"renew_interval_ms", 100},
                                                    {"lease_timeout_ms", 350}});
            }
            if (action == "model_select") {
                auto inventory = nativeModelInventory(context, "2");
                inventory.insert("configured_model_id", body.value("model_id"));
                inventory.insert("model_generation", QString(64, 'b'));
                result.insert("native_target_revision", "2");
                result.insert("model_inventory", inventory);
                result.insert("selection_status", "configured");
                result.insert("model_id", body.value("model_id"));
            }
            return {202,
                    {{"action_id", "action-fixture"},
                     {"action_type", action},
                     {"status", "success"},
                     {"accepted", true},
                     {"executed", true},
                     {"dry_run", false},
                     {"confirmed", true},
                     {"idempotency_key", body.value("idempotency_key")},
                     {"result", result}},
                    {}};
        }
        return {404, {}, {}};
    }

    bool respond(qsizetype index, const HttpResponse& response)
    {
        QTcpSocket* socket = requests[index].socket;
        if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
            return false;
        }
        const QByteArray body = QJsonDocument(response.body).toJson(QJsonDocument::Compact);
        QByteArray bytes = "HTTP/1.1 " + QByteArray::number(response.status) +
                           " Test\r\n"
                           "Content-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                           QByteArray::number(body.size()) + "\r\n";
        for (auto it = response.headers.cbegin(); it != response.headers.cend(); ++it) {
            bytes += it.key() + ": " + it.value() + "\r\n";
        }
        socket->write(bytes + "\r\n" + body);
        socket->disconnectFromHost();
        return true;
    }

    QByteArray tag;
    QJsonObject context;
    QJsonObject targetSnapshot;
    QJsonObject modelSnapshot;
    QJsonObject followingSnapshot;
    QByteArray deferredPath;
    QMap<QByteArray, HttpResponse> overrides;
    QList<HttpRequest> requests;
};

inline void configure(PixEagleClient& client, const CompanionServer& server, const QString& uid = AIRCRAFT_UID)
{
    client.setEndpoint(server.endpoint());
    client.setVehicleIdentity(7, uid, true);
    client.setEnabled(true);
}

inline bool signIn(PixEagleClient& client, const QString& password = QStringLiteral("test-only-password"))
{
    client.signIn(QStringLiteral("pilot"), password);
    return QTest::qWaitFor([&client]() { return client.authenticated() && !client.busy(); }, TestTimeout::mediumMs());
}

inline bool verify(PixEagleClient& client)
{
    client.verifyVehicle();
    return QTest::qWaitFor([&client]() { return !client.busy(); }, TestTimeout::mediumMs());
}

inline void setContextField(QJsonObject& context, const QString& object, const QString& field, const QJsonValue& value)
{
    if (object.isEmpty()) {
        context[field] = value;
    } else {
        QJsonObject nested = context.value(object).toObject();
        nested[field] = value;
        context[object] = nested;
    }
}
}  // namespace PixEagleTest
