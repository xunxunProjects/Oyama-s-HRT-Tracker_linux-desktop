import QtQuick
import QtQuick.Layouts

// With a mouse the sidebar rests as a strip of drawings and opens over the
// page while it is pointed at, or while a key has moved focus into it. The
// strip only holds its room in the layout; the panel inside it is what
// widens, so the page never reflows under the pointer.
Item {
    id: root
    property int currentIndex: 0
    signal navigate(int index)

    readonly property bool opened: hover.hovered || panel.activeFocus || focusInside
    property bool focusInside: false

    Layout.preferredWidth: Theme.railWidth
    Layout.fillHeight: true
    z: 40

    HoverHandler { id: hover }

    // Anchored rather than placed, so in a right-to-left language the strip
    // sits at the right edge and opens leftwards over the page.
    Rectangle {
        id: panel
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.opened ? Theme.sidebarWidth : Theme.railWidth
        color: Theme.bgSubtle
        clip: true
        Behavior on width {
            NumberAnimation { duration: Theme.durationSlow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
        }

        Rectangle {
            anchors.right: parent.right
            width: Theme.hairline
            height: parent.height
            color: Theme.border
        }

        ColumnLayout {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Theme.sidebarWidth
            spacing: 0

            // The app icon in a rounded tile, then the name. The tile is the
            // drawings' size and centred over them, so it stays in line with
            // them in the narrow strip too.
            Row {
                Layout.leftMargin: 22
                Layout.topMargin: 24
                Layout.bottomMargin: 20
                spacing: 14

                Image {
                    width: 36
                    height: 36
                    source: "../assets/app-icon.png"
                    smooth: true
                    mipmap: true
                }

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    opacity: root.opened ? 1 : 0
                    Behavior on opacity {
                        NumberAnimation { duration: Theme.durationBase; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
                    }

                    UiText {
                        text: "Oyama"
                        font.weight: Font.DemiBold
                        font.letterSpacing: -0.3
                    }
                    UiText {
                        text: "Tracker"
                        color: Theme.textMuted
                        font.letterSpacing: -0.3
                    }
                }
            }

            Repeater {
                model: [
                    { key: "nav.home", doodle: "house" },
                    { key: "nav.history", doodle: "notebook" },
                    { key: "nav.settings", doodle: "gear" }
                ]

                delegate: NavItem {
                    required property int index
                    required property var modelData
                    Layout.leftMargin: 12
                    Layout.bottomMargin: 4
                    Layout.preferredWidth: (root.opened ? Theme.sidebarWidth : Theme.railWidth) - 24
                    text: T.t(modelData.key)
                    doodle: modelData.doodle
                    showLabel: root.opened
                    selected: root.currentIndex === index
                    onClicked: root.navigate(index)
                    onActiveFocusChanged: root.focusInside = activeFocus && focusReason === Qt.TabFocusReason
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
