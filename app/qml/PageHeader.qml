import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string title: ""
    property string subtitle: ""
    default property alias extra: extraRow.data
    Layout.fillWidth: true
    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: root.title
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontTitle
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                visible: root.subtitle !== ""
                text: root.subtitle
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLabel
                wrapMode: Text.Wrap
            }
        }

        RowLayout {
            id: extraRow
            spacing: 8
            Layout.alignment: Qt.AlignVCenter
        }
    }
}
