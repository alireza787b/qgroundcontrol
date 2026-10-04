#include "PixEagleModelClientTest.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonDocument>
#include <QtTest/QSignalSpy>

#include "PixEagleTestServer.h"

using namespace PixEagleTest;

namespace {
struct ModelHarness
{
    ModelHarness()
        : client(nullptr, true)
    {
        advertiseTargets(server.context, true);
        advertiseModels(server.context);
    }

    bool start()
    {
        if (!server.start()) {
            return false;
        }
        configure(client, server);
        client.setModelsRequested(true);
        return signIn(client) &&
               QTest::qWaitFor([this]() { return client.modelSelectionAllowed(); }, TestTimeout::mediumMs());
    }

    bool select()
    {
        return client.submitModelSelection("detector-a", client.targetGuard(),
                                           client.modelInventory().value("model_generation").toString());
    }

    CompanionServer server;
    PixEagleClient client;
};

qsizetype requestCount(const CompanionServer& server, const QByteArray& path)
{
    qsizetype count = 0;
    for (const auto& request : server.requests) {
        count += request.target.endsWith(path) ? 1 : 0;
    }
    return count;
}

void removeScope(QJsonObject& context, const QString& scope)
{
    auto permissions = context.value("permissions").toObject();
    auto scopes = permissions.value("scopes").toArray();
    for (qsizetype index = scopes.size(); index > 0; --index) {
        if (scopes[index - 1] == scope) {
            scopes.removeAt(index - 1);
        }
    }
    permissions.insert("scopes", scopes);
    context.insert("permissions", permissions);
}
}  // namespace

void PixEagleModelClientTest::_inventoryDiscoveryAndPermissions_data()
{
    QTest::addColumn<QString>("gate");
    QTest::addColumn<bool>("readable");
    QTest::addColumn<bool>("selectable");
    QTest::newRow("operator") << QStringLiteral("operator") << true << true;
    QTest::newRow("viewer") << QStringLiteral("viewer") << true << false;
    QTest::newRow("model-specific-scope") << QStringLiteral("no-actions-scope") << true << true;
    QTest::newRow("missing-read-scope") << QStringLiteral("no-read-scope") << false << false;
    QTest::newRow("missing-capability") << QStringLiteral("no-capability") << false << false;
    QTest::newRow("unverified-aircraft") << QStringLiteral("unverified-aircraft") << false << false;
}

void PixEagleModelClientTest::_inventoryDiscoveryAndPermissions()
{
    QFETCH(QString, gate);
    QFETCH(bool, readable);
    QFETCH(bool, selectable);
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context, gate != "unverified-aircraft");
    advertiseModels(server.context);
    if (gate == "no-capability") {
        auto capabilities = server.context.value("capabilities").toArray();
        for (qsizetype index = capabilities.size(); index > 0; --index) {
            if (capabilities[index - 1] == "models.operations.v1") {
                capabilities.removeAt(index - 1);
            }
        }
        server.context.insert("capabilities", capabilities);
    } else if (gate == "no-read-scope") {
        removeScope(server.context, "models:read");
    } else if (gate == "viewer") {
        removeScope(server.context, "models:select");
    } else if (gate == "no-actions-scope") {
        removeScope(server.context, "actions:execute");
    }
    PixEagleClient client(nullptr, gate != "unverified-aircraft");
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(server.lastRequest(MODEL_INVENTORY_PATH).target.isEmpty());
    client.setModelsRequested(true);
    if (readable) {
        QTRY_VERIFY_WITH_TIMEOUT(client.modelStateFresh() && client.targetStateFresh(), TestTimeout::mediumMs());
        QCOMPARE(client.modelInventory().value("models").toArray().size(), 1);
        QCOMPARE(server.lastRequest(MODEL_INVENTORY_PATH).headers.value("cookie"),
                 QByteArray("pixeagle_session=session-first"));
    } else {
        QCoreApplication::processEvents();
        QVERIFY(server.lastRequest(MODEL_INVENTORY_PATH).target.isEmpty());
        QVERIFY(!client.modelStateFresh());
    }
    QCOMPARE(client.modelSelectionAllowed(), selectable);
    if (!selectable) {
        QVERIFY(!client.submitModelSelection("detector-a", client.targetGuard(), QString(64, 'a')));
        QVERIFY(server.lastRequest(MODEL_SELECT_PATH).target.isEmpty());
    }
}

