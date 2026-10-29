import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: page.width
            spacing: 18
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            Layout.topMargin: 24
            Layout.bottomMargin: 24

            Label {
                text: "Add dose"
                color: "#f4f6f8"
                font.pixelSize: 26
                font.weight: Font.DemiBold
                Layout.leftMargin: 28
                Layout.topMargin: 24
            }
            Label {
                text: "Record a dose for " + app.mode +
                      ". The curve updates immediately."
                color: "#7f8c9a"
                font.pixelSize: 12
                Layout.leftMargin: 28
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.preferredHeight: 430
                radius: 18
                color: "#151c23"
                border.width: 1
                border.color: "#24313c"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 14

                    Label {
                        text: "ROUTE"
                        color: "#8492a0"
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                    }
                    ComboBox {
                        id: routeBox
                        Layout.fillWidth: true
                        model: ["oral", "sublingual", "injection", "gel",
                                "patchApply", "patchRemove"]
                    }

                    Label {
                        text: "ESTER / DRUG"
                        color: "#8492a0"
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                    }
                    ComboBox {
                        id: esterBox
                        Layout.fillWidth: true
                        model: ["E2", "EV", "EB", "EC", "EN", "EU",
                                "CPA", "T", "TC", "TE", "TU"]
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Label {
                                text: "DOSE (mg)"
                                color: "#8492a0"
                                font.pixelSize: 10
                                font.letterSpacing: 1.2
                            }
                            TextField {
                                id: doseField
                                Layout.fillWidth: true
                                placeholderText: "2"
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Label {
                                text: "DAYS AGO"
                                color: "#8492a0"
                                font.pixelSize: 10
                                font.letterSpacing: 1.2
                            }
                            TextField {
                                id: daysField
                                Layout.fillWidth: true
                                text: "0"
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                            }
                        }
                    }

                    Label {
                        text: "The time is recorded as now minus the days entered. " +
                              "Detailed extras (patch wear, gel site, sublingual tier) " +
                              "will be added in the next UI pass."
                        color: "#677684"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Button {
                            text: "Add dose"
                            Layout.preferredWidth: 140
                            Layout.preferredHeight: 42
                            onClicked: {
                                var dose = parseFloat(doseField.text);
                                var days = parseFloat(daysField.text);
                                if (!isFinite(dose) || dose <= 0) {
                                    message.text = "Enter a dose greater than zero.";
                                    return;
                                }
                                if (!isFinite(days) || days < 0) {
                                    message.text = "Days ago must be zero or positive.";
                                    return;
                                }
                                var timeH = app.nowHours() - days * 24;
                                if (app.addDose(routeBox.currentText,
                                                esterBox.currentText,
                                                dose, timeH)) {
                                    message.text = "Dose added.";
                                    doseField.text = "";
                                } else {
                                    message.text = app.status;
                                }
                            }
                            background: Rectangle {
                                radius: 10
                                color: parent.down ? "#d48ba0" : "#e8a0b4"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#171014"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                        Label {
                            id: message
                            color: "#9aa8b6"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
