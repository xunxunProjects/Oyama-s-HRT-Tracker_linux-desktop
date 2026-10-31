import QtQuick
import QtQuick.Controls

// A field is a line, not a box: the text sits on a hand-drawn rule, and focus
// draws an ink stroke over it. No glow, no coloured ring, no red outline; what
// is wrong goes in words under the field.
TextField {
    id: root
    implicitHeight: Theme.fieldHeight
    leftPadding: 0
    rightPadding: 0
    topPadding: 10
    bottomPadding: 8
    color: enabled ? Theme.text : Theme.textMuted
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.accentSubtleHover
    selectedTextColor: Theme.text
    font.pixelSize: Theme.body
    font.family: Theme.fontFamily
    horizontalAlignment: Text.AlignLeft

    background: Item {
        implicitWidth: 120
        implicitHeight: Theme.fieldHeight

        InkRule {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 6
            inked: root.activeFocus
        }
    }
}
