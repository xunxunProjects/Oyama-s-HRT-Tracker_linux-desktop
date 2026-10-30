import QtQuick

// The hormone level timeline (ResultChart upstream). With cyproterone beside
// estradiol the chart splits in two, estradiol above and cyproterone below,
// each on its own axis and each named above itself. A quiet wash marks the
// target range; "Now" is a dotted line with the current reading on it, and
// each logged dose is a hollow ring on its curve. The line draws itself on
// once per run of the app.
Canvas {
    id: chart
    property var curve: []
    property var doses: []
    property bool transmasc: false
    property bool secondary: false    // the cyproterone plot
    property string range: "7d"
    property real nowH: 0
    property string primaryLabel: ""
    property string secondaryLabel: ""
    property string nowLabel: ""
    property string targetLabel: ""
    property real sweep: 1
    property bool hoverActive: false
    property real hoverT: 0
    property real hoverX: -1
    property real windowStart: 0
    property real windowEnd: 1
    property real plotLeft: 0
    property real plotRight: 0
    readonly property string primaryKey: transmasc ? "tLevel" : "e2"
    readonly property bool loading: sweepAnim.running

    readonly property int day: 24
    readonly property real cpaMin: 0.1
    // A scale reads left to right whatever the page direction.
    LayoutMirroring.enabled: false

    function niceStep(raw) {
        if (!(raw > 0)) return 1
        const mag = Math.pow(10, Math.floor(Math.log(raw) / Math.LN10))
        const norm = raw / mag
        const nice = norm < 1.5 ? 1 : norm < 3 ? 2 : norm < 7 ? 5 : 10
        return nice * mag
    }

    // A padded, tick-friendly domain around `steps` gridlines that never
    // spans less than `minSpan`, so a washed-out curve isn't blown up.
    function yDomain(min, max, steps, minSpan) {
        if (!isFinite(min) || !isFinite(max) || max <= 0) return [0, Math.max(1, minSpan)]
        if (max - min < minSpan) {
            const mid = (min + max) / 2
            min = Math.max(0, mid - minSpan / 2)
            max = min + minSpan
        }
        if (max === min) max = max + (max * 0.15 || 1)
        const pad = (max - min) * 0.12
        const step = niceStep((max - min + 2 * pad) / steps)
        const lo = Math.max(0, Math.floor((min - pad) / step) * step)
        let hi = Math.ceil((max + pad) / step) * step
        if (hi <= lo) hi = lo + step
        return [lo, hi]
    }

    function ticks(d, steps) {
        const step = niceStep((d[1] - d[0]) / steps)
        const out = []
        for (let v = Math.ceil(d[0] / step - 1e-9) * step; v <= d[1] + step * 0.5; v += step)
            out.push(Math.round(v / step) * step)
        return out
    }

    function fmt(v) {
        if (v >= 100 || v % 1 === 0) return String(Math.round(v))
        return v < 1 ? v.toFixed(2) : v.toFixed(1)
    }

    function primaryOf(p) { return transmasc ? p.tLevel : p.e2 }

    function valueAt(key, t) {
        const pts = curve
        if (!pts || pts.length === 0) return null
        if (t <= pts[0].t) return pts[0][key]
        for (let i = 1; i < pts.length; ++i) {
            if (pts[i].t >= t) {
                const a = pts[i - 1], b = pts[i]
                const u = (t - a.t) / Math.max(1e-9, b.t - a.t)
                return a[key] + (b[key] - a[key]) * u
            }
        }
        return pts[pts.length - 1][key]
    }

    function timeAtX(x) {
        if (!(windowEnd > windowStart) || plotRight <= plotLeft)
            return NaN
        if (x < plotLeft || x > plotRight)
            return NaN
        const f = (x - plotLeft) / (plotRight - plotLeft)
        return windowStart + f * (windowEnd - windowStart)
    }

    function formatHoverTime(t) {
        if (!isFinite(t)) return ""
        return Qt.formatDateTime(new Date(t * 3600000), "yyyy-MM-dd HH:mm")
    }

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const pts = curve
        if (!pts || pts.length < 2) return
        const pKey = transmasc ? "tLevel" : "e2"

        // The window: centred on now for 7d and 30d, everything for All.
        const fullMin = pts[0].t
        const fullMax = pts[pts.length - 1].t
        let t0 = fullMin, t1 = fullMax
        if (range !== "all") {
            const span = (range === "7d" ? 7 : 30) * day
            const centre = Math.min(Math.max(nowH, fullMin), fullMax)
            t0 = centre - span / 2
            t1 = centre + span / 2
            if (t0 < fullMin) { t0 = fullMin; t1 = Math.min(fullMax, t0 + span) }
            if (t1 > fullMax) { t1 = fullMax; t0 = Math.max(fullMin, t1 - span) }
        }
        if (t1 <= t0) t1 = t0 + 1
        windowStart = t0
        windowEnd = t1

        // What the window holds, for the axes.
        let pLo = Infinity, pHi = -Infinity, sLo = Infinity, sHi = -Infinity
        function take(isP, v) {
            if (v === null || !isFinite(v)) return
            if (isP) { pLo = Math.min(pLo, v); pHi = Math.max(pHi, v) }
            else { sLo = Math.min(sLo, v); sHi = Math.max(sHi, v) }
        }
        take(true, valueAt(pKey, t0)); take(true, valueAt(pKey, t1))
        take(false, valueAt("cpa", t0)); take(false, valueAt("cpa", t1))
        for (let i = 0; i < pts.length; ++i) {
            if (pts[i].t < t0 || pts[i].t > t1) continue
            take(true, pts[i][pKey])
            take(false, pts[i].cpa)
        }
        const yP = yDomain(pLo, pHi, 4, transmasc ? 100 : 20)
        const yS = secondary ? yDomain(sLo, sHi, 3, 1) : [0, 1]
        const cpaInView = secondary && sHi >= cpaMin
        const pTicks = ticks(yP, 4)
        const sTicks = cpaInView ? ticks(yS, 3) : []

        const font = 13
        ctx.font = font + "px " + Theme.canvasFont
        let labelW = 0
        const all = pTicks.concat(sTicks)
        for (let i = 0; i < all.length; ++i)
            labelW = Math.max(labelW, ctx.measureText(fmt(all[i])).width)
        const mL = Math.max(32, labelW + 12)
        const mR = mL
        const mT = 32, mB = 26
        const plotW = Math.max(0, width - mL - mR)
        const plotH = Math.max(0, height - mT - mB)
        plotLeft = mL
        plotRight = mL + plotW
        const gapH = secondary ? 42 : 0
        const sH = secondary ? Math.round((plotH - gapH) * 0.36) : 0
        const pH = Math.max(0, plotH - gapH - sH)
        const sTop = mT + pH + gapH

        function X(t) { return mL + (t - t0) / (t1 - t0) * plotW }
        function YP(v) { return mT + (1 - (v - yP[0]) / (yP[1] - yP[0])) * pH }
        function YS(v) { return sTop + (1 - (v - yS[0]) / (yS[1] - yS[0])) * sH }

        // Target band: a quiet wash, its edges dotted.
        const band = transmasc ? { low: 300, high: 1000 } : { low: 100, high: 200 }
        const yHi = Math.max(mT, Math.min(mT + pH, YP(band.high)))
        const yLo = Math.max(mT, Math.min(mT + pH, YP(band.low)))
        if (yLo > yHi) {
            ctx.fillStyle = Theme.alpha(Theme.chartLine, 0.06)
            ctx.fillRect(mL, yHi, plotW, yLo - yHi)
            ctx.save()
            ctx.strokeStyle = Theme.alpha(Theme.textDisabled, 0.6)
            ctx.lineWidth = 1
            ctx.setLineDash([2, 4])
            for (const y of [yHi, yLo]) {
                if (y <= mT + 0.5 || y >= mT + pH - 0.5) continue
                ctx.beginPath(); ctx.moveTo(mL, y); ctx.lineTo(mL + plotW, y); ctx.stroke()
            }
            ctx.restore()
        }

        // Gridlines and the numbers in the left gutter.
        ctx.lineWidth = 1
        ctx.strokeStyle = Theme.chartGrid
        ctx.fillStyle = Theme.chartAxis
        ctx.textAlign = "right"
        ctx.textBaseline = "middle"
        for (let i = 0; i < pTicks.length; ++i) {
            const y = Math.round(YP(pTicks[i])) + 0.5
            if (y < mT - 1 || y > mT + pH + 1) continue
            ctx.beginPath(); ctx.moveTo(mL, y); ctx.lineTo(mL + plotW, y); ctx.stroke()
            ctx.fillText(fmt(pTicks[i]), mL - 12, y)
        }
        if (secondary) {
            const sGrid = cpaInView ? sTicks : [yS[0], yS[1]]
            for (let i = 0; i < sGrid.length; ++i) {
                const y = Math.round(YS(sGrid[i])) + 0.5
                if (y < sTop - 1 || y > sTop + sH + 1) continue
                ctx.beginPath(); ctx.moveTo(mL, y); ctx.lineTo(mL + plotW, y); ctx.stroke()
                if (cpaInView) ctx.fillText(fmt(sGrid[i]), mL - 12, y)
            }
        }

        // Each plot named above itself: a stroke of its line, then its name.
        function seriesName(y, color, weight, text) {
            ctx.save()
            ctx.strokeStyle = color
            ctx.lineWidth = weight
            ctx.lineCap = "round"
            ctx.beginPath(); ctx.moveTo(mL - 12, y - 4); ctx.lineTo(mL + 2, y - 4); ctx.stroke()
            ctx.textAlign = "left"
            ctx.textBaseline = "alphabetic"
            ctx.fillStyle = Theme.chartAxis
            ctx.fillText(text, mL + 8, y)
            ctx.restore()
        }
        seriesName(mT - 14, Theme.chartLine, 2, primaryLabel)
        if (secondary)
            seriesName(sTop - 14, Theme.chartSecond, 1.5, secondaryLabel)

        // Dates along the bottom, on the start of each day, a whole number of
        // days apart so equal gaps are equal stretches of time.
        const spanDays = (t1 - t0) / day
        const steps = [1, 2, 3, 7, 14, 28, 56, 91, 182, 364]
        let step = Math.ceil(spanDays * 64 / Math.max(1, plotW))
        for (let i = 0; i < steps.length; ++i) {
            if (plotW * steps[i] / spanDays >= 64) { step = steps[i]; break }
        }
        ctx.fillStyle = Theme.chartAxis
        ctx.textAlign = "center"
        ctx.textBaseline = "alphabetic"
        const first = new Date(t0 * 3600000)
        first.setHours(0, 0, 0, 0)
        let d = new Date(first.getTime())
        if (d.getTime() / 3600000 < t0) d.setDate(d.getDate() + 1)
        const phase = step % 7 === 0 ? 3 : 0
        const dayNumber = Math.floor((d.getTime() / 3600000 - d.getTimezoneOffset() / 60) / day)
        const ahead = (step - ((dayNumber + phase) % step)) % step
        d.setDate(d.getDate() + ahead)
        for (; d.getTime() / 3600000 <= t1; d.setDate(d.getDate() + step)) {
            const x = X(d.getTime() / 3600000)
            ctx.fillText(T.shortDay(d), x, height - 8)
        }

        // The curves, drawn on left to right the first time they show.
        ctx.save()
        ctx.beginPath()
        ctx.rect(mL, 0, plotW * sweep, height)
        ctx.clip()

        function line(key, Y, top, h, color, weight) {
            ctx.save()
            ctx.beginPath()
            ctx.rect(mL, top - 4, plotW, h + 8)
            ctx.clip()
            ctx.strokeStyle = color
            ctx.lineWidth = weight
            ctx.lineJoin = "round"
            ctx.lineCap = "round"
            ctx.beginPath()
            let started = false
            let prev = null
            for (let i = 0; i < pts.length; ++i) {
                const p = pts[i]
                if (p.t < t0) { prev = p; continue }
                if (!started) {
                    const v0 = prev ? valueAt(key, t0) : p[key]
                    ctx.moveTo(X(prev ? t0 : p.t), Y(v0))
                    started = true
                }
                if (p.t > t1) {
                    ctx.lineTo(X(t1), Y(valueAt(key, t1)))
                    break
                }
                ctx.lineTo(X(p.t), Y(p[key]))
            }
            ctx.stroke()
            ctx.restore()
        }
        line(pKey, YP, mT, pH, Theme.chartLine, 2)
        if (secondary)
            line("cpa", YS, sTop, sH, Theme.chartSecond, 1.5)

        // "Target", over the curves, with a halo in the page colour.
        if (yLo > yHi) {
            ctx.textAlign = "left"
            ctx.textBaseline = "alphabetic"
            ctx.lineWidth = 3
            ctx.lineJoin = "round"
            ctx.strokeStyle = Theme.chartDot
            const ty = Math.min(mT + pH - 3, yHi + 14)
            ctx.strokeText(targetLabel, mL + 4, ty)
            ctx.fillStyle = Theme.chartAxis
            ctx.fillText(targetLabel, mL + 4, ty)
        }
        ctx.restore()

        const nowIn = nowH >= t0 && nowH <= t1
        if (nowIn && sweep >= 1) {
            const nx = Math.round(X(nowH)) + 0.5
            ctx.save()
            ctx.strokeStyle = Theme.chartAxis
            ctx.lineWidth = 1
            ctx.setLineDash([2, 4])
            const spans = secondary ? [[mT, mT + pH], [sTop, sTop + sH]] : [[mT, mT + plotH]]
            for (const s of spans) {
                ctx.beginPath(); ctx.moveTo(nx, s[0]); ctx.lineTo(nx, s[1]); ctx.stroke()
            }
            ctx.restore()
            ctx.fillStyle = Theme.chartAxis
            ctx.textAlign = "center"
            ctx.textBaseline = "alphabetic"
            ctx.fillText(nowLabel, nx, mT - 14)
        }

        function ring(x, y, r, fill, stroke, w) {
            ctx.beginPath()
            ctx.arc(x, y, r, 0, Math.PI * 2)
            ctx.fillStyle = fill
            ctx.fill()
            ctx.lineWidth = w
            ctx.strokeStyle = stroke
            ctx.stroke()
        }

        // Each logged dose, a hollow ring on its own curve.
        if (sweep >= 1) {
            for (let i = 0; i < doses.length; ++i) {
                const e = doses[i]
                const t = Number(e.timeH)
                if (t < t0 || t > t1) continue
                const isCpa = e.ester === "CPA"
                if (isCpa && !secondary) continue
                if (!isCpa && e.route === "patchRemove") continue
                const v = valueAt(isCpa ? "cpa" : pKey, t)
                if (v === null) continue
                ring(X(t), isCpa ? YS(v) : YP(v), 3.5, Theme.chartDot,
                     isCpa ? Theme.chartSecond : Theme.chartLine, 2)
            }
            if (nowIn) {
                if (secondary) {
                    const sv = valueAt("cpa", nowH)
                    if (sv !== null) ring(X(nowH), YS(sv), 4, Theme.chartSecond, Theme.chartDot, 2)
                }
                const pv = valueAt(pKey, nowH)
                if (pv !== null) ring(X(nowH), YP(pv), 5, Theme.chartLine, Theme.chartDot, 2.5)
            }
        }

        if (sweep >= 1 && hoverActive && hoverT >= t0 && hoverT <= t1) {
            const hx = Math.round(X(hoverT)) + 0.5
            ctx.save()
            ctx.strokeStyle = Theme.chartAxis
            ctx.lineWidth = 1
            ctx.setLineDash([2, 4])
            const spans = secondary ? [[mT, mT + pH], [sTop, sTop + sH]]
                                  : [[mT, mT + plotH]]
            for (const s of spans) {
                ctx.beginPath()
                ctx.moveTo(hx, s[0])
                ctx.lineTo(hx, s[1])
                ctx.stroke()
            }
            ctx.restore()

            const hp = valueAt(pKey, hoverT)
            if (hp !== null)
                ring(X(hoverT), YP(hp), 4.5, Theme.chartLine, Theme.chartDot, 2.5)
            if (secondary) {
                const hs = valueAt("cpa", hoverT)
                if (hs !== null)
                    ring(X(hoverT), YS(hs), 3.5, Theme.chartSecond, Theme.chartDot, 2)
            }
        }
    }

    // Once per run of the app, like the web's entrance sweep.
    property bool played: false
    NumberAnimation on sweep {
        id: sweepAnim
        running: false
        from: 0
        to: 1
        duration: Theme.durationSweep
        easing.type: Easing.BezierSpline
        easing.bezierCurve: Theme.easeOut
    }
    onSweepChanged: requestPaint()
    function play() {
        if (played || curve.length < 2) return
        played = true
        sweepAnim.restart()
    }

    onCurveChanged: requestPaint()
    onRangeChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onSecondaryChanged: requestPaint()
    onTransmascChanged: requestPaint()
    onPrimaryLabelChanged: requestPaint()
    onHoverTChanged: requestPaint()
    onHoverActiveChanged: requestPaint()
    Connections {
        target: Theme
        function onDarkChanged() { chart.requestPaint() }
        function onFontFamilyChanged() { chart.requestPaint() }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        cursorShape: Qt.CrossCursor
        onPositionChanged: {
            const t = chart.timeAtX(mouse.x)
            if (isFinite(t)) {
                chart.hoverX = mouse.x
                chart.hoverT = t
                chart.hoverActive = true
            } else {
                chart.hoverActive = false
            }
        }
        onExited: chart.hoverActive = false
    }

    Rectangle {
        id: hoverCard
        visible: chart.hoverActive && chart.hoverT >= chart.windowStart &&
                 chart.hoverT <= chart.windowEnd
        z: 2
        x: Math.min(chart.width - width - 8,
                    Math.max(8, chart.hoverX + 12))
        y: 8
        width: hoverColumn.implicitWidth + 20
        height: hoverColumn.implicitHeight + 14
        radius: Theme.radiusMd
        color: Theme.surfaceMuted
        border.color: Theme.border
        border.width: 1

        Column {
            id: hoverColumn
            anchors.centerIn: parent
            spacing: 2

            Text {
                text: chart.formatHoverTime(chart.hoverT)
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.caption
            }

            Text {
                text: (chart.transmasc ? T.t("label.total_t") : T.t("label.e2")) +
                      " · " + chart.fmt(chart.valueAt(chart.primaryKey, chart.hoverT)) +
                      " " + (chart.transmasc ? "ng/dL" : "pg/mL")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.body
                font.weight: Font.DemiBold
            }

            Text {
                visible: chart.secondary
                text: T.t("label.cpa_chart") + " · " +
                      chart.fmt(chart.valueAt("cpa", chart.hoverT)) + " ng/mL"
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.caption
            }
        }
    }
}
