import QtQuick

import QGroundControl
import QGroundControl.Controls

Item {
    id: _root

    property Item pipView
    property var toolInsets: null
    property Item pipState: videoPipState
    readonly property bool externalVideoActive: QGroundControl.videoManager.externalVideoActive

    PipState {
        id:         videoPipState
        pipView:    _root.pipView
        isDark:     true

        onWindowAboutToOpen: {
            QGroundControl.videoManager.stopVideo()
            videoStartDelay.start()
        }

        onWindowAboutToClose: {
            QGroundControl.videoManager.stopVideo()
            videoStartDelay.start()
        }

        onStateChanged: {
            if (pipState.state !== pipState.fullState) {
                QGroundControl.videoManager.fullScreen = false
            }
        }
    }

    Timer {
        id:           videoStartDelay
        interval:     2000;
        running:      false
        repeat:       false
        onTriggered:  QGroundControl.videoManager.startVideo()
    }

    //-- Video Streaming
    FlightDisplayViewVideo {
        id:             videoStreaming
        anchors.fill:   parent
        useSmallFont:   _root.pipState.state !== _root.pipState.fullState
        toolInsets:     _root.toolInsets
        visible:        QGroundControl.videoManager.isStreamSource || QGroundControl.videoManager.isUvc
        z:              _root.externalVideoActive ? 1 : 0
    }

    QGCLabel {
        text: qsTr("Double-click to exit full screen")
        font.pointSize: ScreenTools.largeFontPointSize
        visible: QGroundControl.videoManager.fullScreen
        anchors.centerIn: parent

        onVisibleChanged: {
            if (visible) {
                labelAnimation.start()
            }
        }

        PropertyAnimation on opacity {
            id: labelAnimation
            duration: 10000
            from: 1.0
            to: 0.0
            easing.type: Easing.InExpo
        }
    }

    OnScreenGimbalController {
        id:                      onScreenGimbalController
        anchors.fill:            parent
        cameraTrackingEnabled:   !!(videoStreaming._camera && videoStreaming._camera.trackingEnabled)
        visible:                 !_root.externalVideoActive
    }

    OnScreenCameraTrackingController {
        id:                      cameraTrackingController
        anchors.fill:            parent
        camera:                  _root.externalVideoActive ? null : videoStreaming._camera
        videoWidth:              videoStreaming.getWidth()
        videoHeight:             videoStreaming.getHeight()
        externalController:      videoStreaming.externalTrackingController
        z:                       _root.externalVideoActive ? 2 : 0
    }

    MouseArea {
        id:                         flyViewVideoMouseArea
        anchors.fill:               parent
        enabled:                    pipState.state === pipState.fullState

        property real _pressX:      0
        property real _pressY:      0
        property bool _dragging:    false
        property bool _doubleClicked: false
        property string _gestureOwner: ""
        readonly property real _dragThreshold: 10

        function cancelGesture() {
            singleClickTimer.stop()
            onScreenGimbalController.mouseDragEnd()
            cameraTrackingController.cancelSelection()
            _dragging = false
            _doubleClicked = false
            _gestureOwner = ""
        }

        Connections {
            target: _root
            function onExternalVideoActiveChanged() { flyViewVideoMouseArea.cancelGesture() }
        }

        Connections {
            target: videoStreaming
            function onExternalTrackingControllerChanged() { flyViewVideoMouseArea.cancelGesture() }
        }

        Connections {
            target: QGroundControl.multiVehicleManager
            function onActiveVehicleChanged() { flyViewVideoMouseArea.cancelGesture() }
        }

        Connections {
            target: QGroundControl.videoManager
            function onFullScreenChanged() { cameraTrackingController.cancelSelection() }
        }

        onCanceled: cancelGesture()
        onEnabledChanged: { if (!enabled) cancelGesture() }
        onVisibleChanged: { if (!visible) cancelGesture() }

        // Defer single-click handling so a double-click (fullscreen toggle) doesn't also
        // fire an unintended gimbal click-to-point/tracking command on its first click.
        Timer {
            id:         singleClickTimer
            interval:   Qt.styleHints.mouseDoubleClickInterval
            repeat:     false

            property real clickX: 0
            property real clickY: 0

            onTriggered: {
                if (flyViewVideoMouseArea._gestureOwner === "gimbal") onScreenGimbalController.mouseClicked(clickX, clickY)
                else if (flyViewVideoMouseArea._gestureOwner === "tracking") cameraTrackingController.mouseClicked(clickX, clickY)
                flyViewVideoMouseArea._gestureOwner = ""
            }
        }

        onDoubleClicked: {
            // Fires on the second press of a double-click. The second release still emits
            // onReleased, so flag it to prevent re-arming the single-click timer.
            _doubleClicked = true
            singleClickTimer.stop()
            cameraTrackingController.cancelSelection()
            onScreenGimbalController.mouseDragEnd()
            _gestureOwner = ""
            QGroundControl.videoManager.fullScreen = !QGroundControl.videoManager.fullScreen
        }

        onPressed: (mouse) => {
            singleClickTimer.stop()
            _pressX = mouse.x
            _pressY = mouse.y
            _dragging = false
            // Clear any stale flag (e.g. double-click followed by drag releases through the
            // drag branch without consuming it). Safe: pressed is emitted before doubleClicked.
            _doubleClicked = false
            _gestureOwner = _root.externalVideoActive || cameraTrackingController._trackingEnabled ? "tracking" : "gimbal"
            cameraTrackingController.beginGesture(flyViewVideoMouseArea, mouse.x, mouse.y)
        }

        onPositionChanged: (mouse) => {
            if (!pressed || !_gestureOwner) return
            if (!_dragging && (Math.abs(mouse.x - _pressX) >= _dragThreshold || Math.abs(mouse.y - _pressY) >= _dragThreshold)) {
                _dragging = true
                if (_gestureOwner === "gimbal") onScreenGimbalController.mouseDragStart(_pressX, _pressY)
                else cameraTrackingController.mouseDragStart(_pressX, _pressY)
            }
            if (_dragging) {
                if (_gestureOwner === "gimbal") onScreenGimbalController.mouseDragPositionChanged(mouse.x, mouse.y)
                else cameraTrackingController.mouseDragPositionChanged(mouse.x, mouse.y)
            }
        }

        onReleased: (mouse) => {
            if (_dragging) {
                if (_gestureOwner === "gimbal") onScreenGimbalController.mouseDragEnd()
                else if (_gestureOwner === "tracking") cameraTrackingController.mouseDragEnd(mouse.x, mouse.y)
                _gestureOwner = ""
            } else if (_doubleClicked) {
                // Second release of a double-click - fullscreen toggle already handled
                _doubleClicked = false
            } else {
                singleClickTimer.clickX = mouse.x
                singleClickTimer.clickY = mouse.y
                singleClickTimer.restart()
            }
            _dragging = false
        }
    }

    ProximityRadarVideoView{
        visible:        !_root.externalVideoActive
        anchors.fill:   parent
        vehicle:        QGroundControl.multiVehicleManager.activeVehicle
    }

    ObstacleDistanceOverlayVideo {
        visible: !_root.externalVideoActive
        id: obstacleDistance
        showText: pipState.state === pipState.fullState
    }
}
