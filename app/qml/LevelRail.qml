import QtQuick

// "You are here" on a fine tick scale, read like an instrument rather than a
// progress bar: the target band is the run of ticks tinted terracotta, the
// ticks up to the current reading are inked in, and the reading itself is the
// one tall tick in the status colour with the status flown from its top.
// Log-spaced, because hormone levels are read in ratios.
Item {
    id: root
    property real value: 0
    property var band: ({ low: 100, high: 200 })
    property var domain: [10, 1000]
    property string label: ""
    property color tone: Theme.statusNeutral

    // A scale reads left to right whatever the page direction.
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true
    implicitHeight: 20 + 18 + 18

    readonly property int count: Math.min(96, Math.max(24, Math.round(width / 7)))
    readonly property int last: count - 1
    function at(v) {
        const lo = Math.log(domain[0]) / Math.LN10
        const hi = Math.log(domain[1]) / Math.LN10
        const x = (Math.log(Math.max(v, domain[0])) / Math.LN10 - lo) / (hi - lo)
        return Math.min(1, Math.max(0, x))
    }
    readonly property int cur: Math.round(at(value) * last)
    readonly property int bandL: Math.ceil(at(band.low) * last)
    readonly property int bandR: Math.floor(at(band.high) * last)
    // Centre of tick i: each tick sits in a 2px slot spread edge to edge.
    function xAt(i) { return 1 + (i / last) * (width - 2) }

    UiText {
        visible: root.label !== ""
        y: 0
        x: Math.max(0, Math.min(root.width - width, root.xAt(root.cur) - width * root.cur / root.last))
        text: root.label
        size: Theme.caption
        lineHeight: 13
        color: root.tone
        font.weight: Font.DemiBold
    }

    Repeater {
        model: root.count

        delegate: Rectangle {
            required property int index
            readonly property bool current: index === root.cur
            readonly property bool inBand: index >= root.bandL && index <= root.bandR
            readonly property bool reached: index <= root.cur
            width: current ? 3 : 1.5
            height: current ? 18 : (inBand ? 11 : 8)
            radius: width / 2
            x: root.xAt(index) - width / 2
            y: 20 + 18 - height
            color: current ? root.tone
                 : inBand ? (reached ? Theme.accentInk : Theme.alpha(Theme.accentInk, 0.4))
                 : (reached ? Theme.textMuted : Theme.border)
        }
    }

    Repeater {
        model: [[root.bandL, root.band.low], [root.bandR, root.band.high]]

        delegate: UiText {
            required property var modelData
            x: root.xAt(modelData[0]) - width / 2
            y: 20 + 18 + 5
            text: String(modelData[1])
            size: Theme.caption
            lineHeight: 13
            color: Theme.textMuted
            font.weight: Font.Medium
        }
    }
}
