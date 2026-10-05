import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

import QGroundControl
import QGroundControl.Controls

PixEagleMovablePanel {
    id: root

    required property var client
    property var _owner: null
    property string _context: ""
    property var _lastOwner: null
    property string _lastContext: ""
    property var _requestedClient: null
    property bool _gestureActive: false
    property bool _oneShot: false
    property bool _releaseRequired: false
    property string _buttonOperation: ""
    property int _buttonDirection: 0
    property string _notice: ""
    property bool _showDetails: false
    property double _clock: Date.now()
    readonly property var _telemetry: _clock > 0 && client ? client.cameraStatus.telemetry || ({}) : ({})
    readonly property bool _anglesFresh: !!_telemetry.angles_fresh

    readonly property var _status: client ? client.cameraStatus : ({})
    property var _capabilities: []
    readonly property bool _ready: !!client && client.cameraFresh && !!_status.available
                                  && !!_status.connected && !client.cameraActionPending
    readonly property bool _inputDown: pad.touchActive || _buttonOperation.length > 0
    readonly property real _deadZone: 0.18

    signal closed()

    objectName: "pixeagleCameraControls"
    width: content.implicitWidth + ScreenTools.defaultFontPixelWidth * 2
    height: content.implicitHeight + ScreenTools.defaultFontPixelHeight
    dragHandle: grip
    onDragStarted: endGesture(true)
    onVisibleChanged: {
        if (!visible) endGesture(true)
        updateDemand()
    }
    onClientChanged: {
        endGesture(true)
        _capabilities = []
        updateDemand()
    }

    function updateDemand() {
        const next = visible ? client : null
        if (_requestedClient && _requestedClient !== next) _requestedClient.setCameraRequested(false)
        _requestedClient = next
        if (next) next.setCameraRequested(true)
    }

    function captureInput() {
        if (_releaseRequired || _gestureActive || !_ready) return false
        _owner = client
        _context = client.captureCameraContext()
        if (!_context) return false
        _lastOwner = _owner
        _lastContext = _context
        _gestureActive = true
        _notice = ""
        return true
    }

    function endGesture(requireRelease) {
        const owner = _owner
        const context = _context
        _gestureActive = false
        _oneShot = false
        _owner = null
        _context = ""
        _releaseRequired = !!requireRelease && _inputDown
        if (owner && context) {
            // Stop cannot synchronously change a binding that is cancelling a pointer grab.
            Qt.callLater(() => { if (owner) owner.stopCamera(context) })
        }
    }

    function inputReleased() {
        endGesture(false)
        _releaseRequired = false
    }

    function pressButton(operation, direction) {
        if (_inputDown || !_ready) return
        _buttonOperation = operation
        _buttonDirection = direction
        updateMotion()
    }

    function releaseButton() {
        _buttonOperation = ""
        inputReleased()
    }

    function updateMotion() {
        if (!visible || !_inputDown || _releaseRequired) return
        let operation = _buttonOperation
        let direction = _buttonDirection
        if (!operation) {
            const x = pad.xAxis
            const y = pad.yAxis
            if (Math.max(Math.abs(x), Math.abs(y)) < _deadZone) {
                if (_gestureActive) endGesture(false)
                return
            }
            operation = Math.abs(x) >= Math.abs(y) ? "pan" : "tilt"
            direction = (operation === "pan" ? x : -y) > 0 ? 1 : -1
        }
        if (_capabilities.indexOf(operation) < 0) return
        let value = direction
        if (!_buttonOperation) {
            const magnitude = Math.max(Math.abs(pad.xAxis), Math.abs(pad.yAxis))
            value *= Math.min(1, (magnitude - _deadZone) / (1 - _deadZone))
        }
        if (!_gestureActive) {
            if (!captureInput()) return
            if (!_owner.beginCameraManual(operation, value, _context)) endGesture(true)
            return
        }
        if (!_owner || _owner !== client || !_owner.enabled || !_owner.authenticated
            || !_owner.cameraAvailable || !_owner.cameraFresh || _owner.cameraError.length > 0
            || _owner.captureCameraContext() !== _context
            || !_owner.updateCameraManual(operation, value, _context)) {
            endGesture(true)
        }
    }

    function centerCamera() {
        if (_inputDown || !captureInput()) return
        _oneShot = true
        if (!client.cameraStep("home", 0, _context)) endGesture(true)
    }

    function stop() {
        if (_gestureActive) {
            endGesture(true)
        } else {
            const owner = _lastOwner
            const context = _lastContext
            if (owner && context) Qt.callLater(() => { if (owner) owner.stopCamera(context, true) })
        }
    }

    Timer {
        objectName: "pixeagleCameraMotionTimer"
        interval: 50
        repeat: true
        running: root.visible && root._inputDown && !root._releaseRequired
        onTriggered: root.updateMotion()
    }

    Timer {
        interval: 250
        repeat: true
        running: root.visible && root._showDetails
        onTriggered: root._clock = Date.now()
    }

    function angleText(axis) {
        const values = _telemetry.angles_deg || ({})
        return _anglesFresh && typeof values[axis] === "number" ? values[axis].toFixed(1) + "°" : "—"
    }

    component IconButton: QGCButton {
        required property string help
        Accessible.name: help
        ToolTip.text: help
        ToolTip.visible: hovered
        Layout.preferredWidth: ScreenTools.defaultFontPixelHeight * 2
        Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 2
    }

    component HoldButton: IconButton {
        required property string operation
        required property int direction
        enabled: pressed || (root._ready && !root._inputDown)
        onPressed: root.pressButton(operation, direction)
        onReleased: root.releaseButton()
        onCanceled: root.releaseButton()
    }

    ColumnLayout {
        id: content
        anchors.centerIn: parent
        spacing: ScreenTools.defaultFontPixelHeight / 3

        RowLayout {
            Layout.fillWidth: true
            Item {
                id: grip
                objectName: "pixeagleCameraGrip"
                Layout.fillWidth: true
                implicitHeight: ScreenTools.defaultFontPixelHeight * 2
                Accessible.name: qsTr("Move camera controls")
                QGCLabel {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("PixEagle camera")
                }
            }
            IconButton {
                text: "×"
                help: qsTr("Close camera controls")
                onClicked: { root.endGesture(true); root.closed() }
            }
        }
        RowLayout {
            spacing: ScreenTools.defaultFontPixelWidth
            JoystickThumbPad {
                id: pad
                objectName: "pixeagleCameraPad"
                Layout.preferredWidth: ScreenTools.defaultFontPixelHeight * 7
                Layout.preferredHeight: width
                fixedCenter: true
                yAxisReCenter: true
                yAxisPositiveRangeOnly: false
                enabled: touchActive || (root._ready && !root._inputDown)
                visible: root._capabilities.indexOf("pan") >= 0 || root._capabilities.indexOf("tilt") >= 0
                Accessible.name: qsTr("Camera pan and tilt")
                Accessible.description: qsTr("Drag and hold to move one axis at a time. Release to stop.")
                onTouchActiveChanged: {
                    if (!touchActive) root.inputReleased()
                }
            }
            ColumnLayout {
                visible: root._capabilities.indexOf("zoom") >= 0
                HoldButton { text: "+"; help: qsTr("Zoom in"); operation: "zoom"; direction: 1 }
                HoldButton { text: "−"; help: qsTr("Zoom out"); operation: "zoom"; direction: -1 }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            IconButton {
                help: qsTr("Center camera")
                visible: root._capabilities.indexOf("home") >= 0
                enabled: root._ready && !root._inputDown
                contentItem: Item {
                    QGCColoredImage {
                        anchors.centerIn: parent
                        width: ScreenTools.defaultFontPixelHeight
                        height: width
                        source: "qrc:/InstrumentValueIcons/target.svg"
                        color: QGroundControl.globalPalette.buttonText
                        fillMode: Image.PreserveAspectFit
                    }
                }
                onClicked: root.centerCamera()
            }
            QGCButton {
                Layout.fillWidth: true
                text: qsTr("Stop")
                enabled: root._gestureActive || (!!root.client && root.client.cameraCanStop)
                onClicked: root.stop()
            }
            IconButton {
                text: "⋯"
                help: qsTr("Camera readings")
                checked: root._showDetails
                onClicked: root._showDetails = !root._showDetails
            }
        }
        RowLayout {
            visible: root._capabilities.indexOf("roll") >= 0
            Layout.alignment: Qt.AlignHCenter
            HoldButton { text: "↶"; help: qsTr("Roll left"); operation: "roll"; direction: -1 }
            HoldButton { text: "↷"; help: qsTr("Roll right"); operation: "roll"; direction: 1 }
        }
        GridLayout {
            visible: root._showDetails
            columns: 2
            Layout.fillWidth: true
            QGCLabel { text: qsTr("Yaw") }
            QGCLabel { text: root.angleText("yaw"); Layout.alignment: Qt.AlignRight }
            QGCLabel { text: qsTr("Pitch") }
            QGCLabel { text: root.angleText("pitch"); Layout.alignment: Qt.AlignRight }
            QGCLabel { text: qsTr("Roll") }
            QGCLabel { text: root.angleText("roll"); Layout.alignment: Qt.AlignRight }
            QGCLabel { text: qsTr("Zoom"); visible: !!root._telemetry.zoom_available }
            QGCLabel {
                visible: !!root._telemetry.zoom_available
                text: typeof root._telemetry.zoom === "number" ? root._telemetry.zoom.toFixed(1) + "×" : "—"
                Layout.alignment: Qt.AlignRight
            }
            QGCLabel {
                Layout.columnSpan: 2
                text: root._anglesFresh ? (String(root._telemetry.coordinate_system).toUpperCase() === "GIMBAL_BODY"
                      ? qsTr("Camera body angles") : qsTr("Camera angles")) : qsTr("Readings unavailable")
                color: root._anglesFresh ? QGroundControl.globalPalette.text : QGroundControl.globalPalette.warningText
                font.pointSize: ScreenTools.smallFontPointSize
            }
        }
        QGCLabel {
            Layout.maximumWidth: pad.width + ScreenTools.defaultFontPixelHeight * 3
            Layout.fillWidth: true
            visible: text.length > 0
            text: root._notice || (root.client ? root.client.cameraError : "")
                  || (root.client && root.client.cameraManualState === "preparing" ? qsTr("Taking manual control…") : "")
                  || (root.client && root.client.cameraManualState === "refreshing" ? qsTr("Refreshing camera controls…") : "")
                  || (!root._ready && !root._gestureActive
                      ? qsTr("Camera unavailable") : "")
            color: root._notice.length > 0 || (root.client && root.client.cameraError.length > 0)
                   || (!root._ready && !root._gestureActive)
                   ? QGroundControl.globalPalette.warningText : QGroundControl.globalPalette.text
            wrapMode: Text.WordWrap
        }
    }

    Connections {
        target: root.Window.window
        function onActiveChanged() {
            if (!root.Window.window || !root.Window.window.active) root.endGesture(true)
        }
    }
    Connections {
        target: root.client
        function onCameraChanged() {
            if (root.client && root.client.cameraFresh) {
                root._capabilities = root.client.cameraStatus.capabilities || []
                if (root._gestureActive && (root.client.captureCameraContext() !== root._context
                    || !root.client.cameraStatus.available || root.client.cameraStatus.following_active))
                    root.endGesture(true)
            }
            if (root._gestureActive && root.client && root.client.cameraError.length > 0) root.endGesture(true)
            if (root._gestureActive && root.client && root.client.cameraManualState === "refreshing") root.endGesture(true)
            if (root._oneShot && root.client && !root.client.cameraActionPending && root.client.cameraError.length === 0) {
                root._oneShot = false
                root._gestureActive = false
                root._owner = null
                root._context = ""
            }
        }
        function onChanged() {
            if (!root.client || !root.client.authenticated || !root.client.enabled || !root.client.cameraAvailable
                || !root.client.cameraFresh) root.endGesture(true)
        }
    }
    Component.onDestruction: {
        endGesture(true)
        if (_requestedClient) _requestedClient.setCameraRequested(false)
    }
}
