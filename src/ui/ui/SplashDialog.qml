import QtQuick 2.3
import QtQuick.Window 2.0
import QtQuick.Controls 2.2
import kyla 1.0

Rectangle {
    id: rectangle

    Text {
        id: applicationName
        text: SetupLogic.applicationName
        anchors.verticalCenterOffset: -32
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: 32
    }

    BusyIndicator {
        id: busyIndicator
        padding: 0
        anchors.verticalCenterOffset: 32
        anchors.horizontalCenterOffset: 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
    }

    Text {
        id: status
        y: 410
        color: "#b3b3b3"
        text: SetupLogic.status
        horizontalAlignment: Text.AlignHCenter
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        font.pixelSize: 12
    }
}
