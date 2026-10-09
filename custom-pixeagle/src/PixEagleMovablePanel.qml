import QGroundControl
import QtQuick
import QtCore
import QGroundControl.Controls

Rectangle {
    id: root

    required property Item dragHandle
    required property string settingsKey
    property real initialX: 0
    property real initialY: 0
    property real edgeMargin: ScreenTools.defaultFontPixelWidth
    property bool moved: false
    property bool resizable: true
    property real uiScale: 1.0
    readonly property real resizeHandleSize: Math.max(ScreenTools.minTouchPixels, 44)
    readonly property real resizeFooterHeight: resizable ? resizeHandleSize + ScreenTools.defaultFontPixelHeight / 6 : 0
    readonly property real minimumUiScale: 1.0
    readonly property real maximumUiScale: 2.0
    property real _resizeStartScale: 1.0
    property point _resizeOrigin
    property point _resizeStartPosition
    property real _resizeMaximumScale: 2.0
    property bool _resizing: false
    signal dragStarted()

    color: qgcPal.windowShade
    radius: ScreenTools.defaultBorderRadius

    function constrain() {
        if (!parent || !visible) return
        if (_resizing) return
        const fittingScale = Math.min(maximumUiScale,
            (parent.width - edgeMargin * 2) / width, (parent.height - edgeMargin * 2) / height)
        if (uiScale > Math.max(minimumUiScale, fittingScale)) {
            uiScale = Math.max(minimumUiScale, fittingScale)
        }
        const scaledWidth = width * uiScale
        const scaledHeight = height * uiScale
        x = Math.max(edgeMargin, Math.min(moved ? x : initialX, parent.width - scaledWidth - edgeMargin))
        y = Math.max(edgeMargin, Math.min(moved ? y : initialY, parent.height - scaledHeight - edgeMargin))
    }
    function resetPosition() {
        uiScale = minimumUiScale
        moved = false
        constrain()
    }

    onInitialXChanged: Qt.callLater(constrain)
    onInitialYChanged: Qt.callLater(constrain)
    onWidthChanged: Qt.callLater(constrain)
    onHeightChanged: Qt.callLater(constrain)
    onUiScaleChanged: Qt.callLater(constrain)
    Component.onCompleted: constrain()

    Settings {
        category: "PixEaglePanels/" + root.settingsKey
        property alias panelX: root.x
        property alias panelY: root.y
        property alias panelMoved: root.moved
    }

    QGCPalette { id: qgcPal }
    Connections {
        target: root
        function onVisibleChanged() {
            if (!root.visible) root._resizing = false
            else Qt.callLater(root.constrain)
        }
    }
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
        drag.maximumX: Math.max(root.edgeMargin, root.parent.width - root.width * root.uiScale - root.edgeMargin)
        drag.minimumY: root.edgeMargin
        drag.maximumY: Math.max(root.edgeMargin, root.parent.height - root.height * root.uiScale - root.edgeMargin)
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
        z: 10
        width: root.resizeHandleSize
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
            objectName: "pixeaglePanelResizeHandle"
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            onPressed: function(mouse) {
                root._resizeStartScale = root.uiScale
                root._resizeStartPosition = resizeArea.mapToItem(root.parent, mouse.x, mouse.y)
                root._resizeOrigin = Qt.point(root.x, root.y)
                root._resizeMaximumScale = Math.min(root.maximumUiScale,
                    (root.parent.width - root.edgeMargin * 2) / root.width,
                    (root.parent.height - root.edgeMargin * 2) / root.height)
                root.moved = true
                root._resizing = true
                root.dragStarted()
            }
            onReleased: { root._resizing = false; root.constrain() }
            onCanceled: { root._resizing = false; root.constrain() }
            onPositionChanged: function(mouse) {
                if (!pressed) return
                const position = resizeArea.mapToItem(root.parent, mouse.x, mouse.y)
                const deltaX = position.x - root._resizeStartPosition.x
                const deltaY = position.y - root._resizeStartPosition.y
                const deltaScale = (deltaX * root.width + deltaY * root.height)
                    / (root.width * root.width + root.height * root.height)
                root.uiScale = Math.max(root.minimumUiScale,
                    Math.min(root._resizeMaximumScale, root._resizeStartScale + deltaScale))
                root.x = Math.max(root.edgeMargin, Math.min(root._resizeOrigin.x,
                    root.parent.width - root.width * root.uiScale - root.edgeMargin))
                root.y = Math.max(root.edgeMargin, Math.min(root._resizeOrigin.y,
                    root.parent.height - root.height * root.uiScale - root.edgeMargin))
            }
        }
    }
}
