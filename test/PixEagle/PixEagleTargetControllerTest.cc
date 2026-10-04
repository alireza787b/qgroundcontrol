#include "PixEagleTargetControllerTest.h"

#include <algorithm>
#include <memory>

#include <QtMultimedia/QVideoFrame>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickWindow>
#include <QtTest/QSignalSpy>

#include "PixEagleTargetController.h"
#include "PixEagleTestServer.h"
#include "QGCVideoFrameContextStore.h"

using namespace PixEagleTest;

namespace {
struct TargetHarness
{
    CompanionServer server;
    PixEagleClient client{nullptr, true};
    PixEagleTargetController controller;
    QQuickWindow window;
    PixEagleVideoItem surface{window.contentItem()};
    std::shared_ptr<QGCVideoFrameContextStore> store = std::make_shared<QGCVideoFrameContextStore>(8);
    quint64 epoch = store->beginEpoch();

    bool connect(const QString& mode = QStringLiteral("classic"), bool tracking = false, bool models = false)
    {
        if (!server.start()) {
            return false;
        }
        advertiseTargets(server.context, true);
        if (models) {
            advertiseModels(server.context);
            server.modelSnapshot = nativeModelInventory(server.context);
        }
        setContextField(server.context, "video", "variant", "processed_osd");
        setContextField(server.context, "video", "width", 32);
        setContextField(server.context, "video", "height", 24);
        server.targetSnapshot = nativeTargetState(server.context);
        server.targetSnapshot.insert("tracking_active", tracking);
        server.targetSnapshot.insert("target_status", tracking ? "tracking" : "idle");
        server.targetSnapshot.insert("mode", mode);
        setContextField(server.targetSnapshot, "guard", "mode", mode);
        if (mode == "external") {
            server.targetSnapshot.insert("external_selection_mode", "classic");
            server.targetSnapshot.insert(
                "external_selection_modes",
                QJsonArray{
                    QJsonObject{{"id", "classic"}, {"label", "Camera Classic"}, {"point", true}, {"rectangle", true}},
                    QJsonObject{{"id", "smart"}, {"label", "Camera Smart"}, {"point", true}, {"rectangle", false}}});
        }
        server.overrides.insert(
            TARGET_CATALOG_PATH,
            {200,
             {{"schema_version", 1},
              {"source", "tracking_catalog"},
              {"ui_trackers", QJsonArray{QJsonObject{{"name", "csrt"},
                                                     {"request_tracker_type", "CSRT"},
                                                     {"display_name", "Classic CSRT"},
                                                     {"available", true}},
                                         QJsonObject{{"name", "kcf"},
                                                     {"request_tracker_type", "KCF"},
                                                     {"display_name", "Classic KCF"},
                                                     {"available", true}},
                                         QJsonObject{{"name", "vendor_camera"},
                                                     {"available", true},
                                                     {"request_tracker_type", "VendorCamera"},
                                                     {"target_engine", "camera"},
                                                     {"display_name", "Camera tracker"}},
                                         QJsonObject{{"name", "smart"}, {"available", true}, {"smart_mode", true}},
                                         QJsonObject{{"name", "missing"}, {"available", false}}}},
              {"tracker_types", QJsonObject{}}},
             {}});
        configure(client, server);
        if (!signIn(client) ||
            !QTest::qWaitFor([this]() { return client.targetStateFresh() && !client.targetCatalog().isEmpty(); },
                             TestTimeout::mediumMs())) {
            return false;
        }
        QSignalSpy refreshed(&client, &PixEagleClient::targetChanged);
        controller.setClient(&client);
        controller.setSurface(&surface);
        return QTest::qWaitFor([&refreshed]() { return refreshed.count() >= 2; }, TestTimeout::mediumMs());
    }

    bool expose()
    {
        window.resize(128, 96);
        window.setColor(Qt::black);
        surface.setSize(QSizeF(128, 96));
        window.show();
        if (!QTest::qWaitForWindowExposed(&window, TestTimeout::mediumMs())) {
            return false;
        }
        QCoreApplication::processEvents();
        surface.setStream(store, server.context);
        return true;
    }

    QJsonObject submitFrame(int number, const QString& targetRevision = {})
    {
        auto provenance = server.context.value("video").toObject();
        provenance.remove("provenance_version");
        provenance.remove("ws_path");
        provenance.remove("width");
        provenance.remove("height");
        provenance.insert("version", "1");
        provenance.insert("instance_id", server.context.value("instance_id"));
        provenance.insert("runtime_id", server.context.value("runtime_id"));
        provenance.insert("frame_id", QString::number(number));
        provenance.insert("capture_id", QString::number(number));
        provenance.insert("capture_state", "fresh");
        provenance.insert("capture_age_ms", 0);
        provenance.insert("publication_age_ms", 0);
        provenance.insert("encoded_width", 32);
        provenance.insert("encoded_height", 24);
        provenance.insert("geometry_verified", false);
        const QJsonObject geometry{
            {"version", "1"},
            {"verified", true},
            {"geometry_id", "geometry-a"},
            {"mapping", "full_frame_scale"},
            {"encoded_width", 32},
            {"encoded_height", 24},
            {"analysis_width", 64},
            {"analysis_height", 48},
            {"target_revision",
             targetRevision.isEmpty() ? server.targetSnapshot.value("target_revision") : QJsonValue(targetRevision)},
            {"token", QStringLiteral("retained-%1").arg(number)},
            {"max_age_ms", 1500}};
        const QJsonObject metadata{
            {"type", "frame"}, {"frame_id", number}, {"provenance", provenance}, {"selection_geometry", geometry}};
        QImage image(32, 24, QImage::Format_RGBA8888);
        image.fill(number == 1 ? Qt::red : Qt::green);
        QVideoFrame frame(image);
        frame.setStartTime(store->insert(epoch, metadata));
        surface.videoSink()->setVideoFrame(frame);
        return {{"provenance", provenance}, {"selection_geometry", geometry}};
    }

    QString presentedNumber() const
    {
        return surface.capturePresentedContext().value(QStringLiteral("frame_id")).toString();
    }

    bool hasAction() const
    {
        return std::any_of(server.requests.cbegin(), server.requests.cend(),
                           [](const HttpRequest& request) { return request.target.contains("/api/v1/actions/"); });
    }

    int requestCount(const QByteArray& path) const
    {
        return static_cast<int>(std::count_if(server.requests.cbegin(), server.requests.cend(),
                                              [path](const HttpRequest& request) { return request.target == path; }));
    }

