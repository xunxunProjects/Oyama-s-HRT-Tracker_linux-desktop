import QtQuick
import QtQuick.Layouts

// Age: the year of birth as one figure, what it changes in words, Save, and
// the way to take it away again.
SubPage {
    id: root
    title: T.t("settings.age")
    property string error: ""
    readonly property int thisYear: new Date().getFullYear()

    function save() {
        const year = parseInt(figure.text)
        if (!(isFinite(year) && year >= 1900 && year <= thisYear)) {
            error = T.fill(T.t("age.range_error"), [1900, thisYear])
            return
        }
        error = settings ? settings.saveWith({ birthYear: year }) : ""
        if (error === "")
            back()
    }

    function remove() {
        error = settings ? settings.saveWith({ birthYear: 0 }) : ""
        if (error === "")
            back()
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: 32
        spacing: 6

        UiText {
            text: T.t("age.birth_year")
            size: Theme.caption
            color: Theme.textMuted
            font.weight: Font.Medium
        }

        FigureField {
            id: figure
            text: app.birthYear > 0 ? String(app.birthYear) : ""
            figureWidth: 136
            validator: IntValidator { bottom: 0; top: 9999 }
            onEdited: root.error = ""
            onAccepted: root.save()
            Component.onCompleted: field.forceActiveFocus()
        }

        UiText {
            readonly property int age: root.thisYear - parseInt(figure.text)
            visible: root.error !== "" || (isFinite(age) && age > 0 && age < 120)
            text: root.error !== "" ? root.error : T.fill(T.t("age.this_year"), [age])
            size: Theme.caption
            color: root.error !== "" ? Theme.danger : Theme.textMuted
        }
    }

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 20
        text: T.fill(T.t("age.explain"), [54])
        color: Theme.textMuted
        wrapMode: Text.Wrap
        lineHeightMode: Text.ProportionalHeight
        lineHeight: 1.15
    }

    Hairline { Layout.topMargin: 20 }

    AppButton {
        Layout.fillWidth: true
        Layout.topMargin: 24
        text: T.t("btn.save")
        enabled: app.backend !== "none"
        onClicked: root.save()
    }

    SettingsRow {
        Layout.topMargin: 12
        visible: app.birthYear > 0
        danger: true
        showChevron: false
        title: T.t("age.remove")
        onClicked: root.remove()
    }
}