void PixEagleModelClientTest::_invalidInventory_data()
{
    QTest::addColumn<QString>("fault");
    for (const auto* fault :
         {"schema", "instance", "runtime", "generation", "duplicate-id", "unsafe-id", "unknown-configured-id",
          "unknown-active-id", "too-many-models", "too-many-labels", "inconsistent-label-count", "label-type",
          "negative-size", "oversize", "redirect", "expired-session", "forbidden"}) {
        QTest::newRow(fault) << QString::fromLatin1(fault);
    }
}

void PixEagleModelClientTest::_invalidInventory()
{
    QFETCH(QString, fault);
    ModelHarness test;
    QVERIFY(test.start());
    test.client.setModelsRequested(false);
    auto inventory = test.client.modelInventory();
    auto rows = inventory.value("models").toArray();
    auto row = rows.first().toObject();
    if (fault == "schema") {
        inventory.insert("schema_version", 2);
    } else if (fault == "instance" || fault == "runtime") {
        inventory.insert(fault + "_id", "different-owner");
    } else if (fault == "generation") {
        inventory.insert("model_generation", "not-a-generation");
    } else if (fault == "duplicate-id") {
        rows.append(row);
    } else if (fault == "unsafe-id") {
        row.insert("model_id", "../other.pt");
    } else if (fault == "unknown-configured-id" || fault == "unknown-active-id") {
        inventory.insert(fault == "unknown-configured-id" ? "configured_model_id" : "active_model_id",
                         "unlisted-model");
    } else if (fault == "too-many-models") {
        for (int index = 1; index < 257; ++index) {
            auto another = row;
            another.insert("model_id", QStringLiteral("model-%1").arg(index));
            rows.append(another);
        }
    } else if (fault == "too-many-labels") {
        QJsonArray labels;
        for (int index = 0; index < 33; ++index) {
            labels.append(QStringLiteral("label-%1").arg(index));
        }
        row.insert("labels", labels);
        row.insert("total_labels", 33);
    } else if (fault == "inconsistent-label-count") {
        row.insert("total_labels", 3);
    } else if (fault == "label-type") {
        row.insert("labels", QJsonArray{false, "car"});
    } else if (fault == "negative-size") {
        row.insert("size_mb", -1);
    } else if (fault == "oversize") {
        inventory.insert("untrusted_extra", QString(129 * 1024, 'x'));
    }
    rows[0] = row;
    inventory.insert("models", rows);
    CompanionServer destination("other");
    QVERIFY(destination.start());
    const int status = fault == "expired-session" ? 401 : fault == "forbidden" ? 403 : fault == "redirect" ? 307 : 200;
    test.server.overrides.insert(MODEL_INVENTORY_PATH,
                                 {status, inventory, {{"Location", destination.endpoint().toLatin1()}}});
    QSignalSpy changed(&test.client, &PixEagleClient::modelsChanged);
    test.client.refreshModels();
    QTRY_VERIFY_WITH_TIMEOUT(!changed.isEmpty() && !test.client.modelStateFresh(), TestTimeout::mediumMs());
    QVERIFY(test.client.modelInventory().isEmpty());
    QVERIFY(!test.client.modelSelectionAllowed());
    QVERIFY(destination.requests.isEmpty());
    QCOMPARE(test.client.authenticated(), fault != "expired-session");
}

void PixEagleModelClientTest::_inventoryExpiresWithoutDemand()
{
    ModelHarness test;
    QVERIFY(test.start());
    test.client.setModelsRequested(false);
    const auto requests = requestCount(test.server, MODEL_INVENTORY_PATH);
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.modelStateFresh(), TestTimeout::longMs());
    QCOMPARE(requestCount(test.server, MODEL_INVENTORY_PATH), requests);
    QVERIFY(!test.client.modelSelectionAllowed());
    QVERIFY(test.client.authenticated());
    QVERIFY(!test.client.connectionContext().isEmpty());
}

