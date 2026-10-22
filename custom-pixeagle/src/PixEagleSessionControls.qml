import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

ColumnLayout {
    id: root
    required property var client
    property int gestureGeneration: 0
    property string expandedControl: ""
    property string editContext: ""
    property int editGestureGeneration: -1
    readonly property var editedControl: {
        const controls = client ? client.followingSessionControls : []
        return controls.find(control => control.control_id === expandedControl) || null
    }
    readonly property bool editCurrent: !!editedControl && editContext.length > 0
        && editGestureGeneration === gestureGeneration
        && editContext === client.captureSessionControl(expandedControl)

    function openEditor(control) {
        if (expandedControl === control.control_id) {
            expandedControl = ""
            return
        }
        expandedControl = control.control_id
        editContext = client.captureSessionControl(control.control_id)
        editGestureGeneration = gestureGeneration
        speedEntry.text = String(control.value)
    }

    function applyEntry() {
        if (applyEntryButton.enabled && client.setSessionControlValue(expandedControl, Number(speedEntry.text), editContext))
            expandedControl = ""
    }

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
        Layout.preferredWidth: Layout.minimumWidth
        leftPadding: ScreenTools.defaultFontPixelWidth
        rightPadding: ScreenTools.defaultFontPixelWidth
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
                QGCColoredImage {
                    objectName: "pixeagleSessionSpeedIcon"
                    source: "qrc:/InstrumentValueIcons/dashboard.svg"
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    color: qgcPal.text
                    Accessible.name: sessionControl.modelData.label
                    ToolTip.visible: speedHover.hovered
                    ToolTip.text: sessionControl.modelData.label
                    HoverHandler { id: speedHover }
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
                    Layout.fillWidth: true
                    Layout.minimumWidth: 80
                    pointSize: ScreenTools.defaultFontPointSize * 1.15
                    text: root.speedText(sessionControl.modelData.value) + " " + sessionControl.modelData.unit
                    Accessible.name: qsTr("Requested 3D speed %1 %2, this session. Show details")
                                     .arg(root.speedText(sessionControl.modelData.value)).arg(sessionControl.modelData.unit)
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Requested speed · tap to enter a value")
                    onClicked: root.openEditor(sessionControl.modelData)
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
                    checked: sessionControl.modelData.override_active === true
                    Accessible.name: qsTr("Restore saved speed limits")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Restore saved speed limits")
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
    RowLayout {
        Layout.fillWidth: true
        visible: !!root.editedControl
        spacing: ScreenTools.defaultFontPixelWidth / 2
        QGCTextField {
            id: speedEntry
            objectName: "pixeagleSessionSpeedEntry"
            Layout.fillWidth: true
            Layout.minimumWidth: 60
            Layout.minimumHeight: Math.max(ScreenTools.minTouchPixels, 44)
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            Accessible.name: qsTr("Requested speed in metres per second")
            onAccepted: root.applyEntry()
        }
        ControlButton {
            id: applyEntryButton
            objectName: "pixeagleSessionSpeedApply"
            text: qsTr("Apply")
            readonly property real requestedValue: speedEntry.text.trim().length > 0 ? Number(speedEntry.text) : NaN
            enabled: root.editCurrent && isFinite(requestedValue)
                     && requestedValue >= root.editedControl.minimum
                     && (root.editedControl.maximum === null || requestedValue <= root.editedControl.maximum)
                     && requestedValue !== root.editedControl.value
                     && (requestedValue <= root.editedControl.value || root.editedControl.increase_allowed)
            onClicked: root.applyEntry()
        }
        ControlButton {
            text: qsTr("Cancel")
            onClicked: root.expandedControl = ""
        }
    }
    QGCLabel {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pointSize: ScreenTools.smallFontPointSize
        visible: !!root.editedControl
        text: !root.editedControl ? "" : qsTr("Requested 3D speed · this following session.")
              + (root.editedControl.reset_supported === true
                 ? "\n" + (root.editedControl.override_active
                   ? qsTr("Saved speed limits are overridden. Restore reapplies them and keeps or lowers this request.")
                   : qsTr("Saved speed limits apply."))
                   + "\n" + qsTr("Saved ceiling: %1 %2.")
                     .arg(root.editedControl.configured_maximum).arg(root.editedControl.unit)
                 : root.editedControl.maximum === null ? " " + qsTr("No configured velocity ceiling.")
                 : " " + qsTr("Maximum %1 %2.").arg(root.editedControl.maximum).arg(root.editedControl.unit))
              + "\n" + qsTr("Altitude and rate settings are unchanged. Zero stops translation; Stop ends following.")
              + (typeof root.editedControl.effective_command_magnitude === "number"
                 ? "\n" + qsTr("Command: %1 %2.").arg(Number(root.editedControl.effective_command_magnitude).toFixed(1)).arg(root.editedControl.unit) : "")
    }
    QGCLabel {
        Layout.fillWidth: true
        visible: !!root.editedControl && !root.editCurrent
        text: qsTr("Speed or session changed. Close and reopen the editor.")
        wrapMode: Text.WordWrap
    }
}
