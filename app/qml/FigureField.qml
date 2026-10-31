import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A number set as a display figure on its own line (body weight, year of
// birth): large light digits centred over a hand-drawn line, the unit after.
RowLayout {
    id: root
    property alias text: input.text
    property alias validator: input.validator
    property alias field: input
    property string unit: ""
    property int figureWidth: 112
    signal edited()
    signal accepted()

    spacing: 8
    LayoutMirroring.enabled: false

    TextField {
        id: input
        Layout.preferredWidth: root.figureWidth
        implicitHeight: 56
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 8
        horizontalAlignment: Text.AlignHCenter
        color: Theme.text
        selectionColor: Theme.accentSubtleHover
        selectedTextColor: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.readingSm
        font.weight: Font.Light
        font.letterSpacing: -0.03 * Theme.readingSm
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        onTextEdited: root.edited()
        onAccepted: root.accepted()

        background: Item {
            InkRule {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 6
                inked: true
            }
        }
    }

    UiText {
        Layout.alignment: Qt.AlignBottom
        Layout.bottomMargin: 10
        visible: root.unit !== ""
        text: root.unit
        color: Theme.textMuted
        font.weight: Font.Medium
    }
}