    void setTargetRevision(const QString& revision)
    {
        server.targetSnapshot.insert("target_revision", revision);
        setContextField(server.targetSnapshot, "guard", "target_revision", revision);
    }

    bool beginNativeSelection(const QByteArray& path)
    {
        if (!expose()) {
            return false;
        }
        submitFrame(1);
        if (!QTest::qWaitFor([this]() { return controller.canSelect(); }, TestTimeout::mediumMs())) {
            return false;
        }
        server.deferredPath = path;
        if (!controller.tapToTarget()) {
            controller.armSelection();
        }
        if (!controller.beginGesture(&surface, 32, 24)) {
            return false;
        }
        controller.finishGesture(32, 24, false);
        return QTest::qWaitFor([this, path]() { return !server.lastRequest(path).target.isEmpty(); },
                               TestTimeout::mediumMs());
    }

    bool confirmNativeSelection(const QByteArray& path)
    {
        setTargetRevision("2");
        server.targetSnapshot.insert("tracking_active", true);
        server.targetSnapshot.insert("target_status", "tracking");
        auto response = server.responseFor(server.lastRequest(path));
        setContextField(response.body, "result", "target_state", server.targetSnapshot);
        if (!server.respond(server.lastRequestIndex(path), response)) {
            return false;
        }
        return QTest::qWaitFor([this]() { return !controller.busy() && client.targetState() == server.targetSnapshot; },
                               TestTimeout::mediumMs());
    }
};
}  // namespace

void PixEagleTargetControllerTest::_selectionUsesPressedFrame_data()
{
    QTest::addColumn<QString>("mode");
    QTest::addColumn<bool>("tapToTarget");
    QTest::addColumn<bool>("rectangle");
    QTest::addColumn<QString>("action");
    for (const bool automatic : {false, true}) {
        const QByteArray prefix = automatic ? "tap-" : "manual-";
        QTest::newRow((prefix + "classic-point").constData())
            << QStringLiteral("classic") << automatic << false << QStringLiteral("tracking-start");
        QTest::newRow((prefix + "classic-rectangle").constData())
            << QStringLiteral("classic") << automatic << true << QStringLiteral("tracking-start");
        QTest::newRow((prefix + "smart-point").constData())
            << QStringLiteral("smart") << automatic << false << QStringLiteral("smart-click");
        QTest::newRow((prefix + "external-point").constData())
            << QStringLiteral("external") << automatic << false << QStringLiteral("gimbal-control");
        QTest::newRow((prefix + "external-rectangle").constData())
            << QStringLiteral("external") << automatic << true << QStringLiteral("gimbal-control");
    }
}

void PixEagleTargetControllerTest::_selectionUsesPressedFrame()
{
    QFETCH(QString, mode);
    QFETCH(bool, tapToTarget);
    QFETCH(bool, rectangle);
    QFETCH(QString, action);
    TargetHarness test;
    QVERIFY(test.connect(mode, true));
    test.controller.setTapToTarget(tapToTarget);
    QVERIFY(test.expose());
    const auto pressedFrame = test.submitFrame(1);
    QTRY_VERIFY_WITH_TIMEOUT(test.controller.canSelect(), TestTimeout::mediumMs());
    QVERIFY(test.controller.trackingActive());
    if (!tapToTarget) {
        QVERIFY(!test.controller.selectionArmed());
        QVERIFY(!test.controller.beginGesture(&test.surface, 32, 24));
        test.controller.armSelection();
    }
    QVERIFY(test.controller.selectionArmed());
    const auto pressedGuard = test.client.targetState().value("guard").toObject();
    QVERIFY(test.controller.beginGesture(&test.surface, 32, 24));
    test.submitFrame(2);
    QTRY_COMPARE_WITH_TIMEOUT(test.presentedNumber(), QStringLiteral("2"), TestTimeout::mediumMs());
    const auto path = QByteArray("/api/v1/actions/") + action.toLatin1();
    test.server.deferredPath = path;
    test.controller.finishGesture(96, 72, rectangle);
    QVERIFY(!test.controller.selectionArmed());
    QVERIFY(test.controller.busy());
    QVERIFY(test.controller.statusText().contains("Updating"));
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(path).target.isEmpty(), TestTimeout::mediumMs());
    const auto body = QJsonDocument::fromJson(test.server.lastRequest(path).body).object();
    const auto native = body.value("native_context").toObject();
    QCOMPARE(native.value("frame").toObject(), pressedFrame);
    QCOMPARE(native.value("guard").toObject(), pressedGuard);
    if (mode == "external") {
        QCOMPARE(body.value("operation").toString(), QStringLiteral("select"));
        QCOMPARE(body.value("x").toDouble(), rectangle ? 0.5 : 0.25);
        QCOMPARE(body.value("y").toDouble(), rectangle ? 0.5 : 0.25);
        if (rectangle) {
            QCOMPARE(body.value("width").toDouble(), 0.5);
            QCOMPARE(body.value("height").toDouble(), 0.5);
        }
    } else {
        const auto coordinates = body.value(mode == "smart" ? "click" : rectangle ? "bbox" : "point").toObject();
        QCOMPARE(coordinates.value("coordinate_space").toString(), QStringLiteral("normalized"));
        QCOMPARE(coordinates.value("x").toDouble(), 0.25);
        QCOMPARE(coordinates.value("y").toDouble(), 0.25);
        if (rectangle) {
            QCOMPARE(coordinates.value("width").toDouble(), 0.5);
            QCOMPARE(coordinates.value("height").toDouble(), 0.5);
        }
    }
}

void PixEagleTargetControllerTest::_tapRetargetUsesFreshContext_data()
{
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QByteArray>("path");
    QTest::newRow("classic") << QStringLiteral("classic") << QByteArray("/api/v1/actions/tracking-start");
    QTest::newRow("smart") << QStringLiteral("smart") << QByteArray("/api/v1/actions/smart-click");
    QTest::newRow("external") << QStringLiteral("external") << QByteArray("/api/v1/actions/gimbal-control");
}

