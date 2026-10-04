#include "PixEagleManagerTest.h"

#include <QtCore/QPointer>
#include <QtCore/QSettings>
#include <QtTest/QSignalSpy>

#include "MAVLinkProtocol.h"
#include "PixEagleManager.h"
#include "PixEagleSettings.h"
#include "PixEagleTestServer.h"
#include "Vehicle.h"

namespace {
void sendAutopilotVersion(MockLink* link, int systemId, int componentId, quint64 uid)
{
    mavlink_autopilot_version_t version{};
    version.uid = uid;
    version.flight_sw_version = (1U << 24) | (17U << 16) | FIRMWARE_VERSION_TYPE_OFFICIAL;
    version.capabilities = MAV_PROTOCOL_CAPABILITY_MAVLINK2 | MAV_PROTOCOL_CAPABILITY_MISSION_INT |
                           MAV_PROTOCOL_CAPABILITY_MISSION_FENCE | MAV_PROTOCOL_CAPABILITY_MISSION_RALLY |
                           MAV_PROTOCOL_CAPABILITY_COMMAND_INT;
    mavlink_status_t status{};
    mavlink_message_t message{};
    mavlink_msg_autopilot_version_encode_status(static_cast<uint8_t>(systemId), static_cast<uint8_t>(componentId),
                                                &status, &message, &version);
    uint8_t bytes[MAVLINK_MAX_PACKET_LEN]{};
    const uint16_t size = mavlink_msg_to_send_buffer(bytes, &message);
    MAVLinkProtocol::instance()->receiveBytes(link, QByteArray(reinterpret_cast<const char*>(bytes), size));
}
}  // namespace

void PixEagleManagerTest::_dashboardDefaults_data()
{
    QTest::addColumn<QString>("endpoint");
    QTest::addColumn<QString>("dashboard");
    QTest::newRow("unconfigured") << QString() << QStringLiteral("http://127.0.0.1:3040/");
    QTest::newRow("local") << QStringLiteral("http://127.0.0.1:5077") << QStringLiteral("http://127.0.0.1:3040/");
    QTest::newRow("replay-port") << QStringLiteral("http://localhost:8097") << QStringLiteral("http://localhost:3040/");
    QTest::newRow("ipv6") << QStringLiteral("http://[::1]:5077") << QStringLiteral("http://[::1]:3040/");
    QTest::newRow("remote-prefix") << QStringLiteral("https://companion.example:8443/pixeagle-api/")
                                   << QStringLiteral("https://companion.example:3040/");
}

void PixEagleManagerTest::_dashboardDefaults()
{
    QFETCH(QString, endpoint);
    QFETCH(QString, dashboard);
    PixEagleSettings settings;
    settings.integrationEnabled()->setRawValue(false);
    PixEagleManager manager(&settings);
    manager.activeClient()->setEndpoint(endpoint);
    QCOMPARE(manager.dashboardUrl().toString(), dashboard);
    QVERIFY(manager.dashboardUrlOverride().isEmpty());
    QVERIFY(!manager.activeClient()->enabled());
}

void PixEagleManagerTest::_dashboardOverridePolicy_data()
{
    QTest::addColumn<QString>("address");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("custom-port") << QStringLiteral("http://127.0.0.1:3050/") << true;
    QTest::newRow("proxy-path") << QStringLiteral("https://dashboard.example:8443/pixeagle/") << true;
    QTest::newRow("browser-http-lan") << QStringLiteral("http://192.0.2.1:3040/") << true;
    QTest::newRow("ipv6") << QStringLiteral("http://[::1]:3050/") << true;
    QTest::newRow("credentials") << QStringLiteral("https://pilot:secret@example.test/") << false;
    QTest::newRow("query") << QStringLiteral("https://example.test/?token=secret") << false;
    QTest::newRow("fragment") << QStringLiteral("https://example.test/#secret") << false;
    QTest::newRow("executable-scheme") << QStringLiteral("javascript:alert(1)") << false;
    QTest::newRow("local-file") << QStringLiteral("file:///tmp/pixeagle") << false;
    QTest::newRow("relative") << QStringLiteral("/dashboard/") << false;
    QTest::newRow("missing-host") << QStringLiteral("https:///dashboard/") << false;
    QTest::newRow("invalid-port") << QStringLiteral("https://example.test:65536/") << false;
    QTest::newRow("zero-port") << QStringLiteral("https://example.test:0/") << false;
}

