import QtQuick 2.0
import QtQuick.Controls 2.1
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.3

Window {
    id: window
    width: 320
    height: 128
    ColumnLayout {
        id: columnLayout
        anchors.fill: parent

        Text {
            id: applicationName
            text: qsTr("kyla Development Kit")
            renderType: Text.NativeRendering
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 20
        }

        ColumnLayout {
            id: columnLayout1
            width: 100
            height: 100
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillWidth: true

            Text {
                id: prepareText
                text: qsTr("Preparing installation")
                renderType: Text.NativeRendering
                font.pixelSize: 12
            }

            ProgressBar {
                id: progressBar
                indeterminate: true
                value: 0.5
            }
        }



    }

}
