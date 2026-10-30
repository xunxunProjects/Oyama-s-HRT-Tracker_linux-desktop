import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A page in the sidebar: its drawing and its name. The current page sits on a
// filled pill; the rest lift to full text on hover. In the narrow strip each
// pill is a square with its drawing in the middle.
Button {
    id: root
    property string doodle: ""
    property bool selected: false
    property bool showLabel: true
    implicitHeight: 56
    padding: 0
    leftPadding: 10
    rightPadding: 10
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    clip: true

    Behavior on Layout.preferredWidth {
        NumberAnimation { duration: Theme.durationSlow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: root.selected ? Theme.surfacePressed
             : root.hovered ? Theme.surfaceHover : "transparent"
        border.width: root.visualFocus ? 2 : 0
        border.color: Theme.focusRing
    }

    contentItem: Row {
        spacing: 14

        Doodle {
            anchors.verticalCenter: parent.verticalCenter
            name: root.doodle
            size: 36
        }

        UiText {
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.sidebarWidth - 24 - 20 - 36 - 14
            text: root.text
            color: root.selected || root.hovered ? Theme.text : Theme.textMuted
            font.weight: root.selected ? Font.Medium : Font.Normal
            elide: Text.ElideRight
            opacity: root.showLabel ? 1 : 0
            Behavior on opacity {
                NumberAnimation { duration: Theme.durationBase; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
            }
        }
    }
}
