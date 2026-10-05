#include "PixEagleClientTest.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslServer>
#include <QtNetwork/QSslSocket>
#include <QtTest/QSignalSpy>

#include "GPS/NTRIP/NTRIPTlsTestFixtures.h"
#include "PixEagleTestServer.h"

using namespace PixEagleTest;

void PixEagleClientTest::_followingRequiresAirborneVehicle()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    advertiseFollowing(server.context);
    server.targetSnapshot = nativeTargetState(server.context);
    server.targetSnapshot.insert("tracking_active", true);
    server.targetSnapshot.insert("target_status", "tracking");
    server.followingSnapshot = nativeFollowingStatus(server.context, true);
    server.followingSnapshot.insert("start_allowed", false);
    server.followingSnapshot.insert("start_reason_codes", QJsonArray{"vehicle_not_airborne"});

    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QTRY_COMPARE_WITH_TIMEOUT(client.followingStatusText(), QStringLiteral("Take off before following"),
                              TestTimeout::mediumMs());
    QVERIFY(!client.canStartFollowing());
    QCOMPARE(client.followerChoices().size(), 1);

    server.followingSnapshot.insert("start_allowed", true);
    server.followingSnapshot.insert("start_reason_codes", QJsonArray{});
    client.refreshFollowing();
    QTRY_VERIFY_WITH_TIMEOUT(client.canStartFollowing(), TestTimeout::mediumMs());
}

void PixEagleClientTest::_emptyEndpointUsesLocalDefault()
{
    PixEagleClient client;
    QCOMPARE(client.endpoint(), QStringLiteral("http://127.0.0.1:5077"));
    client.setEndpoint("https://companion.example");
    client.setEndpoint("   ");
    QCOMPARE(client.endpoint(), client.defaultEndpoint());
    client.setEndpoint("https://companion.example");
    client.setEnabled(true);
    client.signInAt("", "", "");
    QCOMPARE(client.endpoint(), client.defaultEndpoint());
    QVERIFY(!client.busy());
    QVERIFY(client.statusText().contains("username"));
}

void PixEagleClientTest::_endpointPolicy_data()
{
    QTest::addColumn<QString>("endpoint");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("https") << QStringLiteral("https://companion.example/pixeagle-api/") << true;
    QTest::newRow("ipv4-loopback") << QStringLiteral("http://127.0.0.1:5077") << true;
    QTest::newRow("ipv6-loopback") << QStringLiteral("http://[::1]:5077") << true;
    QTest::newRow("localhost") << QStringLiteral("http://localhost:5077") << true;
    QTest::newRow("remote-http-bench") << QStringLiteral("http://192.0.2.1:5077") << true;
    QTest::newRow("remote-hostname") << QStringLiteral("http://companion.example:5077") << true;
    QTest::newRow("userinfo") << QStringLiteral("https://pilot:password@companion.example") << false;
    QTest::newRow("query-secret") << QStringLiteral("https://companion.example/?token=secret") << false;
    QTest::newRow("fragment") << QStringLiteral("https://companion.example/#secret") << false;
    QTest::newRow("parent-path") << QStringLiteral("https://companion.example/a/../api") << false;
    QTest::newRow("encoded-parent-path") << QStringLiteral("https://companion.example/a/%2e%2e/api") << false;
    QTest::newRow("backslash") << QStringLiteral("https://companion.example/a%5cb") << false;
    QTest::newRow("zero-port") << QStringLiteral("https://companion.example:0") << false;
    QTest::newRow("invalid-port") << QStringLiteral("https://companion.example:65536") << false;
    QTest::newRow("missing-host") << QStringLiteral("https:///api") << false;
    QTest::newRow("wrong-scheme") << QStringLiteral("file:///tmp/pixeagle") << false;
}

void PixEagleClientTest::_nativeFollowingUsesCapturedVehicleAndSurvivesVideoLoss()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    advertiseFollowing(server.context);
    server.targetSnapshot = nativeTargetState(server.context);
    server.targetSnapshot.insert("tracking_active", true);
    server.targetSnapshot.insert("target_status", "tracking");
    server.followingSnapshot = nativeFollowingStatus(server.context, true);
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.canStartFollowing(), TestTimeout::mediumMs());
    QCOMPARE(client.followingState(), QStringLiteral("ready"));
    QCOMPARE(client.followerChoices().size(), 1);
    const QString startContext = client.captureFollowingContext();
    QVERIFY(!startContext.isEmpty());
    server.deferredPath = FOLLOW_START_PATH;
    QVERIFY(client.startFollowing(startContext));
    QCOMPARE(client.followingState(), QStringLiteral("starting"));
    QTRY_VERIFY_WITH_TIMEOUT(server.lastRequestIndex(FOLLOW_START_PATH) >= 0, TestTimeout::mediumMs());
    const auto startRequest = server.lastRequest(FOLLOW_START_PATH);
    const auto startBody = QJsonDocument::fromJson(startRequest.body).object();
    QCOMPARE(startRequest.headers.value("x-test-csrf"), QByteArray("csrf-first"));
    QCOMPARE(startBody.value("profile_mode").toString(), QStringLiteral("mc_velocity_position"));
    QVERIFY(QRegularExpression("^[0-9a-f]{32}$").match(startBody.value("start_attempt_id").toString()).hasMatch());
    QCOMPARE(startBody.value("native_context").toObject().value("binding_mode").toString(), QStringLiteral("vehicle"));
    QCOMPARE(startBody.value("native_context").toObject().value("guard").toObject(),
             server.followingSnapshot.value("guard").toObject());
    QVERIFY(!client.startFollowing(startContext));
    server.followingSnapshot = nativeFollowingStatus(server.context, false, true);
    QVERIFY(server.respond(server.lastRequestIndex(FOLLOW_START_PATH), server.responseFor(startRequest)));
    QTRY_VERIFY_WITH_TIMEOUT(client.followingActive() && client.canStopFollowing(), TestTimeout::mediumMs());
    QCOMPARE(client.followingState(), QStringLiteral("active"));

    client.setVehicleIdentity(7, AIRCRAFT_UID, false);
    QVERIFY(!client.canStartFollowing());
    QVERIFY(client.canStopFollowing());
    const QString stopContext = client.captureFollowingStop();
    QVERIFY(!stopContext.isEmpty());
    server.deferredPath = FOLLOW_STOP_PATH;
    QVERIFY(client.stopFollowing(stopContext));
    QCOMPARE(client.followingState(), QStringLiteral("updating"));
    QTRY_VERIFY_WITH_TIMEOUT(server.lastRequestIndex(FOLLOW_STOP_PATH) >= 0, TestTimeout::mediumMs());
    const auto stopBody = QJsonDocument::fromJson(server.lastRequest(FOLLOW_STOP_PATH).body).object();
    QCOMPARE(stopBody.value("follow_session_id").toString(), QString(32, 'a'));
    QCOMPARE(stopBody.value("aircraft_uid").toString(), AIRCRAFT_UID);
    QVERIFY(!stopBody.contains("native_context"));
}

void PixEagleClientTest::_followingContinuityStatusIsVisible_data()
{
    QTest::addColumn<QString>("authority");
    QTest::addColumn<bool>("transitionPending");
    QTest::addColumn<bool>("preview");
    QTest::addColumn<QString>("state");
    QTest::addColumn<QString>("summary");
    for (bool preview : {false, true}) {
        const QByteArray prefix = preview ? "preview-" : "aircraft-";
        QTest::newRow((prefix + "loss").constData())
            << QStringLiteral("COASTING") << false << preview << QStringLiteral("coasting")
            << (preview ? QStringLiteral("Test: target lost") : QStringLiteral("Target lost"));
        QTest::newRow((prefix + "retarget").constData())
            << QStringLiteral("COASTING") << true << preview << QStringLiteral("retargeting")
            << (preview ? QStringLiteral("Test: changing target") : QStringLiteral("Changing target"));
        QTest::newRow((prefix + "reacquire").constData())
            << QStringLiteral("REACQUIRING") << false << preview << QStringLiteral("reacquiring")
            << (preview ? QStringLiteral("Test: reacquiring") : QStringLiteral("Reacquiring"));
        QTest::newRow((prefix + "stop").constData())
            << QStringLiteral("HANDOFF_PENDING") << true << preview << QStringLiteral("stopping")
            << (preview ? QStringLiteral("Stopping test") : QStringLiteral("Stopping"));
    }
}

void PixEagleClientTest::_followingContinuityStatusIsVisible()
{
    QFETCH(QString, authority);
    QFETCH(bool, transitionPending);
    QFETCH(bool, preview);
    QFETCH(QString, state);
    QFETCH(QString, summary);
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    advertiseFollowing(server.context);
    server.targetSnapshot = nativeTargetState(server.context);
    server.followingSnapshot = nativeFollowingStatus(server.context, false, true);
    server.followingSnapshot.insert("continuity_authority_state", authority);
    server.followingSnapshot.insert("continuity_target_transition_pending", transitionPending);
    server.followingSnapshot.insert("execution_mode", preview ? "COMMAND_PREVIEW" : "PX4");

    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QTRY_COMPARE_WITH_TIMEOUT(client.followingState(), state, TestTimeout::mediumMs());
    QCOMPARE(client.followingSummaryText(), summary);
    QVERIFY(!client.followingStatusText().isEmpty());
    QCOMPARE(client.followingStatusText().contains(QStringLiteral("no aircraft commands")), preview);
}

