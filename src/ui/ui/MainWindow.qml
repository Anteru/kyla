import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Window 2.2
import QtQuick.Dialogs 1.2
import kyla 1.0

ApplicationWindow {
    title: qsTr("Installation")
    width: 400
    height: 240
    visible: true

	var isReady : false

    Loader {
        id: loader
        anchors.fill: parent
        source: "SplashDialog.qml"

		onLoaded: {
			if (SetupLogic.ready && isReady == false) {
				isReady = true;
				source = "SetupDialog.qml";
			}
		}
    }

	Connections {
		target: SetupLogic
		onRepositoryOpened: {
			if (success) {
				loader.source = "SetupDialog.qml";
			}
		}
	}
}
