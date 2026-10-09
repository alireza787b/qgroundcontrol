import QGroundControl
import QtQuick
import QGroundControl.Controls

Rectangle {
    id: root

    required property Item dragHandle
    property real initialX: 0
    property real initialY: 0
    property real edgeMargin: ScreenTools.defaultFontPixelWidth
    property bool moved: false
    property bool resizable: true
    property real uiScale: 1.0
    readonly property real minimumUiScale: 1.0
    readonly property real maximumUiScale: 2.0
    property real _resizeStartScale: 1.0
    property real _resizeStartCoordinate: 0
    signal dragStarted()

    color: qgcPal.windowShade
    radius: ScreenTools.defaultBorderRadius

    function constrain() {
        if (!parent) return
        const scaledWidth = width * uiScale
        const scaledHeight = height * uiScale
        x = Math.max(edgeMargin, Math.min(moved ? x : initialX, parent.width - scaledWidth - edgeMargin))
        y = Math.max(edgeMargin, Math.min(moved ? y : initialY, parent.height - scaledHeight - edgeMargin))
    }
    function resetPosition() {
        moved = false
        constrain()
    }

    onInitialXChanged: Qt.callLater(constrain)
    onInitialYChanged: Qt.callLater(constrain)
    onWidthChanged: Qt.callLater(constrain)
    onHeightChanged: Qt.callLater(constrain)
    onUiScaleChanged: Qt.callLater(constrain)
    Component.onCompleted: constrain()

    QGCPalette { id: qgcPal }
    Connections {
        target: root.parent
        function onWidthChanged() { root.constrain() }
        function onHeightChanged() { root.constrain() }
    }
    MouseArea { anchors.fill: parent }
    MouseArea {
        parent: root.dragHandle
        anchors.fill: parent
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        drag.target: root
        drag.minimumX: root.edgeMargin
        drag.maximumX: Math.max(root.edgeMargin, root.parent.width - root.width - root.edgeMargin)
        drag.minimumY: root.edgeMargin
        drag.maximumY: Math.max(root.edgeMargin, root.parent.height - root.height - root.edgeMargin)
        onPressed: root.dragStarted()
        onPositionChanged: { if (drag.active) root.moved = true }
        onDoubleClicked: root.resetPosition()
    }
    transform: Scale {
        origin.x: 0
        origin.y: 0
        xScale: root.uiScale
        yScale: root.uiScale
    }
    Item {
        id: resizeHandle
        width: Math.max(ScreenTools.minTouchPixels, ScreenTools.defaultFontPixelHeight * 2)
        height: width
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.resizable
        Accessible.name: qsTr("Resize panel")
        Accessible.description: qsTr("Drag to enlarge or reduce this panel")
        Canvas {
            anchors.centerIn: parent
            width: ScreenTools.defaultFontPixelWidth * 2
            height: width
            opacity: resizeArea.pressed ? 0.8 : 0.45
            onPaint: {
                const context = getContext("2d")
                context.strokeStyle = qgcPal.text
                context.lineWidth = 1.2
                context.lineCap = "round"
                context.beginPath()
                context.moveTo(width * 0.2, height * 0.8)
                context.lineTo(width * 0.8, height * 0.2)
                context.stroke()
            }
        }
        MouseArea {
            id: resizeArea
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            onPressed: function(mouse) {
                root._resizeStartScale = root.uiScale
                root._resizeStartCoordinate = mouse.x + mouse.y
                root.dragStarted()
            }
            onPositionChanged: function(mouse) {
                const delta = (mouse.x + mouse.y) - root._resizeStartCoordinate
                const nextScale = root._resizeStartScale + delta / Math.max(root.width, root.height)
                root.uiScale = Math.max(root.minimumUiScale, Math.min(root.maximumUiScale, nextScale))
            }
        }
    }
}
