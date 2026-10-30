import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property int currentIndex: 0
    signal navigate(int index)

    Layout.preferredWidth: 236
    Layout.fillHeight: true
    color: Theme.sidebar
    LayoutMirroring.enabled: T.rtl
    LayoutMirroring.childrenInherit: true

    Rectangle {
        width: 1
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.bottomMargin: 12
            spacing: 10

            Rectangle {
                implicitWidth: 36
                implicitHeight: 36
                radius: 10
                color: Theme.accentSubtle

                Text {
                    anchors.centerIn: parent
                    text: "O"
                    color: Theme.accentInk
                    font.family: Theme.fontFamily
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                }
            }

            ColumnLayout {
                spacing: 0

                Text {
                    text: "Oyama"
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }

                Text {
                    text: "Tracker"
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                }
            }

            Item { Layout.fillWidth: true }
        }

        Repeater {
            model: [
                { key: "overview", icon: "home" },
                { key: "add_dose", icon: "plus-circle" },
                { key: "history", icon: "list" },
                { key: "settings", icon: "sliders" }
            ]

            delegate: NavItem {
                required property int index
                required property var modelData
                Layout.fillWidth: true
                text: T.t("nav." + modelData.key)
                iconName: modelData.icon
                selected: root.currentIndex === index
                onClicked: root.navigate(index)
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            AppButton {
                Layout.fillWidth: true
                text: T.t("mode.transfem")
                variant: app.mode === "transfem" ? "primary" : "ghost"
                onClicked: app.mode = "transfem"
            }

            AppButton {
                Layout.fillWidth: true
                text: T.t("mode.transmasc")
                variant: app.mode === "transmasc" ? "primary" : "ghost"
                onClicked: app.mode = "transmasc"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: 8

            Rectangle {
                implicitWidth: 8
                implicitHeight: 8
                radius: 4
                color: app.backend === "none" ? Theme.textFaint : Theme.green
            }

            Text {
                text: T.t(app.backend === "none" ? "status.not_connected" : "status.local_socket")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        AppButton {
            Layout.fillWidth: true
            Layout.topMargin: 8
            variant: "ghost"
            iconName: Theme.dark ? "sun" : "moon"
            Accessible.name: T.t("theme.label")
            onClicked: Ui.theme = Theme.dark ? "light" : "dark"
        }

        Item { Layout.preferredHeight: 16 }
    }
}
