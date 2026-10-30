import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property string label: ""
    property string value: ""
    property string unit: ""
    property color accentColor: Theme.accent
    implicitHeight: 112
    radius: Theme.radiusCard
    color: Theme.surface

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 4

        Text {
            Layout.fillWidth: true
            text: root.label
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontCaption
            font.weight: Font.Medium
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 0.6
            elide: Text.ElideRight
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: root.value
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 32
                font.weight: Font.DemiBold
            }

            Text {
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: 5
                text: root.unit
                color: root.accentColor
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLabel
                font.weight: Font.Medium
            }
        }
    }
}
