import QtQuick
import QtQuick.Layouts

// Appearance: light, dark, or the system's, ticked; it changes at once.
SubPage {
    title: T.t("settings.theme")

    Item { Layout.preferredHeight: 12 }

    Repeater {
        model: ["light", "dark", "system"]
        delegate: ChoiceRow {
            required property string modelData
            required property int index
            divider: index > 0
            title: T.t("theme." + modelData)
            selected: Ui.theme === modelData
            onClicked: Ui.theme = modelData
        }
    }
}
