import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

Item {
    id: root

    anchors.top: parent.top
    anchors.bottom: parent.bottom
    width: activity.implicitWidth + ScreenTools.defaultFontPixelWidth * 2
    objectName: "toolbar_pixeagleIndicator"
    activeFocusOnTab: showIndicator

    readonly property bool showIndicator: !!_client && _client.enabled
    readonly property var _manager: QGroundControl.corePlugin.pixeagle
    readonly property var _client: _manager ? _manager.activeClient : null
    readonly property var _video: _manager ? _manager.video : null
    readonly property bool _verified: !!_client && _client.authenticated
                                      && (_client.companionOnly || _client.associationVerified)
    readonly property bool _needsVerification: !!_client && _client.authenticated
                                               && !_client.companionOnly && !_client.associationVerified
    readonly property var _targets: _video ? _video.targets : null
    readonly property string _trackingState: _verified && _targets ? _targets.trackingState : "unknown"
    readonly property string _followingState: _verified ? _client.followingState : "unknown"
    readonly property string _trackingText: _trackingState === "tracking" ? qsTr("Tracking")
                                          : _trackingState === "lost" ? qsTr("Lost")
                                          : _trackingState === "acquiring" ? qsTr("Acquiring")
                                          : _trackingState === "updating" ? qsTr("Updating…")
                                          : _trackingState === "idle" ? qsTr("No target") : qsTr("Unknown")
    readonly property string _followingText: _verified ? _client.followingSummaryText : qsTr("Unknown")
    readonly property string _status: !_client ? qsTr("PixEagle unavailable")
                                             : !_verified || _client.busy ? _client.statusText
                                             : _video ? _video.statusText : _client.statusText

    Accessible.role: Accessible.Button
    Accessible.name: qsTr("PixEagle")
    Accessible.description: _status + ". " + qsTr("Target: %1").arg(_trackingText)
                            + (_client && !_client.companionOnly ? ". " + qsTr("Aircraft following: %1").arg(_followingText) : "")
    Accessible.onPressAction: openDetails()

    function openDetails() {
        mainWindow.showIndicatorDrawer(connectionPage, root)
    }

    Keys.onReturnPressed: openDetails()
    Keys.onSpacePressed: openDetails()

    QGCPalette { id: qgcPal }

    ColumnLayout {
        id: activity
        anchors.centerIn: parent
        spacing: 0

        QGCLabel {
            text: qsTr("PixEagle")
            Layout.alignment: Qt.AlignHCenter
        }

        RowLayout {
            spacing: ScreenTools.defaultFontPixelWidth / 2
            visible: root._verified

            QGCColoredImage {
                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight
                Layout.preferredWidth: height
                source: "qrc:/InstrumentValueIcons/target.svg"
                fillMode: Image.PreserveAspectFit
                color: root._trackingState === "tracking" ? qgcPal.colorGreen
                       : root._trackingState === "idle" ? qgcPal.text : qgcPal.colorOrange
            }
            QGCLabel {
                objectName: "pixeagleToolbarTargetState"
                font.pointSize: ScreenTools.smallFontPointSize
                text: root._trackingText
            }
            QGCColoredImage {
                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight
                Layout.preferredWidth: height
                visible: !!root._client && !root._client.companionOnly
                source: "qrc:/InstrumentValueIcons/airplane.svg"
                fillMode: Image.PreserveAspectFit
                color: root._followingState === "active" ? qgcPal.colorGreen
                       : ["idle", "ready"].includes(root._followingState) ? qgcPal.text : qgcPal.colorOrange
            }
            QGCLabel {
                objectName: "pixeagleToolbarFollowingState"
                visible: !!root._client && !root._client.companionOnly
                font.pointSize: ScreenTools.smallFontPointSize
                text: root._followingText
            }
        }
        QGCLabel {
            font.pointSize: ScreenTools.smallFontPointSize
            Layout.alignment: Qt.AlignHCenter
            visible: !root._verified
            text: root._needsVerification ? qsTr("Verify vehicle")
                  : root._client && root._client.authenticated ? qsTr("Check connection") : qsTr("Sign in")
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: qgcPal.text
        border.width: 1
        radius: ScreenTools.defaultBorderRadius
        visible: root.activeFocus
    }

    MouseArea {
        id: indicatorMouse
        anchors.fill: parent
        hoverEnabled: !ScreenTools.isMobile
        onClicked: root.openDetails()
    }

    ToolTip.visible: indicatorMouse.containsMouse
    ToolTip.text: root.Accessible.description

    Component {
        id: connectionPage

        ToolIndicatorPage {
            id: page

            contentComponent: ColumnLayout {
                spacing: ScreenTools.defaultFontPixelHeight / 2
                width: ScreenTools.defaultFontPixelWidth * 36

                QGCLabel {
                    Layout.fillWidth: true
                    text: root._client ? qsTr("PixEagle · %1").arg(root._client.vehicleLabel) : qsTr("PixEagle")
                    font.pointSize: ScreenTools.mediumFontPointSize
                    wrapMode: Text.WordWrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    text: root._status
                    wrapMode: Text.WordWrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    visible: root._verified
                    text: qsTr("Target: %1").arg(root._trackingText)
                    wrapMode: Text.WordWrap
                }

                QGCLabel {
                    Layout.fillWidth: true
                    visible: root._verified && !!root._client && !root._client.companionOnly
                    text: root._client ? root._client.followingStatusText : ""
                    wrapMode: Text.WordWrap
                }

                QGCButton {
                    property var capturedClient: null

                    Layout.fillWidth: true
                    objectName: "pixeagleToolbarVerify"
                    text: root._client && root._client.busy ? qsTr("Verifying…") : qsTr("Verify vehicle")
                    visible: root._needsVerification
                    enabled: !!root._client && root._client.canVerify
                    onPressed: capturedClient = root._client
                    onCanceled: capturedClient = null
                    onClicked: {
                        if (capturedClient && capturedClient === root._client && capturedClient.canVerify) {
                            capturedClient.verifyVehicle()
                        }
                        capturedClient = null
                    }
                }

                QGCButton {
                    Layout.fillWidth: true
                    objectName: "pixeagleToolbarDashboard"
                    iconSource: "qrc:/InstrumentValueIcons/browser-window-open.svg"
                    text: qsTr("Dashboard")
                    onClicked: {
                        if (!Qt.openUrlExternally(root._manager.dashboardUrl)) {
                            mainWindow.showMessage(qsTr("Could not open the browser. Check the address under Web dashboard in PixEagle settings."))
                        }
                    }
                }

                QGCButton {
                    Layout.fillWidth: true
                    objectName: "pixeagleToolbarSettings"
                    text: root._client && root._client.authenticated ? qsTr("Settings") : qsTr("Sign in")
                    onClicked: {
                        if (page.drawer) page.drawer.close()
                        mainWindow.showSettingsTool("PixEagle")
                    }
                }
            }
        }
    }
}
