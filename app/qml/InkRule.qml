import QtQuick
import QtQuick.Shapes

// The line under a field: a hand-drawn rule, one gentle wave stretched to the
// field, and, while `inked`, a darker stroke of the same wave drawn over it
// from left to right. The stroke slides in rather than growing, as on the web,
// so the line never blinks while it is drawn.
Item {
    id: root
    property bool inked: false
    implicitHeight: 6
    clip: true
    // The software renderer does not clip drawn shapes, so a scrolled or
    // folded drawing would show outside its view. There it is drawn into a
    // texture first, which is clipped like any image.
    layer.enabled: GraphicsInfo.api === GraphicsInfo.Software
    layer.smooth: true

    // The web's rule, M1 3.4C42 2.5 86 4 132 3.2s96-1 140 .2c17 .5 32 .2 47-.4
    // on a 320 by 6 box, in absolute points with x scaled to the width.
    function wave(w) {
        const k = w / 320
        function x(v) { return (v * k).toFixed(2) }
        return "M" + x(1) + " 3.4C" + x(42) + " 2.5 " + x(86) + " 4 " + x(132) + " 3.2"
             + "C" + x(178) + " 2.4 " + x(228) + " 2.2 " + x(272) + " 3.4"
             + "C" + x(289) + " 3.9 " + x(304) + " 3.6 " + x(319) + " 3"
    }

    Shape {
        anchors.fill: parent
        ShapePath {
            strokeColor: Theme.inkRule
            strokeWidth: 1.2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: root.wave(root.width) }
        }
    }

    Shape {
        width: root.width
        height: root.height
        x: root.inked ? 0 : -root.width
        Behavior on x {
            NumberAnimation { duration: Theme.durationInk; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeInk }
        }
        ShapePath {
            strokeColor: Theme.inkRun
            strokeWidth: 1.75
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathSvg { path: root.wave(root.width) }
        }
    }
}
