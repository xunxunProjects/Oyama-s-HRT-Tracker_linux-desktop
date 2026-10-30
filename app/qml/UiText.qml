import QtQuick

// Text in the app's type: one family, four sizes, the web's line heights.
Text {
    property int size: Theme.body
    color: Theme.text
    font.family: Theme.fontFamily
    font.pixelSize: size
    lineHeightMode: Text.FixedHeight
    lineHeight: Theme.leading(size)
    verticalAlignment: Text.AlignVCenter
    // Set, so a line in another script (فارسی in an English list) keeps to the
    // page's side instead of its own; the page's mirroring still applies.
    horizontalAlignment: Text.AlignLeft
    textFormat: Text.PlainText
}
