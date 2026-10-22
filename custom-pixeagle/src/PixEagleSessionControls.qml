import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

ColumnLayout {
    id: root
    required property var client
    property int gestureGeneration: 0
    property string expandedControl: ""

    function speedText(value) {
        const text = Number(value).toFixed(6).replace(/0+$/, "")
        return text.endsWith(".") ? text + "0" : text
    }

    QGCPalette { id: qgcPal }
    component FocusOutline: Rectangle {
        anchors.fill: parent
        border.color: qgcPal.text
        border.width: 1
        color: "transparent"
        radius: ScreenTools.defaultBorderRadius
        visible: parent.visualFocus
    }
    component ControlButton: QGCButton {
        focusPolicy: Qt.StrongFocus
        Layout.minimumHeight: Math.max(ScreenTools.minTouchPixels, 44)
        Layout.minimumWidth: Math.max(ScreenTools.minTouchPixels, 44)
        Keys.priority: Keys.BeforeItem
        Keys.onPressed: (event) => {
            if (event.isAutoRepeat) event.accepted = true
        }
        FocusOutline { }
    }
    Repeater {
        model: root.client ? root.client.followingSessionControls : []
        delegate: ColumnLayout {
            id: sessionControl
            required property var modelData
            readonly property string controlId: modelData.control_id
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelHeight / 4
            RowLayout {
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelWidth / 2
                QGCLabel {
                    text: sessionControl.modelData.label
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                ControlButton {
                    property string contextToken: ""
                    property int gestureGeneration: -1
                    objectName: "pixeagleSessionDecrease"
                    text: "−"
                    enabled: sessionControl.modelData.available
                             && sessionControl.modelData.value > sessionControl.modelData.minimum
                    Accessible.name: qsTr("Decrease requested speed by %1 %2").arg(sessionControl.modelData.step).arg(sessionControl.modelData.unit)
                    onPressed: {
                        gestureGeneration = root.gestureGeneration
                        contextToken = root.client.captureSessionControl(sessionControl.controlId)
                    }
                    onCanceled: contextToken = ""
                    onClicked: {
                        if (gestureGeneration === root.gestureGeneration)
                            root.client.adjustSessionControl(sessionControl.controlId, -1, contextToken)
                        contextToken = ""
                    }
                }
                ControlButton {
                    objectName: "pixeagleSessionValue"
                    text: root.speedText(sessionControl.modelData.value) + " " + sessionControl.modelData.unit
                    Accessible.name: qsTr("Requested 3D speed %1 %2, this session. Show details")
                                     .arg(root.speedText(sessionControl.modelData.value)).arg(sessionControl.modelData.unit)
                    onClicked: root.expandedControl = root.expandedControl === sessionControl.controlId ? "" : sessionControl.controlId
                }
                ControlButton {
                    property string contextToken: ""
                    property int gestureGeneration: -1
                    objectName: "pixeagleSessionIncrease"
                    text: "+"
                    enabled: sessionControl.modelData.available && sessionControl.modelData.increase_allowed
                             && (sessionControl.modelData.maximum === null || sessionControl.modelData.value < sessionControl.modelData.maximum)
                    Accessible.name: qsTr("Increase requested speed by %1 %2").arg(sessionControl.modelData.step).arg(sessionControl.modelData.unit)
                    onPressed: {
                        gestureGeneration = root.gestureGeneration
                        contextToken = root.client.captureSessionControl(sessionControl.controlId)
                    }
                    onCanceled: contextToken = ""
                    onClicked: {
                        if (gestureGeneration === root.gestureGeneration)
                            root.client.adjustSessionControl(sessionControl.controlId, 1, contextToken)
                        contextToken = ""
                    }
                }
                ControlButton {
                    property string contextToken: ""
                    property int gestureGeneration: -1
                    objectName: "pixeagleSessionRestore"
                    iconSource: "qrc:/InstrumentValueIcons/refresh.svg"
                    visible: sessionControl.modelData.reset_supported === true
                    enabled: sessionControl.modelData.available && sessionControl.modelData.override_active === true
                    highlighted: sessionControl.modelData.override_active === true
                    Accessible.name: qsTr("Restore configured velocity limits")
                    onPressed: {
                        gestureGeneration = root.gestureGeneration
                        contextToken = root.client.captureSessionControl(sessionControl.controlId)
                    }
                    onCanceled: contextToken = ""
                    onClicked: {
                        if (gestureGeneration === root.gestureGeneration)
                            root.client.adjustSessionControl(sessionControl.controlId, 0, contextToken)
                        contextToken = ""
                    }
                }
            }
            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pointSize: ScreenTools.smallFontPointSize
                visible: root.expandedControl === sessionControl.controlId
                text: qsTr("3D request · this session. Zero stops translation; use Stop to end following.")
                      + (sessionControl.modelData.reset_supported === true
                         ? " " + qsTr("Velocity override %1; configured ceiling %2 %3. Altitude and rate protections remain unchanged.")
                           .arg(sessionControl.modelData.override_active ? qsTr("active") : qsTr("inactive"))
                           .arg(sessionControl.modelData.configured_maximum).arg(sessionControl.modelData.unit)
                         : sessionControl.modelData.maximum === null ? " " + qsTr("No configured velocity ceiling.")
                         : " " + qsTr("Maximum %1 %2.").arg(sessionControl.modelData.maximum).arg(sessionControl.modelData.unit))
                      + (typeof sessionControl.modelData.effective_command_magnitude === "number"
                         ? " " + qsTr("Command: %1 %2.").arg(Number(sessionControl.modelData.effective_command_magnitude).toFixed(1)).arg(sessionControl.modelData.unit) : "")
            }
            QGCLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pointSize: ScreenTools.smallFontPointSize
                visible: !!root.client && (root.client.followingActionPending || !!sessionControl.modelData.reason_code
                                           || !!sessionControl.modelData.limiting_reason)
                text: root.client && root.client.followingActionPending ? qsTr("Updating…")
                      : sessionControl.modelData.reason_code === "status_stale" ? qsTr("Refreshing…")
                      : sessionControl.modelData.limiting_reason === "descent_limited" ? qsTr("Minimum altitude: descent limited")
                      : sessionControl.modelData.limiting_reason === "climb_limited" ? qsTr("Maximum altitude: climb limited")
                      : sessionControl.modelData.limiting_reason === "target_recovery" ? qsTr("Reacquiring: speed increases paused")
                      : sessionControl.modelData.limiting_reason === "slowing_down" ? qsTr("Slowing toward the requested speed")
                      : sessionControl.modelData.limiting_reason === "guidance_limited" ? qsTr("Guidance, ramp or limits reduce the command")
                      : qsTr("Speed adjustment unavailable")
            }
        }
    }
}
