import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1180
    height: 760
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "Oyama's HRT Tracker"
    color: "#0b0f13"

    property int currentPage: 0

    component NavButton: Button {
        id: control
        property int page
        Layout.fillWidth: true
        Layout.preferredHeight: 44
        text: ""
        checkable: true
        checked: window.currentPage === page
        onClicked: window.currentPage = page
        background: Rectangle {
            radius: 10
            color: control.checked ? "#26313c"
                                   : control.hovered ? "#1b232c" : "transparent"
            border.width: control.checked ? 1 : 0
            border.color: "#384654"
        }
        contentItem: Text {
            leftPadding: 16
            text: control.text
            color: control.checked ? "#f4f6f8" : "#9aa8b6"
            font.pixelSize: 14
            font.weight: control.checked ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 68
            color: "#11171d"
            border.width: 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 16

                ColumnLayout {
                    spacing: 1
                    Label {
                        text: "Oyama's HRT Tracker"
                        color: "#f4f6f8"
                        font.pixelSize: 19
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: "local-first · no account · no network"
                        color: "#7f8c9a"
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: "MODE"
                    color: "#6f7d8b"
                    font.pixelSize: 10
                    font.letterSpacing: 1.2
                }
                ComboBox {
                    id: modeBox
                    Layout.preferredWidth: 140
                    model: ["transfem", "transmasc"]
                    currentIndex: app.mode === "transmasc" ? 1 : 0
                    onActivated: app.mode = currentText
                }
                Label {
                    text: app.backend
                    color: "#7f8c9a"
                    font.pixelSize: 11
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
                color: "#0d1318"
                border.width: 0

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8

                    Label {
                        text: "NAVIGATION"
                        color: "#596775"
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                        Layout.leftMargin: 8
                        Layout.topMargin: 4
                    }
                    NavButton { page: 0; text: "Overview" }
                    NavButton { page: 1; text: "Add dose" }
                    NavButton { page: 2; text: "History" }

                    Item { Layout.fillHeight: true }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: "#1d2731"
                    }
                    Label {
                        text: "Data stays on this machine.\nPostgreSQL over a local socket."
                        color: "#657381"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        Layout.leftMargin: 4
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.currentPage
                OverviewPage {}
                AddDosePage {}
                HistoryPage {}
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#0d1318"
            Label {
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                text: app.status
                color: app.status.indexOf("error") >= 0 ||
                       app.status.indexOf("No database") >= 0
                       ? "#e8a0b4" : "#7f8c9a"
                font.pixelSize: 11
                elide: Text.ElideRight
                width: parent.width - 48
            }
        }
    }
}
