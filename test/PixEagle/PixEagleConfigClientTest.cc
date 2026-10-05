#include "PixEagleConfigClientTest.h"

#include <QtTest/QSignalSpy>

#include "PixEagleTestServer.h"

using namespace PixEagleTest;

namespace {
const QByteArray CONFIG_PATH = "/api/v1/integration/config";
const QByteArray OSD_PATH = "/api/v1/actions/osd-set";
const QByteArray APPLY_PATH = "/api/v1/actions/config-apply";

QJsonObject configState(const QJsonObject& context)
{
    return {
        {"schema_version", 1},
        {"source", "native_config_state"},
        {"instance_id", context.value("instance_id")},
        {"runtime_id", context.value("runtime_id")},
        {"config_generation", QString(64, 'a')},
        {"pending", false},
        {"pending_changes", QJsonArray{}},
        {"osd", QJsonObject{{"available", true},
                            {"can_set", true},
                            {"saved_enabled", true},
                            {"running_enabled", true},
                            {"unavailable_reason", QJsonValue::Null},
                            {"scope", "backend_overlay"}}},
        {"apply", QJsonObject{{"available", false}, {"reason", QJsonValue::Null}, {"reload_tier", QJsonValue::Null}}},
        {"system_restart", QJsonObject{{"available", false}, {"reason", "supervisor_not_verified"}}}};
}

struct ConfigHarness
{
    ConfigHarness()
        : client(nullptr, false)
    {
        advertiseTargets(server.context, true);
        auto caps = server.context.value("capabilities").toArray();
        caps.append("config.operations.v1");
        server.context.insert("capabilities", caps);
        auto permissions = server.context.value("permissions").toObject();
        auto scopes = permissions.value("scopes").toArray();
        scopes.append("config:read");
        scopes.append("config:write");
        scopes.append("control:write");
        permissions.insert("scopes", scopes);
        server.context.insert("permissions", permissions);
        state = configState(server.context);
    }

    void publish() { server.overrides.insert(CONFIG_PATH, {200, state, {}}); }

    bool start()
    {
        publish();
        if (!server.start()) {
            return false;
        }
        configure(client, server);
        client.setConfigRequested(true);
        return signIn(client) && QTest::qWaitFor([this]() { return client.configFresh(); }, TestTimeout::mediumMs());
    }

    CompanionServer server;
    PixEagleClient client;
    QJsonObject state;
};
}  // namespace

void PixEagleConfigClientTest::_readAndApply()
{
    ConfigHarness test;
    QVERIFY(test.start());
    QVERIFY(test.client.osdEnabled());
    QVERIFY(test.client.canSetOsd());
    QVERIFY(!test.client.canApplyConfig());
    test.server.deferredPath = OSD_PATH;
    const auto context = test.client.captureConfigContext();
    QVERIFY(test.client.setOsdEnabled(false, context));
    QVERIFY(test.client.configBusy());
    QVERIFY(!test.client.configFresh());
    QVERIFY(!test.client.setOsdEnabled(true, context));
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(OSD_PATH).target.isEmpty(), TestTimeout::mediumMs());
    const auto request = test.server.lastRequest(OSD_PATH);
    QCOMPARE(request.headers.value("x-test-csrf"), QByteArray("csrf-first"));
    QCOMPARE(request.headers.value("cookie"), QByteArray("pixeagle_session=session-first"));
    const auto body = QJsonDocument::fromJson(request.body).object();
    QCOMPARE(body.value("enabled"), QJsonValue(false));
    QCOMPARE(body.value("confirm"), QJsonValue(true));
    QCOMPARE(body.value("runtime_id"), test.server.context.value("runtime_id"));
    QCOMPARE(body.value("config_generation"), QJsonValue(QString(64, 'a')));
    QVERIFY(!body.value("idempotency_key").toString().isEmpty());
    auto osd = test.state.value("osd").toObject();
    osd.insert("saved_enabled", false);
    osd.insert("running_enabled", false);
    test.state.insert("osd", osd);
    test.state.insert("config_generation", QString(64, 'b'));
    test.publish();
    QVERIFY(test.server.respond(test.server.lastRequestIndex(OSD_PATH),
                                {202,
                                 {{"status", "success"},
                                  {"action_type", "osd_set"},
                                  {"executed", true},
                                  {"idempotency_key", body.value("idempotency_key")}},
                                 {}}));
    QTRY_VERIFY_WITH_TIMEOUT(test.client.configFresh() && !test.client.configBusy(), TestTimeout::mediumMs());
    QVERIFY(!test.client.osdEnabled());
    QVERIFY(test.client.configActionError().isEmpty());
    test.state.insert("pending", true);
    test.state.insert("pending_changes",
                      QJsonArray{QJsonObject{{"path", "Tracking.TEST"}, {"reload_tier", "tracker_restart"}}});
    test.state.insert(
        "apply", QJsonObject{{"available", true}, {"reason", QJsonValue::Null}, {"reload_tier", "tracker_restart"}});
    test.publish();
    test.client.refreshConfig();
    QTRY_VERIFY_WITH_TIMEOUT(test.client.canApplyConfig(), TestTimeout::mediumMs());
    test.server.deferredPath = APPLY_PATH;
    QVERIFY(test.client.applyConfig(test.client.captureConfigContext()));
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(APPLY_PATH).target.isEmpty(), TestTimeout::mediumMs());
    QCOMPARE(QJsonDocument::fromJson(test.server.lastRequest(APPLY_PATH).body).object().value("reload_tier"),
             QJsonValue("tracker_restart"));
}

