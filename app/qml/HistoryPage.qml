import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    Layout.fillWidth: true
    Layout.fillHeight: true

    property var pendingDelete: null

    signal editRequested(var dose)
    signal addRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pageMargin
        spacing: 24

        PageHeader {
            title: T.t("history.title")
            subtitle: T.t("history.subtitle", [T.t("mode." + app.mode)])
            Layout.fillWidth: true
        }

        Text {
            Layout.fillWidth: true
            text: T.t("history.count", [app.doses.length])
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontLabel
        }

        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: app.doses.length > 0
                clip: true
                spacing: 4
                model: app.doses

                delegate: Rectangle {
                    id: row
                    required property var modelData

                    width: list.width
                    height: 64
                    radius: Theme.radiusControl
                    color: hover.hovered ? Theme.surfaceHover : "transparent"

                    HoverHandler { id: hover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        spacing: 12

                        Rectangle {
                            implicitWidth: 36
                            implicitHeight: 36
                            radius: 18
                            color: Theme.accentSubtle

                            Icon {
                                anchors.centerIn: parent
                                name: "pill"
                                size: 18
                                color: Theme.accentInk
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: T.t("route." + row.modelData.route) + " · " +
                                      T.t("ester." + row.modelData.ester)
                                color: Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontBody
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: row.modelData.timeLabel + " · " +
                                      Number(row.modelData.doseMG).toFixed(2) + " mg"
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontLabel
                            }
                        }

                        AppButton {
                            implicitWidth: 36
                            implicitHeight: 36
                            leftPadding: 0
                            rightPadding: 0
                            variant: "ghost"
                            iconName: "pencil"
                            Accessible.name: T.t("common.edit")
                            onClicked: page.editRequested(row.modelData)
                        }

                        AppButton {
                            implicitWidth: 36
                            implicitHeight: 36
                            leftPadding: 0
                            rightPadding: 0
                            variant: "danger"
                            iconName: "trash"
                            Accessible.name: T.t("common.delete")
                            onClicked: {
                                page.pendingDelete = row.modelData
                                confirm.open()
                            }
                        }
                    }
                }
            }

            EmptyState {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: app.doses.length === 0
                iconName: "pill"
                title: T.t("history.empty_title")
                body: T.t("history.empty_body")
                actionText: T.t("dose.add")
                actionEnabled: app.backend !== "none"
                onActionClicked: page.addRequested()
            }
        }
    }

    ConfirmDialog {
        id: confirm
        anchors.fill: parent
        title: T.t("history.delete_title")
        body: T.t("history.delete_body")
        confirmText: T.t("common.delete")
        onConfirmed: {
            if (!page.pendingDelete)
                return
            const ok = app.removeDose(page.pendingDelete.id)
            toast.show(app.statusKey ? T.t(app.statusKey, app.statusArgs)
                                     : app.status, !ok)
            page.pendingDelete = null
        }
        onCancelled: page.pendingDelete = null
    }

    Toast {
        id: toast
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        z: 60
    }
}
