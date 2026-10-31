import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A row in a hairline list (SettingsListItem upstream): a title, a short
// description, what's set now and a chevron. A clickable row bleeds 12px into
// the gutter on each side so its hover fill has room around the content.
// `divider` draws the hairline above it; the first row in a run has none.
AbstractButton {
    id: root
    property string title: ""
    property string description: ""
    property string value: ""
    property string iconName: ""
    property bool danger: false
    property bool divider: false
    property bool showChevron: clickable
    property bool clickable: true
    default property alias trailing: trailingSlot.data

    Layout.fillWidth: true
    implicitHeight: Math.max(Theme.rowHeight, content.implicitHeight + 28)
    hoverEnabled: clickable
    focusPolicy: clickable ? Qt.StrongFocus : Qt.NoFocus
    Accessible.name: title
    Accessible.description: description
    opacity: enabled ? 1 : 0.45

    background: Item {
        Rectangle {
            x: root.clickable ? -12 : 0
            width: parent.width + (root.clickable ? 24 : 0)
            height: parent.height
            radius: Theme.radiusMd
            color: root.clickable && (root.hovered || root.down) ? Theme.surfaceHover : "transparent"
            border.width: root.visualFocus ? 2 : 0
            border.color: Theme.focusRing
        }

        Rectangle {
            visible: root.divider
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: root.iconName !== "" ? 32 : 0
            height: Theme.hairline
            color: Theme.border
        }
    }

    contentItem: RowLayout {
        id: content
        spacing: 12

        Icon {
            visible: root.iconName !== ""
            name: root.iconName
            size: 20
            color: root.danger ? Theme.danger : Theme.textMuted
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            UiText {
                Layout.fillWidth: true
                text: root.title
                color: root.danger ? Theme.danger : Theme.text
                font.weight: Font.Medium
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

        UiText {
            visible: root.value !== ""
            text: root.value
            color: Theme.textMuted
            Layout.maximumWidth: root.width * 0.5
            elide: Text.ElideRight
        }

        Item {
            id: trailingSlot
            visible: children.length > 0
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
        }

        Icon {
            visible: root.showChevron
            name: "chevron-right"
            size: 18
            color: Theme.textMuted
            opacity: 0.7
            // A chevron points the way the page reads.
            transform: Scale { origin.x: 9; xScale: T.rtl ? -1 : 1 }
        }
    }
}
