import QtQuick 2.0
import QtQuick.Controls 2.0
import QtQuick.Window 2.1
import QtQuick.Layouts 1.1

Window {
    id: splash
    title: "Splash window"
    modality: Qt.ApplicationModal
    flags: Qt.SplashScreen
    x: (Screen.width - splash.width) / 2
    y: (Screen.height - splash.height) / 2

    width: 480
    height: 270

    GridLayout {
        id: grid1
        anchors.fill: parent

        columns: 5
        rows: 5

        ProgressBar {
            id: progressBar1
            indeterminate: true
            value: 0.5

            Layout.row: 1
            Layout.column: 2
            Layout.rowSpan: 3
        }
    }
}
