import QtQuick
import QtQuick.Layouts

// The disclaimer, as words on the page.
SubPage {
    title: T.t("disclaimer.title")

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 16
        text: T.t("disclaimer.text.intro")
        wrapMode: Text.Wrap
        lineHeightMode: Text.ProportionalHeight
        lineHeight: 1.2
    }

    Repeater {
        model: ["disclaimer.text.point1", "disclaimer.text.point2", "disclaimer.text.point3"]
        delegate: RowLayout {
            required property string modelData
            Layout.fillWidth: true
            Layout.topMargin: 12
            spacing: 12

            Rectangle {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 9
                implicitWidth: 4
                implicitHeight: 4
                radius: 2
                color: Theme.textMuted
            }
            UiText {
                Layout.fillWidth: true
                text: T.t(modelData)
                color: Theme.textMuted
                wrapMode: Text.Wrap
                lineHeightMode: Text.ProportionalHeight
                lineHeight: 1.2
            }
        }
    }

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 24
        text: T.t("disclaimer.local")
        size: Theme.caption
        color: Theme.textMuted
        wrapMode: Text.Wrap
        lineHeightMode: Text.ProportionalHeight
        lineHeight: 1.0
    }
}
