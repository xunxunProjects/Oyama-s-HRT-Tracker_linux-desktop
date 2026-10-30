pragma Singleton
import QtQuick

QtObject {
    function t(key, args) {
        var _language = Ui.language
        return Ui.translate(key, args || [])
    }

    readonly property bool rtl: Ui.rtl
    readonly property string language: Ui.language
    readonly property int textAlign: Ui.rtl ? Text.AlignRight : Text.AlignLeft
}
