import QtQuick
import QtQuick.Layouts

// A rule between things, half a pixel where the screen is sharp enough.
Rectangle {
    Layout.fillWidth: true
    implicitHeight: Theme.hairline
    Layout.preferredHeight: Theme.hairline
    color: Theme.border
}
