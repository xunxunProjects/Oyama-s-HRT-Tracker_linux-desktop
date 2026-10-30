import QtQuick
import QtQuick.Shapes

Item {
    id: root
    property string name: ""
    property real size: 20
    property color color: Theme.textMuted
    property real strokeWidth: 1.75
    implicitWidth: size
    implicitHeight: size

    readonly property var paths: ({
        "home": "M3 11 12 3l9 8v9a1 1 0 0 1-1 1h-5v-6H9v6H4a1 1 0 0 1-1-1z",
        "plus-circle": "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zM12 8v8M8 12h8",
        "list": "M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01",
        "sliders": "M4 7h10M18 7h2M4 17h2M10 17h10M14 4v6M6 14v6",
        "sun": "M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8zM12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4",
        "moon": "M20 14.5A8.5 8.5 0 0 1 9.5 4a8.5 8.5 0 1 0 10.5 10.5z",
        "globe": "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zM3 12h18M12 3c2.5 2.5 3.5 5.5 3.5 9s-1 6.5-3.5 9c-2.5-2.5-3.5-5.5-3.5-9S9.5 5.5 12 3z",
        "check": "M4 12l5 5L20 6",
        "chevron-right": "M9 6l6 6-6 6",
        "chevron-down": "M6 9l6 6 6-6",
        "pill": "M10.5 20.5a6 6 0 0 1-8.5-8.5l6-6a6 6 0 0 1 8.5 8.5zM8 8l8 8",
        "pencil": "M4 20h4L19 9l-4-4L4 16zM14 6l4 4",
        "trash": "M4 7h16M9 7V4h6v3M6 7l1 14h10l1-14M10 11v6M14 11v6",
        "x": "M6 6l12 12M18 6L6 18",
        "alert": "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zM12 8v5M12 16h.01",
        "info": "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zM12 11v5M12 8h.01",
        "database": "M4 6c0-1.7 3.6-3 8-3s8 1.3 8 3-3.6 3-8 3-8-1.3-8-3zM4 6v6c0 1.7 3.6 3 8 3s8-1.3 8-3V6M4 12v6c0 1.7 3.6 3 8 3s8-1.3 8-3v-6",
        "syringe": "M18 2l4 4M17 3l4 4M19 7l-9 9M14 6l4 4M13 9l-7 7v4h4l7-7",
        "droplet": "M12 3s6 6.5 6 11a6 6 0 0 1-12 0c0-4.5 6-11 6-11z"
    })

    Shape {
        width: 24
        height: 24
        transform: Scale {
            origin.x: 0
            origin.y: 0
            xScale: root.size / 24
            yScale: root.size / 24
        }
        ShapePath {
            strokeColor: root.color
            strokeWidth: root.strokeWidth * 24 / root.size
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.paths[root.name] || "" }
        }
    }
}