void PixEagleConfigClientTest::_invalidState_data()
{
    QTest::addColumn<QString>("fault");
    for (const auto* fault : {"schema", "instance", "runtime", "generation", "missing-osd", "inconsistent-osd",
                              "pending", "tier", "overflow", "redirect", "forbidden", "session"}) {
        QTest::newRow(fault) << QString::fromLatin1(fault);
    }
}

void PixEagleConfigClientTest::_invalidState()
{
    QFETCH(QString, fault);
    ConfigHarness test;
    QVERIFY(test.start());
    test.client.setConfigRequested(false);
    if (fault == "schema") {
        test.state.insert("schema_version", 2);
    }
    if (fault == "instance" || fault == "runtime") {
        test.state.insert(fault + "_id", "other-owner");
    }
    if (fault == "generation") {
        test.state.insert("config_generation", "invalid");
    }
    if (fault == "missing-osd") {
        test.state.remove("osd");
    }
    if (fault == "inconsistent-osd") {
        auto osd = test.state.value("osd").toObject();
        osd.insert("running_enabled", QJsonValue::Null);
        test.state.insert("osd", osd);
    }
    if (fault == "pending") {
        test.state.insert("pending_changes",
                          QJsonArray{QJsonObject{{"path", "OSD.OSD_ENABLED"}, {"reload_tier", "immediate"}}});
    }
    if (fault == "tier") {
        test.state.insert("apply",
                          QJsonObject{{"available", true}, {"reason", QJsonValue::Null}, {"reload_tier", "arbitrary"}});
    }
    if (fault == "overflow") {
        test.state.insert("extra", QString(130 * 1024, 'x'));
    }
    CompanionServer destination("other");
    QVERIFY(destination.start());
    test.server.overrides.insert(CONFIG_PATH, {fault == "redirect"    ? 307
                                               : fault == "forbidden" ? 403
                                               : fault == "session"   ? 401
                                                                      : 200,
                                               test.state,
                                               {{"Location", destination.endpoint().toLatin1()}}});
    QSignalSpy changed(&test.client, &PixEagleClient::configChanged);
    test.client.refreshConfig();
    QTRY_VERIFY_WITH_TIMEOUT(!changed.isEmpty() && !test.client.configFresh(), TestTimeout::mediumMs());
    QVERIFY(!test.client.canSetOsd());
    QVERIFY(!test.client.canApplyConfig());
    QVERIFY(destination.requests.isEmpty());
    QCOMPARE(test.client.authenticated(), fault != "session");
}

