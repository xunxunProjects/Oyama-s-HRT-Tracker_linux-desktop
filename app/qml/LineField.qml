import QtQuick
import QtQuick.Layouts

// A labelled line field: the label above, the line, and a hint or an error in
// words under it.
ColumnLayout {
    id: root
    property alias label: labelText.text
    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias validator: field.validator
    property alias inputMethodHints: field.inputMethodHints
    property alias field: field
    property string hint: ""
    property string error: ""
    signal edited()

    spacing: 0

    UiText {
        id: labelText
        Layout.fillWidth: true
        visible: text !== ""
        size: Theme.caption
        color: Theme.textMuted
        font.weight: Font.Medium
    }

    AppTextField {
        id: field
        Layout.fillWidth: true
        Layout.topMargin: labelText.visible ? 4 : 0
        onTextEdited: root.edited()
    }

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 6
        visible: text !== ""
        text: root.error !== "" ? root.error : root.hint
        size: Theme.caption
        color: root.error !== "" ? Theme.danger : Theme.textMuted
        wrapMode: Text.Wrap
        lineHeightMode: Text.ProportionalHeight
        lineHeight: 1.0
    }
}