void PixEagleClientTest::_followingSimulationIsVisible()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    advertiseFollowing(server.context);
    server.targetSnapshot = nativeTargetState(server.context);
    server.followingSnapshot = nativeFollowingStatus(server.context, false, true);
    server.followingSnapshot.insert("sih_replay_authorized", true);
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QTRY_COMPARE_WITH_TIMEOUT(client.followingSummaryText(), QStringLiteral("SIH following"), TestTimeout::mediumMs());
    QVERIFY(client.followingStatusText().contains(QStringLiteral("SIH simulation")));
    QVERIFY(!client.followerTestActive());
    client.setEnabled(false);
    QVERIFY(!client.followingSummaryText().contains(QStringLiteral("SIH")));
}

void PixEagleClientTest::_lastFollowingHandoffIsAuthoritative()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    advertiseFollowing(server.context);
    server.targetSnapshot = nativeTargetState(server.context);
    server.followingSnapshot = nativeFollowingStatus(server.context);
    QJsonObject handoff{{"follow_session_id", QString(32, 'b')},
                        {"aircraft_uid", AIRCRAFT_UID},
                        {"reason_code", "maximum_coast_time_reached"},
                        {"result", "confirmed_hold"},
                        {"execution_mode", "PX4"}};
    server.followingSnapshot.insert("last_handoff", handoff);
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QTRY_COMPARE_WITH_TIMEOUT(client.followingSummaryText(), QStringLiteral("Hold"), TestTimeout::mediumMs());
    QCOMPARE(client.followingStatusText(), QStringLiteral("Aircraft Hold confirmed; target recovery budget exhausted"));

    handoff.insert("result", "failed");
    server.followingSnapshot.insert("last_handoff", handoff);
    client.refreshFollowing();
    QTRY_COMPARE_WITH_TIMEOUT(client.followingSummaryText(), QStringLiteral("Stop unconfirmed"),
                              TestTimeout::mediumMs());
    QVERIFY(client.followingStatusText().contains(QStringLiteral("Hold unconfirmed")));

    handoff.insert("result", "pending");
    server.followingSnapshot.insert("last_handoff", handoff);
    client.refreshFollowing();
    QTRY_COMPARE_WITH_TIMEOUT(client.followingState(), QStringLiteral("stopping"), TestTimeout::mediumMs());
    QCOMPARE(client.followingSummaryText(), QStringLiteral("Stopping"));

    handoff.insert("result", "confirmed_hold");
    handoff.insert("aircraft_uid", "different-aircraft");
    server.followingSnapshot.insert("last_handoff", handoff);
    client.refreshFollowing();
    QTRY_COMPARE_WITH_TIMEOUT(client.followingState(), QStringLiteral("idle"), TestTimeout::mediumMs());
    QCOMPARE(client.followingSummaryText(), QStringLiteral("Stopped"));
    QVERIFY(!client.followingStatusText().contains(QStringLiteral("Hold confirmed")));

    handoff.insert("aircraft_uid", AIRCRAFT_UID);
    handoff.insert("execution_mode", "COMMAND_PREVIEW");
    handoff.insert("result", "stopped");
    server.followingSnapshot.insert("last_handoff", handoff);
    client.refreshFollowing();
    QTRY_COMPARE_WITH_TIMEOUT(client.followingSummaryText(), QStringLiteral("Test stopped"), TestTimeout::mediumMs());
    QVERIFY(client.followingStatusText().contains(QStringLiteral("no aircraft commands")));

    handoff.insert("result", "confirmed_hold");
    server.followingSnapshot.insert("last_handoff", handoff);
    client.refreshFollowing();
    QTRY_COMPARE_WITH_TIMEOUT(client.followingSummaryText(), QStringLiteral("Stopped"), TestTimeout::mediumMs());
    QVERIFY(!client.followingStatusText().contains(QStringLiteral("Hold confirmed")));

    server.followingSnapshot = nativeFollowingStatus(server.context, false, true);
    client.refreshFollowing();
    QTRY_COMPARE_WITH_TIMEOUT(client.followingSummaryText(), QStringLiteral("Following"), TestTimeout::mediumMs());
    QVERIFY(!client.followingStatusText().contains(QStringLiteral("Hold confirmed")));
}

void PixEagleClientTest::_pendingNativeStartCanBeStopped()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    advertiseFollowing(server.context);
    server.targetSnapshot = nativeTargetState(server.context);
    server.targetSnapshot.insert("tracking_active", true);
    server.targetSnapshot.insert("target_status", "tracking");
    server.followingSnapshot = nativeFollowingStatus(server.context, true);
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.canStartFollowing(), TestTimeout::mediumMs());
    server.deferredPath = FOLLOW_START_PATH;
    QVERIFY(client.startFollowing(client.captureFollowingContext()));
    QTRY_VERIFY_WITH_TIMEOUT(server.lastRequestIndex(FOLLOW_START_PATH) >= 0, TestTimeout::mediumMs());
    const auto body = QJsonDocument::fromJson(server.lastRequest(FOLLOW_START_PATH).body).object();
    const QString attemptId = body.value("start_attempt_id").toString();
    QVERIFY(!attemptId.isEmpty());
    server.followingSnapshot.insert("pending_start_id", attemptId);
    server.followingSnapshot.insert("pending_aircraft_uid", AIRCRAFT_UID);
    server.followingSnapshot.insert("stop_allowed", true);
    client.refreshFollowing();
    QTRY_VERIFY_WITH_TIMEOUT(client.canStopFollowing(), TestTimeout::mediumMs());
    QCOMPARE(client.followingState(), QStringLiteral("starting"));
    QVERIFY(client.stopFollowing(client.captureFollowingStop()));
    QTRY_VERIFY_WITH_TIMEOUT(server.lastRequestIndex(FOLLOW_STOP_PATH) >= 0, TestTimeout::mediumMs());
    const auto stopBody = QJsonDocument::fromJson(server.lastRequest(FOLLOW_STOP_PATH).body).object();
    QCOMPARE(stopBody.value("follow_session_id").toString(), attemptId);
    QCOMPARE(stopBody.value("aircraft_uid").toString(), AIRCRAFT_UID);
}

void PixEagleClientTest::_followingStateExpiresAndRecovers()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    advertiseFollowing(server.context);
    server.followingSnapshot = nativeFollowingStatus(server.context, false, true);
    PixEagleClient client;
    QCOMPARE(client.followingState(), QStringLiteral("unknown"));
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QTRY_COMPARE_WITH_TIMEOUT(client.followingState(), QStringLiteral("active"), TestTimeout::mediumMs());

    server.deferredPath = FOLLOWING_PATH;
    client.refreshFollowing();
    QSignalSpy changed(&client, &PixEagleClient::followingChanged);
    QTRY_COMPARE_WITH_TIMEOUT(client.followingState(), QStringLiteral("unknown"), TestTimeout::longMs());
    QCOMPARE(client.followingSummaryText(), QStringLiteral("Unknown"));
    QVERIFY(!changed.isEmpty());
    QVERIFY(client.followingActive());
    QVERIFY(!client.canStartFollowing());
    QVERIFY(client.canStopFollowing());

    server.deferredPath.clear();
    client.refreshFollowing();
    QTRY_COMPARE_WITH_TIMEOUT(client.followingState(), QStringLiteral("active"), TestTimeout::mediumMs());
    QVERIFY(client.canStopFollowing());
}

void PixEagleClientTest::_followingStartRejectsChangedContext_data()
{
    QTest::addColumn<QString>("change");
    QTest::newRow("profile") << QStringLiteral("profile");
    QTest::newRow("target") << QStringLiteral("target");
}

void PixEagleClientTest::_followingStartRejectsChangedContext()
{
    QFETCH(QString, change);
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    advertiseFollowing(server.context);
    server.targetSnapshot = nativeTargetState(server.context);
    server.targetSnapshot.insert("tracking_active", true);
    server.targetSnapshot.insert("target_status", "tracking");
    server.followingSnapshot = nativeFollowingStatus(server.context, true);
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.canStartFollowing(), TestTimeout::mediumMs());
    const QString captured = client.captureFollowingContext();
    QVERIFY(!captured.isEmpty());

    if (change == "profile") {
        server.followingSnapshot.insert("profile_generation", QString(64, 'b'));
    } else {
        setContextField(server.targetSnapshot, "guard", "target_revision", "2");
        server.targetSnapshot.insert("target_revision", "2");
        server.followingSnapshot.insert("guard", server.targetSnapshot.value("guard"));
        client.refreshTargetState();
    }
    client.refreshFollowing();
    QTRY_VERIFY_WITH_TIMEOUT(client.canStartFollowing() && client.captureFollowingContext() != captured,
                             TestTimeout::mediumMs());
    QSignalSpy changed(&client, &PixEagleClient::followingChanged);
    QVERIFY(!client.startFollowing(captured));
    QVERIFY(!client.followingActionError().isEmpty());
    QCOMPARE(changed.count(), 1);
    QVERIFY(server.lastRequest(FOLLOW_START_PATH).target.isEmpty());
    QVERIFY(client.startFollowing(client.captureFollowingContext()));
    QVERIFY(client.followingActionError().isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(!server.lastRequest(FOLLOW_START_PATH).target.isEmpty(), TestTimeout::mediumMs());
}

