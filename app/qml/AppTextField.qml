import QtQuick
import QtQuick.Controls

TextField {
    id: root
    implicitHeight: 44
    leftPadding: 14
    rightPadding: 14
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.accent
    selectedTextColor: Theme.onAccent
    font.pixelSize: Theme.fontBody
    font.family: Theme.fontFamily
    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.surfaceAlt
        border.width: root.activeFocus ? 1.5 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: Theme.durationTheme } }
    }
}