void PixEagleConfigClientTest::_permissionAndCapabilityGates()
{
    ConfigHarness test;
    auto permissions = test.server.context.value("permissions").toObject();
    permissions.insert("scopes", QJsonArray{"status:read", "telemetry:read", "config:read"});
    test.server.context.insert("permissions", permissions);
    QVERIFY(test.start());
    QVERIFY(!test.client.canSetOsd());
    QVERIFY(!test.client.setOsdEnabled(false, test.client.captureConfigContext()));
    QVERIFY(test.server.lastRequest(OSD_PATH).target.isEmpty());
    test.server.context.insert("capabilities", QJsonArray{});
    test.client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.configAvailable(), TestTimeout::mediumMs());
}

void PixEagleConfigClientTest::_conflictAndStaleConfirmation()
{
    ConfigHarness test;
    QVERIFY(test.start());
    const auto captured = test.client.captureConfigContext();
    test.state.insert("config_generation", QString(64, 'b'));
    test.publish();
    test.client.refreshConfig();
    QTRY_VERIFY_WITH_TIMEOUT(test.client.captureConfigContext() != captured, TestTimeout::mediumMs());
    QVERIFY(!test.client.setOsdEnabled(false, captured));
    test.server.overrides.insert(OSD_PATH, {409, {}, {}});
    QVERIFY(test.client.setOsdEnabled(false, test.client.captureConfigContext()));
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.configActionError().isEmpty(), TestTimeout::mediumMs());
    QTRY_VERIFY_WITH_TIMEOUT(test.client.configFresh(), TestTimeout::mediumMs());
    QVERIFY(test.client.osdEnabled());
}

void PixEagleConfigClientTest::_lateReplyCannotCrossEndpoint()
{
    ConfigHarness test;
    QVERIFY(test.start());
    test.server.deferredPath = OSD_PATH;
    QVERIFY(test.client.setOsdEnabled(false, test.client.captureConfigContext()));
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(OSD_PATH).target.isEmpty(), TestTimeout::mediumMs());
    CompanionServer destination("other");
    QVERIFY(destination.start());
    test.client.setEndpoint(destination.endpoint());
    QVERIFY(!test.client.configFresh());
    QVERIFY(!test.client.configBusy());
    test.server.respond(test.server.lastRequestIndex(OSD_PATH), {200, {{"status", "success"}}, {}});
    QCoreApplication::processEvents();
    QVERIFY(!test.client.configFresh());
    QVERIFY(destination.requests.isEmpty());
}

void PixEagleConfigClientTest::_stateExpiresWithoutDemand()
{
    ConfigHarness test;
    QVERIFY(test.start());
    test.client.setConfigRequested(false);
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.configFresh(), TestTimeout::longMs());
    QVERIFY(!test.client.canSetOsd());
    QVERIFY(test.client.captureConfigContext().isEmpty());
}

void PixEagleConfigClientTest::_runtimeRestartInvalidatesState()
{
    ConfigHarness test;
    QVERIFY(test.start());
    test.client.setConfigRequested(false);
    const auto captured = test.client.captureConfigContext();
    test.server.context.insert("runtime_id", "restarted-runtime");
    test.client.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(test.client.connectionContext().value("runtime_id").toString(),
                              QStringLiteral("restarted-runtime"), TestTimeout::mediumMs());
    QVERIFY(!test.client.configFresh());
    QVERIFY(!test.client.setOsdEnabled(false, captured));
    QVERIFY(test.server.lastRequest(OSD_PATH).target.isEmpty());
}

void PixEagleConfigClientTest::_restartRequirementIsReadOnly()
{
    ConfigHarness test;
    // Definition changes can require restart without individual saved-value differences.
    test.state.insert("pending", true);
    test.state.insert(
        "apply",
        QJsonObject{{"available", false}, {"reason", "supervisor_not_verified"}, {"reload_tier", "system_restart"}});
    QVERIFY(test.start());
    QVERIFY(!test.client.canApplyConfig());
    QVERIFY(!test.client.applyConfig(test.client.captureConfigContext()));
    QVERIFY(test.server.lastRequest(APPLY_PATH).target.isEmpty());
    QVERIFY(test.client.configStatusText().contains("need a PixEagle restart"));
    QVERIFY(!test.client.canRestartBackend());
    QVERIFY(test.client.backendRestartStatus().contains("launcher"));
}

