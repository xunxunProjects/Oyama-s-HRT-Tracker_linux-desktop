import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string title: ""
    property string body: ""
    property string confirmText: T.t("common.delete")
    signal confirmed()
    signal cancelled()

    visible: false
    z: 100
    focus: visible

    function open() {
        visible = true
        cancelButton.forceActiveFocus()
    }

    function close() {
        visible = false
    }

    Keys.onEscapePressed: {
        root.cancelled()
        root.close()
    }

    Rectangle {
        anchors.fill: parent
        color: "#66000000"
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.cancelled()
            root.close()
        }
    }

    Rectangle {
        id: card
        width: Math.min(360, root.width - 40)
        height: content.implicitHeight + 48
        anchors.centerIn: parent
        radius: Theme.radiusCard
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 24
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: root.title
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSection
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                visible: root.body !== ""
                text: root.body
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                AppButton {
                    id: cancelButton
                    variant: "ghost"
                    text: T.t("common.cancel")
                    onClicked: {
                        root.cancelled()
                        root.close()
                    }
                }

                AppButton {
                    variant: "danger"
                    text: root.confirmText
                    onClicked: {
                        root.confirmed()
                        root.close()
                    }
                }
            }
        }
    }
}
