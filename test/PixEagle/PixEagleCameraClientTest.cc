#include "PixEagleCameraClientTest.h"

#include <memory>

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtTest/QSignalSpy>

#include "ColoredSvgImageProvider.h"
#include "PixEagleTestServer.h"

using namespace PixEagleTest;

namespace {
const QByteArray CAMERA_PATH = "/api/v1/gimbal/control";
const QByteArray CONTROL_PATH = "/api/v1/actions/gimbal-control";

QJsonObject cameraStatus(const QJsonObject& context)
{
    return {
        {"instance_id", context.value("instance_id")},
        {"runtime_id", context.value("runtime_id")},
        {"enabled", true},
        {"available", true},
        {"connected", true},
        {"motion_active", false},
        {"following_active", false},
        {"camera_id", "camera-a"},
        {"camera_generation", "generation-a"},
        {"source_epoch", context.value("video").toObject().value("source_epoch")},
        {"target_engine", "camera"},
        {"capabilities", QJsonArray{"pan", "tilt", "home", "stop", "manual_begin", "manual_update"}},
        {"selection_modes",
         QJsonArray{QJsonObject{{"id", "classic"}, {"label", "Camera Classic"}, {"point", true}, {"rectangle", true}}}},
        {"guard", QJsonObject{{"camera_id", "camera-a"},
                              {"camera_generation", "generation-a"},
                              {"source_epoch", context.value("video").toObject().value("source_epoch")}}}};
}

struct CameraHarness
{
    CameraHarness()
        : client(nullptr, true)
    {
        advertiseTargets(server.context, true);
        auto capabilities = server.context.value("capabilities").toArray();
        capabilities.append("camera.control.v1");
        server.context.insert("capabilities", capabilities);
        auto permissions = server.context.value("permissions").toObject();
        auto scopes = permissions.value("scopes").toArray();
        scopes.append("control:read");
        permissions.insert("scopes", scopes);
        server.context.insert("permissions", permissions);
        server.overrides.insert(CAMERA_PATH, {200, cameraStatus(server.context), {}});
    }

    bool start()
    {
        if (!server.start()) {
            return false;
        }
        configure(client, server);
        client.setCameraRequested(true);
        return signIn(client) && QTest::qWaitFor([this]() { return client.cameraFresh(); }, TestTimeout::mediumMs());
    }

    CompanionServer server;
    PixEagleClient client;
};

qsizetype cameraOperationCount(const CompanionServer& server, const QString& operation)
{
    qsizetype count = 0;
    for (const auto& request : server.requests) {
        if (request.target.endsWith(CONTROL_PATH) &&
            QJsonDocument::fromJson(request.body).object().value("operation").toString() == operation) {
            ++count;
        }
    }
    return count;
}

}  // namespace

void PixEagleCameraClientTest::_capabilityAndPermissionGates()
{
    CameraHarness test;
    QVERIFY(test.start());
    const auto context = test.client.captureCameraContext();
    QVERIFY(!context.isEmpty());
    QVERIFY(!test.client.cameraStep("roll", 1, context));
    QVERIFY(!test.client.cameraStep("pan", 0, context));
    QVERIFY(!test.client.cameraStep("pan", 1, "old"));
    QVERIFY(test.server.lastRequest(CONTROL_PATH).target.isEmpty());
    auto permissions = test.server.context.value("permissions").toObject();
    permissions.insert("scopes", QJsonArray{"status:read", "telemetry:read", "control:read", "media:read"});
    test.server.context.insert("permissions", permissions);
    test.client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.busy(), TestTimeout::mediumMs());
    QVERIFY(!test.client.cameraStep("pan", 1, test.client.captureCameraContext()));
    test.server.context.insert("capabilities", QJsonArray{});
    test.client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.busy(), TestTimeout::mediumMs());
    QVERIFY(!test.client.cameraAvailable());
}

