import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Settings: General and About, each a few runs of hairline rows under short
// headings. A row that changes something opens its own page, with the way
// back in its title; the new page is put down a little to the side and
// settles, and is never faded through the old one.
Item {
    id: page
    property string sub: ""
    property string tab: "general"
    signal introRequested()

    readonly property var pages: ({
        "mode": "ModeSettings.qml",
        "weight": "WeightSettings.qml",
        "age": "AgeSettings.qml",
        "calibration": "CalibrationSettings.qml",
        "params": "ParamsSettings.qml",
        "language": "LanguageSettings.qml",
        "appearance": "AppearanceSettings.qml",
        "disclaimer": "DisclaimerPage.qml"
    })

    property real shift: 0
    NumberAnimation {
        id: settle
        target: page
        property: "shift"
        to: 0
        duration: Theme.durationPage
        easing.type: Easing.BezierSpline
        easing.bezierCurve: Theme.easePage
    }

    function open(name) {
        if (name === "about") {
            tab = "about"
            name = ""
        }
        if (name !== "" && !pages[name])
            name = ""
        if (name === sub)
            return
        sub = name
        shift = T.rtl ? -24 : 24
        settle.restart()
    }

    // Writes one or more settings, keeping the rest as they are. Returns ""
    // or what went wrong, in words.
    function saveWith(changes) {
        if (app.backend === "none")
            return T.t("db.cta_disabled")
        const value = function(key, current) {
            return changes[key] !== undefined ? changes[key] : current
        }
        const ok = app.saveSettings(value("weight", app.weight),
                                    value("birthYear", app.birthYear),
                                    value("method", app.calibrationMethod),
                                    value("history", app.calibrationHistory),
                                    value("params", app.pkParamsJson || "{}"))
        if (ok)
            return ""
        return app.statusKey ? T.t(app.statusKey, app.statusArgs) : app.status
    }

    function languageName(code) {
        const list = Ui.availableLanguages
        for (let i = 0; i < list.length; ++i)
            if (list[i].code === code)
                return list[i].nativeName
        return code
    }

    PageColumn {
        anchors.fill: parent
        visible: page.sub === ""
        transform: Translate { x: page.sub === "" ? page.shift : 0 }

        PageHeader {
            title: T.t("nav.settings")
        }

        Tabs {
            options: [
                { value: "general", label: T.t("settings.group.general") },
                { value: "about", label: T.t("settings.group.about") }
            ]
            currentValue: page.tab
            onPicked: function(value) { page.tab = value }
        }

        // General.
        ColumnLayout {
            Layout.fillWidth: true
            visible: page.tab === "general"
            spacing: 0

            GroupHeading { text: T.t("settings.section.model") }
            SettingsRow {
                title: T.t("settings.hrt_mode")
                value: T.t("mode." + app.mode)
                onClicked: page.open("mode")
            }
            SettingsRow {
                divider: true
                title: T.t("settings.weight")
                value: T.fill(T.t("unit.kg_value"), [Number(app.weight).toFixed(app.weight % 1 === 0 ? 0 : 1)])
                onClicked: page.open("weight")
            }
            SettingsRow {
                divider: true
                title: T.t("settings.age")
                description: T.t("settings.age_beta_desc")
                value: app.birthYear > 0 ? String(app.birthYear) : T.t("settings.age_unset")
                onClicked: page.open("age")
            }
            SettingsRow {
                divider: true
                title: T.t("settings.calibration")
                value: T.t("cal." + app.calibrationMethod)
                onClicked: page.open("calibration")
            }
            SettingsRow {
                divider: true
                title: T.t("settings.pk_params")
                value: app.pkParamsJson && app.pkParamsJson !== "{}" ? T.t("pk.customized") : ""
                onClicked: page.open("params")
            }

            GroupHeading { text: T.t("settings.section.display"); Layout.topMargin: 32 }
            SettingsRow {
                title: T.t("settings.language")
                value: page.languageName(Ui.language)
                onClicked: page.open("language")
            }
            SettingsRow {
                divider: true
                title: T.t("settings.theme")
                value: T.t("theme." + Ui.theme)
                onClicked: page.open("appearance")
            }
        }

        // About.
        ColumnLayout {
            Layout.fillWidth: true
            visible: page.tab === "about"
            spacing: 0

            GroupHeading { text: T.t("settings.section.about") }
            SettingsRow {
                clickable: false
                title: T.t("settings.version")
                value: Qt.application.version
            }
            SettingsRow {
                divider: true
                clickable: false
                title: T.t("settings.database")
                description: app.backend === "none" ? T.t("db.not_set_up") : T.t("settings.database_local")
                value: app.backend === "none" ? T.t("settings.database_none") : T.t("settings.database_ok")
            }
            SettingsRow {
                divider: true
                title: T.t("drawer.github")
                onClicked: Qt.openUrlExternally("https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop")
            }
            SettingsRow {
                divider: true
                title: T.t("disclaimer.title")
                onClicked: page.open("disclaimer")
            }
            SettingsRow {
                divider: true
                title: T.t("settings.show_intro")
                onClicked: page.introRequested()
            }
        }
    }

    Loader {
        id: subPage
        anchors.fill: parent
        active: page.sub !== ""
        source: page.sub !== "" ? page.pages[page.sub] : ""
        transform: Translate { x: page.shift }
        onLoaded: item.settings = page
    }
}