void PixEagleManagerTest::_dashboardOverridePolicy()
{
    QFETCH(QString, address);
    QFETCH(bool, accepted);
    PixEagleSettings settings;
    settings.integrationEnabled()->setRawValue(false);
    PixEagleManager manager(&settings);
    manager.activeClient()->setEndpoint("http://localhost:5077");
    const QUrl previous = manager.dashboardUrl();
    const quint64 generation = manager.activeClient()->sessionGeneration();
    QCOMPARE(manager.setDashboardUrlOverride(address), accepted);
    QCOMPARE(manager.dashboardUrl(), accepted ? QUrl(address) : previous);
    QCOMPARE(manager.activeClient()->sessionGeneration(), generation);
    if (!accepted) {
        QVERIFY(!QSettings().allKeys().join(',').contains("secret"));
        QVERIFY(manager.dashboardUrlOverride().isEmpty());
    }
    QVERIFY(manager.setDashboardUrlOverride(""));
}

void PixEagleManagerTest::_dashboardOverridesFollowEndpoint()
{
    PixEagleSettings settings;
    settings.integrationEnabled()->setRawValue(false);
    PixEagleManager manager(&settings);
    const QString first = "https://companion.example:8443/a";
    const QString second = "https://companion.example:8443/b";
    manager.activeClient()->setEndpoint(first);
    QSignalSpy changed(&manager, &PixEagleManager::dashboardUrlChanged);
    QVERIFY(manager.setDashboardUrlOverride("  https://dashboard.example/pixeagle/  "));
    QCOMPARE(changed.count(), 1);
    manager.activeClient()->setEndpoint(second);
    QVERIFY(manager.dashboardUrlOverride().isEmpty());
    QVERIFY(manager.setDashboardUrlOverride("http://localhost:3050/"));
    manager.activeClient()->setEndpoint("https://companion.example:8444/a");
    QVERIFY(manager.dashboardUrlOverride().isEmpty());
    manager.activeClient()->setEndpoint(first + "/");
    QCOMPARE(manager.dashboardUrl().toString(), "https://dashboard.example/pixeagle/");
    PixEagleManager restored(&settings);
    QCOMPARE(restored.dashboardUrl(), manager.dashboardUrl());
    QVERIFY(manager.setDashboardUrlOverride(""));
    QVERIFY(manager.dashboardUrlOverride().isEmpty());
    manager.activeClient()->setEndpoint(second);
    QCOMPARE(manager.dashboardUrl().toString(), "http://localhost:3050/");
    QVERIFY(manager.setDashboardUrlOverride(""));
}

void PixEagleManagerTest::_vehicleEndpointDefaultsAndRestores_data()
{
    QTest::addColumn<QString>("saved");
    QTest::newRow("default") << QString();
    QTest::newRow("saved-aircraft") << QStringLiteral("http://127.0.0.1:8094");
}

void PixEagleManagerTest::_vehicleEndpointDefaultsAndRestores()
{
    QFETCH(QString, saved);
    const QString key = "PixEagle/Endpoints/" + PixEagleTest::AIRCRAFT_UID;
    QSettings().setValue(key, saved);
    PixEagleSettings settings;
    settings.integrationEnabled()->setRawValue(true);
    PixEagleManager manager(&settings);
    manager.activeClient()->setEndpoint("https://other-companion.example");
    _mockLink = MockLink::startPX4MockLink(MockConfiguration::OptionNone,
                                           MockConfiguration::FailInitialConnectRequestMessageAutopilotVersionLost);
    QVERIFY(_mockLink);
    connect(_mockLink, &QObject::destroyed, this, [this]() { _mockLink = nullptr; });
    QTRY_VERIFY_WITH_TIMEOUT(!manager.activeClient()->companionOnly(), TestTimeout::mediumMs());
    auto* client = manager.activeClient();
    QCOMPARE(client->endpoint(), client->defaultEndpoint());
    _vehicle = MultiVehicleManager::instance()->activeVehicle();
    QVERIFY(_vehicle);
    QTRY_VERIFY_WITH_TIMEOUT(_mockLink->receivedRequestMessageCount(MAVLINK_MSG_ID_AUTOPILOT_VERSION) > 0,
                             TestTimeout::mediumMs());
    sendAutopilotVersion(_mockLink, _vehicle->id(), _vehicle->defaultComponentId(),
                         PixEagleTest::AIRCRAFT_UID.toULongLong());
    QTRY_COMPARE_WITH_TIMEOUT(client->aircraftUid(), PixEagleTest::AIRCRAFT_UID, TestTimeout::mediumMs());
    QCOMPARE(client->endpoint(), saved.isEmpty() ? client->defaultEndpoint() : saved);
    QVERIFY(!client->authenticated());
    _disconnectMockLink();
    QSettings().remove(key);
}

void PixEagleManagerTest::_rawIdentityConflictAndVehicleRemoval_data()
{
    QTest::addColumn<quint64>("observedUid");
    QTest::newRow("different-known-uid") << quint64(123);
    QTest::newRow("uid-became-unknown") << quint64(0);
}