void PixEagleModelClientTest::_inventoryDoesNotRewindTargetRevision()
{
    ModelHarness test;
    QVERIFY(test.start());
    test.client.setModelsRequested(false);
    test.server.deferredPath = MODEL_INVENTORY_PATH;
    const auto before = requestCount(test.server, MODEL_INVENTORY_PATH);
    test.client.refreshModels();
    QTRY_COMPARE_WITH_TIMEOUT(requestCount(test.server, MODEL_INVENTORY_PATH), before + 1, TestTimeout::mediumMs());
    const auto inventoryIndex = test.server.lastRequestIndex(MODEL_INVENTORY_PATH);
    test.server.targetSnapshot = nativeTargetState(test.server.context, "10");
    test.client.refreshTargetState();
    QTRY_COMPARE_WITH_TIMEOUT(test.client.targetState().value("target_revision").toString(), QStringLiteral("10"),
                              TestTimeout::mediumMs());
    QSignalSpy changed(&test.client, &PixEagleClient::modelsChanged);
    QVERIFY(test.server.respond(inventoryIndex, {200, nativeModelInventory(test.server.context, "9"), {}}));
    QTRY_VERIFY_WITH_TIMEOUT(!changed.isEmpty(), TestTimeout::mediumMs());
    QVERIFY(test.client.modelStateFresh());
    QCOMPARE(test.client.targetState().value("target_revision").toString(), QStringLiteral("10"));
}

void PixEagleModelClientTest::_selectionEnvelopeAndSerialization()
{
    ModelHarness test;
    QVERIFY(test.start());
    test.server.deferredPath = MODEL_SELECT_PATH;
    const auto guard = test.client.targetGuard();
    const auto generation = test.client.modelInventory().value("model_generation").toString();
    QSignalSpy finished(&test.client, &PixEagleClient::targetActionFinished);
    QVERIFY(test.select());
    QVERIFY(test.client.targetMutationPending());
    QVERIFY(!test.client.modelStateFresh());
    QVERIFY(!test.client.submitTargetAction("tracking_stop", {}, guard));
    QVERIFY(!test.client.submitModelSelection("detector-a", guard, generation));
    QTRY_COMPARE_WITH_TIMEOUT(requestCount(test.server, MODEL_SELECT_PATH), 1, TestTimeout::mediumMs());
    const auto request = test.server.lastRequest(MODEL_SELECT_PATH);
    const auto body = QJsonDocument::fromJson(request.body).object();
    QCOMPARE(request.headers.value("x-test-csrf"), QByteArray("csrf-first"));
    QCOMPARE(request.headers.value("cookie"), QByteArray("pixeagle_session=session-first"));
    QCOMPARE(body.value("model_id").toString(), QStringLiteral("detector-a"));
    QCOMPARE(body.value("model_generation").toString(), generation);
    QCOMPARE(body.value("device").toString(), QStringLiteral("auto"));
    QCOMPARE(body.value("source").toString(), QStringLiteral("qgroundcontrol"));
    QCOMPARE(body.value("confirm"), QJsonValue(true));
    QCOMPARE(body.value("dry_run"), QJsonValue(false));
    QVERIFY(!body.value("idempotency_key").toString().isEmpty());
    const auto native = body.value("native_context").toObject();
    QCOMPARE(native.value("binding_mode").toString(), QStringLiteral("companion_only"));
    auto expectedGuard = guard;
    expectedGuard.remove("_client_generation");
    expectedGuard.remove("_client_context");
    QCOMPARE(native.value("guard").toObject(), expectedGuard);
    const auto response = test.server.responseFor(request);
    test.server.modelSnapshot = response.body.value("result").toObject().value("model_inventory").toObject();
    test.server.targetSnapshot = nativeTargetState(test.server.context, "2");
    QVERIFY(test.server.respond(test.server.lastRequestIndex(MODEL_SELECT_PATH), response));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, TestTimeout::mediumMs());
    QCOMPARE(finished.first()[0].toString(), QStringLiteral("model_select"));
    QCOMPARE(finished.first()[1].toString(), QStringLiteral("accepted"));
    QTRY_VERIFY_WITH_TIMEOUT(test.client.modelSelectionAllowed(), TestTimeout::mediumMs());
    QCOMPARE(test.client.modelInventory().value("configured_model_id").toString(), QStringLiteral("detector-a"));

    test.server.deferredPath = "/api/v1/actions/tracking-stop";
    QVERIFY(test.client.submitTargetAction("tracking_stop", {}, test.client.targetGuard()));
    QVERIFY(!test.client.submitModelSelection("detector-a", guard, generation));
    QCOMPARE(requestCount(test.server, MODEL_SELECT_PATH), 1);
}

