import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1200
    height: 800
    visible: true
    title: "Oyama's HRT Tracker"
    color: "#0e1318"

    property int pageIndex: 0
    property var editingDose: null

    readonly property var navItems: [
        { label: "Overview", icon: "◉" },
        { label: "Add dose", icon: "＋" },
        { label: "History", icon: "≡" },
        { label: "Settings", icon: "⚙" }
    ]

    Item {
        id: shell
        objectName: "appShell"
        anchors.fill: parent
        enabled: app.vaultState === "unlocked"

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: "#141b22"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    spacing: 16

                    Text {
                        text: "Oyama's HRT Tracker"
                        color: "#f4f6f8"
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                    }

                    Item { Layout.fillWidth: true }

                    ComboBox {
                        id: modeBox
                        model: ["transfem", "transmasc"]
                        currentIndex: app.mode === "transmasc" ? 1 : 0
                        onActivated: app.mode = currentText
                    }

                    Text {
                        text: app.backend
                        color: "#8a98a8"
                        font.pixelSize: 12
                    }

                    Button {
                        objectName: "lockButton"
                        text: qsTr("Lock")
                        onClicked: app.lockVault()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                Rectangle {
                    Layout.preferredWidth: 220
                    Layout.fillHeight: true
                    color: "#101821"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 6

                        Repeater {
                            model: window.navItems

                            delegate: Button {
                                required property int index
                                required property var modelData
                                Layout.fillWidth: true
                                text: modelData.icon + "  " + modelData.label
                                flat: true
                                highlighted: window.pageIndex === index
                                onClicked: {
                                    if (index === 1) {
                                        window.editingDose = null;
                                        addPage.resetForm();
                                    }
                                    window.pageIndex = index;
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        Text {
                            text: "Local · socket only"
                            color: "#5f6d7c"
                            font.pixelSize: 11
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: window.pageIndex

                    OverviewPage {}

                    AddDosePage {
                        id: addPage
                        onSaved: {
                            window.editingDose = null;
                            window.pageIndex = 0;
                        }
                    }

                    HistoryPage {
                        onEditRequested: function(dose) {
                            window.editingDose = dose;
                            addPage.loadDose(dose);
                            window.pageIndex = 1;
                        }
                    }

                    SettingsPage {}
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                color: "#0b1015"

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    verticalAlignment: Text.AlignVCenter
                    text: app.status
                    color: "#8a98a8"
                    elide: Text.ElideRight
                    font.pixelSize: 12
                }
            }
        }
    }

    VaultGate {
        objectName: "vaultGate"
        anchors.fill: parent
        visible: app.vaultState !== "unlocked"
    }
}