void PixEagleCameraClientTest::_boundedStepAndCapturedStop()
{
    CameraHarness test;
    QVERIFY(test.start());
    const auto context = test.client.captureCameraContext();
    QVERIFY(test.client.cameraStep("pan", 1, context));
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(CONTROL_PATH).target.isEmpty(), TestTimeout::mediumMs());
    const auto request = test.server.lastRequest(CONTROL_PATH);
    const auto body = QJsonDocument::fromJson(request.body).object();
    QCOMPARE(body.value("operation").toString(), QStringLiteral("pan"));
    QCOMPARE(body.value("direction").toInt(), 1);
    QVERIFY(!body.contains("duration_ms"));  // Provider chooses its advertised bounded default.
    QCOMPARE(body.value("camera_context").toObject().value("guard").toObject().value("camera_id").toString(),
             QStringLiteral("camera-a"));
    QVERIFY(!body.value("camera_context").toObject().value("client_id").toString().isEmpty());
    QCOMPARE(request.headers.value("x-test-csrf"), QByteArray("csrf-first"));
    QCOMPARE(request.headers.value("cookie"), QByteArray("pixeagle_session=session-first"));
    QVERIFY(body.value("confirm").toBool());
    QVERIFY(!body.value("dry_run").toBool());
    QVERIFY(test.client.stopCamera(context));
    QTRY_COMPARE_WITH_TIMEOUT(
        QJsonDocument::fromJson(test.server.lastRequest(CONTROL_PATH).body).object().value("operation").toString(),
        QStringLiteral("stop"), TestTimeout::mediumMs());
}

void PixEagleCameraClientTest::_invalidAndReplacedOwner()
{
    CameraHarness test;
    QVERIFY(test.start());
    const auto captured = test.client.captureCameraContext();
    auto invalid = cameraStatus(test.server.context);
    invalid.insert("runtime_id", "different-runtime");
    test.server.overrides.insert(CAMERA_PATH, {200, invalid, {}});
    test.client.refreshCamera();
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraFresh(), TestTimeout::mediumMs());
    QVERIFY(!test.client.cameraStep("pan", 1, captured));
    auto replacement = cameraStatus(test.server.context);
    replacement.insert("camera_id", "camera-b");
    auto guard = replacement.value("guard").toObject();
    guard.insert("camera_id", "camera-b");
    replacement.insert("guard", guard);
    test.server.overrides.insert(CAMERA_PATH, {200, replacement, {}});
    test.client.refreshCamera();
    QTRY_VERIFY_WITH_TIMEOUT(test.client.cameraFresh(), TestTimeout::mediumMs());
    QVERIFY(!test.client.cameraStep("pan", 1, captured));
    QVERIFY(test.client.captureCameraContext() != captured);
}

void PixEagleCameraClientTest::_stopPreemptsPendingStep()
{
    CameraHarness test;
    QVERIFY(test.start());
    test.server.deferredPath = CONTROL_PATH;
    const auto context = test.client.captureCameraContext();
    QVERIFY(test.client.cameraStep("pan", 1, context));
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(CONTROL_PATH).target.isEmpty(), TestTimeout::mediumMs());
    QVERIFY(test.client.cameraActionPending());
    QVERIFY(test.client.stopCamera(context));
    QTRY_COMPARE_WITH_TIMEOUT(
        QJsonDocument::fromJson(test.server.lastRequest(CONTROL_PATH).body).object().value("operation").toString(),
        QStringLiteral("stop"), TestTimeout::mediumMs());
    QVERIFY(!test.client.cameraStep("tilt", 1, context));
}

void PixEagleCameraClientTest::_endpointChangeStopsOriginalOwner()
{
    CameraHarness test;
    QVERIFY(test.start());
    CompanionServer replacement("replacement");
    QVERIFY(replacement.start());
    test.server.deferredPath = CONTROL_PATH;
    const auto context = test.client.captureCameraContext();
    QVERIFY(test.client.cameraStep("pan", 1, context));
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(CONTROL_PATH).target.isEmpty(), TestTimeout::mediumMs());
    test.client.setEndpoint(replacement.endpoint());
    QTRY_COMPARE_WITH_TIMEOUT(
        QJsonDocument::fromJson(test.server.lastRequest(CONTROL_PATH).body).object().value("operation").toString(),
        QStringLiteral("stop"), TestTimeout::mediumMs());
    const auto body = QJsonDocument::fromJson(test.server.lastRequest(CONTROL_PATH).body).object();
    QCOMPARE(body.value("camera_context").toObject().value("guard").toObject().value("camera_id").toString(),
             QStringLiteral("camera-a"));
    QVERIFY(replacement.requests.isEmpty());
    QVERIFY(!test.client.authenticated());
    QVERIFY(!test.client.stopCamera(context));
}