void PixEagleModelClientTest::_selectionGates_data()
{
    QTest::addColumn<QString>("gate");
    for (const auto* gate :
         {"missing-model", "unavailable-model", "unavailable-runtime", "stale-model-generation",
          "stale-target-revision", "following", "smart-target", "command-connected", "telemetry-connected"}) {
        QTest::newRow(gate) << QString::fromLatin1(gate);
    }
}

void PixEagleModelClientTest::_selectionGates()
{
    QFETCH(QString, gate);
    ModelHarness test;
    QVERIFY(test.start());
    test.client.setModelsRequested(false);
    const auto guard = test.client.targetGuard();
    const auto generation = test.client.modelInventory().value("model_generation").toString();
    if (gate == "unavailable-model" || gate == "unavailable-runtime") {
        test.server.modelSnapshot = test.client.modelInventory();
        if (gate == "unavailable-runtime") {
            test.server.modelSnapshot.insert("available", false);
        } else {
            auto rows = test.server.modelSnapshot.value("models").toArray();
            auto row = rows.first().toObject();
            row.insert("available", false);
            rows[0] = row;
            test.server.modelSnapshot.insert("models", rows);
        }
        QSignalSpy changed(&test.client, &PixEagleClient::modelsChanged);
        test.client.refreshModels();
        QTRY_VERIFY_WITH_TIMEOUT(!changed.isEmpty(), TestTimeout::mediumMs());
    } else if (gate == "following" || gate == "stale-target-revision" || gate == "smart-target") {
        test.server.targetSnapshot = nativeTargetState(test.server.context, "2");
        test.server.targetSnapshot.insert("following_active", gate == "following");
        if (gate == "smart-target") {
            test.server.targetSnapshot.insert("mode", "smart");
            test.server.targetSnapshot.insert("tracking_active", true);
            setContextField(test.server.targetSnapshot, "guard", "mode", "smart");
        }
        test.client.refreshTargetState();
        QTRY_COMPARE_WITH_TIMEOUT(test.client.targetState().value("target_revision").toString(), QStringLiteral("2"),
                                  TestTimeout::mediumMs());
    } else if (gate == "command-connected" || gate == "telemetry-connected") {
        setContextField(test.server.context, gate.section('-', 0, 0), "connected", true);
        test.client.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(test.client.connectionContext(), test.server.context, TestTimeout::mediumMs());
    }
    const auto submittedGuard = gate == "smart-target" || gate == "following" ? test.client.targetGuard() : guard;
    QVERIFY(!test.client.submitModelSelection(gate == "missing-model" ? "missing" : "detector-a", submittedGuard,
                                              gate == "stale-model-generation" ? QString(64, 'b') : generation));
    QVERIFY(test.server.lastRequest(MODEL_SELECT_PATH).target.isEmpty());
}

void PixEagleModelClientTest::_selectionOutcomes_data()
{
    QTest::addColumn<QString>("fault");
    QTest::addColumn<QString>("outcome");
    for (const auto* fault :
         {"wrong-model", "wrong-configured-model", "wrong-active-model", "mismatched-inventory-target", "wrong-key",
          "wrong-action", "missing-target", "wrong-revision", "bad-inventory", "wrong-selection-status", "redirect",
          "disconnect", "oversize"}) {
        QTest::newRow(fault) << QString::fromLatin1(fault) << QStringLiteral("unknown");
    }
    QTest::newRow("conflict") << QStringLiteral("conflict") << QStringLiteral("conflict");
    QTest::newRow("forbidden") << QStringLiteral("forbidden") << QStringLiteral("rejected");
    QTest::newRow("expired-session") << QStringLiteral("expired-session") << QStringLiteral("rejected");
    QTest::newRow("failed-action") << QStringLiteral("failed-action") << QStringLiteral("rejected");
}

