import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Doses.js" as Doses

// The dose form, unfolded in place in History: at the top for a new dose, or
// under the row it edits. Time, route, compound, what the route needs, the
// dose, and Save. Editing adds a trash mark whose question is a line under
// the buttons, never a dialog.
ColumnLayout {
    id: form
    property var dose: null
    readonly property bool editing: dose !== null && dose !== undefined
    readonly property bool transmasc: app.mode === "transmasc"
    signal done()

    property date when: new Date()
    property string route: "injection"
    property string ester: "EV"
    property int tier: 2
    property bool customTheta: false
    property int gelSite: 0
    property string error: ""
    property bool timeOpen: false
    property bool asking: false
    // Open with the delete question already asked (screenshots, tests).
    property bool askOnOpen: false

    readonly property var routes: Doses.routesFor(transmasc)
    readonly property var esters: Doses.estersFor(route, transmasc)
    readonly property real factor: Doses.factor(ester)

    spacing: 0

    function drugLabel(code) {
        return T.t("ester." + code)
    }

    // The last route and compound logged, so a new dose opens on the one
    // most likely to be taken again.
    function reset() {
        error = ""
        asking = false
        timeOpen = false
        customTheta = false
        if (editing) {
            const extras = Doses.parseExtras(dose.extras)
            when = new Date(Number(dose.timeH) * 3600000)
            route = dose.route
            ester = dose.ester
            doseField.text = Doses.trim(dose.doseMG)
            tier = Number(extras.sublingualTier !== undefined ? extras.sublingualTier : 2)
            customTheta = extras.sublingualTheta !== undefined
            thetaField.text = customTheta ? String(extras.sublingualTheta) : ""
            gelSite = Number(extras.gelSite !== undefined ? extras.gelSite : 0)
            rateField.text = extras.releaseRateUGPerDay !== undefined ? String(extras.releaseRateUGPerDay) : ""
            wearField.text = extras.patchWearH !== undefined ? Doses.trim(extras.patchWearH / 24) : ""
            return
        }
        when = new Date()
        const routeList = Doses.routesFor(transmasc)
        let last = null
        for (let i = 0; i < app.doses.length; ++i)
            if (!last || Number(app.doses[i].timeH) > Number(last.timeH))
                last = app.doses[i]
        route = last && routeList.indexOf(last.route) >= 0 && last.route !== "patchRemove" ? last.route : routeList[0]
        const esterList = Doses.estersFor(route, transmasc)
        ester = last && esterList.indexOf(last.ester) >= 0 ? last.ester : esterList[0]
        doseField.text = ""
        tier = 2
        thetaField.text = ""
        gelSite = 0
        rateField.text = ""
        wearField.text = ""
    }

    // A route whose list doesn't hold the current drug starts on the drug
    // last logged by that route, then on the list's first.
    onRouteChanged: {
        const list = Doses.estersFor(route, transmasc)
        if (list.indexOf(ester) < 0) {
            let last = null
            for (let i = 0; i < app.doses.length; ++i) {
                const d = app.doses[i]
                if (d.route === route && list.indexOf(d.ester) >= 0
                        && (!last || Number(d.timeH) > Number(last.timeH)))
                    last = d
            }
            ester = last ? last.ester : list[0]
        }
    }
    onTransmascChanged: reset()

    function extras() {
        const out = {}
        if (route === "sublingual") {
            out.sublingualTier = tier
            const theta = parseFloat(thetaField.text)
            if (customTheta && isFinite(theta) && theta > 0 && theta <= 1)
                out.sublingualTheta = theta
        } else if (route === "gel") {
            out.gelSite = gelSite
        } else if (route === "patchApply") {
            const rate = parseFloat(rateField.text)
            if (isFinite(rate) && rate > 0)
                out.releaseRateUGPerDay = rate
            const days = parseFloat(wearField.text)
            if (isFinite(days) && days > 0)
                out.patchWearH = days * 24
        }
        return out
    }

    function save() {
        if (app.backend === "none") {
            error = T.t("db.cta_disabled")
            return
        }
        const removal = route === "patchRemove"
        const amount = removal ? 0 : parseFloat(doseField.text)
        if (!removal && !(isFinite(amount) && amount > 0)) {
            error = T.t("status.dose_positive")
            doseField.field.forceActiveFocus()
            return
        }
        const drug = removal || route === "gel" || route === "patchApply"
            ? (transmasc ? "T" : "E2") : ester
        const timeH = when.getTime() / 3600000
        const ok = editing
            ? app.updateDose(dose.id, route, drug, amount, timeH, extras())
            : app.addDose(route, drug, amount, timeH, extras())
        if (ok) {
            error = ""
            done()
        } else {
            error = app.statusKey ? T.t(app.statusKey, app.statusArgs) : app.status
        }
    }

    function remove() {
        if (app.removeDose(dose.id)) {
            asking = false
            done()
        } else {
            error = app.statusKey ? T.t(app.statusKey, app.statusArgs) : app.status
        }
    }

    Component.onCompleted: {
        reset()
        asking = askOnOpen && editing
    }
    onDoseChanged: reset()

    // Time: a row that unfolds a month of days under it.
    AbstractButton {
        id: timeRow
        Layout.fillWidth: true
        implicitHeight: Theme.rowHeight
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.name: T.t("field.time")
        Accessible.description: T.dayAndClock(form.when)
        onClicked: form.timeOpen = !form.timeOpen

        background: Rectangle {
            color: "transparent"
            border.width: timeRow.visualFocus ? 2 : 0
            border.color: Theme.focusRing
            radius: Theme.radiusSm
        }

        contentItem: RowLayout {
            spacing: 8
            UiText {
                Layout.fillWidth: true
                text: T.t("field.time")
            }
            UiText {
                text: T.dayAndClock(form.when)
                color: Theme.textMuted
            }
            Icon {
                name: "chevron-down"
                size: 16
                color: Theme.textMuted
                rotation: form.timeOpen ? 180 : 0
                Behavior on rotation {
                    NumberAnimation { duration: Theme.durationBase; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
                }
            }
        }
    }

    Fold {
        open: form.timeOpen

        DateTimePicker {
            Layout.fillWidth: true
            value: form.when
            onPicked: function(when) { form.when = when }
        }
    }

    Hairline {}

    ChoiceField {
        Layout.topMargin: 16
        label: T.t("field.route")
        model: form.routes.map(function(r) { return { value: r, label: T.t("route." + r) } })
        currentValue: form.route
        onActivated: function(value) { form.route = value }
    }

    ChoiceField {
        Layout.topMargin: 16
        visible: form.route !== "patchRemove" && form.esters.length > 1
        label: T.t("field.ester")
        model: form.esters.map(function(e) { return { value: e, label: form.drugLabel(e) } })
        currentValue: form.ester
        onActivated: function(value) { form.ester = value }
    }

    // Sublingual: how long the tablet is held, or a theta of one's own.
    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: 16
        visible: form.route === "sublingual"
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            UiText {
                Layout.fillWidth: true
                text: T.t("field.sl_absorption")
                size: Theme.caption
                color: Theme.textMuted
                font.weight: Font.Medium
            }
            AppButton {
                variant: "ghostAccent"
                small: true
                implicitHeight: 22
                leftPadding: 6
                rightPadding: 6
                text: form.customTheta ? T.t("sl.use_presets") : T.t("sl.use_custom")
                onClicked: form.customTheta = !form.customTheta
            }
        }

        ChoiceField {
            visible: !form.customTheta
            Layout.topMargin: 4
            model: Doses.TIERS.map(function(t) {
                return { value: t.value, label: T.t("sl.tier." + t.key),
                         description: t.hold + " " + T.t("unit.min_short") }
            })
            currentValue: form.tier
            onActivated: function(value) { form.tier = value }
        }

        LineField {
            id: thetaField
            visible: form.customTheta
            Layout.fillWidth: true
            placeholderText: T.t("dose.theta_placeholder")
            hint: T.t("dose.theta_hint")
            validator: DoubleValidator { bottom: 0; top: 1 }
        }
    }

    ChoiceField {
        Layout.topMargin: 16
        visible: form.route === "gel"
        label: T.t("field.gel_site")
        model: [
            { value: 0, label: T.t("gel.site.arm") },
            { value: 1, label: T.t("gel.site.thigh") },
            { value: 2, label: T.t("gel.site.scrotal") }
        ]
        currentValue: form.gelSite
        onActivated: function(value) { form.gelSite = value }
    }

    LineField {
        id: rateField
        Layout.fillWidth: true
        Layout.topMargin: 16
        visible: form.route === "patchApply"
        label: T.t("field.patch_rate")
        placeholderText: T.t("patch.rate_placeholder")
        hint: T.t("field.patch_rate_hint")
        validator: DoubleValidator { bottom: 0 }
    }

    LineField {
        id: wearField
        Layout.fillWidth: true
        Layout.topMargin: 16
        visible: form.route === "patchApply"
        label: T.t("field.patch_wear")
        placeholderText: T.t("field.patch_wear_placeholder")
        hint: T.t("field.patch_wear_hint")
        validator: DoubleValidator { bottom: 0 }
    }

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 16
        visible: form.route === "patchRemove"
        text: T.t("patch.remove_hint")
        size: Theme.caption
        color: Theme.textMuted
        wrapMode: Text.Wrap
        lineHeightMode: Text.ProportionalHeight
        lineHeight: 1.0
    }

    LineField {
        id: doseField
        Layout.fillWidth: true
        Layout.topMargin: 16
        visible: form.route !== "patchRemove"
        label: form.route === "patchApply" ? T.t("field.patch_total") : T.t("field.dose_raw")
        placeholderText: "0.0"
        validator: DoubleValidator { bottom: 0 }
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        onEdited: form.error = ""
    }

    // What the dose comes to in estradiol (or testosterone).
    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 12
        visible: form.route !== "patchRemove" && form.factor !== 1
        readonly property real amount: parseFloat(doseField.text)
        text: (Doses.isTestosterone(form.ester) ? T.t("field.dose_t") : T.t("field.dose_e2")) + ": "
              + (isFinite(amount) && amount > 0 ? Doses.trim(amount * form.factor) : "--")
        size: Theme.caption
        color: Theme.textMuted
    }

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 12
        visible: form.error !== ""
        text: form.error
        size: Theme.caption
        color: Theme.danger
        wrapMode: Text.Wrap
        lineHeightMode: Text.ProportionalHeight
        lineHeight: 1.0
    }

    Hairline { Layout.topMargin: 16 }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 12
        spacing: 8

        AppButton {
            visible: form.editing
            variant: form.asking ? "ghostDanger" : "ghost"
            iconOnly: true
            iconName: "trash"
            iconSize: 16
            Accessible.name: T.t("btn.delete")
            onClicked: {
                form.asking = !form.asking
                if (form.asking)
                    prompt.focusKeep()
            }
        }

        Item { Layout.fillWidth: true }

        AppButton {
            visible: form.editing
            variant: "ghost"
            text: T.t("btn.cancel")
            onClicked: form.done()
        }

        AppButton {
            text: T.t("btn.save")
            iconName: "save"
            enabled: app.backend !== "none"
            onClicked: form.save()
        }
    }

    Fold {
        open: form.asking
        Layout.topMargin: 12

        InlinePrompt {
            id: prompt
            title: T.t("modal.dose.delete_confirm")
            message: form.editing
                ? T.fill(T.t("dose.delete_amount"), [Doses.trim(form.dose.doseMG), form.drugLabelInSentence(form.dose.ester)])
                  + "\n" + T.fill(T.t("dose.delete_when"), [T.t("route." + form.dose.route),
                                                            T.dayAndClock(new Date(Number(form.dose.timeH) * 3600000))])
                : ""
            cancelText: T.t("btn.keep")
            confirmText: T.t("btn.delete")
            onCancelled: form.asking = false
            onConfirmed: form.remove()
        }
    }

    // "estradiol valerate (EV)" inside a sentence, where the language
    // writes the name in lower case.
    function drugLabelInSentence(code) {
        const label = drugLabel(code)
        if (T.language === "en" || T.language === "ru")
            return label.charAt(0).toLocaleLowerCase() + label.slice(1)
        return label
    }

    Item { Layout.preferredHeight: 16 }
}
