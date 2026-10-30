import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    Layout.fillWidth: true
    Layout.fillHeight: true

    property var editingDose: null
    property string editId: ""
    property string message: ""
    property bool saveSucceeded: false
    readonly property string errorText: message !== ""
                                                ? message
                                                : (app.backend === "none"
                                                   ? T.t("db.cta_disabled") : "")

    signal saved()

    function parseExtras(value) {
        if (!value)
            return {}
        if (typeof value === "object")
            return value
        try {
            return JSON.parse(value)
        } catch (error) {
            return {}
        }
    }

    function loadDose(dose) {
        if (!dose)
            return
        editId = dose.id || ""
        routeBox.currentIndex = routeBox.indexOfValue(dose.route)
        esterBox.currentIndex = esterBox.indexOfValue(dose.ester)
        doseField.text = Number(dose.doseMG).toFixed(2)
        daysField.text = ((app.nowHours() - Number(dose.timeH)) / 24).toFixed(2)
        const extras = parseExtras(dose.extras)
        tierBox.currentIndex = tierBox.indexOfValue(
            Number(extras.sublingualTier ?? 2))
        thetaField.text = extras.sublingualTheta !== undefined
                          ? String(extras.sublingualTheta) : ""
        gelBox.currentIndex = gelBox.indexOfValue(Number(extras.gelSite ?? 0))
        releaseField.text = extras.releaseRateUGPerDay !== undefined
                            ? String(extras.releaseRateUGPerDay) : ""
        wearField.text = extras.patchWearH !== undefined
                         ? String(extras.patchWearH) : ""
        message = ""
        saveSucceeded = false
    }

    function resetForm() {
        editId = ""
        routeBox.currentIndex = 0
        esterBox.currentIndex = 0
        doseField.text = "2"
        daysField.text = "0"
        tierBox.currentIndex = tierBox.indexOfValue(2)
        thetaField.text = ""
        gelBox.currentIndex = gelBox.indexOfValue(0)
        releaseField.text = ""
        wearField.text = ""
        message = ""
        saveSucceeded = false
    }

    function selectedExtras() {
        const extras = {}
        if (routeBox.currentValue === "sublingual") {
            extras.sublingualTier = tierBox.currentValue
            const theta = parseFloat(thetaField.text)
            if (isFinite(theta) && theta > 0)
                extras.sublingualTheta = theta
        } else if (routeBox.currentValue === "gel") {
            extras.gelSite = gelBox.currentValue
        } else if (routeBox.currentValue === "patchApply") {
            const rate = parseFloat(releaseField.text)
            if (isFinite(rate) && rate > 0)
                extras.releaseRateUGPerDay = rate
            const wear = parseFloat(wearField.text)
            if (isFinite(wear) && wear > 0)
                extras.patchWearH = wear
        }
        return extras
    }

    function save() {
        if (app.backend === "none") {
            message = T.t("db.cta_disabled")
            return
        }
        const dose = parseFloat(doseField.text)
        const days = parseFloat(daysField.text)
        if (!isFinite(dose) || dose <= 0) {
            message = T.t("status.dose_positive")
            return
        }
        if (!isFinite(days)) {
            message = T.t("dose.error_days")
            return
        }
        const timeH = app.nowHours() - days * 24
        const extras = selectedExtras()
        const ok = editId !== ""
                ? app.updateDose(editId, routeBox.currentValue,
                                 esterBox.currentValue, dose, timeH, extras)
                : app.addDose(routeBox.currentValue, esterBox.currentValue,
                              dose, timeH, extras)
        if (ok) {
            message = ""
            saveSucceeded = true
            successTimer.restart()
        } else {
            message = app.statusKey ? T.t(app.statusKey, app.statusArgs)
                                    : app.status
        }
    }

    Component.onCompleted: resetForm()

    Timer {
        id: successTimer
        interval: 850
        onTriggered: {
            page.saveSucceeded = false
            page.saved()
            page.resetForm()
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: form.implicitHeight + Theme.pageMargin * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: form
            x: Math.max(Theme.pageMargin,
                        (page.width - Math.min(640, page.width
                                              - Theme.pageMargin * 2)) / 2)
            y: Theme.pageMargin
            width: Math.min(640, page.width - Theme.pageMargin * 2)
            spacing: 20

            PageHeader {
                title: page.editId !== "" ? T.t("dose.edit_title")
                                          : T.t("dose.add_title")
                subtitle: T.t("dose.subtitle")
                Layout.fillWidth: true
            }

            Card {
                Layout.fillWidth: true
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 16

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: T.t("dose.route")
                                color: Theme.textSoft
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontLabel
                                font.weight: Font.Medium
                            }

                            AppComboBox {
                                id: routeBox
                                Layout.fillWidth: true
                                textRole: "label"
                                valueRole: "value"
                                model: [
                                    { value: "oral", label: T.t("route.oral") },
                                    { value: "sublingual", label: T.t("route.sublingual") },
                                    { value: "injection", label: T.t("route.injection") },
                                    { value: "gel", label: T.t("route.gel") },
                                    { value: "patchApply", label: T.t("route.patchApply") },
                                    { value: "patchRemove", label: T.t("route.patchRemove") }
                                ]
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: T.t("dose.ester")
                                color: Theme.textSoft
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontLabel
                                font.weight: Font.Medium
                            }

                            AppComboBox {
                                id: esterBox
                                Layout.fillWidth: true
                                textRole: "label"
                                valueRole: "value"
                                model: [
                                    { value: "E2", label: T.t("ester.E2") },
                                    { value: "EV", label: T.t("ester.EV") },
                                    { value: "EB", label: T.t("ester.EB") },
                                    { value: "EC", label: T.t("ester.EC") },
                                    { value: "EN", label: T.t("ester.EN") },
                                    { value: "EU", label: T.t("ester.EU") },
                                    { value: "CPA", label: T.t("ester.CPA") },
                                    { value: "T", label: T.t("ester.T") },
                                    { value: "TC", label: T.t("ester.TC") },
                                    { value: "TE", label: T.t("ester.TE") },
                                    { value: "TU", label: T.t("ester.TU") }
                                ]
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: T.t("dose.amount")
                                color: Theme.textSoft
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontLabel
                                font.weight: Font.Medium
                            }

                            AppTextField {
                                id: doseField
                                Layout.fillWidth: true
                                text: "2"
                                validator: DoubleValidator { bottom: 0.0001 }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: T.t("dose.days_ago")
                                color: Theme.textSoft
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontLabel
                                font.weight: Font.Medium
                            }

                            AppTextField {
                                id: daysField
                                Layout.fillWidth: true
                                text: "0"
                                validator: DoubleValidator { bottom: -3650; top: 3650 }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        opacity: routeBox.currentValue === "sublingual" ? 1 : 0
                        visible: opacity > 0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: Theme.durationHover
                                easing.type: Easing.OutCubic
                            }
                        }

                        Text {
                            text: T.t("dose.sublingual_tier")
                            color: Theme.textSoft
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            font.weight: Font.Medium
                        }

                        AppComboBox {
                            id: tierBox
                            Layout.fillWidth: true
                            textRole: "label"
                            valueRole: "value"
                            model: [
                                { value: 0, label: T.t("tier.quick") },
                                { value: 1, label: T.t("tier.casual") },
                                { value: 2, label: T.t("tier.standard") },
                                { value: 3, label: T.t("tier.strict") }
                            ]
                        }

                        Text {
                            text: T.t("dose.theta")
                            color: Theme.textSoft
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            font.weight: Font.Medium
                        }

                        AppTextField {
                            id: thetaField
                            Layout.fillWidth: true
                            placeholderText: T.t("dose.theta_placeholder")
                            validator: DoubleValidator { bottom: 0; top: 1 }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        opacity: routeBox.currentValue === "gel" ? 1 : 0
                        visible: opacity > 0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: Theme.durationHover
                                easing.type: Easing.OutCubic
                            }
                        }

                        Text {
                            text: T.t("dose.gel_site")
                            color: Theme.textSoft
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            font.weight: Font.Medium
                        }

                        AppComboBox {
                            id: gelBox
                            Layout.fillWidth: true
                            textRole: "label"
                            valueRole: "value"
                            model: [
                                { value: 0, label: T.t("site.arm") },
                                { value: 1, label: T.t("site.thigh") },
                                { value: 2, label: T.t("site.scrotal") }
                            ]
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        opacity: routeBox.currentValue === "patchApply" ? 1 : 0
                        visible: opacity > 0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: Theme.durationHover
                                easing.type: Easing.OutCubic
                            }
                        }

                        Text {
                            text: T.t("dose.release_rate")
                            color: Theme.textSoft
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            font.weight: Font.Medium
                        }

                        AppTextField {
                            id: releaseField
                            Layout.fillWidth: true
                            placeholderText: T.t("dose.release_placeholder")
                            validator: DoubleValidator { bottom: 0 }
                        }

                        Text {
                            text: T.t("dose.wear")
                            color: Theme.textSoft
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            font.weight: Font.Medium
                        }

                        AppTextField {
                            id: wearField
                            Layout.fillWidth: true
                            placeholderText: T.t("dose.wear_placeholder")
                            validator: DoubleValidator { bottom: 0 }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: page.errorText !== ""
                        text: page.errorText
                        color: Theme.brick
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontLabel
                        wrapMode: Text.Wrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        spacing: 12

                        AppButton {
                            text: T.t("common.cancel")
                            variant: "ghost"
                            onClicked: {
                                page.resetForm()
                                page.saved()
                            }
                        }

                        Item { Layout.fillWidth: true }

                        AppButton {
                            text: page.saveSucceeded
                                  ? T.t("common.saved")
                                  : (page.editId !== "" ? T.t("dose.save")
                                                        : T.t("dose.add"))
                            iconName: page.saveSucceeded ? "check" : ""
                            enabled: !page.saveSucceeded && app.backend !== "none"
                            onClicked: page.save()
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
