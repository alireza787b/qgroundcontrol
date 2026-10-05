import QGC
import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property bool compactView: false
    property var toolInsets: null
    readonly property var _controller: QGroundControl.corePlugin.pixeagle.video
    readonly property var _client: QGroundControl.corePlugin.pixeagle.activeClient
    readonly property var trackingController: _controller ? _controller.targets : null
    readonly property bool _connectionReady: !!_client && _client.authenticated
    readonly property bool _needsVerification: !!_client && _client.authenticated
                                               && !_client.companionOnly && !_client.associationVerified
    readonly property bool _compact: compactView
    readonly property var _tapFact: QGroundControl.settingsManager.pixEagleSettings.tapToTarget
    readonly property bool _modelsRequested: visible && !_compact && !!_client && _client.authenticated
                                              && !!trackingController && (trackingController.smartMode || !!_optionsDialog)
                                              && !trackingController.externalTracker
    readonly property var _models: trackingController ? trackingController.modelChoices : []
    readonly property var _followers: _client ? _client.followerChoices : []
    readonly property int _followerIndex: {
        for (let index = 0; index < _followers.length; ++index) {
            if (_followers[index].mode === _client.selectedFollower) return index
        }
        return -1
    }
    readonly property bool _smartMode: trackingController && trackingController.smartMode
    readonly property int _modelIndex: {
        const current = trackingController ? trackingController.configuredModelId : ""
        for (let index = 0; index < _models.length; ++index) {
            if (_models[index].modelId === current) return index
        }
        return -1
    }

    property var _optionsDialog: null
    property bool _cameraPanelOpen: false
    readonly property string _trackingState: trackingController ? trackingController.trackingState : "unknown"
    readonly property string _followingState: _client ? _client.followingState : "unknown"
    readonly property string _trackerName: {
        if (!trackingController) return ""
        if (trackingController.externalTracker) return qsTr("Camera")
        if (_smartMode) return _modelIndex >= 0 ? _models[_modelIndex].label : qsTr("Smart model")
        const choices = trackingController.trackerChoices
        return trackingController.trackerIndex >= 0 ? choices[trackingController.trackerIndex].label : qsTr("Tracker")
    }

    component FocusOutline: Rectangle {
        anchors.fill: parent
        border.color: qgcPal.text
        border.width: 1
        color: "transparent"
        radius: ScreenTools.defaultBorderRadius
        visible: parent.visualFocus
    }

    component StatusRow: RowLayout {
        required property string icon
        required property string name
        required property string stateText
        required property string state
        required property string detail

        QGCColoredImage {
            Layout.preferredWidth: ScreenTools.defaultFontPixelHeight
            Layout.preferredHeight: width
            source: parent.icon
            color: qgcPal.text
            fillMode: Image.PreserveAspectFit
        }
        QGCLabel {
            Layout.fillWidth: true
            text: parent.name
            elide: Text.ElideRight
            Accessible.description: parent.detail
        }
        Rectangle {
            Layout.preferredWidth: ScreenTools.defaultFontPixelHeight / 2
            Layout.preferredHeight: width
            radius: width / 2
            color: parent.state === "tracking" || parent.state === "active" ? qgcPal.colorGreen
                   : ["unknown", "lost", "starting", "updating", "acquiring", "retargeting",
                      "coasting", "reacquiring", "stopping", "target_lost"].includes(parent.state)
                     ? qgcPal.colorOrange : qgcPal.text
        }
        QGCLabel { text: parent.stateText }
        ToolTip.visible: statusHover.hovered
        ToolTip.text: detail
        HoverHandler { id: statusHover }
    }

    Binding {
        target: root.trackingController
        property: "tapToTarget"
        value: root._tapFact.rawValue
        when: !!root.trackingController
    }
    Binding {
        target: root.trackingController
        property: "selectionEnabled"
        value: root.visible && !root._compact && !root._optionsDialog
        when: !!root.trackingController
    }

    function requestModels() {
        if (trackingController) trackingController.setModelsRequested(_modelsRequested)
    }
    function closeChoices() {
        if (_optionsDialog) _optionsDialog.close()
        _cameraPanelOpen = false
        startFollowing.clearIntent()
    }
    function captureControl() {
        if (!trackingController) return ""
        trackingController.cancelPointerGesture()
        return trackingController.captureControlContext()
    }
    function openOptions() {
        _cameraPanelOpen = false
        captureControl()
        startFollowing.clearIntent()
        if (!_optionsDialog) _optionsDialog = optionsFactory.open({client: _client, trackingController: trackingController})
    }

    on_ModelsRequestedChanged: Qt.callLater(root.requestModels)
    onVisibleChanged: { if (!visible) closeChoices() }
    on_ClientChanged: { closeChoices(); Qt.callLater(root.requestModels) }
    readonly property real _ratio: frame.sourceSize.height > 0 ? frame.sourceSize.width / frame.sourceSize.height : 16 / 9
    on_CompactChanged: {
        if (_compact) {
            closeChoices()
            if (trackingController) trackingController.cancelGesture()
        }
    }
    Component.onCompleted: {
        if (_controller) _controller.attachSurface(frame)
        requestModels()
    }
    Component.onDestruction: {
        if (root.trackingController) root.trackingController.setModelsRequested(false)
        if (root._controller) root._controller.detachSurface(frame)
    }

    QGCPopupDialogFactory { id: optionsFactory; dialogComponent: optionsComponent }
    Component {
        id: optionsComponent
        PixEagleOptionsDialog {
            onClosed: root._optionsDialog = null
            onResetPanelRequested: { targetPanel.resetPosition(); connectionPanel.resetPosition(); cameraPanel.resetPosition() }
            onCameraControlsRequested: {
                root.captureControl()
                startFollowing.clearIntent()
                root._cameraPanelOpen = true
            }
        }
    }

    PixEagleCameraControls {
        id: cameraPanel
        z: 2
        client: root._client
        visible: root._cameraPanelOpen && root.visible && !root._compact
        initialX: root.width - width - ScreenTools.defaultFontPixelWidth * 2
        initialY: ScreenTools.defaultFontPixelHeight * 4
        onClosed: root._cameraPanelOpen = false
    }

    PixEagleVideoItem {
        id: frame
        anchors.centerIn: parent
        height: Math.min(parent.height, parent.width / root._ratio)
        objectName: "pixeagleVideoFrame"
        width: height * root._ratio
    }
    QGCPalette { id: qgcPal; colorGroupEnabled: root.enabled }

    PixEagleMovablePanel {
        id: connectionPanel
        initialX: (root.width - width) / 2
        initialY: ScreenTools.defaultFontPixelHeight * 4
        width: Math.min(root.width - edgeMargin * 2, ScreenTools.defaultFontPixelWidth * 44)
        height: connectionDetails.implicitHeight + ScreenTools.defaultFontPixelHeight
        visible: !root._compact && (!root._connectionReady || !root._controller || !root._controller.live)
        dragHandle: connectionGrip
        onDragStarted: { root.captureControl(); startFollowing.clearIntent() }

        ColumnLayout {
            id: connectionDetails
            anchors.centerIn: parent
            width: parent.width - ScreenTools.defaultFontPixelWidth * 2
            spacing: ScreenTools.defaultFontPixelHeight / 3
            Item {
                id: connectionGrip
                Layout.fillWidth: true
                implicitHeight: ScreenTools.defaultFontPixelHeight
                Accessible.name: qsTr("Move connection panel")
                Rectangle { anchors.centerIn: parent; width: ScreenTools.defaultFontPixelWidth * 3; height: 3; radius: 1.5; color: qgcPal.text }
            }
            QGCLabel {
                Layout.fillWidth: true
                text: root._controller ? root._controller.statusText : qsTr("Connecting…")
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                QGCButton {
                    property var capturedClient: null
                    enabled: !!root._client && root._client.canVerify
                    objectName: "pixeagleFlyVerify"
                    text: root._client && root._client.busy ? qsTr("Verifying…") : qsTr("Verify vehicle")
                    visible: root._needsVerification
                    onPressed: capturedClient = root._client
                    onCanceled: capturedClient = null
                    onClicked: {
                        if (capturedClient && capturedClient === root._client) capturedClient.verifyVehicle()
                        capturedClient = null
                    }
                }
                QGCButton {
                    objectName: "pixeagleFlyConnectionSettings"
                    text: root._client && root._client.authenticated ? qsTr("Settings") : qsTr("Sign in")
                    onClicked: mainWindow.showSettingsTool("PixEagle")
                }
            }
        }
    }

    PixEagleMovablePanel {
        id: targetPanel
        readonly property real instrumentInset: !root.toolInsets || QGroundControl.videoManager.fullScreen ? 0
            : Math.max(root.toolInsets.bottomEdgeLeftInset, root.toolInsets.bottomEdgeRightInset)
        initialX: (root.width - width) / 2
        initialY: root.height - height - instrumentInset - ScreenTools.defaultFontPixelHeight
        width: Math.min(root.width - edgeMargin * 2, ScreenTools.defaultFontPixelWidth * 45)
        height: controls.implicitHeight + ScreenTools.defaultFontPixelHeight
        visible: !root._compact && !!root.trackingController && !!root._client
                 && ((root._connectionReady && root._controller && root._controller.live) || root._client.canStopFollowing)
        dragHandle: panelGrip
        objectName: "pixeagleOperatorPanel"
        onDragStarted: { root.captureControl(); startFollowing.clearIntent() }

        ColumnLayout {
            id: controls
            anchors.centerIn: parent
            width: parent.width - ScreenTools.defaultFontPixelWidth * 2
            spacing: ScreenTools.defaultFontPixelHeight / 3
            Item {
                id: panelGrip
                Layout.fillWidth: true
                implicitHeight: ScreenTools.defaultFontPixelHeight
                Accessible.name: qsTr("Move PixEagle panel")
                QGCLabel {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("PixEagle")
                    font.pointSize: ScreenTools.defaultFontPointSize
                    font.bold: true
                }
                Rectangle { anchors.centerIn: parent; width: ScreenTools.defaultFontPixelWidth * 3; height: 3; radius: 1.5; color: qgcPal.text }
            }
            RowLayout {
                Layout.fillWidth: true
                Repeater {
                    model: root.trackingController ? root.trackingController.selectionModes : []
                    QGCButton {
                        required property int index
                        required property var modelData
                        property string contextToken: ""
                        Layout.fillWidth: true
                        checked: !!root.trackingController && root.trackingController.selectedSelectionMode === modelData.id
                        enabled: !!root.trackingController && (checked || (root.trackingController.canChangeMode && modelData.available))
                        text: modelData.label
                        objectName: modelData.id === "classic" ? "pixeagleClassicMode" : "pixeagleSmartMode"
                        focusPolicy: Qt.StrongFocus
                        onPressed: contextToken = root.captureControl()
                        onCanceled: contextToken = ""
                        onClicked: {
                            if (!checked) root.trackingController.selectSelectionMode(modelData.id, contextToken)
                            contextToken = ""
                        }
                        FocusOutline { }
                    }
                }
                QGCButton {
                    Layout.preferredWidth: implicitHeight
                    Layout.maximumWidth: implicitHeight
                    leftPadding: ScreenTools.defaultFontPixelWidth / 2
                    rightPadding: ScreenTools.defaultFontPixelWidth / 2
                    objectName: "pixeagleTrackingOptions"
                    iconSource: "qrc:/InstrumentValueIcons/cog.svg"
                    Accessible.name: qsTr("Tracking options")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Options")
                    onClicked: root.openOptions()
                    focusPolicy: Qt.StrongFocus
                    FocusOutline { }
                }
            }
            StatusRow {
                Layout.fillWidth: true
                objectName: "pixeagleTrackerSummary"
                icon: "qrc:/InstrumentValueIcons/target.svg"
                name: root._trackerName
                state: root._trackingState
                stateText: state === "tracking" ? qsTr("Tracking") : state === "lost" ? qsTr("Lost")
                    : state === "acquiring" ? qsTr("Acquiring") : state === "updating" ? qsTr("Updating…")
                    : state === "unknown" ? qsTr("Checking…") : qsTr("No target")
                detail: root.trackingController ? root.trackingController.statusText : ""
            }
            StatusRow {
                Layout.fillWidth: true
                objectName: "pixeagleFollowerSummary"
                visible: !!root._client && root._client.selectedFollower.length > 0
                icon: "qrc:/InstrumentValueIcons/airplane.svg"
                name: root._followerIndex >= 0 ? root._followers[root._followerIndex].label : qsTr("Follower")
                state: root._followingState
                stateText: root._client ? root._client.followingSummaryText : qsTr("Unknown")
                detail: root._client ? root._client.followingStatusText : ""
            }
            RowLayout {
                Layout.fillWidth: true
                visible: !!root._client && root._client.safetyFresh
                         && (root._client.safetyActive || root._client.safetyFollowerTest)
                spacing: ScreenTools.defaultFontPixelWidth / 2
                QGCColoredImage {
                    Layout.preferredWidth: ScreenTools.defaultFontPixelHeight
                    Layout.preferredHeight: width
                    source: "qrc:/InstrumentValueIcons/airplane.svg"
                    color: qgcPal.text
                }
                QGCLabel {
                    font.pointSize: ScreenTools.smallFontPointSize
                    text: root._client && root._client.safetyFollowerTest ? qsTr("Follower test")
                          : qsTr("Commands blocked")
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: !root._tapFact.rawValue || (root.trackingController && root.trackingController.canCancel)
                QGCButton {
                    Layout.fillWidth: true
                    visible: !root._tapFact.rawValue
                    enabled: !!root.trackingController && (root.trackingController.canSelect || root.trackingController.selectionArmed)
                    checked: !!root.trackingController && root.trackingController.selectionArmed
                    text: checked ? qsTr("Cancel selection") : root.trackingController && root.trackingController.trackingActive ? qsTr("Retarget") : qsTr("Select target")
                    objectName: "pixeagleSelectTarget"
                    onClicked: root.trackingController.armSelection()
                }
                QGCButton {
                    property string contextToken: ""
                    Layout.fillWidth: true
                    visible: !!root.trackingController && root.trackingController.canCancel
                    text: qsTr("Clear target")
                    objectName: "pixeagleCancelTracking"
                    onPressed: contextToken = root.captureControl()
                    onCanceled: contextToken = ""
                    onClicked: { root.trackingController.cancelTracking(contextToken); contextToken = "" }
                }
            }
            QGCDelayButton {
                id: startFollowing
                property string contextToken: ""
                property var capturedClient: null
                function clearIntent() { contextToken = ""; capturedClient = null }
                Layout.fillWidth: true
                objectName: "pixeagleStartFollowing"
                text: qsTr("Start following")
                Accessible.description: qsTr("Hold to confirm following the selected target")
                visible: !!root._client && !root._client.companionOnly && !root._client.canStopFollowing
                         && root._trackingState === "tracking"
                enabled: !!root._client && root._client.canStartFollowing && !root._optionsDialog
                focusPolicy: Qt.StrongFocus
                onPressed: {
                    root.captureControl()
                    capturedClient = root._client
                    contextToken = capturedClient.captureFollowingContext()
                }
                onCanceled: clearIntent()
                onReleased: { clearIntent(); checked = false }
                onVisibleChanged: { if (!visible) clearIntent() }
                onEnabledChanged: { if (!enabled) clearIntent() }
                onActivated: {
                    if (capturedClient && capturedClient === root._client && contextToken.length > 0)
                        capturedClient.startFollowing(contextToken)
                    clearIntent()
                }
                FocusOutline { }
            }
            QGCButton {
                property string contextToken: ""
                property var capturedClient: null
                Layout.fillWidth: true
                objectName: "pixeagleStopFollowing"
                text: qsTr("Stop following")
                visible: !!root._client && root._client.canStopFollowing
                onPressed: { capturedClient = root._client; contextToken = capturedClient.captureFollowingStop() }
                onCanceled: { capturedClient = null; contextToken = "" }
                onClicked: {
                    if (capturedClient && capturedClient === root._client) capturedClient.stopFollowing(contextToken)
                    capturedClient = null
                    contextToken = ""
                }
            }
            QGCLabel {
                Layout.fillWidth: true
                text: root.trackingController ? root.trackingController.actionNotice : ""
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }
            QGCLabel {
                Layout.fillWidth: true
                text: root._client ? root._client.followingActionError : ""
                visible: text.length > 0
                color: qgcPal.warningText
                wrapMode: Text.WordWrap
            }
            QGCLabel {
                Layout.fillWidth: true
                text: root._client ? root._client.followingStatusText : ""
                visible: !!root._client && root._client.selectedFollower.length > 0
                         && (root._client.companionOnly || root._trackingState === "tracking")
                         && !root._client.canStartFollowing && !root._client.canStopFollowing
                wrapMode: Text.WordWrap
            }
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        color: qgcPal.windowShade
        height: compactStatus.implicitHeight + ScreenTools.defaultFontPixelHeight / 3
        visible: root._compact
        width: parent.width

        QGCLabel {
            id: compactStatus

            Accessible.name: root._controller ? root._controller.ownerText + ". " + root._controller.statusText : qsTr("PixEagle")
            anchors.centerIn: parent
            color: qgcPal.text
            elide: Text.ElideRight
            font.pointSize: ScreenTools.smallFontPointSize
            horizontalAlignment: Text.AlignHCenter
            text: root._controller ? root._controller.compactText : qsTr("PixEagle")
            width: parent.width - ScreenTools.defaultFontPixelWidth
        }
    }

    QGCButton {
        id: exitFullscreen

        anchors.margins: ScreenTools.defaultFontPixelHeight / 2
        anchors.right: parent.right
        anchors.top: parent.top
        objectName: "pixeagleExitFullscreen"
        focusPolicy: Qt.StrongFocus
        text: qsTr("Exit fullscreen")
        visible: QGroundControl.videoManager.fullScreen

        onClicked: QGroundControl.videoManager.fullScreen = false
    }
}
