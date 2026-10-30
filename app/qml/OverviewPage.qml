import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The overview: the current estimated level as a large light figure, a tick
// scale with the target band and where today sits on it, then the timeline.
// With nothing logged it is the capsule, one line and the way to log a dose.
Item {
    id: page
    signal logRequested()

    readonly property bool transmasc: app.mode === "transmasc"
    readonly property bool hasDoses: app.doses.length > 0
    readonly property bool hasCPA: !transmasc && app.doses.some(function(d) { return d.ester === "CPA" })
    property bool explainerOpen: false
    property string range: "7d"

    // The reading the page is about, with its band, and the one beside it.
    readonly property var primary: transmasc
        ? { label: T.t("label.total_t"), value: app.currentT, decimals: 0, unit: "ng/dL",
            band: { low: 300, high: 1000 }, domain: [30, 3000] }
        : { label: T.t("label.e2"), value: app.currentE2, decimals: 1, unit: "pg/mL",
            band: { low: 100, high: 200 }, domain: [10, 1000] }
    readonly property var companion: transmasc
        ? (app.currentT > 0 ? { label: "", value: app.currentT / 28.842, decimals: 1, unit: "nmol/L" } : null)
        : (hasCPA ? { label: T.t("label.cpa_chart"), value: app.currentCPA >= 0.1 ? app.currentCPA : 0,
                      decimals: 1, unit: "ng/mL" } : null)

    // Where the reading sits, said in words over its tick.
    readonly property var status: {
        if (transmasc) {
            const c = app.currentT
            if (c <= 0) return null
            if (c > 1000) return { key: "status.level.t_high", tone: Theme.warning }
            if (c >= 600) return { key: "status.level.t_upper", tone: Theme.statusPositive }
            if (c >= 300) return { key: "status.level.t_male", tone: Theme.statusPositive }
            if (c >= 100) return { key: "status.level.t_subtarget", tone: Theme.statusViolet }
            return { key: "status.level.t_low", tone: Theme.statusNeutral }
        }
        const e = app.currentE2
        if (e <= 0) return null
        if (e > 300) return { key: "status.level.high", tone: Theme.warning }
        if (e >= 100 && e <= 200) return { key: "status.level.mtf", tone: Theme.statusPositive }
        if (e >= 70) return { key: "status.level.luteal", tone: Theme.statusInfo }
        if (e >= 30) return { key: "status.level.follicular", tone: Theme.statusViolet }
        if (e >= 8) return { key: "status.level.male", tone: Theme.statusNeutral }
        return { key: "status.level.low", tone: Theme.warning }
    }

    onVisibleChanged: if (visible) chart.play()
    Component.onCompleted: if (visible) chart.play()
    Connections {
        target: app
        function onChanged() { if (page.visible) chart.play() }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: body.implicitHeight + 128
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        visible: page.hasDoses
        ScrollBar.vertical: AppScrollBar {}

        ColumnLayout {
            id: body
            width: parent.width
            spacing: 0

            // The header: readings and their scale, a rule under it that runs
            // the full width of the pane.
            Item {
                Layout.fillWidth: true
                implicitHeight: header.implicitHeight + 24 + 20

                ColumnLayout {
                    id: header
                    x: Math.max(Theme.pageGutter, (parent.width - Theme.pageColumn) / 2)
                    y: 24
                    width: Math.min(Theme.pageColumn, parent.width - Theme.pageGutter * 2)
                    spacing: 0

                    // The whole title unfolds the explainer under it.
                    AbstractButton {
                        id: estimateTitle
                        Layout.bottomMargin: 16
                        implicitWidth: titleRow.implicitWidth
                        implicitHeight: 22
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: T.t("status.estimate")
                        onClicked: page.explainerOpen = !page.explainerOpen

                        background: Rectangle {
                            anchors.fill: parent
                            anchors.margins: -4
                            radius: 6
                            color: "transparent"
                            border.width: estimateTitle.visualFocus ? 2 : 0
                            border.color: Theme.focusRing
                        }

                        contentItem: Row {
                            id: titleRow
                            spacing: 6
                            UiText {
                                anchors.verticalCenter: parent.verticalCenter
                                text: T.t("status.estimate")
                                color: page.explainerOpen || estimateTitle.hovered ? Theme.text : Theme.textMuted
                            }
                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: "info"
                                size: 13
                                color: page.explainerOpen || estimateTitle.hovered ? Theme.text : Theme.textMuted
                                opacity: estimateTitle.hovered ? 1 : 0.7
                            }
                        }
                    }

                    // What the number is, unfolded in place under the title.
                    Fold {
                        open: page.explainerOpen

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.bottomMargin: 20
                            spacing: 12

                            UiText {
                                Layout.fillWidth: true
                                text: T.t("estimate.p1")
                                color: Theme.textMuted
                                wrapMode: Text.Wrap
                                lineHeightMode: Text.ProportionalHeight
                                lineHeight: 1.15
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: calloutText.implicitHeight + 24
                                radius: Theme.radiusMd
                                color: Theme.surfaceMuted
                                UiText {
                                    id: calloutText
                                    x: 14
                                    y: 12
                                    width: parent.width - 28
                                    text: T.t("estimate.p2")
                                    font.weight: Font.Medium
                                    size: Theme.caption
                                    wrapMode: Text.Wrap
                                    lineHeightMode: Text.ProportionalHeight
                                    lineHeight: 1.0
                                }
                            }
                            UiText {
                                Layout.fillWidth: true
                                text: T.t("estimate.p3")
                                color: Theme.textMuted
                                wrapMode: Text.Wrap
                                lineHeightMode: Text.ProportionalHeight
                                lineHeight: 1.15
                            }
                            UiText {
                                Layout.fillWidth: true
                                text: T.t("estimate.source")
                                size: Theme.caption
                                color: Theme.textMuted
                                wrapMode: Text.Wrap
                                lineHeightMode: Text.ProportionalHeight
                                lineHeight: 1.0
                            }
                        }
                    }

                    // The readings: label over number, the second flush right
                    // at the same size, both on one baseline.
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: 18 + 6 + primaryValue.height

                        UiText {
                            anchors.left: parent.left
                            text: page.primary.label
                            size: Theme.caption
                            color: Theme.textMuted
                            font.weight: Font.Medium
                        }

                        // Readings keep their digits left to right, so they are
                            // placed by hand on the page's starting side.
                        Reading {
                            id: primaryValue
                            x: T.rtl ? parent.width - width : 0
                            y: 24
                            value: page.primary.value
                            decimals: page.primary.decimals
                            unit: page.primary.unit
                        }

                        UiText {
                            visible: page.companion !== null && page.companion.label !== ""
                            anchors.right: parent.right
                            text: page.companion ? page.companion.label : ""
                            size: Theme.caption
                            color: Theme.textMuted
                            font.weight: Font.Medium
                        }

                        Reading {
                            visible: page.companion !== null
                            x: T.rtl ? 0 : parent.width - width
                            y: 24
                            value: page.companion ? page.companion.value : 0
                            decimals: page.companion ? page.companion.decimals : 0
                            unit: page.companion ? page.companion.unit : ""
                        }
                    }

                    LevelRail {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        visible: page.primary.value > 0
                        value: page.primary.value
                        band: page.primary.band
                        domain: page.primary.domain
                        label: page.status ? T.t(page.status.key) : ""
                        tone: page.status ? page.status.tone : Theme.statusNeutral
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: Theme.hairline
                    color: Theme.border
                }
            }

            // The timeline.
            Item {
                Layout.fillWidth: true
                implicitHeight: timeline.implicitHeight + 20

                ColumnLayout {
                    id: timeline
                    x: Math.max(Theme.pageGutter, (parent.width - Theme.pageColumn) / 2)
                    y: 20
                    width: Math.min(Theme.pageColumn, parent.width - Theme.pageGutter * 2)
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        UiText {
                            Layout.fillWidth: true
                            text: T.t("chart.title")
                            font.weight: Font.DemiBold
                            font.letterSpacing: -0.15
                        }

                        Segmented {
                            options: [
                                { value: "7d", label: T.t("chart.range_7d") },
                                { value: "30d", label: T.t("chart.range_30d") },
                                { value: "all", label: T.t("chart.range_all") }
                            ]
                            currentValue: page.range
                            onPicked: function(value) { page.range = value }
                        }
                    }

                    HormoneChart {
                        id: chart
                        Layout.fillWidth: true
                        Layout.leftMargin: -24
                        Layout.rightMargin: -24
                        Layout.preferredHeight: secondary ? 448 : 384
                        curve: app.curve
                        doses: app.doses
                        transmasc: page.transmasc
                        secondary: page.hasCPA
                        range: page.range
                        nowH: app.nowHours()
                        primaryLabel: (page.transmasc ? T.t("label.total_t") + " ng/dL" : T.t("label.e2") + " pg/mL")
                        secondaryLabel: T.t("label.cpa_chart") + " ng/mL"
                        nowLabel: T.t("time.now")
                        targetLabel: T.t("chart.target")
                        sweep: played ? 1 : 0

                        Connections {
                            target: app
                            function onChanged() { chart.nowH = app.nowHours(); chart.requestPaint() }
                        }
                        Connections {
                            target: Ui
                            function onLanguageChanged() { chart.requestPaint() }
                        }
                    }

                    DoseHeatmap {
                        Layout.fillWidth: true
                        Layout.topMargin: 24
                        doses: app.doses
                    }
                }
            }
        }
    }

    // Nothing logged yet: the capsule, a line, and the way to log a dose.
    ColumnLayout {
        visible: !page.hasDoses
        anchors.horizontalCenter: parent.horizontalCenter
        y: 96
        width: Math.min(360, parent.width - Theme.pageGutter * 2)
        spacing: 0

        Doodle {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 24
            name: "capsule"
            size: 96
        }

        UiText {
            Layout.fillWidth: true
            text: T.t("home.empty_title")
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        UiText {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 260
            Layout.topMargin: 4
            text: T.t("home.empty_subtitle")
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        AppButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 20
            text: T.t("home.empty_cta")
            enabled: app.backend !== "none"
            onClicked: page.logRequested()
        }

        UiText {
            Layout.fillWidth: true
            Layout.topMargin: 16
            visible: app.backend === "none"
            text: T.t("db.not_set_up")
            size: Theme.caption
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            lineHeightMode: Text.ProportionalHeight
            lineHeight: 1.0
        }
    }
}
