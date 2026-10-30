import QtQuick
import QtQuick.Layouts

// HRT mode: two choices with a tick, and a note that each keeps its records.
SubPage {
    id: root
    title: T.t("settings.hrt_mode")

    Item { Layout.preferredHeight: 12 }

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

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 20
        text: T.t("settings.hrt_mode_note")
        size: Theme.caption
        color: Theme.textMuted
        wrapMode: Text.Wrap
        lineHeightMode: Text.ProportionalHeight
        lineHeight: 1.0
    }
}
