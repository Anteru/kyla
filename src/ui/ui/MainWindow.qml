import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Window 2.2

ApplicationWindow {
    title: qsTr("Installation")
    width: 400
    height: 240
    visible: true

    Loader {
        id: loader
        anchors.fill: parent
        source: "SplashDialog.qml"
    }
}
