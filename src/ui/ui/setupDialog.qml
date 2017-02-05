import QtQuick 2.0
import QtQuick.Controls 2.1
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3

Window {
    id: window
    property alias rowLayout: rowLayout
    ColumnLayout {
        id: mainLayout
        spacing: 0
        anchors.fill: parent

        Rectangle {
            id: rectangle
            height: 64
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: "#949494"
                }

                GradientStop {
                    position: 1
                    color: "#161616"
                }
            }
            anchors.right: parent.right
            anchors.left: parent.left
            anchors.top: parent.top

            Text {
                id: applicationName
                color: "#ffffff"
                text: qsTr("kyla setup")
                anchors.leftMargin: 16
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignLeft
                anchors.fill: parent
                font.pixelSize: 26
            }
        }

        Rectangle {
            id: featuresSelection
            width: 200
            height: 200
            color: "#ffffff"
            Layout.fillHeight: true
            Layout.fillWidth: true

            ColumnLayout {
                id: columnLayout
                anchors.fill: parent

                Text {
                    id: text3
                    text: qsTr("Text")
                    font.pixelSize: 12
                }

                ListView {
                    delegate: Item {

                    }
                }
            }
        }

        Rectangle {
            id: installSettings
            height: 32
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: "#606060"
                }

                GradientStop {
                    position: 1
                    color: "#303030"
                }
            }
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillWidth: true

            RowLayout {
                id: rowLayout
                anchors.rightMargin: 5
                anchors.right: parent.right
                anchors.left: parent.left
                anchors.leftMargin: 5
                anchors.verticalCenter: parent.verticalCenter

                RowLayout {
                    id: rowLayout1
                    width: parent.width * 0.5
                    Layout.maximumWidth: parent.width * 0.5

                    Text {
                        id: text1
                        color: "#dddddd"
                        text: qsTr("Target directory:")
                        font.pixelSize: 12
                    }

                    TextEdit {
                        id: textEdit
                        color: "#dddddd"
                        text: qsTr("Text Edit")
                        Layout.fillWidth: true
                        font.pixelSize: 12
                    }
                }

                Text {
                    id: text2
                    color: "#dddddd"
                    text: qsTr("requiredDiskSpace")
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    horizontalAlignment: Text.AlignRight
                    font.pixelSize: 12
                }

            }
        }

        Rectangle {
            id: installActions
            height: 32
            color: "#303030"
            Layout.fillWidth: true

            RowLayout {
                id: rowLayout2
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottomMargin: 0
                anchors.rightMargin: 5
                anchors.leftMargin: 5

                RowLayout {
                    id: rowLayout3
                    width: 100
                    Layout.fillWidth: true
                    Layout.maximumWidth: parent.width * 0.5

                    Text {
                        id: text4
                        color: "#dddddd"
                        text: qsTr("Installation progress")
                        font.pixelSize: 12
                    }

                    ProgressBar {
                        id: progressBar
                        value: 0.5
                    }
                }

                Button {
                    id: button
                    text: qsTr("Button")
                    Layout.maximumHeight: 16
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                }
            }
        }

    }

}