void PixEagleManagerTest::_rawIdentityConflictAndVehicleRemoval()
{
    QFETCH(quint64, observedUid);
    PixEagleTest::CompanionServer server;
    QVERIFY(server.start());
    PixEagleSettings settings;
    settings.integrationEnabled()->setRawValue(true);
    PixEagleManager manager(&settings);
    auto* companion = manager.activeClient();
    QVERIFY(companion && companion->companionOnly());
    companion->setEndpoint(server.endpoint());
    QVERIFY(manager.setDashboardUrlOverride("https://dashboard.example/companion/"));
    QVERIFY(PixEagleTest::signIn(*companion));
    QVERIFY(!companion->associationVerified());

    // Supply an actual AUTOPILOT_VERSION reply with a nonzero UID during the ordinary handshake.
    _mockLink = MockLink::startPX4MockLink(MockConfiguration::OptionNone,
                                           MockConfiguration::FailInitialConnectRequestMessageAutopilotVersionLost);
    QVERIFY(_mockLink);
    connect(_mockLink, &QObject::destroyed, this, [this]() { _mockLink = nullptr; });
    QTRY_VERIFY_WITH_TIMEOUT(manager.activeClient() != companion, TestTimeout::mediumMs());
    _vehicle = MultiVehicleManager::instance()->activeVehicle();
    QVERIFY(_vehicle);
    QPointer<PixEagleClient> client = manager.activeClient();
    QVERIFY(!client->authenticated());
    QVERIFY(!client->companionOnly());
    QVERIFY(client->aircraftUid().isEmpty());
    client->setEndpoint(server.endpoint("/pixeagle-api"));
    QVERIFY(manager.dashboardUrlOverride().isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(_mockLink->receivedRequestMessageCount(MAVLINK_MSG_ID_AUTOPILOT_VERSION) > 0,
                             TestTimeout::mediumMs());

    const quint64 uid = PixEagleTest::AIRCRAFT_UID.toULongLong();
    sendAutopilotVersion(_mockLink, _vehicle->id(), _vehicle->defaultComponentId(), uid);
    QTRY_COMPARE_WITH_TIMEOUT(_vehicle->vehicleUID(), uid, TestTimeout::mediumMs());
    QVERIFY(waitForInitialConnect());
    QCOMPARE(client->aircraftUid(), PixEagleTest::AIRCRAFT_UID);
    QCOMPARE(QSettings().value("PixEagle/Endpoints/" + PixEagleTest::AIRCRAFT_UID).toString(), client->endpoint());
    server.context = PixEagleTest::aircraftContext(PixEagleTest::AIRCRAFT_UID, _vehicle->id());
    PixEagleTest::advertiseTargets(server.context);
    PixEagleTest::advertiseFollowing(server.context);
    server.followingSnapshot = PixEagleTest::nativeFollowingStatus(server.context, false, true);
    QVERIFY(PixEagleTest::signIn(*client));
    QVERIFY(client->canVerify());
    QVERIFY(PixEagleTest::verify(*client));
    QVERIFY(client->associationVerified());
    QTRY_COMPARE_WITH_TIMEOUT(manager.followingVehicles().size(), 1, TestTimeout::mediumMs());
    QVERIFY(manager.followingVehicles().first().toMap().value("stopAvailable").toBool());

    sendAutopilotVersion(_mockLink, _vehicle->id() + 1, _vehicle->defaultComponentId(), observedUid);
    sendAutopilotVersion(_mockLink, _vehicle->id(), MAV_COMP_ID_CAMERA, observedUid);
    QVERIFY(client->associationVerified());
    sendAutopilotVersion(_mockLink, _vehicle->id(), _vehicle->defaultComponentId(), observedUid);
    QTRY_VERIFY_WITH_TIMEOUT(!client->associationVerified(), TestTimeout::mediumMs());
    QVERIFY(!client->canVerify());
    QVERIFY(client->statusText().contains("Conflicting", Qt::CaseInsensitive));
    sendAutopilotVersion(_mockLink, _vehicle->id(), _vehicle->defaultComponentId(), uid);
    QVERIFY(!client->associationVerified());
    QVERIFY(!client->canVerify());

    _disconnectMockLink();
    QTRY_VERIFY_WITH_TIMEOUT(client.isNull(), TestTimeout::mediumMs());
    QCOMPARE(manager.activeClient(), companion);
    QCOMPARE(manager.dashboardUrl().toString(), "https://dashboard.example/companion/");
    QVERIFY(manager.setDashboardUrlOverride(""));
    QVERIFY(companion->authenticated());
    QVERIFY(manager.vehicleLabels().isEmpty());
    QVERIFY(manager.followingVehicles().isEmpty());
}

UT_REGISTER_TEST(PixEagleManagerTest, TestLabel::Integration, TestLabel::Vehicle)