void PixEagleCameraClientTest::_staleStatusKeepsCapturedStop()
{
    CameraHarness test;
    QVERIFY(test.start());
    test.server.deferredPath = CONTROL_PATH;
    const auto context = test.client.captureCameraContext();
    QVERIFY(test.client.cameraStep("pan", 1, context));
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(CONTROL_PATH).target.isEmpty(), TestTimeout::mediumMs());
    test.server.overrides.insert(CAMERA_PATH, {503, {}, {}});
    test.client.refreshCamera();
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraFresh(), TestTimeout::mediumMs());
    QTRY_COMPARE_WITH_TIMEOUT(
        QJsonDocument::fromJson(test.server.lastRequest(CONTROL_PATH).body).object().value("operation").toString(),
        QStringLiteral("stop"), TestTimeout::mediumMs());
    QVERIFY(!test.client.cameraStep("pan", 1, context));
}

void PixEagleCameraClientTest::_padHoldAndRelease()
{
    CameraHarness test;
    QVERIFY(test.start());
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/PixEagle/PixEagleCameraControls.qml")));
    QTRY_VERIFY_WITH_TIMEOUT(!component.isLoading(), TestTimeout::mediumMs());
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(600, 500);
    std::unique_ptr<QObject> panel(
        component.createWithInitialProperties({{"client", QVariant::fromValue(&test.client)},
                                               {"visible", false},
                                               {"parent", QVariant::fromValue(window.contentItem())}}));
    QVERIFY2(panel, qPrintable(component.errorString()));
    auto* item = qobject_cast<QQuickItem*>(panel.get());
    QVERIFY(item);
    item->setParentItem(window.contentItem());
    item->setVisible(true);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* pad = panel->findChild<QQuickItem*>("pixeagleCameraPad");
    QVERIFY(pad);
    QTRY_VERIFY_WITH_TIMEOUT(pad->isVisible() && pad->isEnabled(), TestTimeout::mediumMs());
    QVERIFY(pad->property("fixedCenter").toBool());
    auto* grip = panel->findChild<QQuickItem*>("pixeagleCameraGrip");
    QVERIFY(grip);
    const auto gripPoint = grip->mapToScene(QPointF(grip->width() / 2, grip->height() / 2)).toPoint();
    const auto initialPosition = item->position();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, gripPoint);
    QTest::mouseMove(&window, gripPoint + QPoint(20, 10));
    QTest::mouseMove(&window, gripPoint + QPoint(80, 60));
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, gripPoint + QPoint(80, 60));
    QTRY_VERIFY_WITH_TIMEOUT(item->position() != initialPosition, TestTimeout::mediumMs());
    QTRY_COMPARE_WITH_TIMEOUT(pad->property("stickPositionY").toDouble(), pad->height() / 2, TestTimeout::mediumMs());
    QCOMPARE(pad->property("xAxis").toDouble(), 0.0);
    QCOMPARE(pad->property("yAxis").toDouble(), 0.0);
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "centerCamera"));
    QTRY_COMPARE_WITH_TIMEOUT(cameraOperationCount(test.server, "home"), 1, TestTimeout::mediumMs());
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraActionPending() && test.client.cameraFresh(), TestTimeout::mediumMs());
    QVERIFY(!panel->property("_gestureActive").toBool());
    const auto center = pad->mapToScene(QPointF(pad->width() / 2, pad->height() / 2)).toPoint();
    const auto right = pad->mapToScene(QPointF(pad->width() * 0.9, pad->height() / 2)).toPoint();
    auto* timer = panel->findChild<QObject*>("pixeagleCameraMotionTimer");
    QVERIFY(timer);
    QSignalSpy ticks(timer, SIGNAL(triggered()));
    QVERIFY(ticks.isValid());
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, center);
    QTRY_VERIFY_WITH_TIMEOUT(ticks.count() >= 2, TestTimeout::mediumMs());
    QVERIFY(!panel->property("_releaseRequired").toBool());
    QCOMPARE(cameraOperationCount(test.server, "pan"), 0);
    QTest::mouseMove(&window, right);
    QTRY_VERIFY_WITH_TIMEOUT(cameraOperationCount(test.server, "manual_update") >= 2, TestTimeout::mediumMs());
    QCOMPARE(cameraOperationCount(test.server, "tilt"), 0);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, right);
    QTRY_VERIFY_WITH_TIMEOUT(cameraOperationCount(test.server, "stop") >= 1, TestTimeout::mediumMs());
    QVERIFY(!panel->property("_gestureActive").toBool());
    QVERIFY(!pad->property("touchActive").toBool());
    QCOMPARE(pad->property("xAxis").toDouble(), 0.0);
    QCOMPARE(pad->property("yAxis").toDouble(), 0.0);
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraActionPending() && test.client.cameraFresh(), TestTimeout::mediumMs());
    const auto tiltBegins = cameraOperationCount(test.server, "manual_begin");
    const auto up = pad->mapToScene(QPointF(pad->width() / 2, pad->height() * 0.1)).toPoint();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, center);
    QTest::mouseMove(&window, up);
    QTRY_VERIFY_WITH_TIMEOUT(cameraOperationCount(test.server, "manual_begin") > tiltBegins, TestTimeout::mediumMs());
    QJsonObject tiltIntent;
    for (auto it = test.server.requests.crbegin(); it != test.server.requests.crend(); ++it) {
        const auto body = QJsonDocument::fromJson(it->body).object();
        if (it->target.endsWith(CONTROL_PATH) && body.value("operation").toString() == "manual_begin") {
            tiltIntent = body.value("intent").toObject();
            break;
        }
    }
    QCOMPARE(tiltIntent.value("axis").toString(), QStringLiteral("tilt"));
    QVERIFY(tiltIntent.value("value").toDouble() < 0);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, up);
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraActionPending() && test.client.cameraFresh(), TestTimeout::mediumMs());
    const auto count = cameraOperationCount(test.server, "pan");
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "updateMotion"));
    QCOMPARE(cameraOperationCount(test.server, "pan"), count);
}

