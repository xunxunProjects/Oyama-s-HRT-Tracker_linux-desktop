import QtQuick
import QtQuick.Controls

// The web app's buttons: capsules, fully round at the ends. primary is the
// one action a view exists for; tonal a strong second; secondary the neutral
// way out; danger only goes through with a delete; ghost (and ghostAccent,
// for the action that does a page's job) is the 32px pill for headers and
// toolbars. `small` is the 30px size for buttons inside a line of text.
Button {
    id: root
    property string variant: "primary"
    property bool small: false
    property string iconName: ""
    property real iconSize: variant === "ghost" || variant === "ghostAccent" ? 15 : 16
    property real iconStroke: variant === "ghost" || variant === "ghostAccent" ? 1.5 : 1.75
    // Hide the label and keep only the icon (a toolbar's trash or bookmark).
    property bool iconOnly: false

    readonly property bool isGhost: variant === "ghost" || variant === "ghostAccent"

    readonly property color fill: {
        const over = hovered && enabled
        switch (variant) {
        case "primary": return over ? Theme.accentHover : Theme.accent
        case "tonal": return over ? Theme.accentSubtleHover : Theme.accentSubtle
        case "secondary": return over ? Theme.surfacePressed : Theme.surfaceMuted
        case "danger": return over ? Theme.dangerFillHover : Theme.dangerFill
        case "ghostAccent": return over ? Theme.accentSubtle : "transparent"
        }
        return over ? Theme.surfaceHover : "transparent"
    }

    readonly property color ink: {
        switch (variant) {
        case "primary": return Theme.textOnAccent
        case "tonal": return Theme.textOnAccentSubtle
        case "secondary": return Theme.text
        case "danger": return Theme.textOnDanger
        case "ghostAccent": return Theme.accentInk
        case "ghostDanger": return Theme.danger
        }
        return hovered && enabled ? Theme.text : Theme.textMuted
    }

    implicitHeight: small ? 30 : (isGhost || variant === "ghostDanger" ? 32 : Theme.controlHeight)
    implicitWidth: iconOnly ? implicitHeight
                            : contentItem.implicitWidth + leftPadding + rightPadding
    leftPadding: iconOnly ? 0 : (small ? 14 : (isGhost || variant === "ghostDanger" ? 10 : 18))
    rightPadding: leftPadding
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: enabled ? 1 : 0.45
    Accessible.name: text

    background: Rectangle {
        radius: height / 2
        color: root.fill
        Behavior on color {
            ColorAnimation { duration: Theme.durationQuick; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeStandard }
        }

        // Keyboard focus is one ring of ink, 2px off the edge.
        Rectangle {
            visible: root.visualFocus
            anchors.fill: parent
            anchors.margins: -4
            radius: height / 2
            color: "transparent"
            border.width: 2
            border.color: Theme.focusRing
        }
    }

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 4

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.iconName !== ""
                name: root.iconName
                size: root.iconSize
                strokeWidth: root.iconStroke
                color: root.ink
            }

            UiText {
                anchors.verticalCenter: parent.verticalCenter
                visible: !root.iconOnly && root.text !== ""
                text: root.text
                color: root.ink
                font.weight: Font.Medium
                font.letterSpacing: -0.075
            }
        }
    }

    scale: down ? 0.97 : 1
    Behavior on scale {
        NumberAnimation { duration: Theme.durationInstant; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeStandard }
    }
}
