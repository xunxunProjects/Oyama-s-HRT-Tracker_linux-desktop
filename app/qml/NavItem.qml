import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root
    property string iconName: ""
    property bool selected: false
    implicitHeight: 48
    padding: 0
    leftPadding: 12
    rightPadding: 12
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text

    background: Rectangle {
        radius: Theme.radiusControl
        color: root.selected
            ? Theme.surfacePressed
            : root.hovered ? Theme.surfaceHover : "transparent"
    }

    contentItem: RowLayout {
        spacing: 10

        Icon {
            name: root.iconName
            size: 20
            color: root.selected ? Theme.accent : Theme.textMuted
        }

        Text {
            Layout.fillWidth: true
            text: root.text
            color: root.selected || root.hovered ? Theme.text : Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: root.selected ? Font.Medium : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    scale: down ? 0.98 : 1
    Behavior on scale {
        NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
    }
}