void PixEagleCameraClientTest::_padFailureRequiresRelease()
{
    CameraHarness test;
    QVERIFY(test.start());
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/PixEagle/PixEagleCameraControls.qml")));
    QTRY_VERIFY_WITH_TIMEOUT(!component.isLoading(), TestTimeout::mediumMs());
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(600, 500);
    std::unique_ptr<QObject> panel(
        component.createWithInitialProperties({{"client", QVariant::fromValue(&test.client)},
                                               {"visible", false},
                                               {"parent", QVariant::fromValue(window.contentItem())}}));
    QVERIFY2(panel, qPrintable(component.errorString()));
    auto* item = qobject_cast<QQuickItem*>(panel.get());
    QVERIFY(item);
    item->setParentItem(window.contentItem());
    item->setVisible(true);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto* pad = panel->findChild<QQuickItem*>("pixeagleCameraPad");
    QVERIFY(pad);
    QTRY_VERIFY_WITH_TIMEOUT(pad->isVisible() && pad->isEnabled(), TestTimeout::mediumMs());
    test.server.overrides.insert(CONTROL_PATH, {409, {}, {}});
    const auto right = pad->mapToScene(QPointF(pad->width() * 0.9, pad->height() / 2)).toPoint();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, right);
    QTRY_VERIFY_WITH_TIMEOUT(panel->property("_releaseRequired").toBool(), TestTimeout::mediumMs());
    QTRY_VERIFY_WITH_TIMEOUT(cameraOperationCount(test.server, "stop") >= 1, TestTimeout::mediumMs());
    QCOMPARE(cameraOperationCount(test.server, "manual_begin"), 1);
    QVERIFY(!panel->property("_gestureActive").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraActionPending(), TestTimeout::mediumMs());
    const auto stops = cameraOperationCount(test.server, "stop");
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "stop"));
    QTRY_VERIFY_WITH_TIMEOUT(cameraOperationCount(test.server, "stop") > stops, TestTimeout::mediumMs());
    QCOMPARE(cameraOperationCount(test.server, "manual_begin"), 1);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, right);
    QVERIFY(!panel->property("_releaseRequired").toBool());
}

