import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    component StatCard: Rectangle {
        id: card
        property string label
        property string value
        property string unit
        property color accent: "#e8a0b4"
        Layout.fillWidth: true
        Layout.preferredHeight: 112
        radius: 16
        color: "#151c23"
        border.width: 1
        border.color: "#24313c"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 2
            Label {
                text: card.label
                color: "#8492a0"
                font.pixelSize: 11
                font.letterSpacing: 1.0
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                spacing: 6
                Label {
                    text: card.value
                    color: "#f4f6f8"
                    font.pixelSize: 30
                    font.weight: Font.DemiBold
                }
                Label {
                    text: card.unit
                    color: card.accent
                    font.pixelSize: 12
                    Layout.alignment: Qt.AlignBottom
                    Layout.bottomMargin: 6
                }
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: page.width
            spacing: 18
            anchors.margins: 28
            Layout.leftMargin: 28
            Layout.rightMargin: 28
            Layout.topMargin: 24
            Layout.bottomMargin: 24

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.topMargin: 24
                ColumnLayout {
                    spacing: 2
                    Label {
                        text: "Overview"
                        color: "#f4f6f8"
                        font.pixelSize: 26
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: "Current exposure and the modeled curve for this mode."
                        color: "#7f8c9a"
                        font.pixelSize: 12
                    }
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: app.mode
                    color: "#e8a0b4"
                    font.pixelSize: 12
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                spacing: 14
                StatCard {
                    label: "ESTRADIOL"
                    value: app.currentE2.toFixed(1)
                    unit: "pg/mL"
                }
                StatCard {
                    label: "CYPROTERONE"
                    value: app.currentCPA.toFixed(2)
                    unit: "ng/mL"
                    accent: "#9ec5a7"
                }
                StatCard {
                    label: "TESTOSTERONE"
                    value: app.currentT.toFixed(1)
                    unit: "ng/dL"
                    accent: "#c6b6e8"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.preferredHeight: 340
                radius: 18
                color: "#151c23"
                border.width: 1
                border.color: "#24313c"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 4
                    Label {
                        text: "Modeled concentration"
                        color: "#f4f6f8"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: "E2 curve from the local C99 model. Times are local."
                        color: "#7f8c9a"
                        font.pixelSize: 11
                    }
                    Canvas {
                        id: chart
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.topMargin: 8
                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            var points = app.curve;
                            if (!points || points.length < 2) {
                                ctx.fillStyle = "#677684";
                                ctx.font = "13px sans-serif";
                                ctx.fillText("No doses yet. Add one to see the curve.",
                                             16, height / 2);
                                return;
                            }
                            var left = 48, right = 12, top = 10, bottom = 30;
                            var plotW = Math.max(10, width - left - right);
                            var plotH = Math.max(10, height - top - bottom);
                            var minT = points[0].t, maxT = points[points.length - 1].t;
                            var maxY = 0;
                            for (var i = 0; i < points.length; i++) {
                                maxY = Math.max(maxY, points[i].e2);
                            }
                            if (maxT <= minT) maxT = minT + 1;
                            maxY = Math.max(1, maxY * 1.15);

                            ctx.strokeStyle = "#24313c";
                            ctx.lineWidth = 1;
                            ctx.fillStyle = "#677684";
                            ctx.font = "10px sans-serif";
                            for (var g = 0; g <= 4; g++) {
                                var gy = top + plotH * g / 4;
                                ctx.beginPath();
                                ctx.moveTo(left, gy);
                                ctx.lineTo(left + plotW, gy);
                                ctx.stroke();
                                var value = maxY * (1 - g / 4);
                                ctx.fillText(value.toFixed(0), 4, gy + 3);
                            }
                            ctx.strokeStyle = "#5b6b7a";
                            ctx.beginPath();
                            ctx.moveTo(left, top);
                            ctx.lineTo(left, top + plotH);
                            ctx.lineTo(left + plotW, top + plotH);
                            ctx.stroke();

                            ctx.strokeStyle = "#e8a0b4";
                            ctx.lineWidth = 2.5;
                            ctx.lineJoin = "round";
                            ctx.beginPath();
                            for (var j = 0; j < points.length; j++) {
                                var x = left + (points[j].t - minT) /
                                        (maxT - minT) * plotW;
                                var y = top + (1 - points[j].e2 / maxY) * plotH;
                                if (j === 0) ctx.moveTo(x, y);
                                else ctx.lineTo(x, y);
                            }
                            ctx.stroke();

                            ctx.fillStyle = "#677684";
                            ctx.font = "10px sans-serif";
                            ctx.fillText(app.formatTime(minT), left, height - 8);
                            ctx.fillText(app.formatTime(maxT), left + plotW - 92,
                                         height - 8);
                        }
                        Connections {
                            target: app
                            function onChanged() { chart.requestPaint() }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.bottomMargin: 28
                spacing: 8
                Label {
                    text: "Recent doses"
                    color: "#f4f6f8"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                Label {
                    visible: app.doses.length === 0
                    text: "No doses recorded for this mode."
                    color: "#7f8c9a"
                    font.pixelSize: 12
                }
                Repeater {
                    model: Math.min(app.doses.length, 5)
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        radius: 10
                        color: "#11171d"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            Label {
                                text: app.doses[index].route + " · " +
                                      app.doses[index].ester
                                color: "#d8e0e7"
                                font.pixelSize: 12
                            }
                            Item { Layout.fillWidth: true }
                            Label {
                                text: app.doses[index].doseMG + " mg"
                                color: "#e8a0b4"
                                font.pixelSize: 12
                            }
                            Label {
                                text: app.doses[index].timeLabel
                                color: "#677684"
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
    }
}
