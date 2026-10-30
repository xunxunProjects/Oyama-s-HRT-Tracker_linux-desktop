import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    default property alias content: layout.data
    property int padding: Theme.cardPadding
    property int spacing: 12
    property color fill: Theme.surface
    property bool hoverable: false
    implicitWidth: 320
    color: hoverable && hover.hovered ? Theme.surfaceHover : fill
    radius: Theme.radiusCard
    Behavior on color { ColorAnimation { duration: Theme.durationTheme } }

    HoverHandler {
        id: hover
        enabled: root.hoverable
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: root.padding
        spacing: root.spacing
    }
}
