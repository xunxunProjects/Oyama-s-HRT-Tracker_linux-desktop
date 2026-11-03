import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The records are locked: the recovery phrase in one line, hidden as it is
// typed, and what went wrong in words under it.
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
            errorText = app.statusKey ? T.t(app.statusKey, app.statusArgs) : app.status
        }
    }

    Component.onCompleted: phraseField.field.forceActiveFocus()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(400, page.width - 64)
        spacing: 0

        Doodle {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 28
            name: "lock"
            size: 56
        }

        UiText {
            Layout.fillWidth: true
            text: T.t("vault.locked_title")
            size: Theme.title
            font.weight: Font.DemiBold
            font.letterSpacing: -0.52
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        UiText {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.bottomMargin: 32
            text: T.t("vault.locked_subtitle")
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            lineHeightMode: Text.ProportionalHeight
            lineHeight: 1.2
        }

        LineField {
            id: phraseField
            objectName: "unlockPhrase"
            Layout.fillWidth: true
            label: T.t("vault.phrase_label")
            error: page.errorText
            enabled: !app.vaultBusy
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
            field.echoMode: TextInput.Password
            field.selectByMouse: false
            onAccepted: page.attemptUnlock()
            onEdited: page.errorText = ""
        }

        AppButton {
            objectName: "unlockButton"
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: 20
            text: app.vaultBusy ? T.t("vault.unlocking") : T.t("vault.unlock")
            enabled: !app.vaultBusy && phraseField.text.trim().length > 0
            onClicked: page.attemptUnlock()
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
