import QtQuick
import QtQuick.Shapes

// The tick by a chosen thing, written short leg first as it appears.
Item {
    id: root
    property bool on: false
    property real size: 16
    property color color: Theme.accentInk
    property real strokeWidth: 2
    implicitWidth: size
    implicitHeight: size

    property real drawn: on ? 1 : 0
    Behavior on drawn {
        NumberAnimation { duration: Theme.durationBase; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
    }

    // The check glyph, M5.3 12.7c1.5 1.3 2.9 2.8 4.2 4.4 2.9-3.7 6-7.2 9.3-10.5,
    // revealed left to right so the short leg comes first.
    Item {
        width: root.size * root.drawn
        height: root.size
        clip: true
        visible: root.drawn > 0

        Icon {
            name: "check"
            size: root.size
            color: root.color
            strokeWidth: root.strokeWidth
        }
    }
}
