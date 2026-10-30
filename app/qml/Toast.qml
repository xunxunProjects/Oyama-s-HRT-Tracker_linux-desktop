import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string message: ""
    property bool isError: false
    property bool shown: false
    implicitWidth: bubble.width
    implicitHeight: bubble.height + 8
    visible: opacity > 0
    opacity: shown ? 1 : 0
    Accessible.name: message

    function show(message, isError) {
        root.message = message
        root.isError = isError === true
        root.shown = true
        hideTimer.restart()
    }

    function hide() {
        root.shown = false
    }

    Behavior on opacity {
        NumberAnimation {
            duration: root.shown ? 220 : 160
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        id: bubble
        width: Math.min(row.implicitWidth + 32, 420)
        height: row.implicitHeight + 24
        y: root.shown ? 0 : 8
        color: Theme.surfaceAlt
        radius: Theme.radiusControl
        border.width: 1
        border.color: Theme.border

        Behavior on y {
            NumberAnimation {
                duration: root.shown ? 220 : 160
                easing.type: Easing.OutCubic
            }
        }

        RowLayout {
            id: row
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            spacing: 8

            Icon {
                name: root.isError ? "alert" : "check"
                size: 18
                color: root.isError ? Theme.brick : Theme.green
            }

            Text {
                Layout.fillWidth: true
                text: root.message
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLabel
                wrapMode: Text.Wrap
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 4000
        onTriggered: root.hide()
    }
}
