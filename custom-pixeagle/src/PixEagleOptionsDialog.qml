import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

QGCPopupDialog {
    id: root

    required property var client
    required property var trackingController
    property string _choiceNotice: ""
    property bool _openCameraAfterClose: false
    readonly property bool _smartMode: !!trackingController && trackingController.smartMode
    readonly property bool _externalTracker: !!trackingController && trackingController.externalTracker
    readonly property var _models: trackingController ? trackingController.modelChoices : []
    readonly property var _followers: client ? client.followerChoices : []
    readonly property int _modelIndex: {
        const current = trackingController ? trackingController.configuredModelId : ""
        for (let index = 0; index < _models.length; ++index) {
            if (_models[index].modelId === current) return index
        }
        return -1
    }
    readonly property int _followerIndex: {
        for (let index = 0; index < _followers.length; ++index) {
            if (_followers[index].mode === client.selectedFollower) return index
        }
        return -1
    }

    signal resetPanelRequested()
    signal cameraControlsRequested()

    title: qsTr("PixEagle options")
    buttons: Dialog.Close
    objectName: "pixeagleOptionsDialog"
    onOpened: {
        closePolicy |= Popup.CloseOnEscape
        if (client) client.setCameraRequested(true)
        if (client) client.refreshSafety()
    }
    onClosed: {
        if (client) client.setCameraRequested(false)
        if (_openCameraAfterClose) cameraControlsRequested()
    }
    on_SmartModeChanged: closeChoices()
    on_ExternalTrackerChanged: closeChoices()

    component FocusOutline: Rectangle {
        anchors.fill: parent
        border.color: QGroundControl.globalPalette.text
        border.width: 1
        color: "transparent"
        radius: ScreenTools.defaultBorderRadius
        visible: parent.visualFocus
    }

    function captureControl() {
        if (!trackingController) return ""
        trackingController.cancelPointerGesture()
        return trackingController.captureControlContext()
    }

    function closeChoices() {
        if (trackerChoice) trackerChoice.popup.close()
        if (modelChoice) modelChoice.popup.close()
        if (followerChoice) followerChoice.popup.close()
    }

    Connections {
        target: QGroundControl.corePlugin.pixeagle
        function onActiveClientChanged() {
            if (QGroundControl.corePlugin.pixeagle.activeClient !== root.client) root.close()
        }
    }

    Connections {
        target: root.client
        function onChanged() {
            if (!root.client || !root.client.authenticated || !root.client.enabled) root.close()
        }
    }

    ColumnLayout {
        spacing: ScreenTools.defaultFontPixelHeight / 2
        width: Math.min(root.maxContentAvailableWidth, ScreenTools.defaultFontPixelWidth * 46)

        RowLayout {
            Layout.fillWidth: true
            visible: root.trackingController && !root.trackingController.externalTracker
                     && !root.trackingController.smartMode

            QGCLabel { text: qsTr("Tracker") }

            QGCComboBox {
                id: trackerChoice

                property string contextToken: ""
                property string choiceNotice: ""
                property var pressedChoices: []
                Layout.fillWidth: true
                currentIndex: root.trackingController ? root.trackingController.trackerIndex : -1
                enabled: root.trackingController && root.trackingController.canConfigure
                model: root.trackingController ? root.trackingController.trackerChoices : []
                objectName: "pixeagleTrackerChoice"
                textRole: "label"
                visible: root.trackingController && !root.trackingController.externalTracker
                         && !root.trackingController.smartMode
                Accessible.name: qsTr("Classic tracker")

                function captureChoice() {
                    choiceNotice = ""
                    pressedChoices = model.slice()
                    contextToken = root.captureControl()
                }

                onModelChanged: currentIndex = Qt.binding(() => root.trackingController ? root.trackingController.trackerIndex : -1)
                onPressedChanged: {
                    if (pressed && !popup.visible) captureChoice()
                }
                Keys.onPressed: event => {
                    if (!popup.visible) captureChoice()
                    event.accepted = false
                }
                onActivated: index => {
                    const choice = pressedChoices[index]
                    const currentChoice = model[index]
                    if (choice && currentChoice && choice.value === currentChoice.value
                        && choice.factory_key === currentChoice.factory_key)
                        root.trackingController.selectTracker(index, contextToken)
                    else
                        choiceNotice = qsTr("Tracker list changed. Choose again.")
                    contextToken = ""
                    pressedChoices = []
                    currentIndex = Qt.binding(() => root.trackingController ? root.trackingController.trackerIndex : -1)
                }
                FocusOutline { }
            }

        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.trackingController && !root.trackingController.externalTracker
                     && (root._smartMode || (!root.trackingController.canChangeMode && root._models.length > 0))

            QGCLabel { text: qsTr("Smart model") }

            QGCComboBox {
                id: modelChoice

                property string contextToken: ""
                property string choiceNotice: ""
                property var pressedChoices: []
                Layout.fillWidth: true
                currentIndex: root._modelIndex
                enabled: root.trackingController && root.trackingController.canSelectModel && count > 0
                model: root._models
                objectName: "pixeagleModelChoice"
                textRole: "label"
                alternateText: currentIndex < 0
                               ? (root.trackingController && root.trackingController.busy
                                  ? qsTr("Updating…") : qsTr("No model selected")) : ""
                Accessible.name: qsTr("Installed Smart model")

                function captureChoice() {
                    choiceNotice = ""
                    if (!root.trackingController) return
                    root.trackingController.cancelPointerGesture()
                    pressedChoices = root._models.slice()
                    contextToken = root.trackingController.captureModelContext()
                }
                onModelChanged: currentIndex = Qt.binding(() => root._modelIndex)
                onPressedChanged: {
                    if (pressed && !popup.visible) captureChoice()
                }
                Keys.onPressed: event => {
                    if (!popup.visible) captureChoice()
                    event.accepted = false
                }
                onActivated: index => {
                    const choice = pressedChoices[index]
                    const currentChoice = root._models[index]
                    if (!choice || !currentChoice || choice.modelId !== currentChoice.modelId)
                        choiceNotice = qsTr("Model list changed. Choose again.")
                    else if (!currentChoice.available)
                        choiceNotice = currentChoice.reason || qsTr("This model is unavailable. Check PixEagle settings.")
                    else if (choice.modelId !== root.trackingController.configuredModelId)
                        root.trackingController.selectModel(choice.modelId, contextToken)
                    contextToken = ""
                    pressedChoices = []
                    currentIndex = Qt.binding(() => root._modelIndex)
                }
                FocusOutline { }
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            objectName: "pixeagleModelRuntimeStatus"
            visible: !!root.trackingController && !root._externalTracker && root._smartMode
            text: root.trackingController ? root.trackingController.modelStatusText : ""
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            visible: !!root.client && root._followers.length > 0

            QGCLabel { text: qsTr("Follower") }

            QGCComboBox {
                id: followerChoice

                property string contextToken: ""
                property var capturedClient: null
                property var pressedChoices: []
                Layout.fillWidth: true
                currentIndex: root._followerIndex
                enabled: root.client && root.client.canSelectFollower && count > 0
                model: root._followers
                objectName: "pixeagleFollowerChoice"
                textRole: "label"
                alternateText: currentIndex < 0 ? qsTr("Choose follower") : ""
                Accessible.name: qsTr("Compatible follower")

                function captureChoice() {
                    root._choiceNotice = ""
                    root.captureControl()
                    capturedClient = root.client
                    contextToken = capturedClient ? capturedClient.captureFollowingContext() : ""
                    pressedChoices = root._followers.slice()
                }
                onModelChanged: currentIndex = Qt.binding(() => root._followerIndex)
                onPressedChanged: {
                    if (pressed && !popup.visible) captureChoice()
                }
                Keys.onPressed: event => {
                    if (!popup.visible) captureChoice()
                    event.accepted = false
                }
                onActivated: index => {
                    const choice = pressedChoices[index]
                    const current = root._followers[index]
                    if (capturedClient && capturedClient === root.client && choice && current
                        && choice.mode === current.mode) {
                        if (choice.mode !== capturedClient.selectedFollower)
                            capturedClient.selectFollower(choice.mode, contextToken)
                    } else {
                        root._choiceNotice = qsTr("Follower list changed. Choose again.")
                    }
                    contextToken = ""
                    capturedClient = null
                    pressedChoices = []
                    currentIndex = Qt.binding(() => root._followerIndex)
                }
                FocusOutline { }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: ScreenTools.defaultFontPixelHeight / 4
            visible: !!root.client && root.client.authenticated

            QGCCheckBox {
                id: blockCommands
                Layout.fillWidth: true
                objectName: "pixeagleBlockFlightCommands"
                text: qsTr("Block PixEagle flight commands")
                checked: root.client && root.client.safetyFresh && root.client.safetyActive
                enabled: !!root.client && root.client.canSetSafety
                         && (!root.client.safetyFollowerTest || !root.client.safetyActive)
                onClicked: {
                    const destination = root.client
                    const context = destination ? destination.captureSafetyContext() : ""
                    if (destination && context.length > 0) {
                        if (checked) {
                            destination.setSafetyActive(true, context)
                        } else {
                            permitFactory.open({destination: destination, contextToken: context})
                        }
                    }
                    checked = Qt.binding(() => root.client && root.client.safetyFresh && root.client.safetyActive)
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                font.pointSize: ScreenTools.smallFontPointSize
                text: root.client ? root.client.safetyStatusText : ""
                wrapMode: Text.WordWrap
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            text: trackerChoice.choiceNotice || root._choiceNotice
                  || (modelChoice.visible ? (modelChoice.choiceNotice
                      || (root.trackingController ? root.trackingController.modelFeedbackText : "")) : "")
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }

        QGCLabel {
            Layout.fillWidth: true
            text: root.trackingController ? root.trackingController.modeUnavailableReason : ""
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true

            QGCButton {
                Layout.fillWidth: true
                objectName: "pixeagleOptionsSettings"
                text: qsTr("Settings")
                focusPolicy: Qt.StrongFocus
                onClicked: {
                    root.close()
                    mainWindow.showSettingsTool("PixEagle")
                }
                FocusOutline { }
            }

            QGCButton {
                Layout.fillWidth: true
                objectName: "pixeagleOptionsDashboard"
                iconSource: "qrc:/InstrumentValueIcons/browser-window-open.svg"
                text: qsTr("Dashboard")
                focusPolicy: Qt.StrongFocus
                onClicked: {
                    if (!Qt.openUrlExternally(QGroundControl.corePlugin.pixeagle.dashboardUrl)) {
                        mainWindow.showMessage(qsTr("Could not open the browser. Check the address under Web dashboard in PixEagle settings."))
                    }
                }
                FocusOutline { }
            }
        }

        QGCButton {
            Layout.fillWidth: true
            text: qsTr("Camera controls")
            objectName: "pixeagleCameraControlsButton"
            visible: !!root.client && root.client.cameraFresh && !!root.client.cameraStatus.enabled
            onClicked: {
                root.closeChoices()
                root._openCameraAfterClose = true
                root.close()
            }
        }

        QGCButton {
            Layout.fillWidth: true
            objectName: "pixeagleResetPanelPosition"
            text: qsTr("Reset panel position")
            focusPolicy: Qt.StrongFocus
            onClicked: root.resetPanelRequested()
            FocusOutline { }
        }
    }

    QGCPopupDialogFactory {
        id: permitFactory
        dialogComponent: QGCPopupDialog {
            id: permitPopup
            required property var destination
            required property string contextToken
            title: qsTr("Permit PixEagle flight commands?")
            buttons: Dialog.Yes | Dialog.Cancel
            onAccepted: {
                if (destination === root.client && !destination.setSafetyActive(false, contextToken)) {
                    destination.refreshSafety()
                }
            }
            QGCLabel {
                width: Math.min(ScreenTools.defaultFontPixelWidth * 48, permitPopup.maxContentAvailableWidth)
                text: qsTr("This permits PixEagle to dispatch aircraft commands when its other following checks pass. It does not change QGC flight controls or camera Stop. Review the aircraft and follower before continuing.")
                wrapMode: Text.WordWrap
            }
        }
    }
}
