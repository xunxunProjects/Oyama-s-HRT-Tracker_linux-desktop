import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    property string errorText: ""

    function attemptUnlock() {
        if (app.vaultBusy || phraseField.text.trim().length === 0) {
            return
        }
        const secret = phraseField.text
        phraseField.text = ""
        errorText = ""
        if (!app.unlockVault(secret)) {
            errorText = app.status
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(Math.max(page.width - 96, 240), 560)
        spacing: 14

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Vault locked")
            color: Theme.text
            font.pixelSize: Theme.title
            font.weight: Font.DemiBold
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Enter the seven-word recovery phrase to unlock " +
                       "your data.")
            color: Theme.textMuted
            font.pixelSize: Theme.body
            wrapMode: Text.WordWrap
        }

        TextField {
            id: phraseField
            objectName: "unlockPhrase"
            Layout.fillWidth: true
            echoMode: TextInput.Password
            selectByMouse: false
            placeholderText: qsTr("Recovery phrase")
            enabled: !app.vaultBusy
            onAccepted: page.attemptUnlock()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                objectName: "unlockButton"
                text: qsTr("Unlock")
                highlighted: true
                enabled: !app.vaultBusy && phraseField.text.trim().length > 0
                onClicked: page.attemptUnlock()
            }

            BusyIndicator {
                objectName: "unlockBusy"
                visible: app.vaultBusy
                running: app.vaultBusy
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
            }

            Item { Layout.fillWidth: true }
        }

        Label {
            objectName: "unlockError"
            Layout.fillWidth: true
            visible: page.errorText.length > 0
            text: page.errorText
            color: Theme.danger
            font.pixelSize: Theme.body
            wrapMode: Text.WordWrap
        }
    }

    Connections {
        target: app
        function onChanged() {
            if (app.vaultState !== "locked") {
                page.errorText = ""
                phraseField.text = ""
            }
        }
    }
}
