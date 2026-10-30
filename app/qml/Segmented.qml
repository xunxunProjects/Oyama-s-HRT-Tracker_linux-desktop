import QtQuick
import QtQuick.Controls

// A few choices side by side in a soft track, the chosen one on a raised
// pill that glides to whichever is picked (7d / 30d / All).
Rectangle {
    id: root
    property var options: []          // [{ value, label }]
    property var currentValue
    signal picked(var value)

    implicitWidth: row.implicitWidth + 6
    implicitHeight: 32
    radius: height / 2
    color: Theme.surfaceMuted

    readonly property Item currentItem: {
        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i)
            if (item && item.value === currentValue)
                return item
        }
        return null
    }

    Rectangle {
        visible: root.currentItem !== null
        x: root.currentItem ? row.x + root.currentItem.x : 0
        y: 3
        width: root.currentItem ? root.currentItem.width : 0
        height: root.height - 6
        radius: height / 2
        color: Theme.controlThumb
        border.width: Theme.dark ? 0 : Theme.hairline
        border.color: Theme.border
        Behavior on x {
            NumberAnimation { duration: Theme.durationBase; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
        }
        Behavior on width {
            NumberAnimation { duration: Theme.durationBase; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
        }
    }

    Row {
        id: row
        x: 3
        y: 3
        spacing: 2

        Repeater {
            id: repeater
            model: root.options

            delegate: AbstractButton {
                id: option
                required property var modelData
                readonly property var value: modelData.value
                readonly property bool selected: root.currentValue === modelData.value
                implicitWidth: label.implicitWidth + 24
                implicitHeight: 26
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.name: modelData.label
                Accessible.role: Accessible.RadioButton
                Accessible.checked: selected
                onClicked: root.picked(modelData.value)

                background: Rectangle {
                    radius: height / 2
                    color: "transparent"
                    border.width: option.visualFocus ? 2 : 0
                    border.color: Theme.focusRing
                }

                contentItem: UiText {
                    id: label
                    text: option.modelData.label
                    size: Theme.caption
                    horizontalAlignment: Text.AlignHCenter
                    color: option.selected || option.hovered ? Theme.text : Theme.textMuted
                    font.weight: option.selected ? Font.DemiBold : Font.Medium
                }
            }
        }
    }
}
