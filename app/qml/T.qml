pragma Singleton
import QtQuick

QtObject {
    readonly property bool hasUi: typeof Ui !== "undefined"

    function t(key, args) {
        if (!hasUi)
            return key
        var _language = Ui.language
        return Ui.translate(key, args || [])
    }

    readonly property bool rtl: hasUi && Ui.rtl
    readonly property string language: hasUi ? Ui.language : "en"
    readonly property int textAlign: rtl ? Text.AlignRight : Text.AlignLeft

    // The locale dates and times are written in, per interface language.
    readonly property var locale: Qt.locale(language === "zh" ? "zh_CN"
                                          : language === "ru" ? "ru_RU"
                                          : language === "fa" ? "fa_IR" : "en_US")

    // "Oct 26, 2026": a list's day heading, which stands on its own.
    function day(date) {
        if (language === "zh")
            return date.toLocaleDateString(locale, "yyyy年M月d日")
        if (language === "en")
            return date.toLocaleDateString(locale, "MMM d, yyyy")
        return date.toLocaleDateString(locale, "d MMMM yyyy")
    }

    // "Oct 28": a chart's date, where the year is understood.
    function shortDay(date) {
        if (language === "zh")
            return date.toLocaleDateString(locale, "M月d日")
        if (language === "en")
            return date.toLocaleDateString(locale, "MMM d")
        return date.toLocaleDateString(locale, "d MMM")
    }

    function clock(date) {
        return date.toLocaleTimeString(locale, "HH:mm")
    }

    function dayAndClock(date) {
        return day(date) + (language === "zh" ? " " : ", ") + clock(date)
    }

    // "%1" placeholders in a translated string, filled in order.
    function fill(text, args) {
        var out = text
        for (var i = 0; i < args.length; ++i)
            out = out.split("%" + (i + 1)).join(String(args[i]))
        return out
    }
}