void PixEagleTargetControllerTest::_tapRetargetUsesFreshContext()
{
    QFETCH(QString, mode);
    QFETCH(QByteArray, path);
    TargetHarness test;
    QVERIFY(test.connect(mode));
    QVERIFY(test.controller.tapToTarget());
    QVERIFY(!test.hasAction());
    QVERIFY(!test.controller.selectionArmed());
    QVERIFY(!test.controller.beginGesture(&test.surface, 32, 24));
    QVERIFY(test.beginNativeSelection(path));
    QVERIFY(!test.controller.selectionArmed());
    const auto first = QJsonDocument::fromJson(test.server.lastRequest(path).body).object();
    QVERIFY(test.confirmNativeSelection(path));
    const auto pressedFrame = test.submitFrame(2);
    QTRY_COMPARE_WITH_TIMEOUT(test.presentedNumber(), QStringLiteral("2"), TestTimeout::mediumMs());
    QVERIFY(test.controller.beginGesture(&test.surface, 64, 48));
    test.controller.cancelPointerGesture();
    QVERIFY(test.controller.selectionArmed());
    QVERIFY(!test.surface.selectionActive());
    QCOMPARE(test.requestCount(path), 1);
    QVERIFY(test.controller.beginGesture(&test.surface, 64, 48));
    const auto pressedGuard = test.client.targetState().value("guard").toObject();
    test.submitFrame(3);
    QTRY_COMPARE_WITH_TIMEOUT(test.presentedNumber(), QStringLiteral("3"), TestTimeout::mediumMs());
    test.controller.finishGesture(64, 48, false);
    QTRY_COMPARE_WITH_TIMEOUT(test.requestCount(path), 2, TestTimeout::mediumMs());
    const auto second = QJsonDocument::fromJson(test.server.lastRequest(path).body).object();
    const auto native = second.value("native_context").toObject();
    QCOMPARE(native.value("frame").toObject(), pressedFrame);
    QCOMPARE(native.value("guard").toObject(), pressedGuard);
    QVERIFY(second.value("idempotency_key") != first.value("idempotency_key"));
    const auto point = mode == "external" ? second : second.value(mode == "smart" ? "click" : "point").toObject();
    QCOMPARE(point.value("x").toDouble(), 0.5);
    QCOMPARE(point.value("y").toDouble(), 0.5);
    QVERIFY(!test.controller.selectionArmed());
    QVERIFY(test.controller.busy());
}

void PixEagleTargetControllerTest::_manualSelectionIsOneShot_data()
{
    QTest::addColumn<QString>("outcome");
    QTest::addColumn<int>("httpStatus");
    QTest::newRow("accepted-tracking") << QStringLiteral("tracking") << 200;
    QTest::newRow("unknown") << QStringLiteral("unknown") << 500;
    QTest::newRow("rejected") << QStringLiteral("rejected") << 403;
    QTest::newRow("conflict") << QStringLiteral("conflict") << 409;
    QTest::newRow("accepted-idle") << QStringLiteral("idle") << 200;
    QTest::newRow("accepted-following") << QStringLiteral("following") << 200;
    QTest::newRow("accepted-changed-mode") << QStringLiteral("mode") << 200;
    QTest::newRow("accepted-changed-external-mode") << QStringLiteral("external-mode") << 200;
    QTest::newRow("accepted-after-disarm") << QStringLiteral("disarm") << 200;
    QTest::newRow("accepted-after-qml-view-cancel") << QStringLiteral("qml-disarm") << 200;
    QTest::newRow("accepted-after-source-change") << QStringLiteral("source") << 200;
}

void PixEagleTargetControllerTest::_tapOutcomeNeverReplaysGesture_data()
{
    QTest::addColumn<int>("httpStatus");
    QTest::newRow("accepted") << 200;
    QTest::newRow("unknown") << 500;
    QTest::newRow("conflict") << 409;
}

void PixEagleTargetControllerTest::_tapOutcomeNeverReplaysGesture()
{
    QFETCH(int, httpStatus);
    TargetHarness test;
    QVERIFY(test.connect());
    const QByteArray path = "/api/v1/actions/tracking-start";
    QVERIFY(test.beginNativeSelection(path));
    test.controller.cancelGesture();
    QVERIFY(!test.controller.beginGesture(&test.surface, 64, 48));
    test.setTargetRevision("2");
    test.server.targetSnapshot.insert("tracking_active", true);
    test.server.targetSnapshot.insert("target_status", "tracking");
    auto response = test.server.responseFor(test.server.lastRequest(path));
    response.status = httpStatus;
    setContextField(response.body, "result", "target_state", test.server.targetSnapshot);
    QSignalSpy finished(&test.client, &PixEagleClient::targetActionFinished);
    QVERIFY(test.server.respond(test.server.lastRequestIndex(path), response));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, TestTimeout::mediumMs());
    QTRY_COMPARE_WITH_TIMEOUT(test.client.targetState(), test.server.targetSnapshot, TestTimeout::mediumMs());
    test.submitFrame(2);
    QTRY_COMPARE_WITH_TIMEOUT(test.presentedNumber(), QStringLiteral("2"), TestTimeout::mediumMs());
    QTRY_VERIFY_WITH_TIMEOUT(test.controller.selectionArmed(), TestTimeout::mediumMs());
    test.controller.finishGesture(64, 48, false);
    QCOMPARE(test.requestCount(path), 1);
    QVERIFY(test.controller.beginGesture(&test.surface, 64, 48));
    test.controller.finishGesture(64, 48, false);
    QTRY_COMPARE_WITH_TIMEOUT(test.requestCount(path), 2, TestTimeout::mediumMs());
}

