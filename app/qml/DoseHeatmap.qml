import QtQuick

// Dose activity (DoseHeatmap upstream): the last months as a grid of days,
// Monday at the top of each week's column, shaded by how many doses each day
// had, today ringed. Pointing at a day names it in the line under the grid
// rather than in a floating tip.
Item {
    id: root
    property var doses: []
    implicitHeight: grid.height + 8 + caption.implicitHeight

    readonly property int gutter: 20
    readonly property int header: 15
    readonly property int gap: 3
    readonly property int weeks: Math.max(4, Math.min(53, Math.floor((width - gutter + gap) / (12 + gap))))
    readonly property real cell: Math.max(7, Math.min(15, (width - gutter - gap * (weeks - 1)) / weeks))
    property string hoverKey: ""

    function dayKey(d) {
        return d.getFullYear() + "-" + (d.getMonth() + 1) + "-" + d.getDate()
    }

    readonly property var today: {
        const n = new Date()
        return new Date(n.getFullYear(), n.getMonth(), n.getDate())
    }

    readonly property var byDay: {
        const m = {}
        for (let i = 0; i < doses.length; ++i) {
            const e = doses[i]
            if (e.route === "patchRemove") continue
            const at = new Date(Number(e.timeH) * 3600000)
            const key = dayKey(at)
            if (!m[key]) m[key] = { count: 0, mg: {} }
            m[key].count += 1
            if (Number(e.doseMG) > 0)
                m[key].mg[e.ester] = (m[key].mg[e.ester] || 0) + Number(e.doseMG)
        }
        return m
    }

    // Columns of days, Monday first, up to today.
    readonly property var columns: {
        const out = []
        const mondayOffset = (today.getDay() + 6) % 7
        const first = new Date(today.getFullYear(), today.getMonth(), today.getDate() - mondayOffset - (weeks - 1) * 7)
        for (let col = 0; col < weeks; ++col) {
            const days = []
            for (let row = 0; row < 7; ++row) {
                const date = new Date(first.getFullYear(), first.getMonth(), first.getDate() + col * 7 + row)
                if (date.getTime() > today.getTime()) continue
                const key = dayKey(date)
                const rec = byDay[key]
                days.push({ col: col, row: row, date: date, key: key,
                            count: rec ? rec.count : 0, mg: rec ? rec.mg : null })
            }
            out.push(days)
        }
        return out
    }

    readonly property var stats: {
        let total = 0, days = 0, busiest = 0
        for (let c = 0; c < columns.length; ++c)
            for (let r = 0; r < columns[c].length; ++r) {
                total += columns[c][r].count
                days += 1
                busiest = Math.max(busiest, columns[c][r].count)
            }
        return { total: total, days: days, busiest: busiest }
    }

    readonly property var hovered: {
        if (hoverKey === "") return null
        for (let c = 0; c < columns.length; ++c)
            for (let r = 0; r < columns[c].length; ++r)
                if (columns[c][r].key === hoverKey) return columns[c][r]
        return null
    }

    function levelOf(count) {
        const ceiling = Math.max(4, stats.busiest)
        return count <= 0 ? 0 : Math.min(4, Math.ceil(count / ceiling * 4))
    }

    Canvas {
        id: grid
        width: root.width
        height: root.header + 7 * root.cell + 6 * root.gap
        LayoutMirroring.enabled: false

        readonly property var levels: Theme.dark
            ? ["#4d271a", "#924833", "#cc785c", "#f0ad96"]
            : ["#fee7de", "#f0ad96", "#cc785c", "#924833"]
        readonly property color empty: Theme.dark ? "#2c2a26" : "#eeedea"
        readonly property color ring: Theme.dark ? "#df8f74" : "#cc785c"

        function colX(col) { return root.gutter + col * (root.cell + root.gap) }
        function rowY(row) { return root.header + row * (root.cell + root.gap) }

        function roundRect(ctx, x, y, w, h, r) {
            ctx.beginPath()
            ctx.moveTo(x + r, y)
            ctx.lineTo(x + w - r, y)
            ctx.arcTo(x + w, y, x + w, y + r, r)
            ctx.lineTo(x + w, y + h - r)
            ctx.arcTo(x + w, y + h, x + w - r, y + h, r)
            ctx.lineTo(x + r, y + h)
            ctx.arcTo(x, y + h, x, y + h - r, r)
            ctx.lineTo(x, y + r)
            ctx.arcTo(x, y, x + r, y, r)
            ctx.closePath()
        }

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const cols = root.columns
            const cell = root.cell
            ctx.font = "13px " + Theme.canvasFont
            ctx.fillStyle = Theme.chartAxis

            // A month's name over the week it starts in, kept apart.
            let last = -99, prev = -1
            ctx.textAlign = "left"
            ctx.textBaseline = "alphabetic"
            for (let c = 0; c < cols.length; ++c) {
                const monday = cols[c][0] ? cols[c][0].date : null
                if (!monday) continue
                const end = new Date(monday.getFullYear(), monday.getMonth(), monday.getDate() + 6)
                if (end.getMonth() !== prev) {
                    prev = end.getMonth()
                    if (c > 0 && c - last >= 3 && c <= cols.length - 2) {
                        ctx.fillText(T.locale.standaloneMonthName(end.getMonth(), Locale.ShortFormat), colX(c), root.header - 5)
                        last = c
                    }
                }
            }

            ctx.textAlign = "right"
            for (const row of [0, 2, 4])
                ctx.fillText(T.locale.dayName((row + 1) % 7, Locale.NarrowFormat), root.gutter - 6, rowY(row) + cell / 2 + 4)

            const todayKey = root.dayKey(root.today)
            for (let c = 0; c < cols.length; ++c) {
                for (let r = 0; r < cols[c].length; ++r) {
                    const d = cols[c][r]
                    const level = root.levelOf(d.count)
                    ctx.globalAlpha = level > 0 ? 1 : 0.85
                    ctx.fillStyle = level === 0 ? empty : levels[level - 1]
                    roundRect(ctx, colX(d.col), rowY(d.row), cell, cell, Math.max(2, cell * 0.22))
                    ctx.fill()
                    ctx.globalAlpha = 1
                    if (d.key === todayKey || d.key === root.hoverKey) {
                        ctx.lineWidth = 1
                        ctx.strokeStyle = ring
                        roundRect(ctx, colX(d.col) + 0.5, rowY(d.row) + 0.5, cell - 1, cell - 1, Math.max(2, cell * 0.22))
                        ctx.stroke()
                    }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onExited: root.hoverKey = ""
            onPositionChanged: function(mouse) {
                const step = root.cell + root.gap
                const px = mouse.x - root.gutter
                const py = mouse.y - root.header
                const col = Math.floor(px / step)
                const row = Math.floor(py / step)
                let key = ""
                if (col >= 0 && col < root.columns.length && row >= 0 && row <= 6
                        && px - col * step <= root.cell && py - row * step <= root.cell) {
                    const day = root.columns[col].find(function(d) { return d.row === row })
                    key = day ? day.key : ""
                }
                root.hoverKey = key
            }
        }

        Connections {
            target: root
            function onColumnsChanged() { grid.requestPaint() }
            function onHoverKeyChanged() { grid.requestPaint() }
            function onCellChanged() { grid.requestPaint() }
        }
        Connections {
            target: Theme
            function onDarkChanged() { grid.requestPaint() }
        }
        Connections {
            target: Ui
            function onLanguageChanged() { grid.requestPaint() }
        }
    }

    // What the grid holds, or the day pointed at.
    UiText {
        id: caption
        y: grid.height + 8
        width: root.width
        size: Theme.caption
        color: Theme.textMuted
        elide: Text.ElideRight
        text: {
            const h = root.hovered
            if (!h) {
                return root.stats.total === 1
                    ? T.fill(T.t("heatmap.summary_one"), [root.stats.days])
                    : T.fill(T.t("heatmap.summary"), [root.stats.total, root.stats.days])
            }
            let line = T.day(h.date) + ": " + (h.count === 0 ? T.t("heatmap.none")
                : h.count === 1 ? T.t("heatmap.dose_one") : T.fill(T.t("heatmap.doses"), [h.count]))
            if (h.mg) {
                const parts = []
                for (const ester in h.mg)
                    parts.push(T.fill(T.t("dose.delete_amount"), [Number(h.mg[ester].toFixed(2)), T.t("ester." + ester)]))
                if (parts.length)
                    line += ". " + parts.join(", ")
            }
            return line
        }
    }
}
