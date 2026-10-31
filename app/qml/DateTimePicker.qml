import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes

// A month of days with the chosen one circled in ink as it is picked and today
// marked with a dot, and the time typed as two digits under it. It unfolds in
// place under the Time row.
ColumnLayout {
    id: root
    property date value: new Date()
    signal picked(date when)

    property int shownYear: value.getFullYear()
    property int shownMonth: value.getMonth()
    spacing: 0

    function setValue(d) {
        value = d
        picked(d)
    }

    function pad2(n) {
        return n < 10 ? "0" + n : String(n)
    }

    function pickDay(dayNumber) {
        const d = new Date(value.getTime())
        d.setFullYear(shownYear, shownMonth, dayNumber)
        setValue(d)
    }

    function step(months) {
        let m = shownMonth + months
        let y = shownYear
        while (m < 0) { m += 12; y -= 1 }
        while (m > 11) { m -= 12; y += 1 }
        shownMonth = m
        shownYear = y
    }

    onValueChanged: {
        shownYear = value.getFullYear()
        shownMonth = value.getMonth()
        hourField.text = pad2(value.getHours())
        minuteField.text = pad2(value.getMinutes())
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 12
        Layout.bottomMargin: 8
        spacing: 8

        UiText {
            text: T.locale.standaloneMonthName(root.shownMonth, Locale.LongFormat)
        }
        UiText {
            text: String(root.shownYear)
            font.weight: Font.Medium
        }
        Item { Layout.fillWidth: true }

        AppButton {
            variant: "ghost"
            iconOnly: true
            iconName: T.rtl ? "chevron-right" : "chevron-left"
            iconSize: 16
            Accessible.name: T.t("picker.previous_month")
            onClicked: root.step(-1)
        }
        AppButton {
            variant: "ghost"
            iconOnly: true
            iconName: T.rtl ? "chevron-left" : "chevron-right"
            iconSize: 16
            Accessible.name: T.t("picker.next_month")
            onClicked: root.step(1)
        }
    }

    // Weekday initials, then the days, Sunday first as on the web.
    GridLayout {
        Layout.fillWidth: true
        columns: 7
        rowSpacing: 0
        columnSpacing: 0

        Repeater {
            model: 7
            delegate: UiText {
                required property int index
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                horizontalAlignment: Text.AlignHCenter
                text: T.locale.dayName(index, Locale.NarrowFormat)
                size: Theme.caption
                color: Theme.textMuted
                topPadding: 4
                bottomPadding: 4
            }
        }

        Repeater {
            model: 42

            delegate: Item {
                id: cell
                required property int index
                readonly property int lead: new Date(root.shownYear, root.shownMonth, 1).getDay()
                readonly property int length: new Date(root.shownYear, root.shownMonth + 1, 0).getDate()
                readonly property int dayNumber: index - lead + 1
                readonly property bool real: dayNumber >= 1 && dayNumber <= length
                readonly property bool chosen: real && root.value.getFullYear() === root.shownYear
                    && root.value.getMonth() === root.shownMonth && root.value.getDate() === dayNumber
                readonly property bool today: {
                    const now = new Date()
                    return real && now.getFullYear() === root.shownYear
                        && now.getMonth() === root.shownMonth && now.getDate() === dayNumber
                }
                visible: index < Math.ceil((lead + length) / 7) * 7
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                implicitHeight: 40

                AbstractButton {
                    id: dayButton
                    visible: cell.real
                    anchors.centerIn: parent
                    width: 40
                    height: 40
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: String(cell.dayNumber)
                    onClicked: root.pickDay(cell.dayNumber)

                    background: Rectangle {
                        radius: 20
                        color: dayButton.hovered && !cell.chosen ? Theme.surfaceHover : "transparent"
                        border.width: dayButton.visualFocus ? 2 : 0
                        border.color: Theme.focusRing
                    }

                    contentItem: UiText {
                        text: String(cell.dayNumber)
                        horizontalAlignment: Text.AlignHCenter
                        font.weight: cell.chosen ? Font.DemiBold : Font.Normal
                    }

                    // The chosen day, circled in ink: a ring that runs a
                    // touch past where it began, as the pen came round.
                    Shape {
                        visible: cell.chosen
                        layer.enabled: GraphicsInfo.api === GraphicsInfo.Software
                        layer.smooth: true
                        x: -6
                        y: -6
                        width: 24
                        height: 24
                        transform: Scale { xScale: 52 / 24; yScale: 52 / 24 }
                        ShapePath {
                            strokeColor: Theme.accentInk
                            strokeWidth: 1.75 * 24 / 52
                            fillColor: "transparent"
                            capStyle: ShapePath.RoundCap
                            PathSvg { path: "M12.4 3.9c-4.6-.2-8.3 3.4-8.3 8.1.1 4.5 3.8 8.1 8.2 8 4.5-.1 8-3.7 7.9-8.2-.1-4.3-3.4-7.8-7.7-8.1-.6 0-1.1.1-1.5.3" }
                        }
                    }

                    Rectangle {
                        visible: cell.today
                        width: 4
                        height: 4
                        radius: 2
                        color: Theme.accentInk
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 5
                    }
                }
            }
        }
    }

    // The time, typed as two digits each side of a colon.
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 12
        Layout.bottomMargin: 12
        spacing: 8

        UiText {
            Layout.fillWidth: true
            text: T.t("time.select")
            color: Theme.textMuted
        }

        Row {
            spacing: 8
            LayoutMirroring.enabled: false

            AppTextField {
                id: hourField
                width: 48
                horizontalAlignment: Text.AlignHCenter
                text: root.pad2(root.value.getHours())
                validator: IntValidator { bottom: 0; top: 23 }
                inputMethodHints: Qt.ImhDigitsOnly
                Accessible.name: T.t("picker.hour")
                onEditingFinished: {
                    const h = parseInt(text)
                    if (isFinite(h) && h >= 0 && h <= 23) {
                        const d = new Date(root.value.getTime())
                        d.setHours(h)
                        root.setValue(d)
                    }
                }
            }
            UiText {
                anchors.verticalCenter: parent.verticalCenter
                text: ":"
                color: Theme.textMuted
            }
            AppTextField {
                id: minuteField
                width: 48
                horizontalAlignment: Text.AlignHCenter
                text: root.pad2(root.value.getMinutes())
                validator: IntValidator { bottom: 0; top: 59 }
                inputMethodHints: Qt.ImhDigitsOnly
                Accessible.name: T.t("picker.minute")
                onEditingFinished: {
                    const m = parseInt(text)
                    if (isFinite(m) && m >= 0 && m <= 59) {
                        const d = new Date(root.value.getTime())
                        d.setMinutes(m)
                        root.setValue(d)
                    }
                }
            }
        }

        AppButton {
            variant: "ghostAccent"
            text: T.t("time.now")
            onClicked: root.setValue(new Date())
        }
    }
}
