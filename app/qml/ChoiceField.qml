import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A choice is a line like a field (CustomSelect upstream): its label, what's
// chosen, and a chevron. Opened, the options unfold under it in place as
// plain rows with a tick by the chosen one. Nothing floats over the page.
ColumnLayout {
    id: root
    property string label: ""
    property var model: []            // [{ value, label, description? }]
    property var currentValue
    property bool open: false
    signal activated(var value)

    readonly property int count: model ? model.length : 0
    readonly property var current: {
        for (let i = 0; i < count; ++i)
            if (model[i].value === currentValue)
                return model[i]
        return count > 0 ? model[0] : null
    }
    readonly property string displayText: current ? current.label : ""

    function indexOfValue(value) {
        for (let i = 0; i < count; ++i)
            if (model[i].value === value)
                return i
        return -1
    }

    function choose(value) {
        currentValue = value
        open = false
        activated(value)
    }

    spacing: 0
    Layout.fillWidth: true

    UiText {
        Layout.fillWidth: true
        visible: root.label !== ""
        text: root.label
        size: Theme.caption
        color: Theme.textMuted
        font.weight: Font.Medium
    }

    AbstractButton {
        id: trigger
        Layout.fillWidth: true
        Layout.topMargin: root.label !== "" ? 4 : 0
        implicitHeight: Theme.fieldHeight
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.ComboBox
        Accessible.name: root.label
        Accessible.description: root.displayText
        onClicked: root.open = !root.open

        background: Item {
            InkRule {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 6
                inked: root.open || trigger.visualFocus
            }
        }

        contentItem: RowLayout {
            spacing: 8

            UiText {
                Layout.fillWidth: true
                text: root.displayText
                elide: Text.ElideRight
                topPadding: 2
            }

            UiText {
                visible: !!(root.current && root.current.description)
                text: root.current && root.current.description ? root.current.description : ""
                size: Theme.caption
                color: Theme.textMuted
                topPadding: 2
            }

            Icon {
                name: "chevron-down"
                size: 16
                color: Theme.textMuted
                rotation: root.open ? 180 : 0
                Behavior on rotation {
                    NumberAnimation { duration: Theme.durationBase; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
                }
            }
        }
    }

    Fold {
        open: root.open

        Item { Layout.preferredHeight: 4 }

        Repeater {
            model: root.model

            delegate: ChoiceRow {
                required property var modelData
                required property int index
                Layout.fillWidth: true
                bleed: false
                selectedWeight: Font.Medium
                divider: index > 0
                title: modelData.label
                selected: modelData.value === root.currentValue
                onClicked: root.choose(modelData.value)
            }
        }

        Item { Layout.preferredHeight: 8 }
    }
}