UT_REGISTER_TEST_LIGHTWEIGHT(PixEagleConfigClientTest, TestLabel::Unit)

void PixEagleConfigClientTest::_restartBackendIdentityAndRecovery()
{
    ConfigHarness test;
    auto caps = test.server.context.value("capabilities").toArray();
    caps.append("config.system_restart.v1");
    test.server.context.insert("capabilities", caps);
    auto permissions = test.server.context.value("permissions").toObject();
    auto scopes = permissions.value("scopes").toArray();
    scopes.append("system:admin");
    permissions.insert("scopes", scopes);
    test.server.context.insert("permissions", permissions);
    test.state.insert("system_restart", QJsonObject{{"available", true}, {"reason", "available"}});
    QVERIFY(test.start());
    QVERIFY(test.client.canRestartBackend());
    const auto captured = test.client.captureConfigContext();
    test.server.deferredPath = "/api/v1/actions/system-restart";
    QVERIFY(test.client.restartBackend(captured));
    QVERIFY(test.client.backendRestarting());
    QVERIFY(!test.client.canRestartBackend());
    QVERIFY(!test.client.restartBackend(captured));
    const QByteArray path = "/api/v1/actions/system-restart";
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(path).target.isEmpty(), TestTimeout::mediumMs());
    const auto body = QJsonDocument::fromJson(test.server.lastRequest(path).body).object();
    QCOMPARE(body.value("restart_context").toObject().value("runtime_id"), QJsonValue("runtime-a"));
    QCOMPARE(body.value("confirm"), QJsonValue(true));
    QVERIFY(
        test.server.respond(test.server.lastRequestIndex(path), {202,
                                                                 {{"status", "success"},
                                                                  {"action_type", "system_restart"},
                                                                  {"executed", true},
                                                                  {"idempotency_key", body.value("idempotency_key")}},
                                                                 {}}));
    QVERIFY(test.client.backendRestarting());
    test.server.context.insert("runtime_id", "runtime-b");
    test.state.insert("runtime_id", "runtime-b");
    test.publish();
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.backendRestarting(), TestTimeout::mediumMs());
    QVERIFY(test.client.backendRestartStatus().contains("restarted"));
    QVERIFY(test.client.configFresh());
}

void PixEagleConfigClientTest::_restartRefusalAndInstanceChange()
{
    for (const bool refused : {true, false}) {
        ConfigHarness test;
        auto caps = test.server.context.value("capabilities").toArray();
        caps.append("config.system_restart.v1");
        test.server.context.insert("capabilities", caps);
        auto permissions = test.server.context.value("permissions").toObject();
        auto scopes = permissions.value("scopes").toArray();
        scopes.append("system:admin");
        permissions.insert("scopes", scopes);
        test.server.context.insert("permissions", permissions);
        test.state.insert("system_restart", QJsonObject{{"available", true}, {"reason", "available"}});
        QVERIFY(test.start());
        const auto captured = test.client.captureConfigContext();
        const QByteArray path = "/api/v1/actions/system-restart";
        test.server.deferredPath = path;
        QVERIFY(test.client.restartBackend(captured));
        QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(path).target.isEmpty(), TestTimeout::mediumMs());
        if (refused) {
            QVERIFY(test.server.respond(test.server.lastRequestIndex(path), {409, {}, {}}));
            QTRY_VERIFY_WITH_TIMEOUT(!test.client.backendRestarting(), TestTimeout::mediumMs());
            QVERIFY(test.client.backendRestartStatus().contains("refused"));
        } else {
            QVERIFY(test.server.respond(test.server.lastRequestIndex(path), {503, {}, {}}));
            test.server.context.insert("instance_id", "different-installation");
            test.client.refresh();
            QTRY_VERIFY_WITH_TIMEOUT(!test.client.backendRestarting(), TestTimeout::mediumMs());
            QVERIFY(test.client.backendRestartStatus().contains("different PixEagle instance"));
            QVERIFY(!test.client.authenticated());
        }
    }
}
