import QtQuick
import QtQuick.Shapes
import "Drawings.js" as Drawings

// The web app's larger drawings (Drawings.js): one per route a dose can take,
// the sidebar's pages and the empty overview's capsule. One line colour and a
// flat wash; the injection syringe alone has volume. `asleep` greys one out.
Item {
    id: root
    property string name: ""
    // Rendered width; the height follows the drawing's own proportions.
    property real size: 36
    property bool asleep: false

    readonly property var drawing: Drawings.doodles[name] || { box: [40, 40], strokes: [] }
    readonly property real scaleBy: size / drawing.box[0]
    implicitWidth: size
    implicitHeight: drawing.box[1] * scaleBy
    opacity: asleep ? 0.45 : 1
    // The software renderer does not clip drawn shapes, so a scrolled or
    // folded drawing would show outside its view. There it is drawn into a
    // texture first, which is clipped like any image.
    layer.enabled: GraphicsInfo.api === GraphicsInfo.Software
    layer.smooth: true

    function paint(key) {
        if (root.asleep) {
            if (key === "line") return Theme.textMuted
            if (key === "shine") return Theme.surface
            return Theme.surfaceHover
        }
        switch (key) {
        case "line": return Theme.illustrationLine
        case "shine": return Theme.illustrationShine
        case "wash": return Theme.illustrationWash
        case "shade": return Theme.illustrationShade
        case "deep": return Theme.illustrationDeep
        }
        return "transparent"
    }

    Repeater {
        model: root.drawing.strokes

        delegate: Shape {
            id: stroke
            required property var modelData
            readonly property var m: modelData.m || [1, 0, 0, 1, 0, 0]
            readonly property real k: root.scaleBy
            width: root.drawing.box[0]
            height: root.drawing.box[1]
            transform: Matrix4x4 {
                matrix: Qt.matrix4x4(stroke.k * stroke.m[0], stroke.k * stroke.m[2], 0, stroke.k * stroke.m[4],
                                     stroke.k * stroke.m[1], stroke.k * stroke.m[3], 0, stroke.k * stroke.m[5],
                                     0, 0, 1, 0,
                                     0, 0, 0, 1)
            }

            ShapePath {
                strokeColor: modelData.s ? root.paint(modelData.s) : "transparent"
                strokeWidth: modelData.w || 1.9
                fillColor: modelData.f ? root.paint(modelData.f) : "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: modelData.d }
            }
        }
    }
}
