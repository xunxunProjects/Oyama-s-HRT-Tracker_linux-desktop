import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1200
    height: 800
    minimumWidth: 980
    minimumHeight: 680
    visible: true
    title: T.t("app.name")
    color: Theme.bg
    LayoutMirroring.enabled: T.rtl
    LayoutMirroring.childrenInherit: true

    property int pageIndex: 0
    property var editingDose: null
    property real pageOpacity: 1
    property real pageOffset: 0

    function goTo(index) {
        if (index === pageIndex) return
        pageOpacity = 0
        pageOffset = 8
        pageIndex = index
        pageAnim.restart()
    }

    ParallelAnimation {
        id: pageAnim
        NumberAnimation { target: window; property: "pageOpacity"; from: 0; to: 1; duration: Theme.durationPage; easing.type: Easing.OutCubic }
        NumberAnimation { target: window; property: "pageOffset"; from: 8; to: 0; duration: Theme.durationPage; easing.type: Easing.OutCubic }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Sidebar {
            Layout.fillHeight: true
            currentIndex: window.pageIndex
            onNavigate: function(index) {
                if (index === 1) addPage.resetForm()
                window.goTo(index)
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                Rectangle {
                    visible: app.backend === "none"
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.pageMargin
                    Layout.rightMargin: Theme.pageMargin
                    Layout.topMargin: 20
                    height: 48
                    radius: Theme.radiusControl
                    color: Theme.accentSubtle
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        Icon { name: "database"; size: 18; color: Theme.accentInk }
                        Text {
                            Layout.fillWidth: true
                            text: T.t("db.banner")
                            color: Theme.accentInk
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            wrapMode: Text.Wrap
                        }
                    }
                }
                StackLayout {
                    currentIndex: window.pageIndex
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    opacity: window.pageOpacity
                    transform: Translate { y: window.pageOffset }
                    OverviewPage {
                        id: overviewPage
                        onViewAllRequested: window.goTo(2)
                        onAddRequested: window.goTo(1)
                    }
                    AddDosePage {
                        id: addPage
                        onSaved: {
                            window.editingDose = null
                            window.goTo(0)
                        }
                    }
                    HistoryPage {
                        onEditRequested: function(dose) {
                            window.editingDose = dose
                            addPage.loadDose(dose)
                            window.goTo(1)
                        }
                    }
                    SettingsPage {}
                }
            }
        }
    }

    LanguagePicker { anchors.fill: parent }
    Toast {
        id: toast
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
    }
}
