import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: gate
    z: 1000

    Rectangle {
        anchors.fill: parent
        color: "#0e1318"
    }

    Loader {
        id: pageLoader
        anchors.fill: parent
        active: gate.visible
        sourceComponent: {
            switch (app.vaultState) {
            case "uninitialized":
                return setupPage
            case "locked":
                return unlockPage
            case "migrating":
                return migratingPage
            default:
                return unavailablePage
            }
        }
    }

    Component {
        id: setupPage
        SetupVaultPage {}
    }

    Component {
        id: unlockPage
        UnlockVaultPage {}
    }

    Component {
        id: migratingPage
        Item {
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(gate.width - 96, 560)
                spacing: 18

                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: true
                }

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Migrating legacy data…")
                    color: "#f4f6f8"
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: app.status
                    color: "#8a98a8"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    Component {
        id: unavailablePage
        Item {
            objectName: "vaultUnavailable"

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(gate.width - 96, 640)
                spacing: 12

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Vault unavailable")
                    color: "#f4f6f8"
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }

                Label {
                    objectName: "vaultStatusText"
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: app.status
                    color: "#8a98a8"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
