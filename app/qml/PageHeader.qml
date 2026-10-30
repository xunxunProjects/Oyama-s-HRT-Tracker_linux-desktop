import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The title at the top of every page: a large title, an optional count under
// it and actions on the right. A page reached from another one gets `back`,
// and the title becomes the way back, an arrow before it.
Item {
    id: root
    property string title: ""
    property string subtitle: ""
    property bool back: false
    default property alias actions: actionRow.data
    signal backRequested()

    Layout.fillWidth: true
    implicitHeight: layout.implicitHeight + 16 + 12

    RowLayout {
        id: layout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 16
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignBottom
            spacing: 2

            AbstractButton {
                id: backButton
                visible: root.back
                Layout.leftMargin: -8
                Layout.maximumWidth: layout.width
                implicitWidth: backRow.implicitWidth + 16
                implicitHeight: backRow.implicitHeight + 12
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.name: root.title + ", " + T.t("nav.back")
                onClicked: root.backRequested()

                background: Rectangle {
                    radius: 8
                    color: backButton.hovered ? Theme.surfaceHover : "transparent"
                    border.width: backButton.visualFocus ? 2 : 0
                    border.color: Theme.focusRing
                }

                contentItem: Item {
                    implicitWidth: backRow.implicitWidth
                    implicitHeight: backRow.implicitHeight

                    Row {
                        id: backRow
                        anchors.centerIn: parent
                        spacing: 12

                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: "arrow-left"
                            size: 22
                            color: Theme.textMuted
                            transform: Scale { origin.x: 11; xScale: T.rtl ? -1 : 1 }
                        }

                        UiText {
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.title
                            size: Theme.title
                            font.weight: Font.DemiBold
                            font.letterSpacing: -0.52
                        }
                    }
                }
            }

            UiText {
                visible: !root.back
                Layout.fillWidth: true
                text: root.title
                size: Theme.title
                font.weight: Font.DemiBold
                font.letterSpacing: -0.52
                elide: Text.ElideRight
                Accessible.role: Accessible.Heading
            }

            UiText {
                Layout.fillWidth: true
                visible: root.subtitle !== ""
                text: root.subtitle
                size: Theme.caption
                color: Theme.textMuted
                elide: Text.ElideRight
            }
        }

        RowLayout {
            id: actionRow
            Layout.alignment: Qt.AlignBottom
            Layout.rightMargin: -8
            spacing: 4
            visible: children.length > 0
        }
    }
}