void PixEagleTargetControllerTest::_manualSelectionIsOneShot()
{
    QFETCH(QString, outcome);
    QFETCH(int, httpStatus);
    TargetHarness test;
    QVERIFY(test.connect(outcome == "external-mode" ? "external" : "classic"));
    test.controller.setTapToTarget(false);
    const QByteArray path =
        outcome == "external-mode" ? "/api/v1/actions/gimbal-control" : "/api/v1/actions/tracking-start";
    QVERIFY(test.beginNativeSelection(path));
    test.setTargetRevision("2");
    test.server.targetSnapshot.insert("tracking_active", outcome != "idle");
    test.server.targetSnapshot.insert("target_status", outcome == "idle" ? "idle" : "tracking");
    if (outcome == "following") {
        test.server.targetSnapshot.insert("following_active", true);
    } else if (outcome == "mode") {
        test.server.targetSnapshot.insert("mode", "smart");
        setContextField(test.server.targetSnapshot, "guard", "mode", "smart");
    } else if (outcome == "external-mode") {
        test.server.targetSnapshot.insert("external_selection_mode", "smart");
    } else if (outcome == "disarm") {
        test.controller.cancelGesture();
    } else if (outcome == "qml-disarm") {
        QQmlEngine engine;
        QQmlComponent component{
            &engine, QUrl(QStringLiteral("qrc:/qml/QGroundControl/FlyView/OnScreenCameraTrackingController.qml"))};
        std::unique_ptr<QObject> adapter{
            component.createWithInitialProperties({{"camera", QVariant::fromValue<QObject*>(nullptr)},
                                                   {"videoWidth", 128},
                                                   {"videoHeight", 96},
                                                   {"externalController", QVariant::fromValue(&test.controller)}})};
        QVERIFY2(adapter, qPrintable(component.errorString()));
        QVERIFY(QMetaObject::invokeMethod(adapter.get(), "beginGesture",
                                          Q_ARG(QVariant, QVariant::fromValue(&test.surface)), Q_ARG(QVariant, 32),
                                          Q_ARG(QVariant, 24)));
        QVERIFY(!adapter->property("_gestureController").value<QObject*>());
        QVERIFY(QMetaObject::invokeMethod(adapter.get(), "cancelSelection"));
    } else if (outcome == "source") {
        test.surface.clearStream();
    }
    auto response = test.server.responseFor(test.server.lastRequest(path));
    response.status = httpStatus;
    setContextField(response.body, "result", "target_state", test.server.targetSnapshot);
    QSignalSpy finished(&test.client, &PixEagleClient::targetActionFinished);
    QVERIFY(test.server.respond(test.server.lastRequestIndex(path), response));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, TestTimeout::mediumMs());
    QTRY_COMPARE_WITH_TIMEOUT(test.client.targetState(), test.server.targetSnapshot, TestTimeout::mediumMs());
    test.submitFrame(2);
    if (outcome != "source") {
        QTRY_COMPARE_WITH_TIMEOUT(test.presentedNumber(), QStringLiteral("2"), TestTimeout::mediumMs());
    }
    QVERIFY(!test.controller.selectionArmed());
    QVERIFY(!test.controller.beginGesture(&test.surface, 64, 48));
    QCOMPARE(test.requestCount(path), 1);
    if (outcome == "tracking") {
        test.controller.armSelection();
        QVERIFY(test.controller.beginGesture(&test.surface, 64, 48));
        test.controller.cancelGesture();
    }
}

void PixEagleTargetControllerTest::_contextChangesCancelSelection_data()
{
    QTest::addColumn<QString>("change");
    QTest::addColumn<bool>("tapToTarget");
    for (const bool automatic : {false, true}) {
        for (const auto* change : {"revision", "mode", "tracker", "session", "client", "source", "following",
                                   "view-disabled", "preference", "cancel", "pointer-cancel"}) {
            const QByteArray row = (automatic ? QByteArray("tap-") : QByteArray("manual-")) + change;
            QTest::newRow(row.constData()) << QString::fromLatin1(change) << automatic;
        }
    }
}

void PixEagleTargetControllerTest::_contextChangesCancelSelection()
{
    QFETCH(QString, change);
    QFETCH(bool, tapToTarget);
    TargetHarness test;
    QVERIFY(test.connect());
    test.controller.setTapToTarget(tapToTarget);
    QVERIFY(test.expose());
    test.submitFrame(1);
    QTRY_VERIFY_WITH_TIMEOUT(test.controller.canSelect(), TestTimeout::mediumMs());
    if (!tapToTarget) {
        test.controller.armSelection();
    }
    QVERIFY(test.controller.beginGesture(&test.surface, 32, 24));
    QSignalSpy invalidated(&test.controller, &PixEagleTargetController::gestureInvalidated);
    if (change == "session") {
        test.client.signOut();
    } else if (change == "client") {
        test.controller.setClient(nullptr);
    } else if (change == "source") {
        test.surface.clearStream();
    } else if (change == "view-disabled") {
        test.controller.setSelectionEnabled(false);
    } else if (change == "preference") {
        test.controller.setTapToTarget(!tapToTarget);
    } else if (change == "cancel") {
        test.controller.cancelGesture();
    } else if (change == "pointer-cancel") {
        test.controller.cancelPointerGesture();
    } else {
        test.setTargetRevision("2");
        if (change == "mode") {
            test.server.targetSnapshot.insert("mode", "smart");
            setContextField(test.server.targetSnapshot, "guard", "mode", "smart");
        } else if (change == "tracker") {
            test.server.targetSnapshot.insert("tracker_type", "KCF");
            setContextField(test.server.targetSnapshot, "guard", "tracker_type", "KCF");
        } else if (change == "following") {
            test.server.targetSnapshot.insert("following_active", true);
        }
        test.client.refreshTargetState();
        QTRY_COMPARE_WITH_TIMEOUT(test.client.targetState(), test.server.targetSnapshot, TestTimeout::mediumMs());
    }
    QTRY_VERIFY_WITH_TIMEOUT(!invalidated.isEmpty(), TestTimeout::mediumMs());
    test.controller.finishGesture(96, 72, true);
    QVERIFY(!test.hasAction());
    QVERIFY(!test.surface.selectionActive());

    if (change == "session" || change == "client" || change == "source" || change == "view-disabled") {
        QVERIFY(!test.controller.selectionArmed());
        QVERIFY(!test.controller.beginGesture(&test.surface, 32, 24));
    }
    if (change == "view-disabled") {
        test.controller.setSelectionEnabled(true);
    }
    if (change == "revision" || change == "mode" || change == "tracker" || change == "following" ||
        change == "view-disabled" || change == "preference" || change == "cancel" || change == "pointer-cancel") {
        test.submitFrame(2);
        QTRY_COMPARE_WITH_TIMEOUT(test.presentedNumber(), QStringLiteral("2"), TestTimeout::mediumMs());
        const bool automatic = test.controller.tapToTarget();
        QCOMPARE(test.controller.selectionArmed(), automatic || change == "pointer-cancel");
        QCOMPARE(test.controller.beginGesture(&test.surface, 32, 24), automatic || change == "pointer-cancel");
        QVERIFY(!test.hasAction());
        test.controller.cancelGesture();
    }
}

