import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    objectName: "panicLockPage"

    Rectangle {
        anchors.fill: parent
        color: "#808080"
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 16

        TextField {
            id: phraseField
            objectName: "panicPhraseField"
            Layout.preferredWidth: 400
            placeholderText: "Recovery phrase"
        }

        Button {
            objectName: "panicUnlockButton"
            text: "Unlock"
            onClicked: app.unlockVault(phraseField.text)
        }
    }
}