void PixEagleClientTest::_followingActionErrorSurvivesPolling()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    advertiseFollowing(server.context);
    server.targetSnapshot = nativeTargetState(server.context);
    server.targetSnapshot.insert("tracking_active", true);
    server.targetSnapshot.insert("target_status", "tracking");
    server.followingSnapshot = nativeFollowingStatus(server.context, true);
    server.overrides.insert(FOLLOW_START_PATH, {403, {}, {}});
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.canStartFollowing(), TestTimeout::mediumMs());
    QVERIFY(client.startFollowing(client.captureFollowingContext()));
    QTRY_VERIFY_WITH_TIMEOUT(!client.followingActionPending() && !client.followingActionError().isEmpty(),
                             TestTimeout::mediumMs());
    const QString refusal = client.followingActionError();
    QTRY_VERIFY_WITH_TIMEOUT(client.canStartFollowing(), TestTimeout::mediumMs());
    QCOMPARE(client.followingActionError(), refusal);

    QSignalSpy changed(&client, &PixEagleClient::followingChanged);
    client.refreshFollowing();
    QTRY_VERIFY_WITH_TIMEOUT(!changed.isEmpty(), TestTimeout::mediumMs());
    QCOMPARE(client.followingActionError(), refusal);
    QCOMPARE(client.followingState(), QStringLiteral("ready"));

    server.overrides.remove(FOLLOW_START_PATH);
    QVERIFY(client.startFollowing(client.captureFollowingContext()));
    QVERIFY(client.followingActionError().isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(!client.followingActionPending(), TestTimeout::mediumMs());
    QVERIFY(client.followingActionError().isEmpty());

    QVERIFY(!client.stopFollowing(QStringLiteral("expired-session")));
    QVERIFY(!client.followingActionError().isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    client.signOut();
    QTRY_VERIFY_WITH_TIMEOUT(!client.authenticated() && !client.busy(), TestTimeout::mediumMs());
    QVERIFY(client.followingActionError().isEmpty());
}

void PixEagleClientTest::_endpointPolicy()
{
    QFETCH(QString, endpoint);
    QFETCH(bool, accepted);
    QCOMPARE(PixEagleClient::validateEndpoint(endpoint), accepted);
}

void PixEagleClientTest::_signInAtValidatesDestination_data()
{
    QTest::addColumn<QString>("address");
    QTest::addColumn<QString>("normalized");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("uppercase-scheme") << QStringLiteral("HTTP://127.0.0.1:%1/pixeagle-api///")
                                      << QStringLiteral("http://127.0.0.1:%1/pixeagle-api") << true;
    QTest::newRow("uppercase-host") << QStringLiteral("http://LOCALHOST:%1/pixeagle-api/")
                                    << QStringLiteral("http://localhost:%1/pixeagle-api") << true;
    QTest::newRow("invalid-new-address") << QStringLiteral("https://user:secret@companion.example") << QString()
                                         << false;
}

void PixEagleClientTest::_signInAtValidatesDestination()
{
    QFETCH(QString, address);
    QFETCH(QString, normalized);
    QFETCH(bool, accepted);
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client;
    configure(client, server);
    QSignalSpy incoming(&server, &QTcpServer::newConnection);
    client.signInAt(address.contains("%1") ? address.arg(server.serverPort()) : address, "pilot", "secret");
    if (accepted) {
        QTRY_VERIFY_WITH_TIMEOUT(client.authenticated() && !client.busy(), TestTimeout::mediumMs());
        QCOMPARE(client.endpoint(), normalized.arg(server.serverPort()));
        QCOMPARE(server.requests.first().target, "/pixeagle-api" + LOGIN_PATH);
    } else {
        QVERIFY_NO_SIGNAL_WAIT(incoming, TestTimeout::shortMs());
        QVERIFY(server.requests.isEmpty());
        QVERIFY(!client.authenticated());
        QCOMPARE(client.endpoint(), server.endpoint());
    }
}

void PixEagleClientTest::_csrfHeaderContract_data()
{
    QTest::addColumn<QString>("header");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("custom-x-header") << QStringLiteral("X-Custom-CSRF") << true;
    QTest::newRow("custom-non-x-header") << QStringLiteral("CSRFToken") << true;
    QTest::newRow("host-collision") << QStringLiteral("Host") << false;
    QTest::newRow("cookie-collision") << QStringLiteral("Cookie") << false;
    QTest::newRow("auth-collision") << QStringLiteral("Authorization") << false;
    QTest::newRow("entity-collision") << QStringLiteral("Content-Length") << false;
    QTest::newRow("header-injection") << QStringLiteral("X-CSRF\r\nInjected: value") << false;
}

void PixEagleClientTest::_csrfHeaderContract()
{
    QFETCH(QString, header);
    QFETCH(bool, accepted);
    CompanionServer server;
    QVERIFY(server.start());
    HttpRequest login;
    login.target = LOGIN_PATH;
    HttpResponse response = server.responseFor(login);
    response.body["csrf_header_name"] = header;
    server.overrides[LOGIN_PATH] = response;
    PixEagleClient client;
    configure(client, server);
    client.signIn("pilot", "password");
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QCOMPARE(client.authenticated(), accepted);
    if (accepted) {
        QVERIFY(verify(client));
        QVERIFY(client.associationVerified());
        QCOMPARE(server.lastRequest(VERIFY_PATH).headers.value(header.toLatin1().toLower()), QByteArray("csrf-first"));
    } else {
        QCOMPARE(server.requests.size(), 1);
        QVERIFY(!client.associationVerified());
    }
}

void PixEagleClientTest::_disabledMakesNoRequests()
{
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client;
    client.setEndpoint(server.endpoint());
    client.setVehicleIdentity(7, AIRCRAFT_UID, true);
    QSignalSpy incoming(&server, &QTcpServer::newConnection);
    client.signIn("pilot", "password");
    client.refresh();
    client.verifyVehicle();
    QVERIFY_NO_SIGNAL_WAIT(incoming, TestTimeout::shortMs());
    QVERIFY(server.requests.isEmpty());
    QVERIFY(!client.authenticated());
    QVERIFY(!client.associationVerified());
}

void PixEagleClientTest::_loginRequiresExplicitVerification()
{
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QCOMPARE(client.signedInAs(), QStringLiteral("pilot"));
    QVERIFY(!client.associationVerified());
    QCOMPARE(server.requests[0].method, QByteArray("POST"));
    QCOMPARE(server.requests[0].target, LOGIN_PATH);
    QCOMPARE(QJsonDocument::fromJson(server.requests[0].body).object().value("username").toString(), QString("pilot"));
    QCOMPARE(server.requests[1].method, QByteArray("GET"));
    QCOMPARE(server.requests[1].target, CONTEXT_PATH);
    QVERIFY(verify(client));
    QVERIFY(client.associationVerified());
    QCOMPARE(client.aircraftUid(), AIRCRAFT_UID);
    QCOMPARE(server.lastRequest(VERIFY_PATH).target, VERIFY_PATH);
    QCOMPARE(server.lastRequest(VERIFY_PATH).method, QByteArray("POST"));
    QCOMPARE(server.lastRequest(VERIFY_PATH).headers.value("x-test-csrf"), QByteArray("csrf-first"));
    QCOMPARE(server.requests.last().target, CONTEXT_PATH);
    QCOMPARE(server.requests.last().method, QByteArray("GET"));
    QCOMPARE(server.requests.last().headers.value("cookie"), QByteArray("pixeagle_session=session-first"));
}

void PixEagleClientTest::_matchingSingleVehicleAutoVerification()
{
    CompanionServer server;
    QVERIFY(server.start());
    auto association = server.context.value("association").toObject();
    association.insert("verified", false);
    server.context.insert("association", association);
    server.deferredPath = VERIFY_PATH;
    PixEagleClient client;
    configure(client, server);
    client.setAutoVerifySingleVehicle(true);
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(server.lastRequestIndex(VERIFY_PATH) >= 0, TestTimeout::mediumMs());
    server.context.insert("association", QJsonObject{{"verified", true}});
    const auto verifyRequest = server.lastRequest(VERIFY_PATH);
    QVERIFY(server.respond(server.lastRequestIndex(VERIFY_PATH), server.responseFor(verifyRequest)));
    QTRY_VERIFY_WITH_TIMEOUT(client.associationVerified(), TestTimeout::mediumMs());
    QCOMPARE(server.lastRequest(VERIFY_PATH).target, VERIFY_PATH);

    PixEagleClient mismatched;
    configure(mismatched, server, QStringLiteral("999"));
    mismatched.setAutoVerifySingleVehicle(true);
    QVERIFY(signIn(mismatched));
    QVERIFY(!mismatched.associationVerified());
    const int requestCount = server.requests.size();
    mismatched.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(server.requests.size() > requestCount, TestTimeout::mediumMs());
    QCOMPARE(server.lastRequest(VERIFY_PATH).target, VERIFY_PATH);
}

void PixEagleClientTest::_autoVerificationStartsWhenSingleVehicleIsLearnedAfterLogin()
{
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(!client.associationVerified());
    QVERIFY(server.lastRequestIndex(VERIFY_PATH) < 0);

    // Vehicle-count information can arrive after login/context discovery. The
    // manager must be able to turn on automatic verification without requiring
    // another sign-in or a manual Verify button press.
    client.setAutoVerifySingleVehicle(true);
    QTRY_VERIFY_WITH_TIMEOUT(server.lastRequestIndex(VERIFY_PATH) >= 0, TestTimeout::mediumMs());
    QTRY_VERIFY_WITH_TIMEOUT(client.associationVerified(), TestTimeout::mediumMs());
}

void PixEagleClientTest::_autoVerificationDiscoversDisconnectedBackend_data()
{
    QTest::addColumn<QString>("commandUid");
    QTest::addColumn<QString>("telemetryUid");
    QTest::addColumn<bool>("discover");
    QTest::newRow("command-not-yet-discovered") << QString() << AIRCRAFT_UID << true;
    QTest::newRow("both-not-yet-discovered") << QString() << QString() << true;
    QTest::newRow("wrong-command-aircraft") << QStringLiteral("999") << AIRCRAFT_UID << false;
    QTest::newRow("wrong-telemetry-aircraft") << QString() << QStringLiteral("999") << false;
    QTest::newRow("invalid-known-command-identity") << QStringLiteral("0") << AIRCRAFT_UID << false;
}

void PixEagleClientTest::_autoVerificationDiscoversDisconnectedBackend()
{
    QFETCH(QString, commandUid);
    QFETCH(QString, telemetryUid);
    QFETCH(bool, discover);
    CompanionServer server;
    QVERIFY(server.start());
    const QJsonObject ready = server.context;
    setContextField(server.context, "command", "connected", false);
    setContextField(server.context, "command", "autopilot_uid", commandUid);
    setContextField(server.context, "telemetry", "autopilot_uid", telemetryUid);
    setContextField(server.context, "association", "verified", false);
    setContextField(server.context, "readiness", "connection_ready", false);
    server.deferredPath = VERIFY_PATH;
    PixEagleClient client;
    configure(client, server);
    client.setAutoVerifySingleVehicle(true);
    client.signIn("pilot", "secret");
    if (discover) {
        QTRY_VERIFY_WITH_TIMEOUT(server.lastRequestIndex(VERIFY_PATH) >= 0, TestTimeout::mediumMs());
        QVERIFY(!client.associationVerified());
        server.context = ready;
        QVERIFY(
            server.respond(server.lastRequestIndex(VERIFY_PATH), server.responseFor(server.lastRequest(VERIFY_PATH))));
        QTRY_VERIFY_WITH_TIMEOUT(client.associationVerified(), TestTimeout::mediumMs());
    } else {
        QTRY_VERIFY_WITH_TIMEOUT(client.authenticated() && !client.connectionContext().isEmpty() && !client.busy(),
                                 TestTimeout::mediumMs());
        client.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
        QVERIFY(server.lastRequestIndex(VERIFY_PATH) < 0);
        QVERIFY(!client.associationVerified());
    }
}

void PixEagleClientTest::_identityValidation_data()
{
    QTest::addColumn<QString>("qgcUid");
    QTest::addColumn<QString>("object");
    QTest::addColumn<QString>("field");
    QTest::addColumn<QJsonValue>("value");
    QTest::newRow("different-command") << AIRCRAFT_UID << QStringLiteral("command") << QStringLiteral("autopilot_uid")
                                       << QJsonValue("41");
    QTest::newRow("different-telemetry") << AIRCRAFT_UID << QStringLiteral("telemetry")
                                         << QStringLiteral("autopilot_uid") << QJsonValue("41");
    QTest::newRow("missing-qgc-uid") << QString() << QStringLiteral("command") << QStringLiteral("autopilot_uid")
                                     << QJsonValue(AIRCRAFT_UID);
    QTest::newRow("zero-qgc-uid") << QString("0") << QStringLiteral("command") << QStringLiteral("autopilot_uid")
                                  << QJsonValue("0");
    QTest::newRow("missing-command-uid") << AIRCRAFT_UID << QStringLiteral("command") << QStringLiteral("autopilot_uid")
                                         << QJsonValue(QJsonValue::Null);
    QTest::newRow("numeric-command-uid") << QString("42") << QStringLiteral("command")
                                         << QStringLiteral("autopilot_uid") << QJsonValue(42);
    QTest::newRow("noncanonical-command-uid")
        << AIRCRAFT_UID << QStringLiteral("command") << QStringLiteral("autopilot_uid") << QJsonValue("042");
    QTest::newRow("overflow-command-uid") << AIRCRAFT_UID << QStringLiteral("command")
                                          << QStringLiteral("autopilot_uid") << QJsonValue("18446744073709551616");
    QTest::newRow("wrong-system") << AIRCRAFT_UID << QStringLiteral("telemetry") << QStringLiteral("system_id")
                                  << QJsonValue(8);
    QTest::newRow("wrong-command-system")
        << AIRCRAFT_UID << QStringLiteral("command") << QStringLiteral("system_id") << QJsonValue(8);
}

void PixEagleClientTest::_identityValidation()
{
    QFETCH(QString, qgcUid);
    QFETCH(QString, object);
    QFETCH(QString, field);
    QFETCH(QJsonValue, value);
    CompanionServer server;
    QVERIFY(server.start());
    server.context = aircraftContext(qgcUid);
    setContextField(server.context, object, field, value);
    PixEagleClient client;
    configure(client, server, qgcUid);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QVERIFY(!client.associationVerified());
}

void PixEagleClientTest::_contextChangesInvalidateVerification_data()
{
    QTest::addColumn<QString>("object");
    QTest::addColumn<QString>("field");
    QTest::addColumn<QJsonValue>("value");
    QTest::newRow("runtime") << QStringLiteral("") << QStringLiteral("runtime_id") << QJsonValue("runtime-b");
    QTest::newRow("instance") << QStringLiteral("") << QStringLiteral("instance_id") << QJsonValue("companion-b");
    QTest::newRow("command-generation") << QStringLiteral("command") << QStringLiteral("connection_generation")
                                        << QJsonValue("4");
    QTest::newRow("telemetry-generation")
        << QStringLiteral("telemetry") << QStringLiteral("connection_generation") << QJsonValue("6");
    QTest::newRow("stale-telemetry") << QStringLiteral("telemetry") << QStringLiteral("fresh") << QJsonValue(false);
    QTest::newRow("command-disconnected")
        << QStringLiteral("command") << QStringLiteral("connected") << QJsonValue(false);
    QTest::newRow("telemetry-disconnected")
        << QStringLiteral("telemetry") << QStringLiteral("connected") << QJsonValue(false);
    QTest::newRow("backend-unverified") << QStringLiteral("association") << QStringLiteral("verified")
                                        << QJsonValue(false);
    QTest::newRow("backend-not-ready") << QStringLiteral("readiness") << QStringLiteral("connection_ready")
                                       << QJsonValue(false);
}

void PixEagleClientTest::_contextChangesInvalidateVerification()
{
    QFETCH(QString, object);
    QFETCH(QString, field);
    QFETCH(QJsonValue, value);
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QVERIFY(client.associationVerified());
    const QJsonObject original = server.context;
    setContextField(server.context, object, field, value);
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QVERIFY(!client.associationVerified());
    server.context = original;
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QVERIFY(!client.associationVerified());
    QVERIFY(verify(client));
    QVERIFY(client.associationVerified());
}

void PixEagleClientTest::_verificationRequiresFreshMatchingConfirmation()
{
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    server.deferredPath = VERIFY_PATH;
    client.verifyVehicle();
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 3, TestTimeout::mediumMs());
    const HttpResponse oldDiscovery = server.responseFor(server.requests.last());
    QVERIFY(!client.associationVerified());
    server.context["runtime_id"] = "runtime-replaced-during-discovery";
    QVERIFY(server.respond(2, oldDiscovery));
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QCOMPARE(server.requests.last().target, CONTEXT_PATH);
    QVERIFY(client.authenticated());
    QVERIFY(!client.associationVerified());
    server.deferredPath.clear();
    QVERIFY(verify(client));
    QVERIFY(client.associationVerified());
}

