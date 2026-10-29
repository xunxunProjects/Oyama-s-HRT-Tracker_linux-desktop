import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 2
                Label {
                    text: "History"
                    color: "#f4f6f8"
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                }
                Label {
                    text: app.doses.length + " dose(s) in " + app.mode
                    color: "#7f8c9a"
                    font.pixelSize: 12
                }
            }
            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 18
            color: "#151c23"
            border.width: 1
            border.color: "#24313c"

            Label {
                anchors.centerIn: parent
                visible: app.doses.length === 0
                text: "No doses recorded yet.\nAdd one from the Add dose page."
                color: "#677684"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
            }

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                spacing: 8
                model: app.doses

                delegate: Rectangle {
                    width: list.width
                    height: 72
                    radius: 12
                    color: "#11171d"
                    border.width: 1
                    border.color: "#1f2b35"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 16

                        Rectangle {
                            Layout.preferredWidth: 40
                            Layout.preferredHeight: 40
                            radius: 12
                            color: "#26313c"
                            Label {
                                anchors.centerIn: parent
                                text: modelData.ester
                                color: "#e8a0b4"
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: modelData.route + " · " + modelData.ester
                                color: "#e8eef3"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            Label {
                                text: modelData.timeLabel
                                color: "#677684"
                                font.pixelSize: 11
                            }
                        }

                        Label {
                            text: modelData.doseMG + " mg"
                            color: "#d8e0e7"
                            font.pixelSize: 14
                        }

                        Button {
                            text: "Delete"
                            Layout.preferredWidth: 82
                            Layout.preferredHeight: 34
                            onClicked: app.removeDose(modelData.id)
                            background: Rectangle {
                                radius: 8
                                color: parent.down ? "#3a2028" : "transparent"
                                border.width: 1
                                border.color: "#5a3a43"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#e8a0b4"
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }
        }
    }
}