void PixEagleTargetControllerTest::_smartRectangleIsRejected()
{
    TargetHarness test;
    QVERIFY(test.connect("smart"));
    QVERIFY(test.expose());
    test.submitFrame(1);
    QTRY_VERIFY_WITH_TIMEOUT(test.controller.canSelect(), TestTimeout::mediumMs());
    QVERIFY(!test.controller.supportsRectangle());
    test.controller.armSelection();
    QVERIFY(test.controller.beginGesture(&test.surface, 32, 24));
    test.controller.finishGesture(96, 72, true);
    QVERIFY(!test.hasAction());
    QVERIFY(!test.controller.busy());
    QVERIFY(test.controller.instructionText().contains("single click"));
}

void PixEagleTargetControllerTest::_staleFrameRevisionIsRejected()
{
    TargetHarness test;
    QVERIFY(test.connect());
    test.controller.setTapToTarget(false);
    QVERIFY(test.expose());
    test.submitFrame(1, "0");
    QTRY_VERIFY_WITH_TIMEOUT(test.controller.canSelect(), TestTimeout::mediumMs());
    test.controller.armSelection();
    QVERIFY(test.controller.beginGesture(&test.surface, 32, 24));
    test.controller.finishGesture(96, 72, true);
    QVERIFY(!test.controller.busy());
    QVERIFY(!test.hasAction());
    QVERIFY(!test.controller.selectionArmed());
}

void PixEagleTargetControllerTest::_cancelUsesTrackingOwnerWithoutVideo_data()
{
    QTest::addColumn<QString>("mode");
    QTest::newRow("classic") << QStringLiteral("classic");
    QTest::newRow("smart") << QStringLiteral("smart");
    QTest::newRow("external") << QStringLiteral("external");
}

void PixEagleTargetControllerTest::_cancelUsesTrackingOwnerWithoutVideo()
{
    QFETCH(QString, mode);
    TargetHarness test;
    QVERIFY(test.connect(mode, true));
    QVERIFY(!test.controller.canSelect());
    QVERIFY(test.controller.canCancel());
    const QByteArray path = mode == "external" ? "/api/v1/actions/gimbal-control" : "/api/v1/actions/tracking-stop";
    test.server.deferredPath = path;
    test.controller.cancelTracking(test.controller.captureControlContext());
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(path).target.isEmpty(), TestTimeout::mediumMs());
    const auto body = QJsonDocument::fromJson(test.server.lastRequest(path).body).object();
    QVERIFY(!body.value("native_context").toObject().contains("frame"));
    QVERIFY(!body.contains("bbox"));
    if (mode == "external") {
        QCOMPARE(body.value("operation").toString(), QStringLiteral("cancel"));
    }
    QVERIFY(test.server.lastRequest("/api/v1/actions/offboard-stop").target.isEmpty());
    QVERIFY(test.server.lastRequest("/api/v1/actions/operator-abort").target.isEmpty());
}

void PixEagleTargetControllerTest::_lostTargetCanBeCancelledWithoutVideo_data()
{
    QTest::addColumn<QString>("mode");
    QTest::newRow("classic") << QStringLiteral("classic");
    QTest::newRow("camera") << QStringLiteral("external");
}

void PixEagleTargetControllerTest::_lostTargetCanBeCancelledWithoutVideo()
{
    QFETCH(QString, mode);
    TargetHarness test;
    QVERIFY(test.connect(mode, mode == "external"));
    test.server.targetSnapshot.insert("target_status", "lost");
    test.client.refreshTargetState();
    QTRY_VERIFY_WITH_TIMEOUT(test.controller.statusText().contains("Target lost"), TestTimeout::mediumMs());
    QCOMPARE(test.controller.trackingActive(), mode == "external");
    QCOMPARE(test.controller.trackingState(), QStringLiteral("lost"));
    QVERIFY(!test.controller.canSelect());
    QVERIFY(test.controller.canCancel());
    const auto guard = test.client.targetState().value("guard").toObject();
    const QString token = test.controller.captureControlContext();
    QVERIFY(!token.isEmpty());
    const QByteArray path = mode == "external" ? "/api/v1/actions/gimbal-control" : "/api/v1/actions/tracking-stop";
    test.server.deferredPath = path;
    QSignalSpy finished(&test.client, &PixEagleClient::targetActionFinished);
    test.controller.cancelTracking(token);
    QVERIFY(test.controller.busy());
    QCOMPARE(test.controller.trackingState(), QStringLiteral("updating"));
    QCOMPARE(test.client.targetState().value("target_status").toString(), QStringLiteral("lost"));
    QVERIFY(finished.isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(path).target.isEmpty(), TestTimeout::mediumMs());
    const auto request = test.server.lastRequest(path);
    const auto body = QJsonDocument::fromJson(request.body).object();
    const auto native = body.value("native_context").toObject();
    QCOMPARE(native.value("guard").toObject(), guard);
    QVERIFY(!native.contains("frame"));
    QVERIFY(test.server.lastRequest("/api/v1/actions/offboard-stop").target.isEmpty());
    QVERIFY(test.server.lastRequest("/api/v1/actions/operator-abort").target.isEmpty());
    test.server.targetSnapshot = nativeTargetState(test.server.context, "2");
    test.server.targetSnapshot.insert("target_status", "idle");
    auto response = test.server.responseFor(request);
    setContextField(response.body, "result", "target_state", test.server.targetSnapshot);
    QVERIFY(test.server.respond(test.server.lastRequestIndex(path), response));
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, TestTimeout::mediumMs());
    QCOMPARE(finished.first()[1].toString(), QStringLiteral("accepted"));
    QTRY_COMPARE_WITH_TIMEOUT(test.controller.statusText(), QStringLiteral("No target selected"),
                              TestTimeout::mediumMs());
    QVERIFY(!test.controller.canCancel());
    QCOMPARE(test.controller.trackingState(), QStringLiteral("idle"));
}

void PixEagleTargetControllerTest::_trackingStateExpiresAndRecovers()
{
    TargetHarness test;
    QCOMPARE(test.controller.trackingState(), QStringLiteral("unknown"));
    QVERIFY(test.connect("external", true));
    QCOMPARE(test.controller.trackingState(), QStringLiteral("tracking"));

    test.server.deferredPath = TARGET_STATE_PATH;
    test.client.refreshTargetState();
    QSignalSpy changed(&test.controller, &PixEagleTargetController::changed);
    QTRY_COMPARE_WITH_TIMEOUT(test.controller.trackingState(), QStringLiteral("unknown"), TestTimeout::longMs());
    QVERIFY(!changed.isEmpty());
    QVERIFY(!test.client.targetStateFresh());
    QVERIFY(!test.controller.trackingActive());
    QVERIFY(!test.controller.canCancel());

    test.server.deferredPath.clear();
    test.client.refreshTargetState();
    QTRY_COMPARE_WITH_TIMEOUT(test.controller.trackingState(), QStringLiteral("tracking"), TestTimeout::mediumMs());
    QVERIFY(test.controller.canCancel());
}