void PixEagleCameraClientTest::_stalePollCannotReplacePostStopState()
{
    CameraHarness test;
    QVERIFY(test.start());
    const auto context = test.client.captureCameraContext();
    test.server.deferredPath = CAMERA_PATH;
    const auto previousPoll = test.server.lastRequestIndex(CAMERA_PATH);
    test.client.refreshCamera();
    QTRY_VERIFY_WITH_TIMEOUT(test.server.lastRequestIndex(CAMERA_PATH) > previousPoll, TestTimeout::mediumMs());
    const auto poll = test.server.lastRequestIndex(CAMERA_PATH);
    const auto obsolete = test.server.responseFor(test.server.requests[poll]);
    QVERIFY(test.client.beginCameraManual("pan", 0.5, context));
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraActionPending(), TestTimeout::mediumMs());
    QVERIFY(test.client.stopCamera(context));
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraActionPending(), TestTimeout::mediumMs());
    auto updated = cameraStatus(test.server.context);
    updated.insert("camera_generation", "after-stop");
    auto guard = updated.value("guard").toObject();
    guard.insert("camera_generation", "after-stop");
    updated.insert("guard", guard);
    test.server.overrides.insert(CAMERA_PATH, {200, updated, {}});
    test.server.deferredPath.clear();
    QVERIFY(test.server.respond(poll, obsolete));
    QTRY_COMPARE_WITH_TIMEOUT(test.client.cameraStatus().value("camera_generation").toString(),
                              QStringLiteral("after-stop"), TestTimeout::mediumMs());
    QCOMPARE(cameraOperationCount(test.server, "stop"), 1);
}

void PixEagleCameraClientTest::_contextConflictRefreshesWithoutOperatorError()
{
    CameraHarness test;
    QVERIFY(test.start());
    test.server.overrides.insert(CONTROL_PATH, {409,
                                                {{"code", "camera_context_conflict"},
                                                 {"detail", "Camera, source or target changed. Refresh camera state."}},
                                                {}});

    const auto context = test.client.captureCameraContext();
    QVERIFY(test.client.cameraStep("pan", 1, context));
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraActionPending(), TestTimeout::mediumMs());
    QVERIFY(test.client.cameraError().isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(test.client.cameraFresh(), TestTimeout::mediumMs());
    QTRY_VERIFY_WITH_TIMEOUT(test.client.cameraManualState().isEmpty(), TestTimeout::mediumMs());
    QCOMPARE(cameraOperationCount(test.server, "pan"), 1);
}

void PixEagleCameraClientTest::_automaticStopPreservesFailure()
{
    CameraHarness test;
    QVERIFY(test.start());
    test.server.deferredPath = CONTROL_PATH;
    QVERIFY(test.client.beginCameraManual("pan", 0.5, test.client.captureCameraContext()));
    QTRY_COMPARE_WITH_TIMEOUT(cameraOperationCount(test.server, "manual_begin"), 1, TestTimeout::mediumMs());
    const auto request = test.server.lastRequestIndex(CONTROL_PATH);
    test.server.deferredPath.clear();
    QVERIFY(test.server.respond(request, {500, {}, {}}));
    QTRY_COMPARE_WITH_TIMEOUT(cameraOperationCount(test.server, "stop"), 1, TestTimeout::mediumMs());
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraActionPending(), TestTimeout::mediumMs());
    QVERIFY(test.client.cameraError().contains("unknown"));
    QVERIFY(!test.client.cameraCanStop());
}

