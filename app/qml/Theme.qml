pragma Singleton
import QtQuick

QtObject {
    // `Ui` is the C++ UiPreferences singleton registered by the app. Keeping the
    // fallback lets previews and QML component tests load Theme without it.
    readonly property bool dark: typeof Ui !== "undefined" &&
        (Ui.theme === "dark" || (Ui.theme === "system" && Ui.systemDark))

    readonly property color bg: dark ? "#1F1D1A" : "#EFE6D0"
    readonly property color sidebar: dark ? "#1B1917" : "#F5EDDF"
    readonly property color surface: dark ? "#2B2823" : "#FFFDF7"
    readonly property color surfaceAlt: dark ? "#35312B" : "#EFE9D8"
    readonly property color surfaceHover: dark ? "#302C27" : "#F2E8D6"
    readonly property color surfacePressed: dark ? "#3A352E" : "#E7DCC6"
    readonly property color text: dark ? "#E6E0D4" : "#33302A"
    readonly property color textSoft: dark ? "#A39C8D" : "#7A7466"
    readonly property color textMuted: dark ? "#A39C8D" : "#6E6759"
    readonly property color textFaint: dark ? "#9A9385" : "#6B6457"
    readonly property color textDisabled: dark ? "#6F695D" : "#A69E8E"
    readonly property color border: dark ? "#3D3830" : "#DED3BD"
    readonly property color accent: dark ? "#C98F52" : "#B5793C"
    readonly property color accentHover: dark ? "#D9A268" : "#A66B31"
    readonly property color accentSubtle: dark ? "#3A2F23" : "#F3E3CC"
    readonly property color accentInk: dark ? "#DFA96F" : "#8C5A28"
    readonly property color green: dark ? "#7FA98A" : "#4F7A5A"
    readonly property color brick: dark ? "#D07A62" : "#B4573E"
    readonly property color onAccent: dark ? "#1F1D1A" : "#2B241B"
    readonly property color chartLine: accent
    readonly property color chartGrid: dark ? "#3A352E" : "#E4D8C2"
    readonly property color shadow: dark ? "#59000000" : "#0F3C2D14"

    readonly property int radiusCard: 18
    readonly property int radiusControl: 12
    readonly property int radiusPill: 999

    readonly property int fontTitle: 28
    readonly property int fontSection: 20
    readonly property int fontBody: 15
    readonly property int fontLabel: 13
    readonly property int fontCaption: 11

    readonly property int durationHover: 150
    readonly property int durationPage: 280
    readonly property int durationTheme: 240
    readonly property int durationSuccess: 320

    readonly property string fontFamily: typeof Ui !== "undefined" ? Ui.fontFamily : "Inter"
    readonly property int pageMargin: 40
    readonly property int cardPadding: 20
}