void PixEagleTargetControllerTest::_modeAndTrackerRequests_data()
{
    QTest::addColumn<QString>("operation");
    QTest::newRow("smart-mode") << QStringLiteral("smart-mode");
    QTest::newRow("external-mode") << QStringLiteral("external-mode");
    QTest::newRow("tracker") << QStringLiteral("tracker");
}

void PixEagleTargetControllerTest::_modeAndTrackerRequests()
{
    QFETCH(QString, operation);
    TargetHarness test;
    QVERIFY(test.connect(operation == "external-mode" ? "external" : "classic"));
    QCOMPARE(test.controller.trackerChoices().size(), 2);
    QCOMPARE(test.controller.trackerIndex(), 0);
    const QByteArray path = operation == "tracker"         ? "/api/v1/actions/tracker-switch"
                            : operation == "external-mode" ? "/api/v1/actions/gimbal-control"
                                                           : "/api/v1/actions/smart-mode-toggle";
    test.server.deferredPath = path;
    if (operation == "tracker") {
        test.controller.selectTracker(1, test.controller.captureControlContext());
    } else {
        test.controller.setSmartMode(true, test.controller.captureControlContext());
    }
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(path).target.isEmpty(), TestTimeout::mediumMs());
    const auto body = QJsonDocument::fromJson(test.server.lastRequest(path).body).object();
    QVERIFY(!body.value("native_context").toObject().contains("frame"));
    if (operation == "tracker") {
        QCOMPARE(body.value("tracker_type").toString(), QStringLiteral("KCF"));
        QCOMPARE(body.value("persist").toBool(), false);
    } else if (operation == "external-mode") {
        QCOMPARE(body.value("operation").toString(), QStringLiteral("set_mode"));
        QCOMPARE(body.value("selection_mode").toString(), QStringLiteral("smart"));
    } else {
        QCOMPARE(body.value("enabled").toBool(), true);
    }
}

void PixEagleTargetControllerTest::_trackerCatalogAliases_data()
{
    QTest::addColumn<QString>("currentTracker");
    QTest::addColumn<int>("currentIndex");
    QTest::newRow("csrt-factory") << QStringLiteral("CSRT") << 0;
    QTest::newRow("csrt-schema") << QStringLiteral("CSRTTracker") << 0;
    QTest::newRow("csrt-case") << QStringLiteral("csrttracker") << 0;
    QTest::newRow("kcf-factory") << QStringLiteral("KCF") << 1;
    QTest::newRow("kcf-schema") << QStringLiteral("KCFKalmanTracker") << 1;
}

void PixEagleTargetControllerTest::_trackerCatalogAliases()
{
    QFETCH(QString, currentTracker);
    QFETCH(int, currentIndex);
    TargetHarness test;
    QVERIFY(test.connect());
    const QJsonObject catalog{
        {"schema_version", 1},
        {"source", "tracking_catalog"},
        {"ui_trackers", QJsonArray{QJsonObject{{"name", "CSRTTracker"},
                                               {"request_tracker_type", "CSRTTracker"},
                                               {"factory_key", "CSRT"},
                                               {"display_name", "CSRT"},
                                               {"available", true}},
                                   QJsonObject{{"name", "KCFKalmanTracker"},
                                               {"request_tracker_type", "KCFKalmanTracker"},
                                               {"factory_key", "KCF"},
                                               {"display_name", "KCF + Kalman"},
                                               {"available", true}},
                                   QJsonObject{{"name", "VitTrackTracker"},
                                               {"request_tracker_type", "VitTrackTracker"},
                                               {"factory_key", "VitTrack"},
                                               {"display_name", "VitTrack"},
                                               {"available", false},
                                               {"unavailable_reason", "VitTrack model is missing."}}}},
        {"tracker_types",
         QJsonObject{{"CSRT", QJsonObject{{"name", "CSRT"},
                                          {"request_tracker_type", "CSRT"},
                                          {"display_name", "CSRT Tracker"},
                                          {"available", true}}},
                     {"Gimbal", QJsonObject{{"name", "Gimbal"}, {"available", false}}},
                     {"SmartTracker", QJsonObject{{"name", "SmartTracker"}, {"smart_mode", true}, {"available", true}}},
                     {"VitTrack", QJsonObject{{"name", "VitTrack"},
                                              {"available", false},
                                              {"unavailable_reason", "Duplicate factory reason."}}}}}};
    test.server.overrides.insert(TARGET_CATALOG_PATH, {200, catalog, {}});
    test.server.targetSnapshot.insert("tracker_type", currentTracker);
    setContextField(test.server.targetSnapshot, "guard", "tracker_type", currentTracker);
    test.client.refreshTargetState();
    QTRY_COMPARE_WITH_TIMEOUT(test.client.targetCatalog(), catalog, TestTimeout::mediumMs());
    QTRY_COMPARE_WITH_TIMEOUT(test.client.targetState().value("tracker_type").toString(), currentTracker,
                              TestTimeout::mediumMs());
    const auto choices = test.controller.trackerChoices();
    QCOMPARE(choices.size(), 2);
    QCOMPARE(choices[0].toMap().value("label").toString(), QStringLiteral("CSRT"));
    QCOMPARE(choices[1].toMap().value("label").toString(), QStringLiteral("KCF + Kalman"));
    QCOMPARE(test.controller.trackerIndex(), currentIndex);
    const auto unavailable = test.controller.unavailableTrackers();
    QCOMPARE(unavailable.size(), 2);
    QCOMPARE(unavailable[0].toMap().value("label").toString(), QStringLiteral("VitTrack"));
    QCOMPARE(unavailable[0].toMap().value("reason").toString(), QStringLiteral("VitTrack model is missing."));
    QCOMPARE(unavailable[1].toMap().value("label").toString(), QStringLiteral("Gimbal"));
    QVERIFY(!unavailable[1].toMap().value("reason").toString().isEmpty());
    test.controller.selectTracker(currentIndex, test.controller.captureControlContext());
    QVERIFY(!test.hasAction());

    const QByteArray path = "/api/v1/actions/tracker-switch";
    test.server.deferredPath = path;
    test.controller.selectTracker(1 - currentIndex, test.controller.captureControlContext());
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest(path).target.isEmpty(), TestTimeout::mediumMs());
    const auto body = QJsonDocument::fromJson(test.server.lastRequest(path).body).object();
    QCOMPARE(body.value("tracker_type").toString(),
             currentIndex == 0 ? QStringLiteral("KCFKalmanTracker") : QStringLiteral("CSRTTracker"));
    QCOMPARE(body.value("persist").toBool(), false);
}

