import QtQuick
import QtQuick.Shapes
import "Drawings.js" as Drawings

// One of the web app's hand-drawn glyphs (Drawings.js), drawn on a 24px grid
// and scaled to `size`, pen and all, as an SVG icon is. 1.75 at rest, 2 for
// the current or chosen thing.
Item {
    id: root
    property string name: ""
    property real size: 24
    property color color: Theme.textMuted
    property real strokeWidth: 1.75
    implicitWidth: size
    implicitHeight: size
    // The software renderer does not clip drawn shapes, so a scrolled or
    // folded drawing would show outside its view. There it is drawn into a
    // texture first, which is clipped like any image.
    layer.enabled: GraphicsInfo.api === GraphicsInfo.Software
    layer.smooth: true

    readonly property var strokes: Drawings.glyphs[name] || []

    Repeater {
        model: root.strokes

        delegate: Shape {
            required property var modelData
            width: 24
            height: 24
            transform: Scale {
                xScale: root.size / 24
                yScale: root.size / 24
            }

            ShapePath {
                readonly property string kind: modelData.k || ""
                strokeColor: kind === "tint" ? "transparent" : root.color
                strokeWidth: kind === "dot" ? root.strokeWidth * 1.5
                           : kind === "thin" ? root.strokeWidth * 0.6
                           : root.strokeWidth
                fillColor: kind === "fill" ? root.color
                         : kind === "tint" ? Theme.alpha(root.color, 0.28)
                         : "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: modelData.d }
            }
        }
    }
}