void PixEagleClientTest::_lateConfirmationCannotVerify()
{
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    server.deferredPath = CONTEXT_PATH;
    client.verifyVehicle();
    QTRY_COMPARE_WITH_TIMEOUT(server.requests.size(), 4, TestTimeout::mediumMs());
    QCOMPARE(server.requests.last().target, CONTEXT_PATH);
    const HttpResponse captured = server.responseFor(server.requests.last());
    QTimer release;
    release.setSingleShot(true);
    release.setTimerType(Qt::PreciseTimer);
    connect(&release, &QTimer::timeout, &server, [&server, captured]() { server.respond(3, captured); });
    // A valid body delivered beyond the documented 3-second context age cannot establish a binding.
    release.start(3200);
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::longMs());
    QVERIFY(client.authenticated());
    QVERIFY(!client.associationVerified());
    QVERIFY(client.instanceId().isEmpty());
}

void PixEagleClientTest::_conflictingAircraftIdentity_data()
{
    QTest::addColumn<QString>("observedUid");
    QTest::newRow("different-known-uid") << QStringLiteral("123");
    QTest::newRow("uid-became-unknown") << QStringLiteral("0");
}

void PixEagleClientTest::_conflictingAircraftIdentity()
{
    QFETCH(QString, observedUid);
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client;
    client.observeAircraftUid("0");
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QVERIFY(client.associationVerified());
    client.observeAircraftUid(observedUid);
    QVERIFY(!client.associationVerified());
    QVERIFY(!client.canVerify());
    client.observeAircraftUid(AIRCRAFT_UID);
    QVERIFY(verify(client));
    QVERIFY(!client.associationVerified());
}

void PixEagleClientTest::_duplicateAssociationRequiresVerificationAgain()
{
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QVERIFY(client.associationVerified());
    client.setDuplicateAssociation(true);
    QVERIFY(!client.associationVerified());
    QVERIFY(!client.canVerify());
    client.setDuplicateAssociation(false);
    QVERIFY(!client.associationVerified());
    QVERIFY(client.statusText().contains("duplicate connection"));
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QVERIFY(client.statusText().contains("duplicate connection"));
    QVERIFY(verify(client));
    QVERIFY(client.associationVerified());
    QVERIFY(!client.statusText().contains("duplicate connection"));
}