void PixEagleTargetControllerTest::_viewOnlyAndFollowingBlockControls_data()
{
    QTest::addColumn<bool>("following");
    QTest::newRow("following") << true;
    QTest::newRow("viewer") << false;
}

void PixEagleTargetControllerTest::_viewOnlyAndFollowingBlockControls()
{
    QFETCH(bool, following);
    TargetHarness test;
    QVERIFY(test.connect("classic", true));
    if (following) {
        test.server.targetSnapshot.insert("following_active", true);
    } else {
        setContextField(test.server.context, "permissions", "scopes",
                        QJsonArray{"status:read", "telemetry:read", "media:read"});
    }
    test.client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(test.client.targetStateFresh() && test.client.targetWriteAllowed() == following,
                             TestTimeout::mediumMs());
    if (following) {
        QVERIFY(test.expose());
        test.submitFrame(1);
        QTRY_VERIFY_WITH_TIMEOUT(test.controller.canSelect(), TestTimeout::mediumMs());
    }
    QVERIFY(!test.controller.canCancel());
    QVERIFY(!test.controller.canChangeMode());
    QVERIFY(!test.controller.canConfigure());
    test.controller.armSelection();
    test.controller.cancelTracking(test.controller.captureControlContext());
    test.controller.setSmartMode(true, test.controller.captureControlContext());
    test.controller.selectTracker(1, test.controller.captureControlContext());
    QCOMPARE(test.controller.selectionArmed(), following);
    QVERIFY(!test.hasAction());
    QVERIFY(test.controller.instructionText().contains(following ? "Tap another target" : "View only"));
}

void PixEagleTargetControllerTest::_unknownOutcomeRemainsVisible()
{
    TargetHarness test;
    QVERIFY(test.connect("classic", true));
    test.server.overrides.insert("/api/v1/actions/tracking-stop", {500, {}, {}});
    test.controller.cancelTracking(test.controller.captureControlContext());
    QTRY_VERIFY_WITH_TIMEOUT(test.controller.instructionText().contains("outcome is unknown"), TestTimeout::mediumMs());
    QTRY_VERIFY_WITH_TIMEOUT(test.client.targetStateFresh(), TestTimeout::mediumMs());
    QVERIFY(test.controller.instructionText().contains("outcome is unknown"));
    QCOMPARE(test.controller.statusText(), QStringLiteral("Tracking target"));
    test.controller.setClient(nullptr);
    QVERIFY(!test.controller.instructionText().contains("outcome is unknown"));
}

void PixEagleTargetControllerTest::_controlIntentCannotCrossContext_data()
{
    QTest::addColumn<QString>("operation");
    QTest::addColumn<QString>("change");
    for (const auto* operation : {"cancel", "mode", "tracker"}) {
        for (const auto* change : {"client", "revision", "mode", "signout"}) {
            const QByteArray row = QByteArray(operation) + '-' + change;
            QTest::newRow(row.constData()) << QString::fromLatin1(operation) << QString::fromLatin1(change);
        }
    }
}

void PixEagleTargetControllerTest::_controlIntentCannotCrossContext()
{
    QFETCH(QString, operation);
    QFETCH(QString, change);
    TargetHarness test;
    TargetHarness other;
    QVERIFY(test.connect("classic", true));
    const QString token = test.controller.captureControlContext();
    QVERIFY(!token.isEmpty());
    if (change == "client") {
        QVERIFY(other.connect("classic", true));
        test.controller.setClient(&other.client);
        QVERIFY(test.controller.canCancel());
    } else if (change == "signout") {
        test.client.signOut();
        QTRY_VERIFY_WITH_TIMEOUT(!test.client.authenticated(), TestTimeout::mediumMs());
    } else {
        test.server.targetSnapshot.insert("target_revision", "2");
        setContextField(test.server.targetSnapshot, "guard", "target_revision", "2");
        if (change == "mode") {
            test.server.targetSnapshot.insert("mode", "external");
            test.server.targetSnapshot.insert("external_selection_mode", "classic");
            test.server.targetSnapshot.insert(
                "external_selection_modes",
                QJsonArray{
                    QJsonObject{{"id", "classic"}, {"label", "Camera Classic"}, {"point", true}, {"rectangle", true}},
                    QJsonObject{{"id", "smart"}, {"label", "Camera Smart"}, {"point", true}, {"rectangle", false}}});
            setContextField(test.server.targetSnapshot, "guard", "mode", "external");
        }
        test.client.refreshTargetState();
        QTRY_COMPARE_WITH_TIMEOUT(test.client.targetState().value("target_revision").toString(), QStringLiteral("2"),
                                  TestTimeout::mediumMs());
        QVERIFY(test.controller.canCancel());
    }
    if (operation == "cancel") {
        test.controller.cancelTracking(token);
    } else if (operation == "mode") {
        test.controller.setSmartMode(true, token);
    } else {
        test.controller.selectTracker(1, token);
    }
    QVERIFY(!test.controller.busy());
    QVERIFY(!test.hasAction());
    QVERIFY(!other.hasAction());
}

void PixEagleTargetControllerTest::_modelControlOwnership_data()
{
    QTest::addColumn<QString>("change");
    for (const auto* change :
         {"unchanged", "same-request", "conflict-retry", "client", "revision", "generation", "closed", "signout"}) {
        QTest::newRow(change) << QString::fromLatin1(change);
    }
}

