import QtQuick
import QtQuick.Controls

// A thin scroll bar that keeps out of the way until the page moves or the
// pointer is near it.
ScrollBar {
    id: root
    policy: ScrollBar.AsNeeded
    implicitWidth: 10
    padding: 2
    hoverEnabled: true

    contentItem: Rectangle {
        implicitWidth: 6
        radius: 3
        color: Theme.borderStrong
        opacity: root.pressed ? 0.7 : (root.hovered || root.active ? 0.45 : 0)
        Behavior on opacity {
            NumberAnimation { duration: Theme.durationBase }
        }
    }

    background: Item {}
}
