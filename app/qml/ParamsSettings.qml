import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The model's parameters, in folding groups: each with its default under its
// name and a short line to change it. Only values that differ from the
// default are stored. Leaving with edits unsaved asks first, in a line.
SubPage {
    id: root
    title: T.t("pk.title")
    property var defaults: ({})
    property var values: ({})
    property var openGroups: ({ e2_inj: true })
    property bool dirty: false
    property bool askingDiscard: false
    property bool askingReset: false
    property string error: ""

    readonly property var groups: [
        { key: "e2_inj", params: ["e2_ff_EB", "e2_ff_EV", "e2_ff_EC", "e2_ff_EN", "e2_ff_EU"] },
        { key: "e2_oral_sl", params: ["e2_oral_bio", "e2_sl_quick", "e2_sl_casual", "e2_sl_standard", "e2_sl_strict"] },
        { key: "e2_gel", params: ["e2_gel_arm", "e2_gel_thigh", "e2_gel_scrotal"] },
        { key: "e2_core", params: ["e2_kClear", "e2_kClearInj"] },
        { key: "t_inj", params: ["t_ff_TC", "t_ff_TE", "t_ff_TU"] },
        { key: "t_other", params: ["t_gel_arm", "t_gel_thigh", "t_gel_scrotal", "t_kClear", "t_kClearInj"] }
    ]

    beforeBack: function() {
        if (!dirty)
            return true
        askingDiscard = true
        root.contentY = 0
        return false
    }

    function load() {
        defaults = app.pkDefaults()
        let stored = {}
        try {
            stored = JSON.parse(app.pkParamsJson || "{}") || {}
        } catch (e) {
            stored = {}
        }
        const next = {}
        for (const k in defaults)
            next[k] = typeof stored[k] === "number" ? stored[k] : defaults[k]
        values = next
        dirty = false
    }

    function setValue(key, text) {
        const v = parseFloat(text)
        if (!isFinite(v) || v < 0)
            return
        const next = Object.assign({}, values)
        next[key] = v
        values = next
        dirty = true
        error = ""
    }

    function toggleGroup(key) {
        const next = Object.assign({}, openGroups)
        next[key] = !next[key]
        openGroups = next
    }

    function save() {
        const out = {}
        for (const k in values)
            if (Math.abs(values[k] - defaults[k]) > 1e-12)
                out[k] = values[k]
        error = settings ? settings.saveWith({ params: JSON.stringify(out) }) : ""
        if (error === "") {
            dirty = false
            back(true)
        }
    }

    function reset() {
        error = settings ? settings.saveWith({ params: "{}" }) : ""
        askingReset = false
        if (error === "")
            load()
    }

    function fmt(v) {
        return Number(v).toPrecision(4).replace(/\.?0+$/, "")
    }

    Component.onCompleted: load()

    // The way back held while edits are unsaved.
    Fold {
        open: root.askingDiscard
        InlinePrompt {
            title: T.t("pk.discard_title")
            message: T.t("pk.discard_confirm")
            cancelText: T.t("btn.keep")
            confirmText: T.t("pk.discard")
            onCancelled: root.askingDiscard = false
            onConfirmed: {
                root.askingDiscard = false
                root.dirty = false
                root.back(true)
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 12
        Layout.bottomMargin: 12
        spacing: 8
        Icon {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 3
            name: "warning"
            size: 14
            color: Theme.warning
        }
        UiText {
            Layout.fillWidth: true
            text: T.t("pk.warn")
            color: Theme.textMuted
            wrapMode: Text.Wrap
        }
    }

    Hairline {}

    Repeater {
        model: root.groups

        delegate: ColumnLayout {
            id: group
            required property var modelData
            readonly property bool open: !!root.openGroups[modelData.key]
            Layout.fillWidth: true
            spacing: 0

            AbstractButton {
                id: groupHead
                Layout.fillWidth: true
                implicitHeight: 50
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.name: T.t("pk.group." + group.modelData.key)
                onClicked: root.toggleGroup(group.modelData.key)

                background: Rectangle {
                    color: "transparent"
                    border.width: groupHead.visualFocus ? 2 : 0
                    border.color: Theme.focusRing
                    radius: Theme.radiusSm
                }

                contentItem: RowLayout {
                    UiText {
                        Layout.fillWidth: true
                        text: T.t("pk.group." + group.modelData.key)
                        size: Theme.caption
                        color: groupHead.hovered ? Theme.text : Theme.textMuted
                        font.weight: Font.DemiBold
                    }
                    Icon {
                        name: "chevron-down"
                        size: 16
                        color: Theme.textMuted
                        rotation: group.open ? 180 : 0
                        Behavior on rotation {
                            NumberAnimation { duration: Theme.durationBase; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
                        }
                    }
                }
            }

            Fold {
                open: group.open

                Repeater {
                    model: group.modelData.params

                    delegate: ColumnLayout {
                        required property string modelData
                        Layout.fillWidth: true
                        spacing: 0

                        Hairline {}

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 12
                            Layout.bottomMargin: 12
                            spacing: 16

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                UiText {
                                    Layout.fillWidth: true
                                    text: T.t("pk." + modelData)
                                    wrapMode: Text.Wrap
                                }
                                UiText {
                                    Layout.fillWidth: true
                                    readonly property bool changed: root.defaults[modelData] !== undefined
                                        && Math.abs(root.values[modelData] - root.defaults[modelData]) > 1e-12
                                    text: T.t("pk.default") + ": " + root.fmt(root.defaults[modelData])
                                          + (changed ? ". " + T.t("pk.modified") : "")
                                    size: Theme.caption
                                    color: changed ? Theme.accentInk : Theme.textMuted
                                }
                            }

                            AppTextField {
                                Layout.preferredWidth: 112
                                text: root.values[modelData] !== undefined ? root.fmt(root.values[modelData]) : ""
                                validator: DoubleValidator { bottom: 0 }
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                Accessible.name: T.t("pk." + modelData)
                                onTextEdited: root.setValue(modelData, text)
                            }
                        }
                    }
                }
            }

            Hairline {}
        }
    }

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 16
        text: T.t("pk.note")
        size: Theme.caption
        color: Theme.textMuted
        wrapMode: Text.Wrap
        lineHeightMode: Text.ProportionalHeight
        lineHeight: 1.0
    }

    UiText {
        Layout.fillWidth: true
        Layout.topMargin: 12
        visible: root.error !== ""
        text: root.error
        size: Theme.caption
        color: Theme.danger
        wrapMode: Text.Wrap
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 20
        spacing: 8

        AppButton {
            variant: "ghost"
            text: T.t("pk.reset")
            enabled: app.backend !== "none"
            onClicked: root.askingReset = !root.askingReset
        }
        Item { Layout.fillWidth: true }
        AppButton {
            text: T.t("btn.save")
            enabled: app.backend !== "none" && root.dirty
            onClicked: root.save()
        }
    }

    Fold {
        open: root.askingReset
        Layout.topMargin: 12
        InlinePrompt {
            title: T.t("pk.reset_confirm")
            cancelText: T.t("btn.keep")
            confirmText: T.t("pk.reset")
            onCancelled: root.askingReset = false
            onConfirmed: root.reset()
        }
    }
}