void PixEagleClientTest::_endpointsIsolateCookiesAndCsrf()
{
    CompanionServer first("alpha");
    CompanionServer second("bravo");
    QVERIFY(first.start());
    QVERIFY(second.start());
    second.context = aircraftContext("9007199254740993");
    PixEagleClient a;
    PixEagleClient b;
    configure(a, first);
    configure(b, second, "9007199254740993");
    QVERIFY(signIn(a));
    QVERIFY(signIn(b));
    QVERIFY(verify(a));
    QVERIFY(verify(b));
    QVERIFY(a.associationVerified());
    QVERIFY(b.associationVerified());
    QVERIFY(first.requests.first().headers.value("cookie").isEmpty());
    QVERIFY(second.requests.first().headers.value("cookie").isEmpty());
    QCOMPARE(first.requests.last().headers.value("cookie"), QByteArray("pixeagle_session=session-alpha"));
    QCOMPARE(second.requests.last().headers.value("cookie"), QByteArray("pixeagle_session=session-bravo"));
    QCOMPARE(first.lastRequest(VERIFY_PATH).headers.value("x-test-csrf"), QByteArray("csrf-alpha"));
    QCOMPARE(second.lastRequest(VERIFY_PATH).headers.value("x-test-csrf"), QByteArray("csrf-bravo"));
    a.signOut();
    QTRY_VERIFY_WITH_TIMEOUT(!a.busy(), TestTimeout::mediumMs());
    QVERIFY(!a.authenticated());
    QVERIFY(b.associationVerified());
    QCOMPARE(first.requests.last().target, LOGOUT_PATH);
    QCOMPARE(first.requests.last().headers.value("x-test-csrf"), QByteArray("csrf-alpha"));
    QCOMPARE(first.requests.last().headers.value("cookie"), QByteArray("pixeagle_session=session-alpha"));
}

void PixEagleClientTest::_endpointPrefixPreserved()
{
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client;
    configure(client, server);
    client.setEndpoint(server.endpoint("/pixeagle-api///"));
    QCOMPARE(client.endpoint(), server.endpoint("/pixeagle-api"));
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QVERIFY(client.associationVerified());
    QCOMPARE(server.requests[0].target, "/pixeagle-api" + LOGIN_PATH);
    QCOMPARE(server.requests[1].target, "/pixeagle-api" + CONTEXT_PATH);
    QCOMPARE(server.lastRequest(VERIFY_PATH).target, "/pixeagle-api" + VERIFY_PATH);
    QCOMPARE(server.requests.last().target, "/pixeagle-api" + CONTEXT_PATH);
}

void PixEagleClientTest::_cancelPendingRequest_data()
{
    QTest::addColumn<QByteArray>("pendingPath");
    QTest::addColumn<QString>("cancelAction");
    QTest::newRow("login-endpoint-change") << LOGIN_PATH << QStringLiteral("endpoint");
    QTest::newRow("login-disable") << LOGIN_PATH << QStringLiteral("disable");
    QTest::newRow("verification-endpoint-change") << VERIFY_PATH << QStringLiteral("endpoint");
    QTest::newRow("verification-vehicle-offline") << VERIFY_PATH << QStringLiteral("offline");
}

void PixEagleClientTest::_cancelPendingRequest()
{
    QFETCH(QByteArray, pendingPath);
    QFETCH(QString, cancelAction);
    CompanionServer oldServer("retired");
    CompanionServer newServer("replacement");
    QVERIFY(oldServer.start());
    QVERIFY(newServer.start());
    PixEagleClient client;
    configure(client, oldServer);
    if (pendingPath == VERIFY_PATH) {
        QVERIFY(signIn(client));
    }
    oldServer.deferredPath = pendingPath;
    if (pendingPath == LOGIN_PATH) {
        client.signIn("pilot", "retired-password");
    } else {
        client.verifyVehicle();
    }
    QTRY_VERIFY_WITH_TIMEOUT(!oldServer.requests.isEmpty() && oldServer.requests.last().target == pendingPath,
                             TestTimeout::mediumMs());
    QVERIFY(client.busy());
    const qsizetype pendingIndex = oldServer.requests.size() - 1;
    if (cancelAction == "endpoint") {
        client.setEndpoint(newServer.endpoint());
    } else if (cancelAction == "disable") {
        client.setEnabled(false);
    } else {
        client.setVehicleIdentity(7, AIRCRAFT_UID, false);
    }
    QTRY_VERIFY_WITH_TIMEOUT(!oldServer.requests[pendingIndex].socket ||
                                 oldServer.requests[pendingIndex].socket->state() == QAbstractSocket::UnconnectedState,
                             TestTimeout::mediumMs());
    QVERIFY(!oldServer.respond(pendingIndex, oldServer.responseFor(oldServer.requests[pendingIndex])));
    QVERIFY(!client.busy());
    QVERIFY(!client.associationVerified());
    if (cancelAction == "endpoint") {
        QVERIFY(!client.authenticated());
        QVERIFY(signIn(client));
        QVERIFY(verify(client));
        QVERIFY(client.associationVerified());
        QVERIFY(newServer.requests.first().headers.value("cookie").isEmpty());
        QCOMPARE(newServer.requests.last().headers.value("cookie"), QByteArray("pixeagle_session=session-replacement"));
        QCOMPARE(newServer.lastRequest(VERIFY_PATH).headers.value("x-test-csrf"), QByteArray("csrf-replacement"));
    }
}

void PixEagleClientTest::_redirectDoesNotForwardCredentials()
{
    CompanionServer source;
    CompanionServer destination;
    QVERIFY(source.start());
    QVERIFY(destination.start());
    source.overrides[LOGIN_PATH] = {
        307, {}, {{"Location", (destination.endpoint() + QString::fromLatin1(LOGIN_PATH)).toUtf8()}}};
    QSignalSpy incoming(&destination, &QTcpServer::newConnection);
    PixEagleClient client;
    configure(client, source);
    client.signIn("pilot", "redirect-password");
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QVERIFY(!client.authenticated());
    QVERIFY_NO_SIGNAL_WAIT(incoming, TestTimeout::shortMs());
    QCOMPARE(source.requests.size(), 1);
    QVERIFY(destination.requests.isEmpty());
}

void PixEagleClientTest::_tlsCertificateRejected()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP("No TLS backend available");
    }
    QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
    configuration.setLocalCertificate(QSslCertificate(NTRIPTlsTestFixtures::SERVER_CERT_PEM, QSsl::Pem));
    configuration.setPrivateKey(QSslKey(NTRIPTlsTestFixtures::PRIVATE_KEY_PEM, QSsl::Rsa, QSsl::Pem));
    configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslServer server;
    server.setSslConfiguration(configuration);
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    QSignalSpy handshakes(&server, &QSslServer::startedEncryptionHandshake);
    QSignalSpy encryptedConnections(&server, &QSslServer::pendingConnectionAvailable);
    PixEagleClient client;
    client.setEndpoint(QStringLiteral("https://127.0.0.1:%1").arg(server.serverPort()));
    client.setEnabled(true);
    client.signIn("pilot", "tls-password");
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QVERIFY(!handshakes.isEmpty());
    QVERIFY(encryptedConnections.isEmpty());
    QVERIFY(!client.authenticated());
    QVERIFY(client.statusText().contains("certificate", Qt::CaseInsensitive));
}

void PixEagleClientTest::_sessionErrors_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<bool>("staysAuthenticated");
    QTest::newRow("expired") << 401 << false;
    QTest::newRow("permission-denied") << 403 << true;
    QTest::newRow("unavailable") << 503 << true;
}

void PixEagleClientTest::_sessionErrors()
{
    QFETCH(int, status);
    QFETCH(bool, staysAuthenticated);
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QVERIFY(client.associationVerified());
    server.overrides[CONTEXT_PATH] = {status, {}, {}};
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QCOMPARE(client.authenticated(), staysAuthenticated);
    QVERIFY(!client.associationVerified());
}

void PixEagleClientTest::_credentialsStayOutOfSettings()
{
    const QString password = QStringLiteral("pixeagle-unique-in-memory-test-password");
    CompanionServer server("unique-in-memory-cookie");
    QVERIFY(server.start());
    {
        PixEagleClient client;
        configure(client, server);
        QVERIFY(signIn(client, password));
        QVERIFY(verify(client));
    }
    QSettings settings;
    settings.sync();
    const auto keys = settings.allKeys();
    for (const QString& key : keys) {
        const QString value = settings.value(key).toString();
        QVERIFY(!value.contains(password));
        QVERIFY(!value.contains("session-unique-in-memory-cookie"));
        QVERIFY(!value.contains("csrf-unique-in-memory-cookie"));
    }
    PixEagleClient replacement;
    configure(replacement, server);
    QVERIFY(!replacement.authenticated());
    QVERIFY(!replacement.associationVerified());
    QSignalSpy incoming(&server, &QTcpServer::newConnection);
    replacement.refresh();
    replacement.verifyVehicle();
    QVERIFY_NO_SIGNAL_WAIT(incoming, TestTimeout::shortMs());
}

void PixEagleClientTest::_rememberSignInPreferenceIsPerEndpoint()
{
    PixEagleClient client;
    client.setEndpoint(QStringLiteral("https://remember-sign-in-a.example.invalid"));
    QVERIFY(client.rememberSignIn());
    client.setRememberSignIn(false);
    QVERIFY(!client.rememberSignIn());

    client.setEndpoint(QStringLiteral("https://remember-sign-in-b.example.invalid"));
    QVERIFY(client.rememberSignIn());
    client.setEndpoint(QStringLiteral("https://remember-sign-in-a.example.invalid"));
    QVERIFY(!client.rememberSignIn());
}

