import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

ColumnLayout {
    id: root

    required property var client
    property var _requestedClient: null
    property string _notice: ""

    visible: !!client && client.configAvailable
    spacing: ScreenTools.defaultFontPixelHeight / 2

    function updateDemand() {
        if (_requestedClient && (_requestedClient !== client || !visible)) {
            _requestedClient.setConfigRequested(false)
            _requestedClient = null
        }
        if (visible && client && _requestedClient !== client) {
            _requestedClient = client
            _requestedClient.setConfigRequested(true)
        }
    }

    onClientChanged: { _notice = ""; updateDemand() }
    onVisibleChanged: updateDemand()
    Component.onCompleted: updateDemand()
    Component.onDestruction: { if (_requestedClient) _requestedClient.setConfigRequested(false) }

    QGCCheckBox {
        id: osd

        property var capturedClient: null
        property string capturedContext: ""
        checked: !!root.client && root.client.osdEnabled
        enabled: !!root.client && root.client.canSetOsd
        text: qsTr("Show PixEagle video overlay")
        objectName: "pixeagleOsdEnabled"
        onPressedChanged: {
            if (pressed) {
                capturedClient = root.client
                capturedContext = capturedClient ? capturedClient.captureConfigContext() : ""
            }
        }
        onClicked: {
            root._notice = ""
            if (!capturedClient || capturedClient !== root.client
                || !capturedClient.setOsdEnabled(checked, capturedContext)) {
                root._notice = qsTr("Settings changed. Review their current state and try again.")
            }
            capturedClient = null
            capturedContext = ""
            checked = Qt.binding(() => !!root.client && root.client.osdEnabled)
        }
    }

    QGCLabel {
        Layout.fillWidth: true
        text: qsTr("Changes the PixEagle overlay for all viewers. QGC instruments and text already in a recording are unchanged.")
        wrapMode: Text.WordWrap
    }

    QGCLabel {
        Layout.fillWidth: true
        text: root.client ? root.client.osdStatusText : ""
        visible: text.length > 0
        wrapMode: Text.WordWrap
    }

    SectionHeader {
        id: backendSettings

        Layout.fillWidth: true
        checked: false
        showSpacer: false
        text: qsTr("Backend settings")
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: backendSettings.checked

        QGCLabel {
            Layout.fillWidth: true
            objectName: "pixeagleConfigStatus"
            text: root.client ? root.client.configStatusText : ""
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: root.client ? root.client.pendingConfigChanges : []

            QGCLabel {
                Layout.fillWidth: true
                text: qsTr("%1 — %2").arg(modelData.name).arg(modelData.apply)
                wrapMode: Text.WrapAnywhere
            }
        }

        RowLayout {
            QGCButton {
                enabled: !!root.client && root.client.canApplyConfig
                objectName: "pixeagleApplyConfig"
                text: qsTr("Apply saved changes…")
                onClicked: applyFactory.open({destination: root.client, contextToken: root.client.captureConfigContext()})
            }

            QGCButton {
                objectName: "pixeagleRestartBackend"
                text: qsTr("Restart PixEagle…")
                enabled: !!root.client && root.client.canRestartBackend
                onClicked: restartFactory.open({destination: root.client, contextToken: root.client.captureConfigContext()})
            }

            QGCButton {
                enabled: !!root.client && !root.client.configBusy && !root.client.backendRestarting
                text: qsTr("Refresh")
                onClicked: root.client.refreshConfig()
            }
        }
    }

    QGCLabel {
        Layout.fillWidth: true
        text: root._notice || (root.client ? root.client.configActionError : "")
        visible: text.length > 0
        wrapMode: Text.WordWrap
    }

    QGCLabel {
        Layout.fillWidth: true
        visible: backendSettings.checked && text.length > 0
        text: root.client ? root.client.backendRestartStatus : ""
        wrapMode: Text.WordWrap
    }

    QGCPopupDialogFactory {
        id: restartFactory

        dialogComponent: QGCPopupDialog {
            required property var destination
            required property string contextToken

            title: qsTr("Restart PixEagle")
            buttons: Dialog.Ok | Dialog.Cancel
            onAccepted: {
                if (destination !== root.client || !destination.restartBackend(contextToken)) {
                    root._notice = qsTr("Settings changed. Review their current state and try again.")
                }
            }

            QGCLabel {
                width: ScreenTools.defaultFontPixelWidth * 45
                text: qsTr("Restart the PixEagle backend and apply saved settings? Video and tracking will disconnect briefly. Tracking, camera movement and following will not resume automatically.")
                wrapMode: Text.WordWrap
            }
        }
    }

    QGCPopupDialogFactory {
        id: applyFactory

        dialogComponent: QGCPopupDialog {
            required property var destination
            required property string contextToken

            title: qsTr("Apply PixEagle settings")
            buttons: Dialog.Apply | Dialog.Cancel
            onAccepted: {
                if (destination !== root.client || !destination.applyConfig(contextToken)) {
                    root._notice = qsTr("Settings changed. Review their current state and try again.")
                }
            }

            QGCLabel {
                width: ScreenTools.defaultFontPixelWidth * 45
                text: qsTr("Apply the saved changes listed in Backend settings? The tracker may restart. Tracking and following will not resume automatically.")
                wrapMode: Text.WordWrap
            }
        }
    }
}
