import QtQuick
import QtQuick.Layouts

// A question put into the page as a line under what was pressed: what it is
// about, then the way out and the way on ("Delete this record? Keep it /
// Delete"). A destructive one says "Keep it" and takes the focus. Escape backs
// out of it.
Item {
    id: root
    property string title: ""
    property string message: ""
    property string confirmText: ""
    property string cancelText: ""
    property bool danger: true
    property bool rules: true
    signal confirmed()
    signal cancelled()

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight + 28

    function focusKeep() {
        keep.forceActiveFocus()
    }

    Keys.onEscapePressed: root.cancelled()

    Rectangle {
        visible: root.rules
        anchors.top: parent.top
        width: parent.width
        height: Theme.hairline
        color: Theme.border
    }

    Rectangle {
        visible: root.rules
        anchors.bottom: parent.bottom
        width: parent.width
        height: Theme.hairline
        color: Theme.border
    }

    RowLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            UiText {
                Layout.fillWidth: true
                text: root.title
                font.weight: Font.Medium
                wrapMode: Text.Wrap
                lineHeightMode: Text.ProportionalHeight
                lineHeight: 1.0
            }

            UiText {
                Layout.fillWidth: true
                visible: root.message !== ""
                text: root.message
                size: Theme.caption
                color: Theme.textMuted
                wrapMode: Text.Wrap
                lineHeightMode: Text.FixedHeight
                lineHeight: 20
            }
        }

        RowLayout {
            spacing: 8
            Layout.alignment: Qt.AlignVCenter

            AppButton {
                id: keep
                small: true
                variant: "secondary"
                text: root.cancelText
                onClicked: root.cancelled()
                Keys.onEscapePressed: root.cancelled()
            }

            AppButton {
                small: true
                variant: root.danger ? "danger" : "primary"
                text: root.confirmText
                onClicked: root.confirmed()
                Keys.onEscapePressed: root.cancelled()
            }
        }
    }
}