void PixEagleTargetControllerTest::_modelControlOwnership()
{
    QFETCH(QString, change);
    TargetHarness test;
    TargetHarness other;
    QVERIFY(test.connect("classic", false, true));
    test.controller.setModelsRequested(true);
    QTRY_VERIFY_WITH_TIMEOUT(test.controller.canSelectModel(), TestTimeout::mediumMs());
    const QString token = test.controller.captureModelContext();
    QVERIFY(!token.isEmpty());
    QVERIFY(!test.controller.canSelect());
    if (change == "same-request") {
        test.controller.setModelsRequested(true);
    } else if (change == "client") {
        QVERIFY(other.connect("classic", false, true));
        test.controller.setClient(&other.client);
        test.controller.setModelsRequested(true);
        QTRY_VERIFY_WITH_TIMEOUT(test.controller.canSelectModel(), TestTimeout::mediumMs());
    } else if (change == "revision") {
        test.setTargetRevision("2");
        test.server.modelSnapshot.insert("target_state", test.server.targetSnapshot);
        test.client.refreshTargetState();
        QTRY_COMPARE_WITH_TIMEOUT(test.client.targetState().value("target_revision").toString(), QStringLiteral("2"),
                                  TestTimeout::mediumMs());
    } else if (change == "generation") {
        test.server.modelSnapshot.insert("model_generation", QString(64, 'b'));
        test.controller.refreshModels();
        QTRY_COMPARE_WITH_TIMEOUT(test.client.modelInventory().value("model_generation").toString(), QString(64, 'b'),
                                  TestTimeout::mediumMs());
    } else if (change == "closed") {
        test.controller.setModelsRequested(false);
    } else if (change == "signout") {
        test.client.signOut();
        QTRY_VERIFY_WITH_TIMEOUT(!test.client.authenticated(), TestTimeout::mediumMs());
    }
    test.server.deferredPath = MODEL_SELECT_PATH;
    test.controller.selectModel(QStringLiteral("detector-a"), token);
    if (change == "unchanged" || change == "same-request" || change == "conflict-retry") {
        QTRY_COMPARE_WITH_TIMEOUT(test.requestCount(MODEL_SELECT_PATH), 1, TestTimeout::mediumMs());
        const auto body = QJsonDocument::fromJson(test.server.lastRequest(MODEL_SELECT_PATH).body).object();
        QCOMPARE(body.value("model_id").toString(), QStringLiteral("detector-a"));
        QCOMPARE(body.value("model_generation").toString(), QString(64, 'a'));
        QCOMPARE(body.value("native_context").toObject().value("guard"), test.server.targetSnapshot.value("guard"));
        if (change == "conflict-retry") {
            QSignalSpy finished(&test.client, &PixEagleClient::targetActionFinished);
            auto response = test.server.responseFor(test.server.lastRequest(MODEL_SELECT_PATH));
            response.status = 409;
            QVERIFY(test.server.respond(test.server.lastRequestIndex(MODEL_SELECT_PATH), response));
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, TestTimeout::mediumMs());
            QCOMPARE(finished.first()[1].toString(), QStringLiteral("conflict"));
            QVERIFY(test.controller.instructionText().contains("changed"));
            QTRY_VERIFY_WITH_TIMEOUT(test.controller.canSelectModel(), TestTimeout::mediumMs());
            test.controller.selectModel(QStringLiteral("detector-a"), test.controller.captureModelContext());
            QVERIFY(test.controller.busy());
            QVERIFY(test.controller.instructionText().contains("Waiting"));
            QVERIFY(test.controller.modelFeedbackText().isEmpty());
            QTRY_COMPARE_WITH_TIMEOUT(test.requestCount(MODEL_SELECT_PATH), 2, TestTimeout::mediumMs());
            response = test.server.responseFor(test.server.lastRequest(MODEL_SELECT_PATH));
            const auto result = response.body.value("result").toObject();
            test.server.targetSnapshot = result.value("target_state").toObject();
            test.server.modelSnapshot = result.value("model_inventory").toObject();
            QVERIFY(test.server.respond(test.server.lastRequestIndex(MODEL_SELECT_PATH), response));
            QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, TestTimeout::mediumMs());
            QCOMPARE(finished.last()[1].toString(), QStringLiteral("accepted"));
            QCOMPARE(test.controller.configuredModelId(), QStringLiteral("detector-a"));
            QVERIFY(!test.controller.instructionText().contains("changed"));
            QVERIFY(test.controller.modelFeedbackText().isEmpty());
            return;
        }
        test.controller.selectModel(QStringLiteral("detector-a"), token);
        QCOMPARE(test.requestCount(MODEL_SELECT_PATH), 1);
    } else {
        QVERIFY(!test.controller.busy());
        QVERIFY(!test.hasAction());
        QVERIFY(!other.hasAction());
    }
}

void PixEagleTargetControllerTest::_cameraModesAndEngineCatalog()
{
    TargetHarness test;
    QVERIFY(test.connect("external"));
    QCOMPARE(test.controller.selectionModes().size(), 2);
    QCOMPARE(test.controller.targetEngines().size(), 2);
    QCOMPARE(test.controller.trackerChoices().size(), 2);
    QCOMPARE(test.controller.selectionModes().first().toMap().value("label").toString(),
             QStringLiteral("Camera Classic"));
    QVERIFY(test.controller.supportsPoint());
    QVERIFY(test.controller.supportsRectangle());
    QVERIFY(test.controller.canChangeMode());
    QVERIFY(test.controller.modelChoices().isEmpty());
    QVERIFY(test.controller.modeUnavailableReason().isEmpty());
    test.server.targetSnapshot.insert(
        "external_selection_modes",
        QJsonArray{QJsonObject{{"id", "classic"}, {"label", "Camera point"}, {"point", true}, {"rectangle", false}}});
    test.client.refreshTargetState();
    QTRY_COMPARE_WITH_TIMEOUT(test.controller.selectionModes().size(), 1, TestTimeout::mediumMs());
    QVERIFY(!test.controller.supportsRectangle());
    QVERIFY(!test.controller.canChangeMode());
    test.controller.setSmartMode(true, test.controller.captureControlContext());
    QVERIFY(test.server.lastRequest("/api/v1/actions/gimbal-control").target.isEmpty());
    test.controller.selectTargetEngine("local", test.controller.captureControlContext());
    QTRY_VERIFY_WITH_TIMEOUT(!test.server.lastRequest("/api/v1/actions/tracker-switch").target.isEmpty(),
                             TestTimeout::mediumMs());
    QVERIFY(QJsonDocument::fromJson(test.server.lastRequest("/api/v1/actions/tracker-switch").body)
                .object()
                .value("restore_engine_selection")
                .toBool());
    QCOMPARE(QJsonDocument::fromJson(test.server.lastRequest("/api/v1/actions/tracker-switch").body)
                 .object()
                 .value("tracker_type")
                 .toString(),
             QStringLiteral("CSRT"));
}

UT_REGISTER_TEST_LIGHTWEIGHT(PixEagleTargetControllerTest, TestLabel::Integration)
