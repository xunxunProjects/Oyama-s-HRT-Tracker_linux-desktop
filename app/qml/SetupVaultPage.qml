import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    readonly property var phraseWords: app.setupPhrase.length > 0
                                      ? app.setupPhrase.split(" ")
                                      : []

    function clearInputs() {
        wroteDown.checked = false
        acknowledged.checked = false
        for (let i = 0; i < challengeRepeater.count; ++i) {
            const field = challengeRepeater.itemAt(i)
            if (field) {
                field.answer = ""
            }
        }
    }

    function collectAnswers() {
        const answers = []
        for (let i = 0; i < challengeRepeater.count; ++i) {
            const field = challengeRepeater.itemAt(i)
            answers.push(field ? field.answer : "")
        }
        return answers
    }

    Component.onCompleted: {
        if (app.vaultState === "uninitialized" && app.setupPhrase.length === 0) {
            app.beginVaultSetup()
        }
    }

    Connections {
        target: app
        function onChanged() {
            if (app.vaultState !== "uninitialized" ||
                    app.setupPhrase.length === 0) {
                page.clearInputs()
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: page.width
            spacing: 14

            Item { Layout.preferredHeight: 24 }

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                Layout.maximumWidth: 760
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Create your vault")
                color: "#f4f6f8"
                font.pixelSize: 26
                font.weight: Font.DemiBold
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                Layout.maximumWidth: 700
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Your data is encrypted with a seven-word recovery " +
                           "phrase. Write it down on paper. There is no " +
                           "recovery if you lose it.")
                color: "#8a98a8"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(
                    Math.max(page.width - 96, 240), 860)
                Layout.preferredHeight: phraseColumn.implicitHeight + 40
                visible: page.phraseWords.length > 0
                radius: 14
                color: "#151c23"
                border.color: "#26313c"

                ColumnLayout {
                    id: phraseColumn
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 8

                    Label {
                        text: qsTr("Recovery phrase")
                        color: "#8a98a8"
                        font.pixelSize: 11
                        font.letterSpacing: 1.0
                    }

                    Repeater {
                        model: page.phraseWords

                        delegate: RowLayout {
                            required property int index
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 12

                            Label {
                                text: (index + 1) + "."
                                Layout.preferredWidth: 30
                                horizontalAlignment: Text.AlignRight
                                color: "#5f6d7c"
                                font.pixelSize: 16
                                font.family: "monospace"
                            }

                            Text {
                                text: modelData
                                Layout.fillWidth: true
                                color: "#f4f6f8"
                                font.pixelSize: 18
                                font.family: "monospace"
                                font.letterSpacing: 1.0
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                visible: page.phraseWords.length === 0
                spacing: 12

                Label {
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("No recovery phrase is ready. Generate one to " +
                               "continue.")
                    color: "#8a98a8"
                    font.pixelSize: 13
                }

                Button {
                    objectName: "beginSetupButton"
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Generate recovery phrase")
                    highlighted: true
                    enabled: !app.vaultBusy
                    onClicked: app.beginVaultSetup()
                }
            }

            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(
                    Math.max(page.width - 96, 240), 860)
                visible: page.phraseWords.length > 0
                spacing: 12

                Label {
                    text: qsTr("Enter the three requested words from the " +
                               "phrase above.")
                    color: "#c9d2dc"
                    font.pixelSize: 13
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Repeater {
                        id: challengeRepeater
                        model: app.setupChallenge

                        delegate: ColumnLayout {
                            id: challengeItem
                            required property int index
                            required property var modelData
                            property alias answer: challengeField.text
                            Layout.fillWidth: true
                            spacing: 4

                            Label {
                                text: qsTr("Word %1").arg(modelData + 1)
                                color: "#8a98a8"
                                font.pixelSize: 12
                            }

                            TextField {
                                id: challengeField
                                objectName: "setupChallenge" + index
                                Layout.fillWidth: true
                                placeholderText: qsTr("Word %1").arg(
                                                     modelData + 1)
                                enabled: !app.vaultBusy
                            }
                        }
                    }
                }

                CheckBox {
                    id: wroteDown
                    text: qsTr("I wrote down the recovery phrase")
                    enabled: !app.vaultBusy
                }

                CheckBox {
                    id: acknowledged
                    text: qsTr("I understand there is no recovery")
                    enabled: !app.vaultBusy
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 12

                    Button {
                        objectName: "confirmSetupButton"
                        text: qsTr("Create vault")
                        highlighted: true
                        enabled: !app.vaultBusy && wroteDown.checked &&
                                 acknowledged.checked
                        onClicked: {
                            app.confirmVaultSetup(page.collectAnswers(),
                                                  acknowledged.checked)
                        }
                    }

                    Button {
                        objectName: "cancelSetupButton"
                        text: qsTr("Cancel")
                        enabled: !app.vaultBusy
                        onClicked: {
                            page.clearInputs()
                            app.cancelVaultSetup()
                        }
                    }

                    BusyIndicator {
                        objectName: "setupBusy"
                        visible: app.vaultBusy
                        running: app.vaultBusy
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                    }
                }

                Label {
                    objectName: "setupStatus"
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: app.status
                    color: "#8a98a8"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            Item { Layout.preferredHeight: 24 }
        }
    }
}
