import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root
    property string variant: "primary"
    property string iconName: ""
    property bool busy: false

    readonly property color backgroundFill: {
        if (variant === "primary")
            return hovered ? Theme.accentHover : Theme.accent
        if (variant === "secondary")
            return hovered ? Theme.surfaceHover : Theme.surfaceAlt
        if (variant === "danger")
            return hovered ? Theme.surfaceHover : "transparent"
        return hovered ? Theme.surfaceHover : "transparent"
    }

    readonly property color foreground: {
        if (variant === "primary")
            return Theme.onAccent
        if (variant === "danger")
            return Theme.brick
        if (variant === "ghost")
            return Theme.textSoft
        return Theme.text
    }

    implicitHeight: 44
    leftPadding: 16
    rightPadding: 16
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: busy ? 0.7 : 1
    Accessible.name: text

    background: Rectangle {
        radius: Theme.radiusControl
        color: root.backgroundFill
        border.width: root.visualFocus ? 2 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: Theme.durationHover } }
    }

    contentItem: Row {
        anchors.centerIn: parent
        spacing: 8

        BusyIndicator {
            anchors.verticalCenter: parent.verticalCenter
            running: root.busy
            visible: root.busy
            implicitWidth: 18
            implicitHeight: 18
        }

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.iconName !== "" && !root.busy
            name: root.iconName
            size: 18
            color: root.foreground
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            color: root.foreground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.weight: Font.Medium
            verticalAlignment: Text.AlignVCenter
        }
    }

    scale: down ? 0.97 : 1
    Behavior on scale {
        NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
    }
}
