pragma Singleton
import QtQuick

// The web app's design system (Oyama design system v3, the tokens in
// src/styles/oyama-tokens.css upstream), converted from oklch to sRGB. A white
// page, warm neutrals, one terracotta accent; dark mode is warm near-black.
QtObject {
    // `Ui` is the C++ UiPreferences singleton registered by the app. Keeping the
    // fallback lets QML component tests load Theme without it.
    readonly property bool dark: typeof Ui !== "undefined" &&
        (Ui.theme === "dark" || (Ui.theme === "system" && Ui.systemDark))

    readonly property color bg: dark ? "#181714" : "#ffffff"
    readonly property color bgSubtle: dark ? "#0f0e0c" : "#fbfbf9"
    readonly property color surface: dark ? "#23211e" : "#ffffff"
    readonly property color surfaceMuted: dark ? "#2c2a26" : "#f7f6f3"
    readonly property color surfaceHover: dark ? "#2c2a26" : "#eeedea"
    readonly property color surfacePressed: dark ? "#373530" : "#e3e1dc"
    readonly property color controlThumb: dark ? "#373530" : "#ffffff"
    readonly property color text: dark ? "#fbfbf9" : "#181714"
    readonly property color textMuted: dark ? "#a7a49e" : "#6b6860"
    readonly property color textDisabled: dark ? "#6b6860" : "#a7a49e"
    readonly property color border: dark ? "#373530" : "#e3e1dc"
    readonly property color borderStrong: dark ? "#838078" : "#838078"
    readonly property color accent: dark ? "#df8f74" : "#ac573d"
    readonly property color accentHover: dark ? "#f0ad96" : "#924833"
    readonly property color textOnAccent: dark ? "#0f0e0c" : "#ffffff"
    readonly property color accentInk: dark ? "#f0ad96" : "#ac573d"
    readonly property color accentSubtle: dark ? "#311911" : "#fef4f0"
    readonly property color accentSubtleHover: dark ? "#4d271a" : "#fee7de"
    readonly property color textOnAccentSubtle: dark ? "#facebd" : "#924833"
    readonly property color focusRing: dark ? "#fbfbf9" : "#181714"
    readonly property color statusPositive: dark ? "#64d8a4" : "#0f7c55"
    readonly property color statusInfo: dark ? "#80bdfb" : "#0267c7"
    readonly property color statusViolet: dark ? "#b0adfb" : "#5e45cd"
    readonly property color statusNeutral: dark ? "#a7a49e" : "#6b6860"
    readonly property color warning: dark ? "#f5b75b" : "#a35303"
    readonly property color danger: dark ? "#fb817f" : "#c22826"
    readonly property color dangerFill: "#cc2827"
    readonly property color dangerFillHover: "#b01f1f"
    readonly property color textOnDanger: "#ffffff"
    readonly property color chartLine: dark ? "#df8f74" : "#cc785c"
    readonly property color chartSecond: dark ? "#838078" : "#a7a49e"
    readonly property color chartGrid: dark ? "#2c2a26" : "#eeedea"
    readonly property color chartAxis: dark ? "#a7a49e" : "#6b6860"
    readonly property color chartDot: dark ? "#181714" : "#ffffff"
    readonly property color illustrationLine: dark ? "#df8f74" : "#cc785c"
    readonly property color illustrationWash: dark ? "#311911" : "#fef4f0"
    readonly property color illustrationShade: dark ? "#46271d" : "#fbe2d7"
    readonly property color illustrationDeep: dark ? "#2b1711" : "#f4c3b1"
    readonly property color illustrationShine: dark ? "#774738" : "#ffffff"
    // The hand-drawn rule under a field, and the ink drawn over it on focus.
    readonly property color inkRule: dark ? "#858279" : "#8c8981"
    readonly property color inkRun: dark ? "#f9f9f7" : "#1a1916"
    readonly property color shadow: dark ? "#80000000" : "#0f181714"

    // Half a pixel on a sharp screen, a whole one where it can't.
    readonly property real hairline: Qt.application.screens.length > 0
        && Qt.application.screens[0].devicePixelRatio >= 2 ? 0.5 : 1

    // Shapes: controls 12, floating panels 16, anything pill-like full.
    readonly property int radiusXs: 6
    readonly property int radiusSm: 10
    readonly property int radiusMd: 12
    readonly property int radiusLg: 16
    readonly property int radiusXl: 24

    // Four type sizes and no others: 13 captions, 15 body, 20 section
    // titles, 26 page titles. The reading figure is the one number drawn
    // larger, light and tight.
    readonly property int caption: 13
    readonly property int body: 15
    readonly property int section: 20
    readonly property int title: 26
    readonly property int reading: 64
    readonly property int readingSm: 40

    function leading(size) {
        if (size <= 13) return 18
        if (size <= 15) return 22
        if (size <= 20) return 26
        if (size <= 26) return 32
        return size
    }

    // Layout: the reading column every page sits in, and the sidebar strip.
    readonly property int pageColumn: 672
    readonly property int pageGutter: 32
    readonly property int railWidth: 80
    readonly property int sidebarWidth: 260
    readonly property int controlHeight: 36
    readonly property int fieldHeight: 44
    readonly property int rowHeight: 56

    // Motion: a press, a colour, a change of state, something unfolding, a
    // page, a pen stroke, the chart. Curves as Easing.BezierSpline points.
    readonly property int durationInstant: 90
    readonly property int durationQuick: 150
    readonly property int durationBase: 220
    readonly property int durationSlow: 320
    readonly property int durationPage: 380
    readonly property int durationInk: 560
    readonly property int durationSweep: 900
    readonly property var easeStandard: [0.2, 0, 0, 1, 1, 1]
    readonly property var easeOut: [0.16, 1, 0.3, 1, 1, 1]
    readonly property var easePage: [0.32, 0.72, 0, 1, 1, 1]
    readonly property var easeInk: [0.55, 0.05, 0.25, 1, 1, 1]

    readonly property string fontFamily: typeof Ui !== "undefined" ? Ui.fontFamily : "Inter Variable"
    readonly property string monoFamily: "monospace"
    // The family as a canvas font, falling back where it isn't installed
    // (a Chinese interface without Noto Sans CJK).
    readonly property string canvasFont: Qt.fontFamilies().indexOf(fontFamily) >= 0
        ? "'" + fontFamily + "'" : "sans-serif"

    function alpha(c, a) {
        return Qt.rgba(c.r, c.g, c.b, a)
    }
}
