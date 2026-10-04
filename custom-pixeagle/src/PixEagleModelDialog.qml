import QGroundControl
import QGroundControl.Controls
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

QGCPopupDialog {
    id: root

    required property var client
    required property var trackingController
    property string selectedModelId: ""
    readonly property var _models: trackingController ? trackingController.modelChoices : []
    readonly property int _selectedIndex: {
        for (let index = 0; index < _models.length; ++index) {
            if (_models[index].modelId === selectedModelId) return index
        }
        return -1
    }
    readonly property var _selectedModel: _selectedIndex >= 0 ? _models[_selectedIndex] : null

    buttons: Dialog.Close
    title: qsTr("Smart model")
    onOpened: closePolicy |= Popup.CloseOnEscape

    component FocusOutline: Rectangle {
        anchors.fill: parent
        border.color: QGroundControl.globalPalette.text
        border.width: 1
        color: "transparent"
        radius: ScreenTools.defaultBorderRadius
        visible: parent.visualFocus
    }

    function selectConfiguredModel() {
        if (!selectedModelId && trackingController)
            selectedModelId = trackingController.configuredModelId
    }

    Component.onCompleted: {
        if (trackingController) trackingController.setModelsRequested(true)
        selectConfiguredModel()
    }
    Component.onDestruction: {
        if (trackingController) trackingController.setModelsRequested(false)
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
            if (!root.client || !root.client.authenticated) root.close()
        }
    }

    Connections {
        target: root.trackingController
        function onChanged() { root.selectConfiguredModel() }
    }

    ColumnLayout {
        spacing: ScreenTools.defaultFontPixelHeight / 2
        width: Math.min(root.maxContentAvailableWidth, ScreenTools.defaultFontPixelWidth * 64)

        QGCLabel {
            Layout.fillWidth: true
            text: root.client && !root.client.companionOnly ? root.client.vehicleLabel : qsTr("PixEagle companion")
            wrapMode: Text.WordWrap
        }

        QGCLabel {
            Layout.fillWidth: true
            text: root.trackingController ? root.trackingController.modelStatusText : qsTr("Connect to PixEagle first.")
            wrapMode: Text.WordWrap
        }

        QGCComboBox {
            Layout.fillWidth: true
            currentIndex: root._selectedIndex
            enabled: root.trackingController && !root.trackingController.busy && root._models.length > 0
            model: root._models
            objectName: "pixeagleModelChoice"
            textRole: "label"
            Accessible.name: qsTr("Installed Smart model")

            onModelChanged: currentIndex = Qt.binding(() => root._selectedIndex)
            onActivated: index => root.selectedModelId = root._models[index].modelId

            FocusOutline { }
        }

        QGCLabel {
            Layout.fillWidth: true
            text: root._selectedModel ? root._selectedModel.reason : ""
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true

            QGCButton {
                property string contextToken: ""
                property string pressedModelId: ""

                Layout.fillWidth: true
                enabled: root.trackingController && root.trackingController.canSelectModel
                         && root._selectedModel && root._selectedModel.available
                         && root.selectedModelId !== root.trackingController.configuredModelId
                focusPolicy: Qt.StrongFocus
                objectName: "pixeagleUseModel"
                text: qsTr("Use model")

                onPressed: {
                    pressedModelId = root.selectedModelId
                    contextToken = root.trackingController.captureModelContext()
                }
                onCanceled: { contextToken = ""; pressedModelId = "" }
                onClicked: {
                    root.trackingController.selectModel(pressedModelId, contextToken)
                    contextToken = ""
                    pressedModelId = ""
                }

                FocusOutline { }
            }

            QGCButton {
                enabled: root.trackingController && !root.trackingController.busy
                focusPolicy: Qt.StrongFocus
                text: qsTr("Refresh")
                onClicked: root.trackingController.refreshModels()

                FocusOutline { }
            }
        }

        QGCLabel {
            Layout.fillWidth: true
            text: root.trackingController && root.trackingController.smartMode
                  ? (root.trackingController.trackingActive
                     ? qsTr("Cancel tracking before changing models.")
                     : qsTr("Choosing a model keeps Smart mode enabled. Device selection is automatic."))
                  : qsTr("To start detection, close this panel and choose Smart in Tracking setup.")
            wrapMode: Text.WordWrap
        }

        QGCButton {
            id: detailsButton

            checkable: true
            enabled: !!root._selectedModel
            focusPolicy: Qt.StrongFocus
            text: qsTr("Model details")

            FocusOutline { }
        }

        QGCLabel {
            Layout.fillWidth: true
            text: root._selectedModel ? qsTr("Task: %1").arg(root._selectedModel.task) : ""
            visible: detailsButton.checked
            wrapMode: Text.WordWrap
        }

        QGCLabel {
            Layout.fillWidth: true
            text: root._selectedModel ? root._selectedModel.labels.join(", ") : ""
            visible: detailsButton.checked
            wrapMode: Text.WordWrap
        }

        QGCLabel {
            Layout.fillWidth: true
            text: root._selectedModel
                  ? qsTr("Showing %1 of %2 classes.").arg(root._selectedModel.labels.length).arg(root._selectedModel.totalLabels)
                  : ""
            visible: detailsButton.checked && root._selectedModel && root._selectedModel.hasMoreLabels
            wrapMode: Text.WordWrap
        }
    }
}