void PixEagleClientTest::_companionVideoWithoutAircraft()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseMedia(server.context);
    server.context.insert("command", QJsonObject{{"connected", false}, {"connection_generation", "0"}});
    server.context.insert("telemetry", QJsonObject{{"connected", false}, {"connection_generation", "0"}});
    server.context.insert("association", QJsonObject{{"verified", false}});
    server.context.insert("readiness", QJsonObject{{"connection_ready", false}, {"following_allowed", false}});
    PixEagleClient client(nullptr, true);
    client.setEnabled(true);
    client.setEndpoint(server.endpoint("/prefix"));
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.mediaAvailable(), TestTimeout::mediumMs());
    QVERIFY(!client.associationVerified());
    QVERIFY(!client.canVerify());
    client.setVehicleIdentity(7, AIRCRAFT_UID, true);
    QVERIFY(client.aircraftUid().isEmpty());
    client.verifyVehicle();
    QVERIFY(server.lastRequest(VERIFY_PATH).target.isEmpty());
    QCOMPARE(client.mediaUrl().toString(), server.endpoint("/prefix/ws/video_feed").replace("http:", "ws:"));
    QCOMPARE(client.mediaOrigin(), server.endpoint());
    QCOMPARE(client.mediaCookie(), QByteArray("pixeagle_session=session-first"));
    QTRY_VERIFY_WITH_TIMEOUT(client.runtimeStatusText().contains("Following inactive"), TestTimeout::mediumMs());
    QVERIFY(client.statusText().contains("No aircraft in QGC"));

    advertiseFollowing(server.context);
    server.followingSnapshot = nativeFollowingStatus(server.context);
    server.followingSnapshot.insert("start_reason_codes",
                                    QJsonArray{"command_preview_not_aircraft_following", "aircraft_not_verified"});
    client.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(client.followingStatusText(), QStringLiteral("Bench: following disabled"),
                              TestTimeout::mediumMs());
    QCOMPARE(client.selectedFollower(), QStringLiteral("mc_velocity_position"));
    QVERIFY(!client.followerChoices().isEmpty());
    QVERIFY(!client.canSelectFollower());
    QVERIFY(!client.canStartFollowing());

    server.followingSnapshot.insert("last_handoff", QJsonObject{{"follow_session_id", QString(32, 'c')},
                                                                {"aircraft_uid", QJsonValue::Null},
                                                                {"reason_code", "operator_stop"},
                                                                {"result", "stopped"},
                                                                {"execution_mode", "COMMAND_PREVIEW"}});
    client.refreshFollowing();
    QTRY_COMPARE_WITH_TIMEOUT(client.followingSummaryText(), QStringLiteral("Test stopped"), TestTimeout::mediumMs());
    QVERIFY(client.followingStatusText().contains(QStringLiteral("no aircraft commands")));

    CompanionServer other("second");
    QVERIFY(other.start());
    advertiseMedia(other.context);
    PixEagleClient second(nullptr, true);
    second.setEnabled(true);
    second.setEndpoint(other.endpoint());
    QVERIFY(signIn(second));
    QTRY_VERIFY_WITH_TIMEOUT(second.mediaAvailable(), TestTimeout::mediumMs());
    QCOMPARE(second.mediaCookie(), QByteArray("pixeagle_session=session-second"));
    QCOMPARE(client.mediaCookie(), QByteArray("pixeagle_session=session-first"));
    client.setEnabled(false);
    QVERIFY(!client.mediaAvailable());
    QVERIFY(client.mediaCookie().isEmpty());
    QVERIFY(client.mediaUrl().isEmpty());
    QVERIFY(second.mediaAvailable());
}

void PixEagleClientTest::_companionTracksWithAircraftButCannotFollow()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    advertiseFollowing(server.context);
    auto capabilities = server.context.value("capabilities").toArray();
    capabilities.append("target.unbound_tracking.v1");
    server.context.insert("capabilities", capabilities);
    server.targetSnapshot = nativeTargetState(server.context);
    server.followingSnapshot = nativeFollowingStatus(server.context);
    server.followingSnapshot.insert("start_allowed", true);
    server.followingSnapshot.insert("start_reason_codes", QJsonArray{});

    PixEagleClient client(nullptr, true);
    client.setEnabled(true);
    client.setEndpoint(server.endpoint());
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.targetWriteAllowed(), TestTimeout::mediumMs());
    QTRY_VERIFY_WITH_TIMEOUT(!client.followerChoices().isEmpty(), TestTimeout::mediumMs());
    QVERIFY(!client.canStartFollowing());
    QVERIFY(!client.associationVerified());

    for (qsizetype index = 0; index < capabilities.size(); ++index) {
        if (capabilities.at(index) == QStringLiteral("target.unbound_tracking.v1")) {
            capabilities.removeAt(index);
            break;
        }
    }
    server.context.insert("capabilities", capabilities);
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!client.targetWriteAllowed(), TestTimeout::mediumMs());
}

void PixEagleClientTest::_companionCanChooseFollowerWithoutStarting()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context, true);
    advertiseFollowing(server.context);
    server.context.insert("association", QJsonObject{{"verified", false}});
    server.followingSnapshot = nativeFollowingStatus(server.context);
    server.followingSnapshot.insert("start_allowed", false);
    server.followingSnapshot.insert("start_reason_codes", QJsonArray{"aircraft_not_verified"});

    PixEagleClient client(nullptr, true);
    client.setEnabled(true);
    client.setEndpoint(server.endpoint());
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.canSelectFollower(), TestTimeout::mediumMs());
    QVERIFY(!client.canStartFollowing());

    const QString context = client.captureFollowingContext();
    QVERIFY(client.selectFollower("mc_velocity_position", context));
    const QByteArray path = "/api/v1/actions/native-follower-select";
    QTRY_VERIFY_WITH_TIMEOUT(server.lastRequestIndex(path) >= 0, TestTimeout::mediumMs());
    const auto body = QJsonDocument::fromJson(server.lastRequest(path).body).object();
    QCOMPARE(body.value("native_context").toObject().value("binding_mode").toString(),
             QStringLiteral("companion_only"));
    QCOMPARE(body.value("native_context").toObject().value("guard").toObject(),
             server.followingSnapshot.value("guard").toObject());
    QVERIFY(!client.startFollowing(context));
    QCOMPARE(server.lastRequestIndex(FOLLOW_START_PATH), -1);
}

void PixEagleClientTest::_mediaRequiresAssociationAndPermissions()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseMedia(server.context);
    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(!client.mediaAvailable());
    QVERIFY(client.mediaCookie().isEmpty());
    QVERIFY(verify(client));
    QVERIFY(client.mediaAvailable());
    auto permissions = server.context.value("permissions").toObject();
    permissions.insert("scopes", QJsonArray{"status:read", "telemetry:read"});
    server.context.insert("permissions", permissions);
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QVERIFY(!client.mediaAvailable());
    advertiseMedia(server.context);
    auto video = server.context.value("video").toObject();
    video.insert("ws_path", "wss://another-host/credentials");
    server.context.insert("video", video);
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QVERIFY(!client.mediaAvailable());
    QVERIFY(client.mediaUrl().isEmpty());
    advertiseMedia(server.context);
    server.context.insert("runtime_id", "replacement-runtime");
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QVERIFY(!client.mediaAvailable());
    QVERIFY(!client.associationVerified());
}

void PixEagleClientTest::_runtimeStatusExpiresAndRejectsLateSession()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseMedia(server.context);
    PixEagleClient client(nullptr, true);
    client.setEnabled(true);
    client.setEndpoint(server.endpoint());
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.runtimeStatusText().contains("Following inactive"), TestTimeout::mediumMs());
    QCOMPARE(server.lastRequest(STATUS_PATH).headers.value("cookie"), QByteArray("pixeagle_session=session-first"));
    QVERIFY(!server.lastRequest(STATUS_PATH).headers.contains("x-test-csrf"));
    server.deferredPath = STATUS_PATH;
    QTRY_VERIFY_WITH_TIMEOUT(client.runtimeStatusText().contains("status unavailable"), 8000);
    client.signOut();
    QTRY_VERIFY_WITH_TIMEOUT(!client.busy(), TestTimeout::mediumMs());
    QVERIFY(!client.authenticated());
    for (qsizetype index = 0; index < server.requests.size(); ++index) {
        if (server.requests[index].target.endsWith(STATUS_PATH)) {
            server.respond(index, {200,
                                   {{"schema_version", 1},
                                    {"source", "tracker_runtime"},
                                    {"active_tracking", true},
                                    {"following_active", true}},
                                   {}});
        }
    }
    QCoreApplication::processEvents();
    QVERIFY(client.runtimeStatusText().contains("status unavailable"));
    QVERIFY(!client.mediaAvailable());
    server.deferredPath.clear();
    server.overrides.insert(STATUS_PATH, {401, {}, {}});
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(!client.authenticated(), TestTimeout::mediumMs());
    QVERIFY(client.statusText().contains("session expired"));
}

