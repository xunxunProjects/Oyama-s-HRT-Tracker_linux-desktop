import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    signal editRequested(var dose)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        Label {
            text: qsTr("History")
            font.pixelSize: 30
            font.bold: true
            color: "#f4f6f8"
        }
        Label {
            text: qsTr("Doses recorded for %1 mode. Edit or delete an entry.").arg(app.mode)
            color: "#8a98a8"
            font.pixelSize: 13
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 14
            color: "#182028"
            border.color: "#26313c"

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 12
                clip: true
                spacing: 8
                model: app.doses

                delegate: Rectangle {
                    required property var modelData
                    width: list.width
                    height: 66
                    radius: 10
                    color: "#111820"
                    border.color: "#26313c"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: 4
                            Layout.preferredHeight: 38
                            radius: 2
                            color: "#e8a0b4"
                        }

                        ColumnLayout {
                            spacing: 2
                            Label {
                                text: modelData.route + " · " + modelData.ester
                                color: "#f4f6f8"
                                font.pixelSize: 15
                                font.bold: true
                            }
                            Label {
                                text: modelData.timeLabel + " · " + Number(modelData.doseMG).toFixed(2) + " mg"
                                color: "#8a98a8"
                                font.pixelSize: 12
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: qsTr("Edit")
                            onClicked: page.editRequested(modelData)
                        }
                        Button {
                            text: qsTr("Delete")
                            onClicked: app.removeDose(modelData.id)
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    text: qsTr("No doses yet. Add one from the Add dose page.")
                    color: "#8a98a8"
                }
            }
        }
    }
}