void PixEagleModelClientTest::_selectionOutcomes()
{
    QFETCH(QString, fault);
    QFETCH(QString, outcome);
    ModelHarness test;
    QVERIFY(test.start());
    test.server.deferredPath = MODEL_SELECT_PATH;
    QSignalSpy finished(&test.client, &PixEagleClient::targetActionFinished);
    QVERIFY(test.select());
    QTRY_COMPARE_WITH_TIMEOUT(requestCount(test.server, MODEL_SELECT_PATH), 1, TestTimeout::mediumMs());
    const auto index = test.server.lastRequestIndex(MODEL_SELECT_PATH);
    auto response = test.server.responseFor(test.server.requests[index]);
    auto result = response.body.value("result").toObject();
    CompanionServer destination("other");
    QVERIFY(destination.start());
    if (fault == "wrong-model") {
        result.insert("model_id", "detector-b");
    } else if (fault == "wrong-configured-model") {
        setContextField(result, "model_inventory", "configured_model_id", QJsonValue::Null);
    } else if (fault == "wrong-active-model") {
        result.insert("selection_status", "active");
    } else if (fault == "mismatched-inventory-target") {
        setContextField(result, "model_inventory", "target_state", nativeTargetState(test.server.context, "3"));
    } else if (fault == "wrong-key") {
        response.body.insert("idempotency_key", "other-action");
    } else if (fault == "wrong-action") {
        response.body.insert("action_type", "tracking_stop");
    } else if (fault == "missing-target") {
        result.remove("target_state");
    } else if (fault == "wrong-revision") {
        result.insert("native_target_revision", "99");
    } else if (fault == "bad-inventory") {
        result.insert("model_inventory", QJsonObject{});
    } else if (fault == "wrong-selection-status") {
        result.insert("selection_status", "loading");
    } else if (fault == "oversize") {
        response.body.insert("untrusted_extra", QString(129 * 1024, 'x'));
    } else if (fault == "conflict" || fault == "forbidden" || fault == "expired-session" || fault == "redirect") {
        response.status = fault == "conflict"          ? 409
                          : fault == "forbidden"       ? 403
                          : fault == "expired-session" ? 401
                                                       : 307;
        response.headers.insert("Location", destination.endpoint().toLatin1());
    } else if (fault == "failed-action") {
        response.body.insert("status", "failure");
    }
    response.body.insert("result", result);
    response.body.insert("detail", "untrusted error text");
    if (fault == "disconnect") {
        test.server.requests[index].socket->disconnectFromHost();
    } else {
        QVERIFY(test.server.respond(index, response));
    }
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, TestTimeout::mediumMs());
    QCOMPARE(finished.first()[1].toString(), outcome);
    QVERIFY(!finished.first()[2].toString().contains("untrusted"));
    QVERIFY(!test.client.targetMutationPending());
    QCOMPARE(test.client.authenticated(), fault != "expired-session");
    if (test.client.authenticated()) {
        QTRY_VERIFY_WITH_TIMEOUT(test.client.modelStateFresh(), TestTimeout::mediumMs());
    }
    QCOMPARE(requestCount(test.server, MODEL_SELECT_PATH), 1);
    QVERIFY(destination.requests.isEmpty());
}

