import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Doses.js" as Doses

// Dose history: records by day under short day headings, as hairline rows.
// "Add dose" unfolds the form at the top of the list and becomes "Cancel"; a
// row unfolds the same form under itself to edit it. Select ticks rows for a
// batch delete, whose question is a line at the top of the list.
Item {
    id: page

    property bool adding: false
    property string editingId: ""
    property bool selecting: false
    property var selected: ({})
    property bool askingBatch: false
    readonly property int selectedCount: Object.keys(selected).length

    readonly property real colWidth: Math.min(Theme.pageColumn, width - Theme.pageGutter * 2)
    readonly property real colX: Math.max(Theme.pageGutter, (width - Theme.pageColumn) / 2)

    function openAdd() {
        selecting = false
        editingId = ""
        adding = true
        pinnedTop = true
        list.contentY = list.originY
    }

    // While the list is at its top, it stays there as the header grows (the
    // add form unfolding); a ListView would otherwise keep its first record
    // in place and push the header up out of sight.
    property bool pinnedTop: true

    // Opens the newest record's form, and its delete question if `ask`
    // (for screenshots and tests: history/edit, history/delete).
    function openFirst(ask) {
        if (records.count === 0)
            return
        selecting = false
        adding = false
        editingId = records.get(0).doseId
        askFirst = ask
    }
    property bool askFirst: false

    function toggleSelected(id) {
        const next = Object.assign({}, selected)
        if (next[id]) delete next[id]
        else next[id] = true
        selected = next
    }

    function selectAll() {
        const next = {}
        if (selectedCount < records.count)
            for (let i = 0; i < records.count; ++i)
                next[records.get(i).doseId] = true
        selected = next
    }

    function endSelect() {
        selecting = false
        askingBatch = false
        selected = {}
    }

    function deleteSelected() {
        const ids = Object.keys(selected)
        for (let i = 0; i < ids.length; ++i)
            app.removeDose(ids[i])
        endSelect()
    }

    // The records, newest first, each with the day it falls on.
    ListModel { id: records }

    function rebuild() {
        records.clear()
        const doses = app.doses.slice().sort(function(a, b) { return Number(b.timeH) - Number(a.timeH) })
        for (let i = 0; i < doses.length; ++i) {
            const d = doses[i]
            const when = new Date(Number(d.timeH) * 3600000)
            records.append({
                doseId: String(d.id),
                route: String(d.route),
                ester: String(d.ester),
                doseMG: Number(d.doseMG),
                timeH: Number(d.timeH),
                extras: typeof d.extras === "string" ? d.extras : JSON.stringify(d.extras || {}),
                day: T.day(when)
            })
        }
    }

    Component.onCompleted: rebuild()
    Connections {
        target: app
        function onChanged() { page.rebuild() }
        function onModeChanged() { page.adding = false; page.editingId = ""; page.endSelect() }
    }
    Connections {
        target: Ui
        function onLanguageChanged() { page.rebuild() }
    }

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: records
        reuseItems: false
        cacheBuffer: 800
        ScrollBar.vertical: AppScrollBar {}
        onMovementEnded: page.pinnedTop = atYBeginning
        onOriginYChanged: if (page.pinnedTop) contentY = originY

        header: Item {
            width: list.width
            implicitHeight: headerColumn.implicitHeight

            ColumnLayout {
                id: headerColumn
                x: page.colX
                width: page.colWidth
                spacing: 0

                PageHeader {
                    title: T.t("timeline.title")
                    subtitle: page.selecting
                        ? T.fill(T.t("timeline.selected"), [page.selectedCount])
                        : (records.count === 1 ? T.t("timeline.records_one")
                                               : T.fill(T.t("timeline.records_n"), [records.count]))

                    AppButton {
                        visible: page.selecting
                        variant: "ghost"
                        iconName: "select"
                        text: T.t("timeline.select_all")
                        onClicked: page.selectAll()
                    }
                    AppButton {
                        visible: page.selecting
                        variant: "ghostDanger"
                        iconName: "trash"
                        iconSize: 15
                        iconStroke: 1.5
                        enabled: page.selectedCount > 0
                        text: T.t("btn.delete") + (page.selectedCount > 0 ? " (" + page.selectedCount + ")" : "")
                        onClicked: page.askingBatch = true
                    }
                    AppButton {
                        visible: page.selecting
                        variant: "ghost"
                        text: T.t("btn.cancel")
                        onClicked: page.endSelect()
                    }

                    AppButton {
                        visible: !page.selecting && records.count > 0
                        variant: "ghost"
                        iconName: "select"
                        text: T.t("timeline.select")
                        onClicked: {
                            page.adding = false
                            page.editingId = ""
                            page.selecting = true
                        }
                    }
                    // "Add dose", whose plus turns into a cross as it becomes
                    // "Cancel".
                    AppButton {
                        visible: !page.selecting
                        variant: "ghostAccent"
                        iconName: page.adding ? "close" : "plus"
                        text: page.adding ? T.t("btn.cancel") : T.t("btn.add")
                        enabled: app.backend !== "none" || page.adding
                        onClicked: {
                            page.editingId = ""
                            page.adding = !page.adding
                        }
                    }
                }

                UiText {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    Layout.bottomMargin: 8
                    visible: app.backend === "none"
                    text: T.t("db.not_set_up")
                    size: Theme.caption
                    color: Theme.textMuted
                    wrapMode: Text.Wrap
                    lineHeightMode: Text.ProportionalHeight
                    lineHeight: 1.0
                }

                Fold {
                    open: page.askingBatch

                    InlinePrompt {
                        title: T.t("timeline.batch_delete_title")
                        message: T.fill(T.t("timeline.batch_delete_confirm"), [page.selectedCount])
                        cancelText: T.t("btn.keep")
                        confirmText: T.t("btn.delete")
                        onCancelled: page.askingBatch = false
                        onConfirmed: page.deleteSelected()
                    }
                }

                Fold {
                    id: addFold
                    open: page.adding

                    Loader {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        active: page.adding || addFold.shown > 0
                        sourceComponent: DoseForm {
                            onDone: page.adding = false
                        }
                    }
                }

                // Nothing logged yet.
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 72
                    visible: records.count === 0 && !page.adding
                    spacing: 0

                    Doodle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.bottomMargin: 24
                        name: "logbook"
                        size: 96
                    }
                    UiText {
                        Layout.fillWidth: true
                        text: T.t("home.empty_title")
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
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
                        onClicked: page.openAdd()
                    }
                }

                Item { Layout.preferredHeight: 8 }
            }
        }

        footer: Item { width: list.width; height: 128 }

        section.property: "day"
        section.delegate: Item {
            required property string section
            width: list.width
            height: 40

            UiText {
                x: page.colX
                width: page.colWidth
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 4
                text: parent.section
                size: Theme.caption
                color: Theme.textMuted
                font.weight: Font.DemiBold
            }
        }

        delegate: Item {
            id: entry
            required property int index
            required property string doseId
            required property string route
            required property string ester
            required property real doseMG
            required property real timeH
            required property string extras
            required property string day

            readonly property bool editing: page.editingId === doseId && !page.selecting
            readonly property bool chosen: !!page.selected[doseId]
            readonly property bool lastOfDay: index === records.count - 1
                || records.get(index + 1).day !== day
            readonly property var parsedExtras: Doses.parseExtras(extras)
            readonly property bool removal: route === "patchRemove"

            width: list.width
            height: rowColumn.implicitHeight

            function asDose() {
                return { id: doseId, route: route, ester: ester, doseMG: doseMG, timeH: timeH, extras: extras }
            }

            ColumnLayout {
                id: rowColumn
                x: page.colX
                width: page.colWidth
                spacing: 0

                AbstractButton {
                    id: rowButton
                    Layout.fillWidth: true
                    implicitHeight: rowContent.implicitHeight + 28
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: (entry.removal ? T.t("route.patchRemove") : T.t("ester." + entry.ester))
                                     + ", " + T.clock(new Date(entry.timeH * 3600000))
                    onClicked: {
                        if (page.selecting) {
                            page.toggleSelected(entry.doseId)
                        } else {
                            page.adding = false
                            page.editingId = entry.editing ? "" : entry.doseId
                        }
                    }

                    background: Rectangle {
                        x: -8
                        width: parent.width + 16
                        height: parent.height
                        radius: Theme.radiusMd
                        color: rowButton.hovered || entry.editing || (page.selecting && entry.chosen)
                               ? Theme.surfaceHover : "transparent"
                        border.width: rowButton.visualFocus ? 2 : 0
                        border.color: Theme.focusRing
                    }

                    contentItem: RowLayout {
                        id: rowContent
                        spacing: 12

                        // The tick dot in select mode.
                        Rectangle {
                            visible: page.selecting
                            implicitWidth: 16
                            implicitHeight: 16
                            radius: 8
                            color: entry.chosen ? Theme.accent : "transparent"
                            border.width: 1
                            border.color: entry.chosen ? Theme.accentInk : Theme.borderStrong
                            Tick {
                                anchors.centerIn: parent
                                on: entry.chosen
                                size: 11
                                strokeWidth: 2.5
                                color: Theme.textOnAccent
                            }
                        }

                        Doodle {
                            name: entry.route
                            size: 28
                            asleep: entry.timeH > app.nowHours()
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                UiText {
                                    Layout.fillWidth: true
                                    text: entry.removal ? T.t("route.patchRemove") : T.t("ester." + entry.ester)
                                    font.weight: Font.Medium
                                    wrapMode: Text.Wrap
                                }
                                UiText {
                                    text: T.clock(new Date(entry.timeH * 3600000))
                                    size: Theme.caption
                                    color: Theme.textMuted
                                }
                            }

                            // A removal is the one line: its route is its name.
                            Flow {
                                Layout.fillWidth: true
                                visible: !entry.removal
                                spacing: 12

                                UiText {
                                    text: T.t("route." + entry.route)
                                    size: Theme.caption
                                    color: Theme.textMuted
                                }
                                UiText {
                                    text: entry.parsedExtras.releaseRateUGPerDay
                                          ? entry.parsedExtras.releaseRateUGPerDay + " µg/d"
                                          : entry.doseMG.toFixed(2) + " mg"
                                    size: Theme.caption
                                    font.weight: Font.Medium
                                }
                                UiText {
                                    readonly property real eq: Doses.factor(entry.ester)
                                    visible: eq !== 1 && !entry.parsedExtras.releaseRateUGPerDay
                                    text: "(" + (Doses.isTestosterone(entry.ester) ? T.t("label.t") : T.t("label.e2"))
                                          + " eq: " + (entry.doseMG * eq).toFixed(2) + " mg)"
                                    size: Theme.caption
                                    color: Theme.textMuted
                                    opacity: 0.7
                                }
                                UiText {
                                    visible: entry.route === "patchApply" && entry.parsedExtras.patchWearH > 0
                                    text: Doses.trim(entry.parsedExtras.patchWearH / 24) + " " + T.t("unit.day_short")
                                    size: Theme.caption
                                }
                            }
                        }

                        Icon {
                            visible: !page.selecting
                            name: "chevron-down"
                            size: 16
                            color: Theme.textMuted
                            rotation: entry.editing ? 180 : 0
                            Behavior on rotation {
                                NumberAnimation { duration: Theme.durationBase; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
                            }
                        }
                    }
                }

                Fold {
                    id: editFold
                    open: entry.editing

                    Loader {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        active: entry.editing || editFold.shown > 0
                        sourceComponent: DoseForm {
                            dose: entry.asDose()
                            askOnOpen: page.askFirst
                            onDone: page.editingId = ""
                        }
                    }
                }

                Hairline {
                    visible: !entry.lastOfDay
                }

                Item {
                    visible: entry.lastOfDay
                    Layout.preferredHeight: 8
                }
            }
        }
    }
}
