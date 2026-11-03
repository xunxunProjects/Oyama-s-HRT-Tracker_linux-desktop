import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The panic screen: flat #808080 over the whole window, no name, no
// navigation, one hidden line for the recovery phrase and a way to unlock. A
// wrong phrase says only that it didn't work, and the grey stays. Its ink is
// fixed rather than the theme's, so it reads the same on the grey either way.
Item {
    id: root
    objectName: "panicLockPage"

    readonly property color ink: "#181714"
    readonly property color inkMuted: "#33312d"
    property bool failed: false

    function attemptUnlock() {
        if (app.vaultBusy || phraseField.text.trim().length === 0)
            return
        const secret = phraseField.text
        phraseField.text = ""
        failed = !app.unlockVault(secret)
    }

    onVisibleChanged: {
        phraseField.text = ""
        failed = false
        if (visible)
            phraseField.forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent
        color: "#808080"
    }

    // Nothing under the grey can be reached.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onWheel: function(wheel) { wheel.accepted = true }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(360, root.width - 64)
        spacing: 0

        AppTextField {
            id: phraseField
            objectName: "panicPhraseField"
            Layout.fillWidth: true
            echoMode: TextInput.Password
            selectByMouse: false
            inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
            placeholderText: T.t("vault.phrase_label")
            placeholderTextColor: root.inkMuted
            color: root.ink
            enabled: !app.vaultBusy
            onTextEdited: root.failed = false
            onAccepted: root.attemptUnlock()

            background: Item {
                implicitWidth: 120
                implicitHeight: Theme.fieldHeight

                InkRule {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 6
                    ruleColor: root.inkMuted
                    inkColor: root.ink
                    inked: phraseField.activeFocus
                }
            }
        }

        UiText {
            objectName: "panicError"
            Layout.fillWidth: true
            Layout.topMargin: 6
            visible: root.failed
            text: T.t("vault.panic_error")
            size: Theme.caption
            color: root.ink
        }

        AppButton {
            objectName: "panicUnlockButton"
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: 20
            variant: "secondary"
            text: T.t("vault.unlock")
            enabled: !app.vaultBusy && phraseField.text.trim().length > 0
            onClicked: root.attemptUnlock()
        }
    }
}
