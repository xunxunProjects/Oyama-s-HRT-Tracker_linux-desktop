import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    function loadSettings() {
        weightField.text = app.weight > 0 ? String(app.weight) : ""
        birthField.text = app.birthYear > 0 ? String(app.birthYear) : ""
        methodCombo.currentIndex = Math.max(0, methodCombo.model.indexOf(app.calibrationMethod))
        historyCombo.currentIndex = Math.max(0, historyCombo.model.indexOf(app.calibrationHistory))
        pkArea.text = app.pkParamsJson && app.pkParamsJson.length > 0 ? app.pkParamsJson : "{}"
    }

    Component.onCompleted: loadSettings()

    Connections {
        target: app
        function onChanged() { page.loadSettings() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 14

        Label {
            text: qsTr("Settings")
            font.pixelSize: 30
            font.bold: true
            color: "#f4f6f8"
        }
        Label {
            text: qsTr("Shared body and model settings. Changes apply to both modes.")
            color: "#8a98a8"
            font.pixelSize: 13
        }

        Rectangle {
            Layout.fillWidth: true
            radius: 14
            color: "#182028"
            border.color: "#26313c"
            implicitHeight: form.implicitHeight + 40

            GridLayout {
                id: form
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 20
                columns: 2
                columnSpacing: 18
                rowSpacing: 14

                Label { text: qsTr("Body weight (kg)"); color: "#c9d2dc" }
                TextField {
                    id: weightField
                    Layout.fillWidth: true
                    placeholderText: qsTr("e.g. 70")
                    validator: DoubleValidator { bottom: 1; top: 500 }
                }

                Label { text: qsTr("Birth year"); color: "#c9d2dc" }
                TextField {
                    id: birthField
                    Layout.fillWidth: true
                    placeholderText: qsTr("e.g. 1990; empty to unset")
                    validator: IntValidator { bottom: 1900; top: 2200 }
                }

                Label { text: qsTr("Calibration method"); color: "#c9d2dc" }
                ComboBox {
                    id: methodCombo
                    Layout.fillWidth: true
                    model: ["mipd", "off", "ekf", "ou_kalman"]
                }

                Label { text: qsTr("Calibration history"); color: "#c9d2dc" }
                ComboBox {
                    id: historyCombo
                    Layout.fillWidth: true
                    model: ["retrospective", "forward"]
                }

                Label {
                    text: qsTr("PK parameter overrides (JSON)")
                    color: "#c9d2dc"
                    Layout.alignment: Qt.AlignTop
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 150
                    TextArea {
                        id: pkArea
                        font.family: "monospace"
                        wrapMode: TextArea.Wrap
                        placeholderText: "{}"
                    }
                }
            }
        }

        RowLayout {
            spacing: 12
            Button {
                text: qsTr("Save settings")
                highlighted: true
                onClicked: {
                    const weight = Number(weightField.text)
                    const birth = birthField.text.trim().length === 0 ? 0 : Number(birthField.text)
                    if (!isFinite(weight) || weight <= 0) {
                        message.text = qsTr("Body weight must be greater than zero.")
                        return
                    }
                    let params = pkArea.text.trim()
                    if (params.length === 0) params = "{}"
                    try {
                        const parsed = JSON.parse(params)
                        if (parsed === null || Array.isArray(parsed) || typeof parsed !== "object") {
                            throw new Error("not an object")
                        }
                    } catch (e) {
                        message.text = qsTr("PK parameters must be a JSON object.")
                        return
                    }
                    if (app.saveSettings(weight, birth, methodCombo.currentText,
                                         historyCombo.currentText, params)) {
                        message.text = app.status
                    } else {
                        message.text = app.status
                    }
                }
            }
            Label {
                id: message
                color: "#8a98a8"
                font.pixelSize: 12
            }
        }

        Item { Layout.fillHeight: true }
    }
}
