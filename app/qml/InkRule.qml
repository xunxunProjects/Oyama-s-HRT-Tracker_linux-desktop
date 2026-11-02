import QtQuick

// The line under a field: a straight 1px rule and, while `inked`, a 1.5px
// stroke of darker ink drawn over it from left to right, as on the web. The
// stroke slides in rather than growing, so the line never blinks while it is
// drawn.
Item {
    id: root
    property bool inked: false
    // A page with its own ground (the panic screen's grey) gives its own ink.
    property color ruleColor: Theme.inkRule
    property color inkColor: Theme.inkRun
    implicitHeight: 6
    clip: true

    Rectangle {
        y: 3
        width: root.width
        height: 1
        color: root.ruleColor
    }

    Rectangle {
        y: 2.5
        width: root.width
        height: 1.5
        x: root.inked ? 0 : -root.width
        color: root.inkColor
        Behavior on x {
            NumberAnimation { duration: Theme.durationInk; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeInk }
        }
    }
}
