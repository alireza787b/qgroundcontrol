#include "OnScreenCameraTrackingControllerTest.h"

#include <memory>

#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>

namespace {
class GestureFixture
{
public:
    QQmlEngine engine;
    QQmlComponent cameraComponent{&engine};
    QQmlComponent externalComponent{&engine};
    QQmlComponent controllerComponent{&engine};
    std::unique_ptr<QObject> camera;
    std::unique_ptr<QObject> external;
    std::unique_ptr<QObject> controller;
    QQuickItem input;

    bool create()
    {
        cameraComponent.setData(R"(
            import QtQuick
            QtObject {
                property bool trackingEnabled: true
                property bool supportsTrackingPoint: true
                property bool supportsTrackingRect: true
                property bool trackingImageIsActive: false
                property int calls: 0
                property point selectedPoint: Qt.point(0, 0)
                property rect selectedRect: Qt.rect(0, 0, 0, 0)
                function startTrackingPoint(point, radius) { selectedPoint = point; calls++ }
                function startTrackingRect(rectangle) { selectedRect = rectangle; calls++ }
            }
        )",
                                QUrl());
        externalComponent.setData(R"(
            import QtQuick
            QtObject {
                property bool armed: true
                property int begins: 0
                property int finishes: 0
                property int cancels: 0
                property int pointerCancels: 0
                property bool rectangle: false
                property point start: Qt.point(0, 0)
                property point end: Qt.point(0, 0)
                signal gestureInvalidated()
                function beginGesture(item, x, y) {
                    if (!armed) return false
                    begins++; start = Qt.point(x, y); return true
                }
                function finishGesture(x, y, isRectangle) {
                    finishes++; end = Qt.point(x, y); rectangle = isRectangle; armed = false
                }
                function cancelGesture() { cancels++; armed = false; gestureInvalidated() }
                function cancelPointerGesture() { pointerCancels++; gestureInvalidated() }
            }
        )",
                                  QUrl());
        camera.reset(cameraComponent.create());
        external.reset(externalComponent.create());
        controllerComponent.loadUrl(
            QUrl(QStringLiteral("qrc:/qml/QGroundControl/FlyView/OnScreenCameraTrackingController.qml")));
        if (!camera || !external || controllerComponent.isError()) {
            return false;
        }
        controller.reset(controllerComponent.createWithInitialProperties({{"camera", QVariant::fromValue(camera.get())},
                                                                          {"videoWidth", 200},
                                                                          {"videoHeight", 100},
                                                                          {"width", 400},
                                                                          {"height", 300}}));
        return !!controller;
    }

    bool begin(qreal x = 150, qreal y = 125)
    {
        return QMetaObject::invokeMethod(controller.get(), "beginGesture", Q_ARG(QVariant, QVariant::fromValue(&input)),
                                         Q_ARG(QVariant, x), Q_ARG(QVariant, y));
    }

    bool invoke(const char* method, qreal x, qreal y)
    {
        return QMetaObject::invokeMethod(controller.get(), method, Q_ARG(QVariant, x), Q_ARG(QVariant, y));
    }
};
}  // namespace

void OnScreenCameraTrackingControllerTest::_stockCoordinates_data()
{
    QTest::addColumn<bool>("rectangle");
    QTest::newRow("point-with-letterbox") << false;
    QTest::newRow("reverse-rectangle-with-letterbox") << true;
}

void OnScreenCameraTrackingControllerTest::_stockCoordinates()
{
    QFETCH(bool, rectangle);
    GestureFixture fixture;
    QVERIFY2(fixture.create(), qPrintable(fixture.controllerComponent.errorString()));
    QVERIFY(fixture.begin());
    if (rectangle) {
        QVERIFY(fixture.invoke("mouseDragStart", 250, 175));
        QVERIFY(fixture.invoke("mouseDragPositionChanged", 150, 125));
        QVERIFY(fixture.invoke("mouseDragEnd", 150, 125));
        QCOMPARE(fixture.camera->property("selectedRect").toRectF(), QRectF(0.25, 0.25, 0.5, 0.5));
    } else {
        QVERIFY(fixture.invoke("mouseClicked", 150, 125));
        QCOMPARE(fixture.camera->property("selectedPoint").toPointF(), QPointF(0.25, 0.25));
    }
    QCOMPARE(fixture.camera->property("calls").toInt(), 1);
    QCOMPARE(fixture.external->property("begins").toInt(), 0);
}

void OnScreenCameraTrackingControllerTest::_externalOwner_data()
{
    QTest::addColumn<bool>("rectangle");
    QTest::addColumn<bool>("armed");
    QTest::newRow("point") << false << true;
    QTest::newRow("rectangle") << true << true;
    QTest::newRow("unarmed-point-consumed") << false << false;
    QTest::newRow("unarmed-rectangle-consumed") << true << false;
}

