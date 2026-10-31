import QtQuick

// A reading: a large light number and its unit after it on the same baseline.
// "--" when there is nothing to read.
Item {
    id: root
    property real value: 0
    property int decimals: 1
    property string unit: ""
    property int size: Theme.reading

    implicitWidth: number.implicitWidth + (value > 0 ? 8 + unitText.implicitWidth : 0)
    implicitHeight: number.implicitHeight
    width: implicitWidth
    height: implicitHeight
    // Numbers read left to right whatever the page direction.
    LayoutMirroring.enabled: false
    LayoutMirroring.childrenInherit: true

    Text {
        id: number
        text: root.value > 0 ? root.value.toFixed(root.decimals) : "--"
        color: root.value > 0 ? Theme.text : Theme.textDisabled
        font.family: Theme.fontFamily
        font.pixelSize: root.size
        font.weight: Font.Light
        font.letterSpacing: -0.035 * root.size
        textFormat: Text.PlainText
    }

    Text {
        id: unitText
        visible: root.value > 0
        anchors.left: number.right
        anchors.leftMargin: 8
        anchors.baseline: number.baseline
        text: root.unit
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.body
        textFormat: Text.PlainText
    }
}
