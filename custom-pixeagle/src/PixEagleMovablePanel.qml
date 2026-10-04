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
    signal dragStarted()

    color: qgcPal.windowShade
    radius: ScreenTools.defaultBorderRadius

    function constrain() {
        if (!parent) return
        x = Math.max(edgeMargin, Math.min(moved ? x : initialX, parent.width - width - edgeMargin))
        y = Math.max(edgeMargin, Math.min(moved ? y : initialY, parent.height - height - edgeMargin))
    }
    function resetPosition() {
        moved = false
        constrain()
    }

    onInitialXChanged: Qt.callLater(constrain)
    onInitialYChanged: Qt.callLater(constrain)
    onWidthChanged: Qt.callLater(constrain)
    onHeightChanged: Qt.callLater(constrain)
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
}
