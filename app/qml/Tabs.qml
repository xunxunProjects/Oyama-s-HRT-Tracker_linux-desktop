import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Tabs across the top of a page: plain words, the current one in the text
// colour over a terracotta rule that slides between them, a hairline under
// the row.
Item {
    id: root
    property var options: []          // [{ value, label }]
    property var currentValue
    signal picked(var value)

    Layout.fillWidth: true
    implicitHeight: 44

    readonly property Item currentItem: {
        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i)
            if (item && item.value === currentValue)
                return item
        }
        return null
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Theme.hairline
        color: Theme.border
    }

    Row {
        id: row
        height: parent.height
        spacing: 26

        Repeater {
            id: repeater
            model: root.options

            delegate: AbstractButton {
                id: tab
                required property var modelData
                readonly property var value: modelData.value
                readonly property bool selected: root.currentValue === modelData.value
                height: row.height
                implicitWidth: label.implicitWidth
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.label
                onClicked: root.picked(modelData.value)

                background: Rectangle {
                    color: "transparent"
                    border.width: tab.visualFocus ? 2 : 0
                    border.color: Theme.focusRing
                    radius: 4
                }

                contentItem: UiText {
                    id: label
                    text: tab.modelData.label
                    color: tab.selected || tab.hovered ? Theme.text : Theme.textMuted
                    font.weight: tab.selected ? Font.Medium : Font.Normal
                }
            }
        }
    }

    Rectangle {
        visible: root.currentItem !== null
        x: root.currentItem ? root.currentItem.x : 0
        width: root.currentItem ? root.currentItem.width : 0
        anchors.bottom: parent.bottom
        height: 2
        color: Theme.accent
        Behavior on x {
            NumberAnimation { duration: Theme.durationBase; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
        }
        Behavior on width {
            NumberAnimation { duration: Theme.durationBase; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
        }
    }
}
