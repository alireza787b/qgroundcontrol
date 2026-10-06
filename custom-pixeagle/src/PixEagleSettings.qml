import QGC
import QGroundControl
import QGroundControl.Controls
import QGroundControl.FactControls
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    readonly property var _client: _manager ? _manager.activeClient : null
    readonly property bool _enabled: _enabledFact.rawValue
    readonly property var _enabledFact: QGroundControl.settingsManager.pixEagleSettings.integrationEnabled
    readonly property var _manager: QGroundControl.corePlugin.pixeagle
    readonly property var _targets: _manager && _manager.video ? _manager.video.targets : null
    property int sectionFilter: -1
    property string _browserError: ""

    component FocusOutline: Rectangle {
        anchors.fill: parent
        border.color: QGroundControl.globalPalette.text
        border.width: 1
        color: "transparent"
        radius: ScreenTools.defaultBorderRadius
        visible: parent.visualFocus
    }

    component Disclosure: SectionHeader {
        Layout.fillWidth: true
        Accessible.name: checked ? qsTr("Collapse %1").arg(text) : qsTr("Expand %1").arg(text)
        Accessible.role: Accessible.Button
        checked: false
        focusPolicy: Qt.StrongFocus
        showSpacer: false

        FocusOutline { }
    }

    function openDashboard() {
        root._browserError = Qt.openUrlExternally(root._manager.dashboardUrl) ? "" : qsTr("Could not open the browser. Check the address under Web dashboard.");
    }

    function signIn() {
        const destination = root._client;
        if (!destination) {
            return;
        }
        destination.signInAtRemembered(address.text, username.text, password.text);
    }

    on_ClientChanged: {
        password.clear();
        username.clear();
        details.checked = false;
        dashboardDetails.checked = false;
        advancedTracking.checked = false;
        root._browserError = "";
        address.text = root._client ? root._client.endpoint : "";
    }
    on_EnabledChanged: password.clear()

    QGCFlickable {
        anchors.fill: parent
        contentHeight: content.height + ScreenTools.defaultFontPixelHeight * 2
        contentWidth: width

        ColumnLayout {
            id: content

            spacing: ScreenTools.defaultFontPixelHeight
            width: Math.min(parent.width - x * 2, ScreenTools.defaultFontPixelWidth * 75)
            x: ScreenTools.defaultFontPixelWidth * 2
            y: ScreenTools.defaultFontPixelHeight

            RowLayout {
                Layout.fillWidth: true

                QGCLabel {
                    Layout.fillWidth: true
                    font.pointSize: ScreenTools.largeFontPointSize
                    text: qsTr("PixEagle")
                }

                QGCButton {
                    enabled: root._manager && root._manager.dashboardUrl.toString().length > 0
                    focusPolicy: Qt.StrongFocus
                    iconSource: "qrc:/InstrumentValueIcons/browser-window-open.svg"
                    objectName: "pixeagleOpenDashboard"
                    text: qsTr("Dashboard")
                    visible: root._enabled
                    ToolTip.text: qsTr("Open dashboard in your browser")
                    ToolTip.visible: hovered || activeFocus

                    onClicked: root.openDashboard()
                    FocusOutline { }
                }
            }

            QGCCheckBox {
                checked: root._enabled
                objectName: "pixeagleEnabled"
                text: qsTr("Enable PixEagle")

                onClicked: root._enabledFact.rawValue = checked
            }

            QGCLabel {
                Layout.fillWidth: true
                text: root._enabled ? qsTr("Sign in to use PixEagle video and tracking. An aircraft is not required.") : qsTr("Enable PixEagle to use its video and target tracking in QGC.")
                visible: !root._client || !root._client.authenticated
                wrapMode: Text.WordWrap
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: ScreenTools.defaultFontPixelHeight
                visible: root._enabled

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: ScreenTools.defaultFontPixelHeight / 2
                    visible: root._client !== null

                    QGCLabel {
                        text: qsTr("QGC vehicle")
                        visible: root._client && !root._client.companionOnly
                    }

                    QGCComboBox {
                        Layout.fillWidth: true
                        currentIndex: root._manager ? root._manager.activeIndex : -1
                        model: root._manager ? root._manager.vehicleLabels : []
                        objectName: "pixeagleVehicle"
                        visible: root._client && !root._client.companionOnly

                        onActivated: index => root._manager.selectVehicle(index)
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        text: qsTr("Backend address")
                        wrapMode: Text.WordWrap
                    }

                    QGCTextField {
                        id: address

                        Layout.fillWidth: true
                        enabled: root._client !== null
                        inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
                        objectName: "pixeagleEndpoint"
                        placeholderText: root._client ? root._client.defaultEndpoint : ""
                        placeholderTextColor: QGroundControl.globalPalette.textFieldText
                        readOnly: !root._client || root._client.busy || root._client.authenticated
                        text: root._client ? root._client.endpoint : ""

                        onEditingFinished: {
                            if (root._client && !root._client.busy && !root._client.authenticated) {
                                root._client.endpoint = text;
                            }
                        }
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        text: qsTr("Leave blank to use the local backend on port 5077. The web dashboard has a separate address.")
                        visible: root._client && !root._client.authenticated
                        wrapMode: Text.WordWrap
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root._client && !root._client.authenticated

                        QGCLabel {
                            text: qsTr("Username")
                        }

                        QGCTextField {
                            id: username

                            Layout.fillWidth: true
                            enabled: root._client && !root._client.busy
                            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                            objectName: "pixeagleUsername"
                        }

                        QGCLabel {
                            text: qsTr("Password")
                        }

                        QGCTextField {
                            id: password

                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            enabled: root._client && !root._client.busy
                            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                            objectName: "pixeaglePassword"

                            onAccepted: root.signIn()
                        }

                        QGCCheckBox {
                            checked: root._client ? root._client.rememberSignIn : true
                            objectName: "pixeagleRememberSignIn"
                            text: qsTr("Remember sign-in on this device")

                            onClicked: root._client.setRememberSignIn(checked)
                        }

                        QGCLabel {
                            Layout.fillWidth: true
                            text: root._client ? root._client.credentialStatus : ""
                            visible: text.length > 0
                            wrapMode: Text.WordWrap
                        }

                        QGCButton {
                            enabled: root._client && !root._client.busy && username.text.trim().length > 0 && password.text.length > 0
                            objectName: "pixeagleSignIn"
                            text: qsTr("Sign in")

                            onClicked: root.signIn()
                        }
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        text: root._client ? qsTr("Signed in as %1").arg(root._client.signedInAs) : ""
                        visible: root._client && root._client.authenticated
                        wrapMode: Text.WordWrap
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        font.bold: true
                        objectName: "pixeagleConnectionStatus"
                        text: root._client ? root._client.statusText : ""
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: root._client && root._client.authenticated

                        QGCButton {
                            enabled: root._client && root._client.canVerify
                            objectName: "pixeagleVerify"
                            visible: root._client && !root._client.companionOnly
                            text: root._client && root._client.associationVerified ? qsTr("Verify again") : qsTr("Verify vehicle")

                            onClicked: root._client.verifyVehicle()
                        }

                        QGCButton {
                            enabled: root._client && root._client.canRefreshConnection
                            visible: root._client && root._client.canRefreshConnection
                            objectName: "pixeagleRefreshConnection"
                            text: qsTr("Reconnect")

                            onClicked: root._client.refresh()
                        }

                        QGCButton {
                            enabled: root._client && (root._client.companionOnly || root._client.associationVerified)
                            objectName: "pixeagleOpenFlyView"
                            text: qsTr("Open Fly View")

                            onClicked: {
                                if (mainWindow.allowViewSwitch()) {
                                    mainWindow.showFlyView();
                                }
                            }
                        }

                        QGCButton {
                            enabled: root._client && !root._client.busy
                            objectName: "pixeagleSignOut"
                            text: qsTr("Sign out")

                            onClicked: root._client.signOut()
                        }
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        text: qsTr("Target and follower controls are in Fly View when permitted.")
                        visible: root._client && root._client.authenticated
                        wrapMode: Text.WordWrap
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        text: root._client ? root._client.followingStatusText : ""
                        visible: root._client && root._client.authenticated && !root._client.companionOnly
                        wrapMode: Text.WordWrap
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root._manager && root._manager.followingVehicles.length > 0

                        QGCLabel { text: qsTr("Following aircraft") }

                        Repeater {
                            model: root._manager ? root._manager.followingVehicles : []

                            RowLayout {
                                Layout.fillWidth: true

                                QGCLabel {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                }

                                QGCButton {
                                    property string capturedToken: ""
                                    property int capturedIndex: -1
                                    enabled: modelData.stopAvailable
                                    focusPolicy: Qt.StrongFocus
                                    objectName: "pixeagleSettingsStopFollowing"
                                    text: qsTr("Stop following")
                                    onPressed: {
                                        capturedToken = modelData.stopToken
                                        capturedIndex = modelData.index
                                    }
                                    onCanceled: { capturedToken = ""; capturedIndex = -1 }
                                    onClicked: {
                                        root._manager.stopFollowingForVehicle(capturedIndex, capturedToken)
                                        capturedToken = ""
                                        capturedIndex = -1
                                    }
                                    FocusOutline { }
                                }
                            }
                        }
                    }

                    QGCLabel {
                        Layout.topMargin: ScreenTools.defaultFontPixelHeight / 2
                        font.bold: true
                        text: qsTr("Video and targeting")
                    }

                    FactCheckBox {
                        fact: QGroundControl.settingsManager.pixEagleSettings.videoEnabled
                        text: qsTr("Show video in Fly View")
                    }

                    FactCheckBox {
                        fact: QGroundControl.settingsManager.pixEagleSettings.tapToTarget
                        text: qsTr("Tap video to select targets")
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        text: qsTr("Tap another target to replace the current one. Turn off to use Select target or Retarget before each selection.")
                        wrapMode: Text.WordWrap
                    }

                    PixEagleConfigSection {
                        Layout.fillWidth: true
                        client: root._client
                    }

                    Disclosure {
                        id: advancedTracking
                        objectName: "pixeagleAdvancedTracking"
                        text: qsTr("Advanced tracking")
                        visible: root._client && root._client.authenticated
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: advancedTracking.checked && root._targets

                        QGCLabel { text: qsTr("Tracking runs on") }

                        QGCComboBox {
                            id: trackingEngineChoice
                            property string contextToken: ""
                            property var capturedChoices: []
                            Layout.fillWidth: true
                            objectName: "pixeagleTrackingEngine"
                            model: root._targets ? root._targets.targetEngines : []
                            textRole: "label"
                            currentIndex: {
                                if (!root._targets) return -1
                                for (let i = 0; i < model.length; ++i) {
                                    if (model[i].id === root._targets.targetEngine) return i
                                }
                                return -1
                            }
                            enabled: root._targets && root._targets.canConfigure
                            Accessible.name: qsTr("Tracking runs on")
                            function captureChoice() {
                                capturedChoices = model.slice()
                                root._targets.cancelPointerGesture()
                                contextToken = root._targets.captureControlContext()
                            }
                            onPressedChanged: { if (pressed && !popup.visible) captureChoice() }
                            Keys.onPressed: event => { if (!popup.visible) captureChoice(); event.accepted = false }
                            onActivated: index => {
                                const selected = capturedChoices[index]
                                if (selected && model[index] && selected.id === model[index].id)
                                    root._targets.selectTargetEngine(selected.id, contextToken, true)
                                contextToken = ""
                                currentIndex = Qt.binding(() => {
                                    if (!root._targets) return -1
                                    for (let i = 0; i < model.length; ++i) {
                                        if (model[i].id === root._targets.targetEngine) return i
                                    }
                                    return -1
                                })
                            }
                            FocusOutline { }
                        }

                        QGCLabel {
                            Layout.fillWidth: true
                            text: qsTr("Applying a new engine also saves it for the next PixEagle start. Select a new target after switching. Camera controls and video source are separate settings.")
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: root._targets && root._targets.savedEngine.length > 0
                                     && root._targets.savedEngine !== root._targets.targetEngine

                            QGCLabel {
                                Layout.fillWidth: true
                                text: qsTr("Current engine differs from the saved startup choice.")
                                wrapMode: Text.WordWrap
                            }
                            QGCButton {
                                text: qsTr("Retry save")
                                enabled: root._targets && root._targets.canConfigure
                                onClicked: {
                                    root._targets.cancelPointerGesture()
                                    const context = root._targets.captureControlContext()
                                    root._targets.selectTargetEngine(root._targets.targetEngine, context, true)
                                }
                            }
                        }
                    }

                    Disclosure {
                        id: dashboardDetails

                        objectName: "pixeagleDashboardDetails"
                        text: qsTr("Web dashboard")
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: dashboardDetails.checked

                        QGCLabel {
                            Layout.fillWidth: true
                            text: qsTr("Opens in your browser with a separate sign-in. Leave the address blank to use port 3040 on the same host and protocol as the backend.")
                            wrapMode: Text.WordWrap
                        }

                        QGCLabel {
                            text: qsTr("Custom address (optional)")
                        }

                        QGCTextField {
                            id: dashboardAddress

                            Layout.fillWidth: true
                            enabled: root._client && root._client.endpoint.length > 0
                            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
                            objectName: "pixeagleDashboardAddress"
                            Accessible.name: qsTr("Custom dashboard address")
                            placeholderText: root._manager ? root._manager.dashboardUrl.toString() : ""
                            placeholderTextColor: QGroundControl.globalPalette.textFieldText
                            text: root._manager ? root._manager.dashboardUrlOverride : ""
                        }

                        RowLayout {
                            QGCButton {
                                enabled: dashboardAddress.enabled
                                focusPolicy: Qt.StrongFocus
                                objectName: "pixeagleSaveDashboardAddress"
                                text: qsTr("Save")

                                onClicked: {
                                    root._browserError = root._manager.setDashboardUrlOverride(dashboardAddress.text) ? "" : qsTr("Enter an HTTP or HTTPS URL without credentials, query or fragment.");
                                }
                                FocusOutline { }
                            }

                            QGCButton {
                                enabled: dashboardAddress.enabled
                                focusPolicy: Qt.StrongFocus
                                text: qsTr("Use default")

                                onClicked: {
                                    root._manager.setDashboardUrlOverride("");
                                    root._browserError = "";
                                }
                                FocusOutline { }
                            }
                        }
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        text: root._browserError
                        visible: text.length > 0
                        wrapMode: Text.WordWrap
                    }

                    Disclosure {
                        id: details

                        text: qsTr("Connection details")
                        visible: root._client && root._client.authenticated
                    }

                    QGCLabel {
                        Layout.fillWidth: true
                        text: root._client ? root._client.diagnostics : ""
                        visible: details.checked && root._client && root._client.authenticated
                        wrapMode: Text.WrapAnywhere
                    }

                    QGCButton {
                        enabled: root._client && !root._client.busy
                        text: qsTr("Retry connection")
                        visible: details.checked && root._client && root._client.authenticated

                        onClicked: root._client.refresh()
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: details.checked && root._client && root._client.authenticated

                        QGCLabel {
                            text: qsTr("Unavailable trackers")
                            visible: trackerAvailability.count > 0
                        }

                        Repeater {
                            id: trackerAvailability
                            model: root._targets ? root._targets.unavailableTrackers : []

                            QGCLabel {
                                required property var modelData
                                Layout.fillWidth: true
                                text: qsTr("%1: %2").arg(modelData.label).arg(modelData.reason)
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            Flow {
                Layout.fillWidth: true
                Layout.topMargin: ScreenTools.defaultFontPixelHeight / 2
                spacing: ScreenTools.defaultFontPixelWidth

                QGCButton {
                    focusPolicy: Qt.StrongFocus
                    objectName: "pixeagleHelp"
                    text: qsTr("Connection help")
                    onClicked: helpFactory.open()
                    FocusOutline { }
                }

                QGCButton {
                    focusPolicy: Qt.StrongFocus
                    objectName: "pixeagleAbout"
                    text: qsTr("About PixEagle")
                    onClicked: aboutFactory.open()
                    FocusOutline { }
                }
            }
        }
    }

    QGCPopupDialogFactory {
        id: helpFactory
        dialogComponent: helpDialog
    }

    Component {
        id: helpDialog

        QGCPopupDialog {
            id: helpPopup
            title: qsTr("Connect to PixEagle")
            buttons: Dialog.Close
            onOpened: closePolicy |= Popup.CloseOnEscape

            ColumnLayout {
                width: Math.min(ScreenTools.defaultFontPixelWidth * 60, helpPopup.maxContentAvailableWidth)
                spacing: ScreenTools.defaultFontPixelHeight

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Enable PixEagle, enter its backend address, then sign in with your PixEagle account. This build requires the native-integration backend.")
                    wrapMode: Text.WordWrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("The usual local address is http://127.0.0.1:5077. For a bench or private network, an HTTP address such as http://192.168.0.226:5077 is supported; use HTTPS for an untrusted network. Enter the final backend URL without /api/v1 and keep any configured reverse-proxy prefix.")
                    wrapMode: Text.WordWrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("With an aircraft connected, select the QGC vehicle and choose Verify vehicle. Without an aircraft, video and permitted tracking work in companion-only mode. Following requires a verified aircraft and a compatible PixEagle follower.")
                    wrapMode: Text.WordWrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Open Dashboard to use the web interface. If its address differs from the default, set it under Web dashboard. Sign in separately in your browser.")
                    wrapMode: Text.WordWrap
                }

                QGCButton {
                    focusPolicy: Qt.StrongFocus
                    text: qsTr("PixEagle documentation")
                    onClicked: Qt.openUrlExternally("https://github.com/alireza787b/PixEagle/blob/main/docs/README.md")
                    FocusOutline { }
                }
            }
        }
    }

    QGCPopupDialogFactory {
        id: aboutFactory
        dialogComponent: aboutDialog
    }

    Component {
        id: aboutDialog

        QGCPopupDialog {
            id: aboutPopup
            title: qsTr("About PixEagle")
            buttons: Dialog.Close
            onOpened: closePolicy |= Popup.CloseOnEscape

            ColumnLayout {
                width: Math.min(ScreenTools.defaultFontPixelWidth * 50, aboutPopup.maxContentAvailableWidth)
                spacing: ScreenTools.defaultFontPixelHeight

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Computer vision and target tracking for PX4 drones.")
                    wrapMode: Text.WordWrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("Copyright 2024-2025 Alireza Ghaderi")
                    wrapMode: Text.WordWrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: qsTr("PixEagle source code is licensed under Apache License 2.0. QGroundControl and third-party components retain their own licenses.")
                    wrapMode: Text.WordWrap
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: ScreenTools.defaultFontPixelWidth

                    QGCButton {
                        focusPolicy: Qt.StrongFocus
                        text: qsTr("Source code")
                        onClicked: Qt.openUrlExternally("https://github.com/alireza787b/PixEagle")
                        FocusOutline { }
                    }

                    QGCButton {
                        focusPolicy: Qt.StrongFocus
                        text: qsTr("License")
                        onClicked: Qt.openUrlExternally("https://github.com/alireza787b/PixEagle/blob/main/LICENSE")
                        FocusOutline { }
                    }
                }
            }
        }
    }

    Connections {
        target: root._client

        function onEndpointChanged() {
            address.text = root._client ? root._client.endpoint : "";
        }

        function onChanged() {
            if (root._client && root._client.authenticated) password.clear();
        }
    }

    Connections {
        target: root._manager

        function onDashboardUrlChanged() {
            dashboardAddress.text = root._manager.dashboardUrlOverride;
            root._browserError = "";
        }
    }
}
