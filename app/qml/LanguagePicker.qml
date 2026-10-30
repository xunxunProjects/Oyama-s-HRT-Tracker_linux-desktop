import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.bg
    z: 50
    opacity: Ui.needsLanguage ? 1 : 0
    visible: opacity > 0
    Behavior on opacity {
        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 720)
        spacing: 24

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: 56
            implicitHeight: 56
            radius: 16
            color: Theme.accentSubtle

            Text {
                anchors.centerIn: parent
                text: "O"
                color: Theme.accentInk
                font.family: Theme.fontFamily
                font.pixelSize: 28
                font.weight: Font.DemiBold
            }
        }

        Text {
            Layout.fillWidth: true
            text: T.t("language.choose_title")
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontTitle
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            text: T.t("language.choose_subtitle")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            horizontalAlignment: Text.AlignHCenter
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            rowSpacing: 12
            columnSpacing: 12

            Repeater {
                model: Ui.availableLanguages

                delegate: AppButton {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 88
                    variant: "secondary"
                    Accessible.name: modelData.nativeName
                    onClicked: Ui.language = modelData.code

                    contentItem: ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 2

                        Text {
                            Layout.fillWidth: true
                            text: modelData.nativeName
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.englishName
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: modelData.sample
                            color: Theme.accentInk
                            font.family: Theme.fontFamily
                            font.pixelSize: 28
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }
        }
    }
}
