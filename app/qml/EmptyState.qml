import QtQuick
import QtQuick.Layouts

Item {
    id: root
    property string iconName: ""
    property string title: ""
    property string body: ""
    property string actionText: ""
    property bool actionEnabled: true
    signal actionClicked()

    implicitWidth: 360
    implicitHeight: content.implicitHeight

    ColumnLayout {
        id: content
        anchors.centerIn: parent
        width: Math.min(parent.width, 420)
        spacing: 14

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 64
            height: 64
            radius: 32
            color: Theme.accentSubtle

            Icon {
                anchors.centerIn: parent
                name: root.iconName
                size: 40
                color: Theme.accentInk
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.title
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSection
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: text !== ""
        }

        Text {
            Layout.fillWidth: true
            Layout.maximumWidth: 360
            Layout.alignment: Qt.AlignHCenter
            text: root.body
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: text !== ""
        }

        AppButton {
            Layout.alignment: Qt.AlignHCenter
            visible: root.actionText !== ""
            text: root.actionText
            enabled: root.actionEnabled
            onClicked: root.actionClicked()
        }
    }
}
