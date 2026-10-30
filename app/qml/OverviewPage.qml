import QtQuick
import QtQuick.Layouts

Item {
    id: page
    Layout.fillWidth: true
    Layout.fillHeight: true

    signal viewAllRequested()
    signal addRequested()

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight + Theme.pageMargin * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: content
            x: Theme.pageMargin
            y: Theme.pageMargin
            width: page.width - Theme.pageMargin * 2
            spacing: 24

            PageHeader {
                title: T.t("overview.title")
                subtitle: T.t("overview.subtitle")
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                StatCard {
                    Layout.fillWidth: true
                    label: T.t("overview.e2")
                    value: app.currentE2.toFixed(1)
                    unit: "pg/mL"
                    accentColor: Theme.accent
                }

                StatCard {
                    Layout.fillWidth: true
                    label: T.t("overview.cpa")
                    value: app.currentCPA.toFixed(2)
                    unit: "ng/mL"
                    accentColor: Theme.green
                }

                StatCard {
                    Layout.fillWidth: true
                    label: T.t("overview.t")
                    value: app.currentT.toFixed(1)
                    unit: "ng/dL"
                    accentColor: Theme.brick
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 420
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8

                    Text {
                        Layout.fillWidth: true
                        text: T.t("overview.chart_title")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSection
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: T.t("overview.chart_subtitle")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontLabel
                        wrapMode: Text.Wrap
                    }

                    Canvas {
                        id: chart
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 320
                        visible: app.curve.length >= 2

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.fillStyle = Theme.surface
                            ctx.fillRect(0, 0, width, height)

                            var points = app.curve
                            if (!points || points.length < 2)
                                return

                            var left = 48
                            var right = 16
                            var top = 12
                            var bottom = 32
                            var plotW = Math.max(10, width - left - right)
                            var plotH = Math.max(10, height - top - bottom)
                            var minT = points[0].t
                            var maxT = points[points.length - 1].t
                            var maxE2 = 0
                            for (var i = 0; i < points.length; i++)
                                maxE2 = Math.max(maxE2, points[i].e2)
                            if (maxT <= minT)
                                maxT = minT + 1
                            maxE2 = Math.max(10, Math.ceil(maxE2 / 10) * 10)

                            ctx.strokeStyle = Theme.chartGrid
                            ctx.lineWidth = 1
                            ctx.fillStyle = Theme.textMuted
                            ctx.font = "11px sans-serif"
                            ctx.textAlign = "right"
                            ctx.textBaseline = "middle"
                            for (var g = 0; g <= 4; g++) {
                                var gy = top + plotH * g / 4
                                ctx.beginPath()
                                ctx.moveTo(left, gy)
                                ctx.lineTo(left + plotW, gy)
                                ctx.stroke()
                                var value = maxE2 * (1 - g / 4)
                                ctx.fillText(value.toFixed(0), left - 8, gy)
                            }

                            function xFor(t) {
                                return left + (t - minT) / (maxT - minT) * plotW
                            }
                            function yFor(e2) {
                                return top + (1 - e2 / maxE2) * plotH
                            }

                            ctx.beginPath()
                            ctx.moveTo(xFor(points[0].t), top + plotH)
                            for (var f = 0; f < points.length; f++)
                                ctx.lineTo(xFor(points[f].t), yFor(points[f].e2))
                            ctx.lineTo(xFor(points[points.length - 1].t),
                                       top + plotH)
                            ctx.closePath()
                            ctx.save()
                            ctx.globalAlpha = 0.10
                            ctx.fillStyle = Theme.chartLine
                            ctx.fill()
                            ctx.restore()

                            ctx.strokeStyle = Theme.chartLine
                            ctx.lineWidth = 2.5
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            ctx.beginPath()
                            for (var j = 0; j < points.length; j++) {
                                var x = xFor(points[j].t)
                                var y = yFor(points[j].e2)
                                if (j === 0)
                                    ctx.moveTo(x, y)
                                else
                                    ctx.lineTo(x, y)
                            }
                            ctx.stroke()

                            ctx.fillStyle = Theme.textMuted
                            ctx.font = "11px sans-serif"
                            ctx.textBaseline = "alphabetic"
                            ctx.textAlign = "left"
                            ctx.fillText(app.formatTime(points[0].t), left,
                                         height - 8)
                            ctx.textAlign = "right"
                            ctx.fillText(
                                app.formatTime(points[points.length - 1].t),
                                left + plotW, height - 8)
                            ctx.textAlign = "left"
                        }

                        Connections {
                            target: app
                            function onChanged() { chart.requestPaint() }
                        }

                        Connections {
                            target: Theme
                            function onDarkChanged() { chart.requestPaint() }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 320
                        visible: app.curve.length < 2

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: T.t("overview.chart_empty")
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontBody
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                            }

                            AppButton {
                                Layout.alignment: Qt.AlignHCenter
                                text: T.t("overview.add_cta")
                                variant: "ghost"
                                enabled: app.backend !== "none"
                                onClicked: page.addRequested()
                            }
                        }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: T.t("overview.recent")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSection
                        font.weight: Font.DemiBold
                    }

                    AppButton {
                        text: T.t("common.view_all")
                        variant: "ghost"
                        onClicked: page.viewAllRequested()
                    }
                }

                EmptyState {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 240
                    visible: app.doses.length === 0
                    iconName: "pill"
                    title: T.t("overview.recent_empty")
                    body: ""
                    actionText: T.t("overview.add_cta")
                    actionEnabled: app.backend !== "none"
                    onActionClicked: page.addRequested()
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    visible: app.doses.length > 0

                    Repeater {
                        model: Math.min(app.doses.length, 5)

                        delegate: Rectangle {
                            id: doseRow
                            required property int index
                            property var dose: app.doses[index]

                            Layout.fillWidth: true
                            Layout.preferredHeight: 56
                            color: "transparent"

                            RowLayout {
                                anchors.fill: parent
                                spacing: 12

                                Rectangle {
                                    implicitWidth: 36
                                    implicitHeight: 36
                                    radius: 18
                                    color: Theme.accentSubtle

                                    Icon {
                                        anchors.centerIn: parent
                                        name: "pill"
                                        size: 18
                                        color: Theme.accentInk
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Text {
                                        Layout.fillWidth: true
                                        text: doseRow.dose
                                              ? T.t("route." + doseRow.dose.route) + " · " +
                                                T.t("ester." + doseRow.dose.ester)
                                              : ""
                                        color: Theme.text
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontBody
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: doseRow.dose ? doseRow.dose.timeLabel : ""
                                        color: Theme.textMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontLabel
                                    }
                                }

                                Text {
                                    text: doseRow.dose
                                          ? Number(doseRow.dose.doseMG).toFixed(2) + " mg"
                                          : ""
                                    color: Theme.accent
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontBody
                                    font.weight: Font.Medium
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
