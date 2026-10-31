import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The intro, a page before the app rather than a layer over it: a language
// (which changes the words at once), what is being tracked, and where the
// records live with the one reminder that matters. Dots and the way on sit in
// a bar at the foot; Skip is always there.
Rectangle {
    id: root
    color: Theme.bg
    property int step: 0
    readonly property int steps: 3
    signal finished()

    // Until one is picked, the system's language if the app has it.
    readonly property string guess: {
        const code = Qt.locale().name.split("_")[0]
        return ["en", "zh", "ru", "fa"].indexOf(code) >= 0 ? code : "en"
    }
    readonly property string chosen: Ui.language !== "" ? Ui.language : guess

    onVisibleChanged: if (visible) step = 0

    function next() {
        if (step === 0)
            Ui.language = chosen
        if (step < steps - 1) {
            step += 1
            shift = T.rtl ? -24 : 24
            settle.restart()
        } else {
            finished()
        }
    }

    function skip() {
        Ui.language = chosen
        finished()
    }

    property real shift: 0
    NumberAnimation {
        id: settle
        target: root
        property: "shift"
        to: 0
        duration: Theme.durationPage
        easing.type: Easing.BezierSpline
        easing.bezierCurve: Theme.easePage
    }

    AppButton {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        variant: "ghost"
        text: T.t("onboarding.skip")
        onClicked: root.skip()
    }

    Flickable {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footer.top
        contentWidth: width
        contentHeight: body.implicitHeight + 96
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: AppScrollBar {}

        ColumnLayout {
            id: body
            width: Math.min(400, parent.width - 64)
            x: (parent.width - width) / 2
            y: 56
            spacing: 0
            transform: Translate { x: root.shift }

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 28
                Layout.preferredWidth: 72
                Layout.preferredHeight: 72
                source: "../assets/app-icon.png"
                smooth: true
                mipmap: true
            }

            UiText {
                Layout.fillWidth: true
                text: root.step === 0 ? T.t("onboarding.welcome_title")
                    : root.step === 1 ? T.t("onboarding.mode_title")
                    : T.t("onboarding.privacy_title")
                size: Theme.title
                font.weight: Font.DemiBold
                font.letterSpacing: -0.52
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            UiText {
                Layout.fillWidth: true
                Layout.topMargin: 12
                Layout.bottomMargin: 40
                text: root.step === 0 ? T.t("onboarding.welcome_subtitle")
                    : root.step === 1 ? T.t("onboarding.mode_subtitle")
                    : T.t("onboarding.privacy_subtitle")
                color: Theme.textMuted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                lineHeightMode: Text.ProportionalHeight
                lineHeight: 1.2
            }

            // A language, in its own words.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 40
                Layout.rightMargin: 40
                visible: root.step === 0
                spacing: 0

                Repeater {
                    model: Ui.availableLanguages
                    delegate: ChoiceRow {
                        required property var modelData
                        required property int index
                        divider: index > 0
                        title: modelData.nativeName
                        selected: root.chosen === modelData.code
                        onClicked: Ui.language = modelData.code
                    }
                }
            }

            // What is being tracked.
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.step === 1
                spacing: 0

                ChoiceRow {
                    title: T.t("mode.transfem")
                    description: T.t("onboarding.mode_transfem_desc")
                    selected: app.mode === "transfem"
                    onClicked: app.mode = "transfem"
                }
                ChoiceRow {
                    divider: true
                    title: T.t("mode.transmasc")
                    description: T.t("onboarding.mode_transmasc_desc")
                    selected: app.mode === "transmasc"
                    onClicked: app.mode = "transmasc"
                }
            }

            // Where the records live, and the one reminder that matters.
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.step === 2
                spacing: 0

                Repeater {
                    model: [
                        { doodle: "lock", title: "onboarding.privacy_local", desc: "onboarding.privacy_local_desc" },
                        { doodle: "caution", title: "onboarding.privacy_medical", desc: "onboarding.privacy_medical_desc" }
                    ]
                    delegate: ColumnLayout {
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: 0

                        Hairline { visible: index > 0 }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 16
                            Layout.bottomMargin: 16
                            spacing: 14

                            Doodle {
                                Layout.alignment: Qt.AlignTop
                                name: modelData.doodle
                                size: 36
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                UiText {
                                    Layout.fillWidth: true
                                    text: T.t(modelData.title)
                                    font.weight: Font.Medium
                                    wrapMode: Text.Wrap
                                }
                                UiText {
                                    Layout.fillWidth: true
                                    text: T.t(modelData.desc)
                                    size: Theme.caption
                                    color: Theme.textMuted
                                    wrapMode: Text.Wrap
                                    lineHeightMode: Text.ProportionalHeight
                                    lineHeight: 1.0
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Item {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 68

        Rectangle {
            width: parent.width
            height: Theme.hairline
            color: Theme.border
        }

        Item {
            width: Math.min(400, parent.width - 64)
            height: parent.height
            anchors.horizontalCenter: parent.horizontalCenter

            Row {
                anchors.centerIn: parent
                spacing: 7
                LayoutMirroring.enabled: false

                Repeater {
                    model: root.steps
                    delegate: Rectangle {
                        required property int index
                        width: 8
                        height: 8
                        radius: 4
                        color: index === root.step ? Theme.accent : "transparent"
                        border.width: index === root.step ? 0 : 1.25
                        border.color: Theme.textMuted
                    }
                }
            }

            AppButton {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.step === root.steps - 1 ? T.t("onboarding.start") : T.t("onboarding.next")
                onClicked: root.next()
            }
        }
    }
}
