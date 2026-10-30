import QtQuick
import QtQuick.Layouts

// Body weight: one figure on its line, what it's for beside it, and Save.
SubPage {
    id: root
    title: T.t("settings.weight")
    property string error: ""

    function save() {
        const weight = parseFloat(figure.text)
        if (!(isFinite(weight) && weight >= 20 && weight <= 400)) {
            error = T.t("weight.range_error")
            return
        }
        error = settings ? settings.saveWith({ weight: weight }) : ""
        if (error === "")
            back()
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 32
        spacing: 24

        ColumnLayout {
            spacing: 6

            FigureField {
                id: figure
                unit: "kg"
                text: Number(app.weight).toFixed(app.weight % 1 === 0 ? 0 : 1)
                validator: DoubleValidator { bottom: 0; top: 999 }
                onEdited: root.error = ""
                onAccepted: root.save()
                Component.onCompleted: field.forceActiveFocus()
            }

            UiText {
                text: root.error !== "" ? root.error : T.t("weight.range")
                size: Theme.caption
                color: root.error !== "" ? Theme.danger : Theme.textMuted
            }
        }

        Item { Layout.fillWidth: true }

        UiText {
            Layout.maximumWidth: 120
            Layout.alignment: Qt.AlignTop
            text: T.t("modal.weight.desc")
            size: Theme.caption
            color: Theme.textMuted
            horizontalAlignment: Text.AlignRight
            wrapMode: Text.Wrap
            lineHeightMode: Text.ProportionalHeight
            lineHeight: 1.0
        }
    }

    Hairline { Layout.topMargin: 20 }

    AppButton {
        Layout.fillWidth: true
        Layout.topMargin: 24
        text: T.t("btn.save")
        enabled: app.backend !== "none"
        onClicked: root.save()
    }
}
