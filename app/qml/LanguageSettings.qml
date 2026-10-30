import QtQuick
import QtQuick.Layouts

// Language: each in its own words, the current one ticked. It changes at once.
SubPage {
    title: T.t("settings.language")

    Item { Layout.preferredHeight: 12 }

    Repeater {
        model: Ui.availableLanguages
        delegate: ChoiceRow {
            required property var modelData
            required property int index
            divider: index > 0
            title: modelData.nativeName
            description: modelData.code === Ui.language ? "" : modelData.englishName
            selected: Ui.language === modelData.code
            onClicked: Ui.language = modelData.code
        }
    }

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 20
        text: T.t("language.help")
        size: Theme.caption
        color: Theme.textMuted
        wrapMode: Text.Wrap
        lineHeightMode: Text.ProportionalHeight
        lineHeight: 1.0
    }
}