void OnScreenCameraTrackingControllerTest::_externalOwner()
{
    QFETCH(bool, rectangle);
    QFETCH(bool, armed);
    GestureFixture fixture;
    QVERIFY2(fixture.create(), qPrintable(fixture.controllerComponent.errorString()));
    QVERIFY(fixture.external->setProperty("armed", armed));
    QVERIFY(fixture.controller->setProperty("externalController", QVariant::fromValue(fixture.external.get())));
    QVERIFY(fixture.begin());
    QCOMPARE(fixture.external->property("begins").toInt(), armed ? 1 : 0);
    if (rectangle) {
        QVERIFY(fixture.invoke("mouseDragStart", 150, 125));
        QVERIFY(fixture.invoke("mouseDragPositionChanged", 250, 175));
        QVERIFY(fixture.invoke("mouseDragEnd", 250, 175));
    } else {
        QVERIFY(fixture.invoke("mouseClicked", 250, 175));
    }
    QCOMPARE(fixture.camera->property("calls").toInt(), 0);
    QCOMPARE(fixture.external->property("finishes").toInt(), armed ? 1 : 0);
    if (armed) {
        QCOMPARE(fixture.external->property("start").toPointF(), QPointF(150, 125));
        QCOMPARE(fixture.external->property("end").toPointF(), QPointF(250, 175));
        QCOMPARE(fixture.external->property("rectangle").toBool(), rectangle);
    }
}

void OnScreenCameraTrackingControllerTest::_cancelAndRearm()
{
    GestureFixture fixture;
    QVERIFY2(fixture.create(), qPrintable(fixture.controllerComponent.errorString()));
    QVERIFY(fixture.controller->setProperty("externalController", QVariant::fromValue(fixture.external.get())));
    QVERIFY(fixture.begin());
    QVERIFY(QMetaObject::invokeMethod(fixture.external.get(), "cancelGesture"));
    QVERIFY(fixture.invoke("mouseClicked", 250, 175));
    QCOMPARE(fixture.external->property("finishes").toInt(), 0);
    QVERIFY(fixture.external->setProperty("armed", true));
    QVERIFY(fixture.begin());
    QVERIFY(fixture.invoke("mouseClicked", 250, 175));
    QCOMPARE(fixture.external->property("begins").toInt(), 2);
    QCOMPARE(fixture.external->property("finishes").toInt(), 1);
    QCOMPARE(fixture.camera->property("calls").toInt(), 0);
}

void OnScreenCameraTrackingControllerTest::_replacingPointerPreservesSelection()
{
    GestureFixture fixture;
    QVERIFY2(fixture.create(), qPrintable(fixture.controllerComponent.errorString()));
    QVERIFY(fixture.controller->setProperty("externalController", QVariant::fromValue(fixture.external.get())));
    QVERIFY(fixture.begin());
    QVERIFY(fixture.begin(220, 170));
    QCOMPARE(fixture.external->property("pointerCancels").toInt(), 1);
    QCOMPARE(fixture.external->property("cancels").toInt(), 0);
    QCOMPARE(fixture.external->property("finishes").toInt(), 0);
    QVERIFY(fixture.external->property("armed").toBool());
    QVERIFY(fixture.invoke("mouseClicked", 220, 170));
    QCOMPARE(fixture.external->property("finishes").toInt(), 1);
    QCOMPARE(fixture.external->property("start").toPointF(), QPointF(220, 170));
    QCOMPARE(fixture.camera->property("calls").toInt(), 0);
}

void OnScreenCameraTrackingControllerTest::_cancelSelectionWithoutPointer_data()
{
    QTest::addColumn<bool>("accepted");
    QTest::newRow("submitted-selection") << true;
    QTest::newRow("busy-controller-refused-pointer") << false;
}

void OnScreenCameraTrackingControllerTest::_cancelSelectionWithoutPointer()
{
    QFETCH(bool, accepted);
    GestureFixture fixture;
    QVERIFY2(fixture.create(), qPrintable(fixture.controllerComponent.errorString()));
    QVERIFY(fixture.external->setProperty("armed", accepted));
    QVERIFY(fixture.controller->setProperty("externalController", QVariant::fromValue(fixture.external.get())));
    QVERIFY(fixture.begin());
    QVERIFY(fixture.invoke("mouseClicked", 220, 170));
    QVERIFY(!fixture.controller->property("_gestureController").value<QObject*>());
    QCOMPARE(fixture.external->property("finishes").toInt(), accepted ? 1 : 0);
    QVERIFY(QMetaObject::invokeMethod(fixture.controller.get(), "cancelSelection"));
    QCOMPARE(fixture.external->property("cancels").toInt(), 1);
    QVERIFY(!fixture.external->property("armed").toBool());
    QCOMPARE(fixture.camera->property("calls").toInt(), 0);
}

void OnScreenCameraTrackingControllerTest::_ownerChangeCancels()
{
    GestureFixture fixture;
    QVERIFY2(fixture.create(), qPrintable(fixture.controllerComponent.errorString()));
    QVERIFY(fixture.controller->setProperty("externalController", QVariant::fromValue(fixture.external.get())));
    QVERIFY(fixture.begin());
    QVERIFY(fixture.invoke("mouseDragStart", 150, 125));
    QVERIFY(fixture.controller->setProperty("externalController", QVariant::fromValue<QObject*>(nullptr)));
    QVERIFY(fixture.invoke("mouseDragEnd", 250, 175));
    QVERIFY(fixture.invoke("mouseClicked", 250, 175));
    QCOMPARE(fixture.external->property("finishes").toInt(), 0);
    QCOMPARE(fixture.camera->property("calls").toInt(), 0);

    QVERIFY(fixture.begin());
    QVERIFY(fixture.controller->setProperty("camera", QVariant::fromValue<QObject*>(nullptr)));
    QVERIFY(fixture.invoke("mouseClicked", 250, 175));
    QCOMPARE(fixture.camera->property("calls").toInt(), 0);
}

UT_REGISTER_TEST(OnScreenCameraTrackingControllerTest)