void PixEagleModelClientTest::_longSelectionKeepsConnectionFresh()
{
    ModelHarness test;
    QVERIFY(test.start());
    test.server.deferredPath = MODEL_SELECT_PATH;
    QSignalSpy finished(&test.client, &PixEagleClient::targetActionFinished);
    const auto contextRequests = requestCount(test.server, CONTEXT_PATH);
    QElapsedTimer duration;
    duration.start();
    QVERIFY(test.select());
    // Pass the six-second context expiry and the shorter target-action timeout.
    QTRY_VERIFY_WITH_TIMEOUT(duration.elapsed() > 6500, TestTimeout::longMs());
    QVERIFY(test.client.targetMutationPending());
    QVERIFY(finished.isEmpty());
    QVERIFY(!test.client.connectionContext().isEmpty());
    QVERIFY(requestCount(test.server, CONTEXT_PATH) > contextRequests + 1);
    const auto index = test.server.lastRequestIndex(MODEL_SELECT_PATH);
    QVERIFY(test.server.respond(index, test.server.responseFor(test.server.requests[index])));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, TestTimeout::mediumMs());
    QCOMPARE(finished.first()[1].toString(), QStringLiteral("accepted"));
    QCOMPARE(requestCount(test.server, MODEL_SELECT_PATH), 1);
}

void PixEagleModelClientTest::_lateReplyCannotCrossContext_data()
{
    QTest::addColumn<bool>("selection");
    QTest::addColumn<QString>("change");
    for (const bool selection : {false, true}) {
        for (const auto* change : {"endpoint", "runtime", "permissions", "capabilities", "disabled"}) {
            const auto name = QStringLiteral("%1-%2").arg(selection ? "selection" : "inventory", change);
            QTest::newRow(qPrintable(name)) << selection << QString::fromLatin1(change);
        }
    }
}

void PixEagleModelClientTest::_lateReplyCannotCrossContext()
{
    QFETCH(bool, selection);
    QFETCH(QString, change);
    ModelHarness test;
    CompanionServer second("second");
    QVERIFY(second.start());
    advertiseTargets(second.context, true);
    advertiseModels(second.context);
    second.context.insert("instance_id", "second-instance");
    QVERIFY(test.start());
    test.client.setModelsRequested(false);
    const auto oldGuard = test.client.targetGuard();
    const QByteArray path = selection ? MODEL_SELECT_PATH : MODEL_INVENTORY_PATH;
    test.server.deferredPath = path;
    const auto count = requestCount(test.server, path);
    QSignalSpy finished(&test.client, &PixEagleClient::targetActionFinished);
    if (selection) {
        QVERIFY(test.select());
    } else {
        test.client.refreshModels();
    }
    QTRY_COMPARE_WITH_TIMEOUT(requestCount(test.server, path), count + 1, TestTimeout::mediumMs());
    const auto index = test.server.lastRequestIndex(path);
    const auto response = test.server.responseFor(test.server.requests[index]);
    if (change == "endpoint") {
        test.client.setEndpoint(second.endpoint());
        QVERIFY(signIn(test.client));
    } else if (change == "disabled") {
        test.client.setEnabled(false);
    } else {
        if (change == "runtime") {
            test.server.context.insert("runtime_id", "runtime-b");
        } else if (change == "capabilities") {
            test.server.context.insert("capabilities", QJsonArray{"target.operations.v1"});
        } else {
            removeScope(test.server.context, "models:select");
        }
        test.client.refresh();
        QTRY_COMPARE_WITH_TIMEOUT(test.client.connectionContext(), test.server.context, TestTimeout::mediumMs());
    }
    QVERIFY(test.client.modelInventory().isEmpty());
    QVERIFY(!test.client.submitModelSelection("detector-a", oldGuard, QString(64, 'a')));
    if (selection) {
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first()[1].toString(), QStringLiteral("unknown"));
    }
    test.server.respond(index, response);
    QCoreApplication::processEvents();
    QVERIFY(test.client.modelInventory().isEmpty());
    QVERIFY(!test.client.targetMutationPending());
    QCOMPARE(finished.count(), selection ? 1 : 0);
    if (change == "endpoint") {
        test.client.refreshModels();
        QTRY_VERIFY_WITH_TIMEOUT(test.client.modelStateFresh(), TestTimeout::mediumMs());
        QCOMPARE(test.client.modelInventory().value("instance_id").toString(), QStringLiteral("second-instance"));
        QCOMPARE(second.lastRequest(MODEL_INVENTORY_PATH).headers.value("cookie"),
                 QByteArray("pixeagle_session=session-second"));
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(PixEagleModelClientTest, TestLabel::Unit)