void PixEagleClientTest::_targetReadsRequireVerifiedBinding()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    PixEagleClient client;
    configure(client, server);
    client.setEndpoint(server.endpoint("/companion"));
    QVERIFY(signIn(client));
    QVERIFY(server.lastRequest(TARGET_STATE_PATH).target.isEmpty());
    QVERIFY(!client.targetWriteAllowed());
    QVERIFY(verify(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.targetStateFresh() && !client.targetCatalog().isEmpty(), TestTimeout::mediumMs());
    QVERIFY(client.targetWriteAllowed());
    QCOMPARE(server.lastRequest(TARGET_STATE_PATH).target, "/companion" + TARGET_STATE_PATH);
    QCOMPARE(server.lastRequest(TARGET_CATALOG_PATH).target, "/companion" + TARGET_CATALOG_PATH);
    QCOMPARE(server.lastRequest(TARGET_STATE_PATH).headers.value("cookie"),
             QByteArray("pixeagle_session=session-first"));
    QVERIFY(!server.lastRequest(TARGET_STATE_PATH).headers.contains("x-test-csrf"));
    client.setEnabled(false);
    QVERIFY(!client.targetStateFresh());
    QVERIFY(client.targetGuard().isEmpty());
    QVERIFY(client.targetCatalog().isEmpty());
}

void PixEagleClientTest::_targetMutationEnvelope_data()
{
    QTest::addColumn<QString>("action");
    QTest::addColumn<QJsonObject>("payload");
    const QJsonObject frame{{"provenance", QJsonObject{{"frame_id", "73"}}},
                            {"selection_geometry", QJsonObject{{"version", "1"}}}};
    QTest::newRow("classic-point") << QStringLiteral("tracking_start")
                                   << QJsonObject{
                                          {"point",
                                           QJsonObject{{"coordinate_space", "normalized"}, {"x", 0.4}, {"y", 0.6}}},
                                          {"frame", frame}};
    QTest::newRow("classic-rectangle") << QStringLiteral("tracking_start")
                                       << QJsonObject{{"bbox", QJsonObject{{"coordinate_space", "normalized"},
                                                                           {"x", 0.1},
                                                                           {"y", 0.2},
                                                                           {"width", 0.3},
                                                                           {"height", 0.4}}},
                                                      {"frame", frame}};
    QTest::newRow("smart-point") << QStringLiteral("smart_click")
                                 << QJsonObject{
                                        {"click",
                                         QJsonObject{{"coordinate_space", "normalized"}, {"x", 0.3}, {"y", 0.7}}},
                                        {"frame", frame}};
    QTest::newRow("cancel") << QStringLiteral("tracking_stop") << QJsonObject{};
    QTest::newRow("mode") << QStringLiteral("smart_mode_toggle") << QJsonObject{{"enabled", true}};
    QTest::newRow("tracker") << QStringLiteral("tracker_switch")
                             << QJsonObject{{"tracker_type", "CSRT"}, {"persist", true}};
    QTest::newRow("external-cancel") << QStringLiteral("gimbal_control") << QJsonObject{{"operation", "cancel"}};
    QTest::newRow("external-select") << QStringLiteral("gimbal_control")
                                     << QJsonObject{{"operation", "select"}, {"x", 0.4}, {"y", 0.6}, {"frame", frame}};
}

void PixEagleClientTest::_targetMutationEnvelope()
{
    QFETCH(QString, action);
    QFETCH(QJsonObject, payload);
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context, true);
    PixEagleClient client(nullptr, true);
    client.setEndpoint(server.endpoint("/companion"));
    client.setEnabled(true);
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.targetWriteAllowed(), TestTimeout::mediumMs());
    const auto guard = client.targetGuard();
    auto resource = action;
    resource.replace('_', '-');
    const auto path = QByteArray("/api/v1/actions/") + resource.toLatin1();
    server.deferredPath = path;
    QSignalSpy finished(&client, &PixEagleClient::targetActionFinished);
    QVERIFY(client.submitTargetAction(action, payload, guard));
    QVERIFY(client.targetMutationPending());
    QVERIFY(!client.targetWriteAllowed());
    QVERIFY(!client.submitTargetAction(action, payload, guard));
    QTRY_VERIFY_WITH_TIMEOUT(!server.lastRequest(path).target.isEmpty(), TestTimeout::mediumMs());
    const auto request = server.lastRequest(path);
    QCOMPARE(request.target, "/companion" + path);
    QCOMPARE(request.method, QByteArray("POST"));
    QCOMPARE(request.headers.value("cookie"), QByteArray("pixeagle_session=session-first"));
    QCOMPARE(request.headers.value("x-test-csrf"), QByteArray("csrf-first"));
    const auto body = QJsonDocument::fromJson(request.body).object();
    QCOMPARE(body.value("source").toString(), QStringLiteral("qgroundcontrol"));
    QCOMPARE(body.value("confirm").toBool(), true);
    QCOMPARE(body.value("dry_run").toBool(), false);
    QVERIFY(!body.value("idempotency_key").toString().isEmpty());
    const auto native = body.value("native_context").toObject();
    QCOMPARE(native.value("binding_mode").toString(), QStringLiteral("companion_only"));
    QVERIFY(!native.value("guard").toObject().contains("_client_generation"));
    QVERIFY(!native.value("guard").toObject().contains("_client_context"));
    QCOMPARE(native.value("guard").toObject(), client.targetState().value("guard").toObject());
    QCOMPARE(native.value("frame"), payload.value("frame"));
    QVERIFY(!body.contains("frame"));
    if (action == "tracker_switch") {
        QCOMPARE(body.value("persist").toBool(), false);
    }
    server.targetSnapshot = nativeTargetState(server.context, "2");
    QVERIFY(server.respond(server.lastRequestIndex(path), server.responseFor(request)));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, TestTimeout::mediumMs());
    QCOMPARE(finished.first()[0].toString(), action);
    QCOMPARE(finished.first()[1].toString(), QStringLiteral("accepted"));
    QVERIFY(!client.targetMutationPending());
    QTRY_COMPARE_WITH_TIMEOUT(client.targetState().value("target_revision").toString(), QStringLiteral("2"),
                              TestTimeout::mediumMs());
}

void PixEagleClientTest::_targetMutationGates_data()
{
    QTest::addColumn<QString>("gate");
    for (const auto* gate : {"viewer", "following", "command-connected", "telemetry-connected", "identity-unknown",
                             "unsupported-action", "flight-stop", "missing-frame", "unverified-vehicle"}) {
        QTest::newRow(gate) << QString::fromLatin1(gate);
    }
}

void PixEagleClientTest::_targetMutationGates()
{
    QFETCH(QString, gate);
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context, true);
    if (gate == "viewer") {
        setContextField(server.context, "permissions", "scopes",
                        QJsonArray{"status:read", "telemetry:read", "media:read"});
    } else if (gate == "command-connected") {
        setContextField(server.context, "command", "connected", true);
    } else if (gate == "telemetry-connected") {
        setContextField(server.context, "telemetry", "connected", true);
    } else if (gate == "identity-unknown") {
        setContextField(server.context, "command", "connected", QJsonValue::Null);
    }
    server.targetSnapshot = nativeTargetState(server.context);
    if (gate == "following") {
        server.targetSnapshot.insert("following_active", true);
    }
    PixEagleClient client(nullptr, gate != "unverified-vehicle");
    configure(client, server);
    QVERIFY(signIn(client));
    if (gate != "unverified-vehicle") {
        QTRY_VERIFY_WITH_TIMEOUT(client.targetStateFresh(), TestTimeout::mediumMs());
    }
    const QString action = gate == "unsupported-action" ? QStringLiteral("offboard_stop")
                           : gate == "flight-stop"      ? QStringLiteral("gimbal_control")
                           : gate == "missing-frame"    ? QStringLiteral("tracking_start")
                                                        : QStringLiteral("tracking_stop");
    const QJsonObject payload = gate == "flight-stop" ? QJsonObject{{"operation", "stop"}} : QJsonObject{};
    QVERIFY(!client.submitTargetAction(action, payload, client.targetGuard()));
    QVERIFY(!client.targetMutationPending());
    for (const auto& request : std::as_const(server.requests)) {
        QVERIFY(!request.target.contains("/api/v1/actions/"));
    }
}

void PixEagleClientTest::_retargetWhileFollowingKeepsOtherTargetMutationsBlocked()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context);
    server.targetSnapshot = nativeTargetState(server.context);
    server.targetSnapshot.insert("following_active", true);

    PixEagleClient client;
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(verify(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.targetStateFresh(), TestTimeout::mediumMs());
    QVERIFY(client.targetWriteAllowed());
    QVERIFY(!client.submitTargetAction("tracking_stop", {}, client.targetGuard()));
    QVERIFY(!client.submitTargetAction("tracker_switch", {{"tracker_type", "KCF"}}, client.targetGuard()));

    server.deferredPath = "/api/v1/actions/tracking-start";
    QVERIFY(client.submitTargetAction("tracking_start", {{"frame", QJsonObject{{"displayed", true}}}},
                                      client.targetGuard()));
    QTRY_VERIFY_WITH_TIMEOUT(server.lastRequestIndex(server.deferredPath) >= 0, TestTimeout::mediumMs());
}

void PixEagleClientTest::_targetGuardInvalidation_data()
{
    QTest::addColumn<QString>("change");
    for (const auto* change : {"revision", "mode", "runtime", "stream", "source", "permissions"}) {
        QTest::newRow(change) << QString::fromLatin1(change);
    }
}