void PixEagleCameraClientTest::_manualInputExpiry()
{
    CameraHarness test;
    QVERIFY(test.start());
    const auto context = test.client.captureCameraContext();
    QVERIFY(test.client.beginCameraManual("pan", 0.5, context));
    QTRY_COMPARE_WITH_TIMEOUT(cameraOperationCount(test.server, "stop"), 1, TestTimeout::mediumMs());
    QVERIFY(!test.client.updateCameraManual("pan", 0.5, context));
    QCOMPARE(cameraOperationCount(test.server, "manual_begin"), 1);
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraActionPending(), TestTimeout::mediumMs());
}

void PixEagleCameraClientTest::_manualLatestInputAndLongHold()
{
    CameraHarness test;
    QVERIFY(test.start());
    const auto context = test.client.captureCameraContext();
    QVERIFY(test.client.beginCameraManual("pan", 0.4, context));
    QTRY_VERIFY_WITH_TIMEOUT(!test.client.cameraActionPending(), TestTimeout::mediumMs());
    QString axis = "pan";
    double value = 0.4;
    QTimer input;
    input.setInterval(50);
    connect(&input, &QTimer::timeout, &test.client, [&]() { test.client.updateCameraManual(axis, value, context); });
    input.start();
    test.server.deferredPath = CONTROL_PATH;
    QTRY_COMPARE_WITH_TIMEOUT(cameraOperationCount(test.server, "manual_update"), 1, TestTimeout::mediumMs());
    const auto deferred = test.server.requests.size() - 1;
    axis = "tilt";
    value = -0.7;
    QVERIFY(test.client.updateCameraManual(axis, value, context));
    QCOMPARE(cameraOperationCount(test.server, "manual_update"), 1);
    test.server.deferredPath.clear();
    QVERIFY(test.server.respond(deferred, test.server.responseFor(test.server.requests[deferred])));
    QTRY_VERIFY_WITH_TIMEOUT(cameraOperationCount(test.server, "manual_update") >= 2, TestTimeout::mediumMs());
    const auto latest = QJsonDocument::fromJson(test.server.lastRequest(CONTROL_PATH).body).object();
    QCOMPARE(latest.value("intent").toObject().value("axis").toString(), QStringLiteral("tilt"));
    QCOMPARE(latest.value("intent").toObject().value("value").toDouble(), -0.7);
    QElapsedTimer hold;
    hold.start();
    // Exercise a live renewed hold beyond the former five-second cutoff.
    QTRY_VERIFY_WITH_TIMEOUT(hold.elapsed() > 5200 && cameraOperationCount(test.server, "manual_update") >= 50,
                             TestTimeout::longMs());
    QCOMPARE(cameraOperationCount(test.server, "stop"), 0);
    input.stop();
    QVERIFY(test.client.stopCamera(context));
    QTRY_COMPARE_WITH_TIMEOUT(cameraOperationCount(test.server, "stop"), 1, TestTimeout::mediumMs());
}

void PixEagleCameraClientTest::_manualReleaseDuringBegin()
{
    CameraHarness test;
    QVERIFY(test.start());
    test.server.deferredPath = CONTROL_PATH;
    const auto context = test.client.captureCameraContext();
    QVERIFY(test.client.beginCameraManual("pan", 1, context));
    QTRY_COMPARE_WITH_TIMEOUT(cameraOperationCount(test.server, "manual_begin"), 1, TestTimeout::mediumMs());
    const auto begin = QJsonDocument::fromJson(test.server.lastRequest(CONTROL_PATH).body).object();
    QVERIFY(test.client.stopCamera(context));
    QTRY_COMPARE_WITH_TIMEOUT(cameraOperationCount(test.server, "stop"), 1, TestTimeout::mediumMs());
    const auto stopped = QJsonDocument::fromJson(test.server.lastRequest(CONTROL_PATH).body).object();
    QCOMPARE(stopped.value("gesture_id"), begin.value("gesture_id"));
    QVERIFY(stopped.value("sequence").toInt() > begin.value("sequence").toInt());
    QCOMPARE(cameraOperationCount(test.server, "manual_update"), 0);
    QVERIFY(!test.client.updateCameraManual("tilt", -1, context));
}

UT_REGISTER_TEST_LIGHTWEIGHT(PixEagleCameraClientTest, TestLabel::Unit)
