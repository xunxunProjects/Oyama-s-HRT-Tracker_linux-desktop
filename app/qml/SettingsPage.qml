import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page
    Layout.fillWidth: true
    Layout.fillHeight: true

    property bool settingsDirty: false
    property bool loadingSettings: false

    function loadSettings() {
        loadingSettings = true
        weightField.text = app.weight > 0 ? String(app.weight) : ""
        birthField.text = app.birthYear > 0 ? String(app.birthYear) : ""
        methodBox.currentIndex = methodBox.indexOfValue(app.calibrationMethod)
        historyBox.currentIndex = historyBox.indexOfValue(app.calibrationHistory)
        pkArea.text = app.pkParamsJson && app.pkParamsJson.length > 0
                      ? app.pkParamsJson : "{}"
        loadingSettings = false
        settingsDirty = false
    }

    function save() {
        if (app.backend === "none") {
            toast.show(T.t("db.cta_disabled"), true)
            return
        }
        const weight = Number(weightField.text)
        if (!isFinite(weight) || weight <= 0) {
            toast.show(T.t("status.weight_positive"), true)
            return
        }
        const birth = birthField.text.trim().length === 0
                      ? 0 : Number(birthField.text)
        if (!isFinite(birth) || (birth !== 0 && (birth < 1900 || birth > 2200))) {
            toast.show(T.t("status.birth_year_range"), true)
            return
        }
        let params = pkArea.text.trim()
        if (params.length === 0)
            params = "{}"
        try {
            const parsed = JSON.parse(params)
            if (parsed === null || Array.isArray(parsed)
                    || typeof parsed !== "object")
                throw new Error("not an object")
        } catch (error) {
            toast.show(T.t("settings.invalid_json"), true)
            return
        }
        const ok = app.saveSettings(weight, birth, methodBox.currentValue,
                                    historyBox.currentValue, params)
        if (ok) {
            settingsDirty = false
            toast.show(T.t("status.settings_saved"))
        } else {
            toast.show(app.statusKey ? T.t(app.statusKey, app.statusArgs)
                                     : app.status, true)
        }
    }

    Component.onCompleted: loadSettings()

    Connections {
        target: app
        function onChanged() {
            if (!page.settingsDirty)
                page.loadSettings()
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight + Theme.pageMargin * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: content
            x: Math.max(Theme.pageMargin,
                        (page.width - Math.min(680, page.width
                                              - Theme.pageMargin * 2)) / 2)
            y: Theme.pageMargin
            width: Math.min(680, page.width - Theme.pageMargin * 2)
            spacing: 20

            PageHeader {
                title: T.t("settings.title")
                subtitle: T.t("settings.subtitle")
                Layout.fillWidth: true
            }

            Card {
                Layout.fillWidth: true
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text {
                        text: T.t("settings.body_section")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSection
                        font.weight: Font.DemiBold
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: T.t("settings.weight")
                            color: Theme.textSoft
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            font.weight: Font.Medium
                        }

                        AppTextField {
                            id: weightField
                            Layout.fillWidth: true
                            placeholderText: T.t("settings.weight_placeholder")
                            validator: DoubleValidator { bottom: 1; top: 500 }
                            onTextEdited: page.settingsDirty = true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: T.t("settings.birth_year")
                            color: Theme.textSoft
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            font.weight: Font.Medium
                        }

                        AppTextField {
                            id: birthField
                            Layout.fillWidth: true
                            placeholderText: T.t("settings.birth_placeholder")
                            validator: IntValidator { bottom: 1900; top: 2200 }
                            onTextEdited: page.settingsDirty = true
                        }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text {
                        text: T.t("settings.model_section")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSection
                        font.weight: Font.DemiBold
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 16

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: T.t("settings.calibration_method")
                                color: Theme.textSoft
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontLabel
                                font.weight: Font.Medium
                            }

                            AppComboBox {
                                id: methodBox
                                Layout.fillWidth: true
                                model: ["mipd", "off", "ekf", "ou_kalman"]
                                onActivated: page.settingsDirty = true
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: T.t("settings.calibration_history")
                                color: Theme.textSoft
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontLabel
                                font.weight: Font.Medium
                            }

                            AppComboBox {
                                id: historyBox
                                Layout.fillWidth: true
                                model: ["retrospective", "forward"]
                                onActivated: page.settingsDirty = true
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Text {
                            text: T.t("settings.pk_overrides")
                            color: Theme.textSoft
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            font.weight: Font.Medium
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 150
                            radius: Theme.radiusControl
                            color: Theme.surfaceAlt

                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: 8
                                clip: true

                                TextArea {
                                    id: pkArea
                                    wrapMode: TextArea.Wrap
                                    font.family: "monospace"
                                    font.pixelSize: Theme.fontLabel
                                    color: Theme.text
                                    placeholderText: "{}"
                                    placeholderTextColor: Theme.textFaint
                                    selectionColor: Theme.accent
                                    selectedTextColor: Theme.onAccent
                                    background: null
                                    onTextChanged: {
                                        if (!page.loadingSettings)
                                            page.settingsDirty = true
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    Text {
                        text: T.t("settings.appearance_section")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSection
                        font.weight: Font.DemiBold
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: T.t("language.label")
                            color: Theme.textSoft
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            font.weight: Font.Medium
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: Ui.availableLanguages

                                delegate: AppButton {
                                    required property var modelData
                                    text: modelData.nativeName
                                    variant: Ui.language === modelData.code
                                             ? "primary" : "secondary"
                                    onClicked: Ui.language = modelData.code
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: T.t("theme.label")
                            color: Theme.textSoft
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            font.weight: Font.Medium
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: [
                                    { value: "light", label: T.t("theme.light") },
                                    { value: "dark", label: T.t("theme.dark") },
                                    { value: "system", label: T.t("theme.system") }
                                ]

                                delegate: AppButton {
                                    required property var modelData
                                    text: modelData.label
                                    variant: Ui.theme === modelData.value
                                             ? "primary" : "secondary"
                                    onClicked: Ui.theme = modelData.value
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Text {
                        text: T.t("settings.about_section")
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSection
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: T.t("app.name") + " · " + T.t("settings.version")
                              + " " + Qt.application.version
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                    }

                    Text {
                        Layout.fillWidth: true
                        text: T.t("settings.database") + ": " +
                              (app.backend === "none"
                               ? T.t("settings.database_none")
                               : T.t("settings.database_ok"))
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item { Layout.fillWidth: true }

                AppButton {
                    text: T.t("settings.save")
                    iconName: "check"
                    enabled: app.backend !== "none"
                    onClicked: page.save()
                }
            }

            Item { Layout.fillHeight: true }
        }
    }

    Toast {
        id: toast
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        z: 60
    }
}