void PixEagleClientTest::_targetGuardInvalidation()
{
    QFETCH(QString, change);
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context, true);
    PixEagleClient client(nullptr, true);
    configure(client, server);
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.targetWriteAllowed(), TestTimeout::mediumMs());
    const auto guard = client.targetGuard();
    server.targetSnapshot = nativeTargetState(server.context, "2");
    if (change == "mode") {
        server.targetSnapshot.insert("mode", "smart");
        setContextField(server.targetSnapshot, "guard", "mode", "smart");
    } else if (change == "runtime") {
        server.context.insert("runtime_id", "runtime-b");
    } else if (change == "stream" || change == "source") {
        setContextField(server.context, "video", change + "_epoch", "replacement");
    } else if (change == "permissions") {
        setContextField(server.context, "permissions", "scopes", QJsonArray{"status:read", "telemetry:read"});
    }
    if (change != "mode" && change != "revision") {
        server.targetSnapshot = nativeTargetState(server.context, "2");
    }
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(client.targetStateFresh() && client.targetGuard() != guard, TestTimeout::mediumMs());
    QVERIFY(!client.submitTargetAction("tracking_stop", {}, guard));
    QVERIFY(server.lastRequest("/api/v1/actions/tracking-stop").target.isEmpty());
}

void PixEagleClientTest::_targetActionErrors_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QString>("outcome");
    QTest::addColumn<QString>("code");
    QTest::newRow("conflict") << 409 << QStringLiteral("conflict") << QString();
    QTest::newRow("frame-evicted") << 409 << QStringLiteral("conflict") << QStringLiteral("frame_evicted");
    QTest::newRow("frame-expired") << 409 << QStringLiteral("conflict") << QStringLiteral("frame_expired");
    QTest::newRow("denied") << 403 << QStringLiteral("rejected") << QString();
    QTest::newRow("expired-session") << 401 << QStringLiteral("rejected") << QString();
    QTest::newRow("server-error") << 500 << QStringLiteral("unknown") << QString();
    QTest::newRow("redirect") << 307 << QStringLiteral("unknown") << QString();
    QTest::newRow("malformed-success") << 202 << QStringLiteral("unknown") << QString();
}

void PixEagleClientTest::_targetActionErrors()
{
    QFETCH(int, status);
    QFETCH(QString, outcome);
    QFETCH(QString, code);
    CompanionServer server;
    CompanionServer destination("other");
    QVERIFY(server.start());
    QVERIFY(destination.start());
    advertiseTargets(server.context, true);
    PixEagleClient client(nullptr, true);
    configure(client, server);
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.targetWriteAllowed(), TestTimeout::mediumMs());
    server.overrides.insert("/api/v1/actions/tracking-stop",
                            {status,
                             {{"code", code}, {"detail", "Do not echo this untrusted server content"}},
                             {{"Location", destination.endpoint().toLatin1() + "/api/v1/actions/tracking-stop"}}});
    QSignalSpy finished(&client, &PixEagleClient::targetActionFinished);
    QVERIFY(client.submitTargetAction("tracking_stop", {}, client.targetGuard()));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, TestTimeout::mediumMs());
    QCOMPARE(finished.first()[1].toString(), outcome);
    if (code == "frame_evicted" || code == "frame_expired") {
        QVERIFY(finished.first()[2].toString().contains("displayed frame"));
    }
    QVERIFY(!finished.first()[2].toString().contains("untrusted"));
    QVERIFY(destination.requests.isEmpty());
    QCOMPARE(client.authenticated(), status != 401);
}

void PixEagleClientTest::_targetTimeoutDoesNotRetry()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseTargets(server.context, true);
    PixEagleClient client(nullptr, true);
    configure(client, server);
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.targetWriteAllowed(), TestTimeout::mediumMs());
    server.deferredPath = "/api/v1/actions/tracking-stop";
    QSignalSpy finished(&client, &PixEagleClient::targetActionFinished);
    QVERIFY(client.submitTargetAction("tracking_stop", {}, client.targetGuard()));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, TestTimeout::longMs());
    QCOMPARE(finished.first()[1].toString(), QStringLiteral("unknown"));
    QTRY_VERIFY_WITH_TIMEOUT(client.targetStateFresh(), TestTimeout::mediumMs());
    qsizetype mutationCount = 0;
    for (const auto& request : std::as_const(server.requests)) {
        mutationCount += request.target.contains("/api/v1/actions/") ? 1 : 0;
    }
    QCOMPARE(mutationCount, 1);
    QVERIFY(!client.targetMutationPending());
}

void PixEagleClientTest::_targetLateReplyCannotCrossSessions()
{
    CompanionServer first;
    CompanionServer second("second");
    QVERIFY(first.start());
    QVERIFY(second.start());
    advertiseTargets(first.context, true);
    advertiseTargets(second.context, true);
    second.context.insert("instance_id", "other-instance");
    PixEagleClient client(nullptr, true);
    configure(client, first);
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.targetWriteAllowed(), TestTimeout::mediumMs());
    const auto guard = client.targetGuard();
    first.deferredPath = "/api/v1/actions/tracking-stop";
    QSignalSpy finished(&client, &PixEagleClient::targetActionFinished);
    QVERIFY(client.submitTargetAction("tracking_stop", {}, guard));
    QTRY_VERIFY_WITH_TIMEOUT(!first.lastRequest(first.deferredPath).target.isEmpty(), TestTimeout::mediumMs());
    const auto oldIndex = first.lastRequestIndex(first.deferredPath);
    client.setEndpoint(second.endpoint());
    QCOMPARE(finished.count(), 1);
    QCOMPARE(finished.first()[1].toString(), QStringLiteral("unknown"));
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.targetWriteAllowed(), TestTimeout::mediumMs());
    QVERIFY(!client.submitTargetAction("tracking_stop", {}, guard));
    first.respond(oldIndex, first.responseFor(first.requests[oldIndex]));
    QCoreApplication::processEvents();
    QCOMPARE(client.targetState().value("instance_id").toString(), QStringLiteral("other-instance"));
    QCOMPARE(finished.count(), 1);
    QCOMPARE(second.lastRequest(TARGET_STATE_PATH).headers.value("cookie"),
             QByteArray("pixeagle_session=session-second"));
}

void PixEagleClientTest::_safetyRequiresBackendCapabilityAndFreshState()
{
    CompanionServer server;
    QVERIFY(server.start());
    PixEagleClient client(nullptr, true);
    configure(client, server);
    QVERIFY(signIn(client));
    QVERIFY(!client.safetyAvailable());
    QVERIFY(!client.canSetSafety());
    QCOMPARE(server.lastRequestIndex(SAFETY_PATH), -1);

    advertiseSafety(server.context);
    server.overrides.insert(SAFETY_PATH, {200, nativeSafetyStatus(server.context), {}});
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(client.safetyFresh(), TestTimeout::mediumMs());
    QVERIFY(client.safetyActive());
    QVERIFY(client.canSetSafety());

    auto stale = nativeSafetyStatus(server.context);
    stale.insert("runtime_id", "retired-runtime");
    server.overrides.insert(SAFETY_PATH, {200, stale, {}});
    client.refreshSafety();
    QTRY_VERIFY_WITH_TIMEOUT(!client.safetyFresh(), TestTimeout::mediumMs());
    QVERIFY(!client.canSetSafety());
    QVERIFY(!client.setSafetyActive(false, client.captureSafetyContext()));
    QCOMPARE(server.lastRequestIndex(SAFETY_SET_PATH), -1);
}

void PixEagleClientTest::_safetyMutationUsesCapturedBackendState()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseSafety(server.context);
    server.overrides.insert(SAFETY_PATH, {200, nativeSafetyStatus(server.context), {}});
    PixEagleClient client(nullptr, true);
    configure(client, server);
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.canSetSafety(), TestTimeout::mediumMs());
    const QString captured = client.captureSafetyContext();
    QVERIFY(!captured.isEmpty());

    server.deferredPath = SAFETY_SET_PATH;
    QVERIFY(client.setSafetyActive(false, captured));
    QVERIFY(client.safetyBusy());
    QVERIFY(!client.safetyFresh());
    QTRY_VERIFY_WITH_TIMEOUT(server.lastRequestIndex(SAFETY_SET_PATH) >= 0, TestTimeout::mediumMs());
    const auto request = server.lastRequest(SAFETY_SET_PATH);
    const auto body = QJsonDocument::fromJson(request.body).object();
    QCOMPARE(request.headers.value("x-test-csrf"), QByteArray("csrf-first"));
    QCOMPARE(body.value("native_safety_context").toObject(), QJsonDocument::fromJson(captured.toUtf8()).object());
    QCOMPARE(body.value("enabled").toBool(), false);
    QCOMPARE(body.value("confirm").toBool(), true);
    QVERIFY(!client.setSafetyActive(false, captured));

    server.overrides.insert(SAFETY_PATH, {200, nativeSafetyStatus(server.context, false), {}});
    QVERIFY(server.respond(server.lastRequestIndex(SAFETY_SET_PATH), server.responseFor(request)));
    QTRY_VERIFY_WITH_TIMEOUT(client.safetyFresh() && !client.safetyActive(), TestTimeout::mediumMs());
    QVERIFY(!client.setSafetyActive(true, captured));
}

void PixEagleClientTest::_followerPreviewIsNotAircraftFollowing()
{
    CompanionServer server;
    QVERIFY(server.start());
    advertiseFollowing(server.context);
    server.followingSnapshot = nativeFollowingStatus(server.context, false, true);
    server.followingSnapshot.insert("execution_mode", "COMMAND_PREVIEW");
    PixEagleClient client(nullptr, true);
    configure(client, server);
    QVERIFY(signIn(client));
    QTRY_VERIFY_WITH_TIMEOUT(client.followerTestActive(), TestTimeout::mediumMs());
    QCOMPARE(client.followingState(), QStringLiteral("preview"));
    QVERIFY(client.followingStatusText().contains("no aircraft commands"));
}

UT_REGISTER_TEST_LIGHTWEIGHT(PixEagleClientTest, TestLabel::Unit)
