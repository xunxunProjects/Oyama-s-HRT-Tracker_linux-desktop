import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    property var editingDose: null
    property string editId: ""
    property string message: ""

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
        routeBox.currentIndex = Math.max(0, routeBox.model.indexOf(dose.route))
        esterBox.currentIndex = Math.max(0, esterBox.model.indexOf(dose.ester))
        doseField.text = Number(dose.doseMG).toFixed(2)
        daysField.text = ((app.nowHours() - Number(dose.timeH)) / 24).toFixed(2)
        const extras = parseExtras(dose.extras)
        tierBox.currentIndex = Number(extras.sublingualTier ?? 2)
        thetaField.text = extras.sublingualTheta !== undefined
                         ? String(extras.sublingualTheta) : ""
        gelBox.currentIndex = Number(extras.gelSite ?? 0)
        releaseField.text = extras.releaseRateUGPerDay !== undefined
                            ? String(extras.releaseRateUGPerDay) : ""
        wearField.text = extras.patchWearH !== undefined
                         ? String(extras.patchWearH) : ""
        message = ""
    }

    function resetForm() {
        editId = ""
        routeBox.currentIndex = 0
        esterBox.currentIndex = 0
        doseField.text = "2"
        daysField.text = "0"
        tierBox.currentIndex = 2
        thetaField.text = ""
        gelBox.currentIndex = 0
        releaseField.text = ""
        wearField.text = ""
        message = ""
    }

    function selectedExtras() {
        const extras = {}
        if (routeBox.currentText === "sublingual") {
            extras.sublingualTier = tierBox.currentIndex
            const theta = parseFloat(thetaField.text)
            if (isFinite(theta) && theta > 0)
                extras.sublingualTheta = theta
        } else if (routeBox.currentText === "gel") {
            extras.gelSite = gelBox.currentIndex
        } else if (routeBox.currentText === "patchApply") {
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
        const dose = parseFloat(doseField.text)
        const days = parseFloat(daysField.text)
        if (!isFinite(dose) || dose <= 0) {
            message = "Dose must be greater than zero."
            return
        }
        if (!isFinite(days)) {
            message = "Days ago must be a number."
            return
        }
        const timeH = app.nowHours() - days * 24
        const extras = selectedExtras()
        const ok = editId !== ""
                ? app.updateDose(editId, routeBox.currentText,
                                 esterBox.currentText, dose, timeH, extras)
                : app.addDose(routeBox.currentText, esterBox.currentText,
                              dose, timeH, extras)
        message = app.status
        if (ok) {
            saved()
            resetForm()
        }
    }

    Component.onCompleted: resetForm()

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: Math.min(760, page.width - 64)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 36
            spacing: 20

            Label {
                text: page.editId !== "" ? "Edit dose" : "Add dose"
                color: "#f4f6f8"
                font.pixelSize: 30
                font.bold: true
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: form.implicitHeight + 40
                radius: 16
                color: "#182028"
                border.color: "#26323d"
                border.width: 1

                ColumnLayout {
                    id: form
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    Label { text: "Route"; color: "#8fa1b3"; font.pixelSize: 13 }
                    ComboBox {
                        id: routeBox
                        Layout.fillWidth: true
                        model: ["oral", "sublingual", "injection", "gel",
                                "patchApply", "patchRemove"]
                    }

                    Label { text: "Ester"; color: "#8fa1b3"; font.pixelSize: 13 }
                    ComboBox {
                        id: esterBox
                        Layout.fillWidth: true
                        model: ["E2", "EV", "EB", "EC", "EN", "EU", "CPA",
                                "T", "TC", "TE", "TU"]
                    }

                    Label { text: "Dose (mg)"; color: "#8fa1b3"; font.pixelSize: 13 }
                    TextField {
                        id: doseField
                        Layout.fillWidth: true
                        text: "2"
                        validator: DoubleValidator { bottom: 0.0001 }
                    }

                    Label { text: "Days ago"; color: "#8fa1b3"; font.pixelSize: 13 }
                    TextField {
                        id: daysField
                        Layout.fillWidth: true
                        text: "0"
                        validator: DoubleValidator { bottom: -3650; top: 3650 }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: routeBox.currentText === "sublingual"

                        Label { text: "Sublingual tier"; color: "#8fa1b3"; font.pixelSize: 13 }
                        ComboBox {
                            id: tierBox
                            Layout.fillWidth: true
                            model: ["quick", "casual", "standard", "strict"]
                            currentIndex: 2
                        }
                        Label { text: "Custom hold theta (optional)"; color: "#8fa1b3"; font.pixelSize: 13 }
                        TextField {
                            id: thetaField
                            Layout.fillWidth: true
                            placeholderText: "e.g. 0.08"
                            validator: DoubleValidator { bottom: 0; top: 1 }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: routeBox.currentText === "gel"
                        Label { text: "Application site"; color: "#8fa1b3"; font.pixelSize: 13 }
                        ComboBox {
                            id: gelBox
                            Layout.fillWidth: true
                            model: ["arm", "thigh", "scrotal"]
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        visible: routeBox.currentText === "patchApply"
                        Label { text: "Release rate (µg/day)"; color: "#8fa1b3"; font.pixelSize: 13 }
                        TextField {
                            id: releaseField
                            Layout.fillWidth: true
                            placeholderText: "e.g. 100"
                            validator: DoubleValidator { bottom: 0 }
                        }
                        Label { text: "Planned wear (hours, optional)"; color: "#8fa1b3"; font.pixelSize: 13 }
                        TextField {
                            id: wearField
                            Layout.fillWidth: true
                            placeholderText: "e.g. 84"
                            validator: DoubleValidator { bottom: 0 }
                        }
                    }

                    RowLayout {
                        Layout.topMargin: 8
                        spacing: 12

                        Button {
                            text: page.editId !== "" ? "Save changes" : "Add dose"
                            highlighted: true
                            onClicked: page.save()
                        }
                        Button {
                            text: "Cancel"
                            onClicked: {
                                page.resetForm()
                                page.saved()
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: page.message
                        color: page.message.indexOf("Could not") === 0 ||
                               page.message.indexOf("must") >= 0
                               ? "#ff9b9b" : "#8fa1b3"
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
