import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Everything before the records: creating the vault, unlocking it, moving
// earlier records in, or why they can't be opened. A page on the app's own
// ground, in the intro's centred column, covering the shell beneath. The
// panic screen draws itself over all of it.
Rectangle {
    id: gate
    z: 1000
    color: Theme.bg

    // The controller's message in the interface language where it has one.
    function statusText() {
        return app.statusKey ? T.t(app.statusKey, app.statusArgs) : app.status
    }

    Loader {
        id: pageLoader
        anchors.fill: parent
        active: gate.visible && app.vaultState !== "panic"
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
                width: Math.min(400, gate.width - 64)
                spacing: 0

                UiText {
                    Layout.fillWidth: true
                    text: T.t("vault.migrating_title")
                    size: Theme.title
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.52
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                UiText {
                    objectName: "migrationStatusText"
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    text: gate.statusText()
                    color: Theme.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    lineHeightMode: Text.ProportionalHeight
                    lineHeight: 1.2
                }

                // While records move, a line drawn over and over; when it
                // stops, the way on.
                InkRule {
                    id: progressRule
                    Layout.preferredWidth: 160
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 32
                    visible: app.vaultBusy

                    SequentialAnimation on inked {
                        running: app.vaultBusy
                        loops: Animation.Infinite
                        PropertyAction { value: true }
                        PauseAnimation { duration: Theme.durationInk + 200 }
                        PropertyAction { value: false }
                        PauseAnimation { duration: Theme.durationInk + 200 }
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 32
                    spacing: 8
                    visible: !app.vaultBusy

                    AppButton {
                        objectName: "cancelMigrationButton"
                        variant: "ghost"
                        text: T.t("btn.cancel")
                        onClicked: app.cancelLegacyMigration()
                    }

                    AppButton {
                        objectName: "retryMigrationButton"
                        text: T.t("vault.retry")
                        enabled: app.migrationRetryAvailable
                        onClicked: app.retryLegacyMigration()
                    }
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
                width: Math.min(400, gate.width - 64)
                spacing: 0

                Doodle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: 28
                    name: "caution"
                    size: 56
                }

                UiText {
                    Layout.fillWidth: true
                    text: T.t("vault.unavailable_title")
                    size: Theme.title
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.52
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                UiText {
                    objectName: "vaultStatusText"
                    Layout.fillWidth: true
                    Layout.topMargin: 12
                    text: gate.statusText()
                    color: Theme.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    lineHeightMode: Text.ProportionalHeight
                    lineHeight: 1.2
                }
            }
        }
    }
}
