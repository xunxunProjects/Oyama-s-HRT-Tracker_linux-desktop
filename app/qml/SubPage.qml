import QtQuick

// A page reached from Settings: its title is the way back, and Escape too.
PageColumn {
    id: root
    property Item settings: null
    property string title: ""
    // A page with unsaved edits can hold the way back and ask first: return
    // false to stay.
    property var beforeBack: null

    function back(force) {
        if (!force && beforeBack && beforeBack() === false)
            return
        if (settings)
            settings.open("")
    }

    focus: true
    Keys.onEscapePressed: back()

    PageHeader {
        title: root.title
        back: true
        onBackRequested: root.back()
    }
}
