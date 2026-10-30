import QtQuick
import QtQuick.Controls

ComboBox {
    id: root
    implicitHeight: 44
    leftPadding: 14
    rightPadding: 40
    hoverEnabled: true
    font.pixelSize: Theme.fontBody
    font.family: Theme.fontFamily

    function indexOfValue(value) {
        for (var i = 0; i < count; ++i) {
            var v = valueRole ? model[i][valueRole] : model[i]
            if (v === value) return i
        }
        return 0
    }

    contentItem: Text {
        text: root.displayText
        color: root.enabled ? Theme.text : Theme.textDisabled
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Icon {
        name: "chevron-down"
        size: 16
        color: Theme.textMuted
        x: root.width - width - 14
        y: (root.height - height) / 2
        rotation: root.popup.visible ? 180 : 0
        Behavior on rotation {
            NumberAnimation { duration: Theme.durationHover; easing.type: Easing.OutCubic }
        }
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.surfaceAlt
        border.width: root.activeFocus ? 1.5 : 1
        border.color: root.activeFocus ? Theme.accent : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.durationTheme } }
    }

    popup: Popup {
        y: root.height + 4
        width: root.width
        padding: 4
        background: Rectangle {
            color: Theme.surfaceAlt
            radius: Theme.radiusControl
            border.width: 1
            border.color: Theme.border
        }
        contentItem: ListView {
            clip: true
            implicitHeight: Math.min(contentHeight, 240)
            model: root.delegateModel
            currentIndex: root.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            delegate: ItemDelegate {
                id: entry
                width: ListView.view.width
                height: 36
                text: root.textRole ? model[root.textRole] : modelData
                hoverEnabled: true
                background: Rectangle {
                    radius: 8
                    color: entry.hovered || entry.highlighted
                        ? Theme.surfaceHover : "transparent"
                }
                contentItem: Text {
                    text: entry.text
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: root.currentIndex === index ? Font.Medium : Font.Normal
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
