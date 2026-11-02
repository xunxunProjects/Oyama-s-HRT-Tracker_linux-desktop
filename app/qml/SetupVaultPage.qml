import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// First run: the recovery phrase to write down, numbered in four columns of
// six so it copies onto paper in order; three of its words typed back as a
// check; the two things to be sure of, ticked; then the vault is created.
Item {
    id: page

    readonly property var phraseWords: app.setupPhrase.length > 0
                                      ? app.setupPhrase.split(" ")
                                      : []
    // The words to type back, in the order they come in the phrase; the
    // controller wants the answers in its own order.
    readonly property var checkOrder: {
        const order = []
        for (let i = 0; i < app.setupChallenge.length; ++i)
            order.push(i)
        return order.sort(function(a, b) { return app.setupChallenge[a] - app.setupChallenge[b] })
    }
    property bool wroteDown: false
    property bool understood: false
    property string errorText: ""

    readonly property bool answered: {
        for (let i = 0; i < challengeRepeater.count; ++i) {
            const field = challengeRepeater.itemAt(i)
            if (!field || field.text.trim().length === 0)
                return false
        }
        return challengeRepeater.count > 0
    }

    function clearInputs() {
        wroteDown = false
        understood = false
        errorText = ""
        for (let i = 0; i < challengeRepeater.count; ++i) {
            const field = challengeRepeater.itemAt(i)
            if (field)
                field.text = ""
        }
    }

    function collectAnswers() {
        const answers = []
        for (let i = 0; i < challengeRepeater.count; ++i) {
            const field = challengeRepeater.itemAt(i)
            answers[checkOrder[i]] = field ? field.text : ""
        }
        return answers
    }

    function create() {
        if (app.vaultBusy || !wroteDown || !understood || !answered)
            return
        errorText = ""
        if (!app.confirmVaultSetup(collectAnswers(), understood))
            errorText = app.statusKey ? T.t(app.statusKey, app.statusArgs) : app.status
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

    Flickable {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footer.top
        contentWidth: width
        contentHeight: body.implicitHeight + 112
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar {}

        ColumnLayout {
            id: body
            width: Math.min(560, parent.width - 64)
            x: (parent.width - width) / 2
            y: 56
            spacing: 0

            Doodle {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 28
                name: "lock"
                size: 56
            }

            UiText {
                Layout.fillWidth: true
                text: page.phraseWords.length > 0 ? T.t("vault.setup_title")
                                                  : T.t("vault.no_phrase")
                size: Theme.title
                font.weight: Font.DemiBold
                font.letterSpacing: -0.52
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            UiText {
                Layout.fillWidth: true
                Layout.topMargin: 12
                visible: page.phraseWords.length > 0
                text: T.t("vault.setup_subtitle", [page.phraseWords.length])
                color: Theme.textMuted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                lineHeightMode: Text.ProportionalHeight
                lineHeight: 1.2
            }

            AppButton {
                objectName: "beginSetupButton"
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 32
                visible: page.phraseWords.length === 0
                text: T.t("vault.get_phrase")
                enabled: !app.vaultBusy
                onClicked: app.beginVaultSetup()
            }

            // The phrase, numbered down each column. Words stay in the
            // order they are written, whatever the page's direction.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: 32
                visible: page.phraseWords.length > 0
                spacing: 0

                GroupHeading {
                    text: T.t("vault.phrase_label")
                    Layout.topMargin: 0
                }

                Hairline {}

                GridLayout {
                    objectName: "setupPhraseGrid"
                    Layout.fillWidth: true
                    Layout.topMargin: 14
                    Layout.bottomMargin: 14
                    flow: GridLayout.TopToBottom
                    rows: Math.ceil(page.phraseWords.length / 4)
                    columnSpacing: 16
                    rowSpacing: 6
                    LayoutMirroring.enabled: false
                    LayoutMirroring.childrenInherit: true

                    Repeater {
                        model: page.phraseWords

                        delegate: RowLayout {
                            required property int index
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            spacing: 8

                            UiText {
                                Layout.preferredWidth: 20
                                text: index + 1
                                size: Theme.caption
                                color: Theme.textMuted
                                horizontalAlignment: Text.AlignRight
                            }

                            UiText {
                                Layout.fillWidth: true
                                text: modelData
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Hairline {}
            }

            // Three of the words typed back from the paper.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: 32
                visible: page.phraseWords.length > 0
                spacing: 0

                GroupHeading {
                    text: T.t("vault.check_heading")
                    Layout.topMargin: 0
                }

                UiText {
                    Layout.fillWidth: true
                    text: T.t("vault.check_hint")
                    size: Theme.caption
                    color: Theme.textMuted
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    spacing: 24

                    Repeater {
                        id: challengeRepeater
                        model: page.checkOrder

                        delegate: LineField {
                            required property int index
                            required property var modelData
                            objectName: "setupChallenge" + index
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            label: T.t("vault.word_n", [app.setupChallenge[modelData] + 1])
                            enabled: !app.vaultBusy
                            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                            onAccepted: page.create()
                            onEdited: page.errorText = ""
                        }
                    }
                }

                UiText {
                    objectName: "setupStatus"
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    visible: page.errorText !== ""
                    text: page.errorText
                    size: Theme.caption
                    color: Theme.danger
                    wrapMode: Text.Wrap
                }
            }

            // The two things to be sure of, ticked like any choice.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: 24
                visible: page.phraseWords.length > 0
                spacing: 0

                ChoiceRow {
                    objectName: "setupWroteDown"
                    title: T.t("vault.wrote_down", [page.phraseWords.length])
                    selected: page.wroteDown
                    selectedWeight: Font.Medium
                    enabled: !app.vaultBusy
                    Accessible.role: Accessible.CheckBox
                    onClicked: page.wroteDown = !page.wroteDown
                }

                ChoiceRow {
                    objectName: "setupUnderstood"
                    divider: true
                    title: T.t("vault.no_recovery")
                    selected: page.understood
                    selectedWeight: Font.Medium
                    enabled: !app.vaultBusy
                    Accessible.role: Accessible.CheckBox
                    onClicked: page.understood = !page.understood
                }
            }
        }
    }

    // The way on, at the foot like the intro's: a new phrase is a way out,
    // creating the vault the one thing the page is for.
    Item {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 68
        visible: page.phraseWords.length > 0

        Rectangle {
            width: parent.width
            height: Theme.hairline
            color: Theme.border
        }

        Item {
            width: Math.min(560, parent.width - 64)
            height: parent.height
            anchors.horizontalCenter: parent.horizontalCenter

            AppButton {
                objectName: "cancelSetupButton"
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                variant: "ghost"
                text: T.t("btn.cancel")
                enabled: !app.vaultBusy
                onClicked: {
                    page.clearInputs()
                    app.cancelVaultSetup()
                }
            }

            AppButton {
                objectName: "confirmSetupButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: app.vaultBusy ? T.t("vault.creating") : T.t("vault.create")
                enabled: !app.vaultBusy && page.wroteDown && page.understood && page.answered
                onClicked: page.create()
            }
        }
    }
}
