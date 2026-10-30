import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One option in a list with a tick: the chosen one is set in medium weight
// with a terracotta tick at its end. A row in a hairline list, so it draws
// the rule above itself when `divider` is set.
AbstractButton {
    id: root
    property string title: ""
    property string description: ""
    property bool selected: false
    property bool divider: false
    // Rows inside a field's unfolded options sit flush; page lists bleed.
    property bool bleed: true
    property int selectedWeight: Font.DemiBold

    Layout.fillWidth: true
    implicitHeight: Math.max(44, content.implicitHeight + (description !== "" ? 28 : 20))
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    checkable: false
    Accessible.role: Accessible.RadioButton
    Accessible.name: title
    Accessible.checked: selected

    background: Item {
        Rectangle {
            x: root.bleed ? -12 : 0
            width: parent.width + (root.bleed ? 24 : 0)
            height: parent.height
            radius: Theme.radiusMd
            color: root.hovered || root.down ? Theme.surfaceHover : "transparent"
            border.width: root.visualFocus ? 2 : 0
            border.color: Theme.focusRing
        }

        Rectangle {
            visible: root.divider
            anchors.top: parent.top
            width: parent.width
            height: Theme.hairline
            color: Theme.border
        }
    }

    contentItem: RowLayout {
        id: content
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            UiText {
                Layout.fillWidth: true
                text: root.title
                font.weight: root.selected ? root.selectedWeight : Font.Normal
                elide: Text.ElideRight
            }

            UiText {
                Layout.fillWidth: true
                visible: root.description !== ""
                text: root.description
                size: Theme.caption
                color: Theme.textMuted
                wrapMode: Text.Wrap
                lineHeightMode: Text.ProportionalHeight
                lineHeight: 1.0
            }
        }

        Tick {
            on: root.selected
        }
    }
}
