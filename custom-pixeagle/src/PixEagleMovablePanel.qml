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
    property rect usableBounds: Qt.rect(0, 0, parent ? parent.width : 0, parent ? parent.height : 0)
    property var obstacles: []
    readonly property real minimumX: Math.max(edgeMargin, usableBounds.x + edgeMargin)
    readonly property real minimumY: Math.max(edgeMargin, usableBounds.y + edgeMargin)
    readonly property real rightBound: Math.min(parent ? parent.width : 0, usableBounds.x + usableBounds.width) - edgeMargin
    readonly property real bottomBound: Math.min(parent ? parent.height : 0, usableBounds.y + usableBounds.height) - edgeMargin
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
            (rightBound - minimumX) / width, (bottomBound - minimumY) / height)
        if (uiScale > Math.max(minimumUiScale, fittingScale)) {
            uiScale = Math.max(minimumUiScale, fittingScale)
        }
        const scaledWidth = width * uiScale
        const scaledHeight = height * uiScale
        const position = unobscuredPosition(moved ? x : initialX, moved ? y : initialY, scaledWidth, scaledHeight)
        x = position.x
        y = position.y
    }
    function unobscuredPosition(wantedX, wantedY, panelWidth, panelHeight) {
        const maxX = Math.max(minimumX, rightBound - panelWidth)
        const maxY = Math.max(minimumY, bottomBound - panelHeight)
        const clampX = value => Math.max(minimumX, Math.min(value, maxX))
        const clampY = value => Math.max(minimumY, Math.min(value, maxY))
        const origin = Qt.point(clampX(wantedX), clampY(wantedY))
        const xs = [origin.x, minimumX, maxX]
        const ys = [origin.y, minimumY, maxY]
        for (const rect of obstacles) {
            xs.push(clampX(rect.x - panelWidth - edgeMargin), clampX(rect.x + rect.width + edgeMargin))
            ys.push(clampY(rect.y - panelHeight - edgeMargin), clampY(rect.y + rect.height + edgeMargin))
        }
        let best = origin
        let bestScore = Infinity
        for (const px of xs) {
            for (const py of ys) {
                let overlap = 0
                for (const rect of obstacles) {
                    overlap += Math.max(0, Math.min(px + panelWidth, rect.x + rect.width) - Math.max(px, rect.x))
                        * Math.max(0, Math.min(py + panelHeight, rect.y + rect.height) - Math.max(py, rect.y))
                }
                // Prefer unobscured controls; nearest position breaks ties.
                const score = overlap * 1e6 + Math.pow(px - origin.x, 2) + Math.pow(py - origin.y, 2)
                if (score < bestScore) { bestScore = score; best = Qt.point(px, py) }
            }
        }
        return best
    }
    function resetPosition() {
        uiScale = minimumUiScale
        moved = false
        constrain()
    }

    onUsableBoundsChanged: Qt.callLater(constrain)
    onObstaclesChanged: Qt.callLater(constrain)
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
    component PanelDragArea: MouseArea {
        anchors.fill: parent
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        drag.target: root
        drag.minimumX: root.minimumX
        drag.maximumX: Math.max(root.minimumX, root.rightBound - root.width * root.uiScale)
        drag.minimumY: root.minimumY
        drag.maximumY: Math.max(root.minimumY, root.bottomBound - root.height * root.uiScale)
        onPressed: root.dragStarted()
        onPositionChanged: { if (drag.active) root.moved = true }
        onReleased: root.constrain()
        onCanceled: root.constrain()
        onDoubleClicked: root.resetPosition()
    }
    PanelDragArea { objectName: "pixeaglePanelBackgroundDrag" }
    PanelDragArea { parent: root.dragHandle }
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
                    (root.rightBound - root.minimumX) / root.width,
                    (root.bottomBound - root.minimumY) / root.height)
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
                root.x = Math.max(root.minimumX, Math.min(root._resizeOrigin.x,
                    root.rightBound - root.width * root.uiScale))
                root.y = Math.max(root.minimumY, Math.min(root._resizeOrigin.y,
                    root.bottomBound - root.height * root.uiScale))
            }
        }
    }
}
