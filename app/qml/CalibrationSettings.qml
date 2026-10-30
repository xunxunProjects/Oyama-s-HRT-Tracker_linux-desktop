import QtQuick
import QtQuick.Layouts

// Calibration: how lab results correct the model, and whether a new one may
// revise the past. Each choice takes effect as it is ticked.
SubPage {
    id: root
    title: T.t("settings.calibration")
    property string error: ""

    function choose(changes) {
        error = settings ? settings.saveWith(changes) : ""
    }

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 8
        visible: root.error !== ""
        text: root.error
        size: Theme.caption
        color: Theme.danger
        wrapMode: Text.Wrap
    }

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 12
        text: T.t("cal.desktop_note")
        size: Theme.caption
        color: Theme.textMuted
        wrapMode: Text.Wrap
        lineHeightMode: Text.ProportionalHeight
        lineHeight: 1.0
    }

    GroupHeading { text: T.t("cal.method") }
    Repeater {
        model: ["off", "mipd", "ekf", "ou_kalman"]
        delegate: ChoiceRow {
            required property string modelData
            required property int index
            divider: index > 0
            title: T.t("cal." + modelData)
            description: T.t("cal.desc." + modelData)
            selected: app.calibrationMethod === modelData
            onClicked: root.choose({ method: modelData })
        }
    }

    GroupHeading { text: T.t("cal.history"); Layout.topMargin: 32 }
    Repeater {
        model: ["retrospective", "forward"]
        delegate: ChoiceRow {
            required property string modelData
            required property int index
            divider: index > 0
            title: T.t("cal.history." + modelData)
            description: T.t("cal.desc." + modelData)
            selected: app.calibrationHistory === modelData
            onClicked: root.choose({ history: modelData })
        }
    }
}
