import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Audio 1.0

ApplicationWindow {
    id: window
    width: 480
    height: 600
    visible: true
    title: qsTr("WebSocket Audio Communication")
    color: "lightblue"

    property alias audioController: audioController

    AudioController {
        id: audioController
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // Title
        Text {
            text: qsTr("WebSocket Audio Communication")
            font.pixelSize: 24
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        // Mode selection
        GroupBox {
            title: qsTr("Mode")
            Layout.fillWidth: true

            RowLayout {
                anchors.fill: parent

                RadioButton {
                    id: serverRadio
                    text: qsTr("Server")
                    checked: audioController.isServer
                    enabled: !audioController.isConnected
                    onClicked: audioController.setMode(0)
                }

                RadioButton {
                    id: clientRadio
                    text: qsTr("Client")
                    checked: !audioController.isServer
                    enabled: !audioController.isConnected
                    onClicked: audioController.setMode(1)
                }
            }
        }

        // Server configuration
        GroupBox {
            title: qsTr("Server Configuration")
            Layout.fillWidth: true
            visible: audioController.isServer

            RowLayout {
                anchors.fill: parent

                Label {
                    text: qsTr("Port:")
                }

                SpinBox {
                    id: portSpinBox
                    from: 1024
                    to: 65535
                    value: audioController.serverPort
                    enabled: !audioController.isConnected
                    onValueChanged: audioController.serverPort = value
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }

        // Client configuration
        GroupBox {
            title: qsTr("Client Configuration")
            Layout.fillWidth: true
            visible: !audioController.isServer

            RowLayout {
                anchors.fill: parent

                Label {
                    text: qsTr("Server IP:")
                }

                TextField {
                    id: serverAddressField
                    placeholderText: qsTr("192.168.1.100")
                    text: "127.0.0.1"
                    enabled: !audioController.isConnected
                    Layout.fillWidth: true

                    Keys.onReturnPressed: {
                        if (!audioController.isConnected) {
                            connectButton.clicked()
                        }
                    }
                }
            }
        }

        // Connection controls
        RowLayout {
            Layout.fillWidth: true

            Button {
                id: connectButton
                text: audioController.isConnected ? qsTr("Disconnect") :
                                                    (audioController.isServer ? qsTr("Start Server") : qsTr("Connect"))
                Layout.fillWidth: true

                onClicked: {
                    if (audioController.isConnected) {
                        audioController.disconnect()
                    } else {
                        if (audioController.isServer) {
                            audioController.startServer()
                        } else {
                            if (serverAddressField.text.trim() !== "") {
                                audioController.connectToServer(serverAddressField.text.trim())
                            } else {
                                statusLabel.text = "Please enter server IP address"
                            }
                        }
                    }
                }
            }
        }

        // Status
        GroupBox {
            title: qsTr("Status")
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                anchors.fill: parent
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                Label {
                    id: statusLabel
                    text: audioController.statusMessage
                    wrapMode: Text.WordWrap
                    width: parent.width

                    Rectangle {
                        anchors.fill: parent
                        color: audioController.isConnected ? "#e8f5e8" : "#f5f5f5"
                        z: -1
                        radius: 4
                    }
                }
            }
        }

        CheckBox{
            id: aecCheckbox
            text: "Echo Cancellation"
            checked: true
            onCheckedChanged: audioController.enableAEC = checked

        }

        // Connection indicator
        Rectangle {
            Layout.fillWidth: true
            height: 40
            color: audioController.isConnected ? "#4CAF50" : "#f44336"
            radius: 8

            Text {
                anchors.centerIn: parent
                text: audioController.isConnected ?
                          qsTr("CONNECTED - Audio Active") :
                          qsTr("DISCONNECTED")
                color: "white"
                font.bold: true
                font.pixelSize: 16
            }
        }
    }

    // Prevent closing while connected
    onClosing: {
        if (audioController.isConnected) {
            audioController.disconnect()
        }
    }
}
