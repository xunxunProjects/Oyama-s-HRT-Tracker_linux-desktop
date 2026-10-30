import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1200
    height: 800
    minimumWidth: 760
    minimumHeight: 560
    visible: true
    title: T.t("app.name")
    color: Theme.bg
    LayoutMirroring.enabled: T.rtl
    LayoutMirroring.childrenInherit: true

    // 0 Overview, 1 History, 2 Settings. A tab chosen in the sidebar is
    // simply there; only a page reached from another one moves in.
    property int pageIndex: 0
    // The intro runs once, before the app, until it is finished: choosing a
    // language on its first step must not end it.
    property bool firstRun: false

    Component.onCompleted: {
        firstRun = Ui.needsLanguage
        if (Ui.mode !== "")
            app.mode = Ui.mode
    }

    Connections {
        target: app
        function onModeChanged() { Ui.mode = app.mode }
    }

    // Where the app opens, for screenshots and tests: overview, history,
    // add, history/edit, history/delete, settings, settings/<page> or
    // first-run.
    function openRoute(name) {
        const parts = String(name).split("/")
        if (parts[0] === "first-run") {
            firstRun = true
            return
        }
        firstRun = false
        if (parts[0] === "history" || parts[0] === "add") {
            pageIndex = 1
            if (parts[0] === "add")
                historyPage.openAdd()
            else if (parts.length > 1)
                historyPage.openFirst(parts[1] === "delete")
        } else if (parts[0] === "settings") {
            pageIndex = 2
            settingsPage.open(parts.length > 1 ? parts[1] : "")
        } else {
            pageIndex = 0
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        visible: !window.firstRun

        Sidebar {
            Layout.fillHeight: true
            currentIndex: window.pageIndex
            onNavigate: function(index) {
                if (index === window.pageIndex && index === 2)
                    settingsPage.open("")
                window.pageIndex = index
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.pageIndex

            OverviewPage {
                id: overviewPage
                onLogRequested: {
                    window.pageIndex = 1
                    historyPage.openAdd()
                }
            }

            HistoryPage {
                id: historyPage
            }

            SettingsPage {
                id: settingsPage
                onIntroRequested: window.firstRun = true
            }
        }
    }

    FirstRun {
        anchors.fill: parent
        visible: window.firstRun
        onFinished: window.firstRun = false
    }
}
