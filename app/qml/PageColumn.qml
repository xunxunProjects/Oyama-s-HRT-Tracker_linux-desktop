import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The reading column every page sits in: one width, one side padding, centred
// in whatever the sidebar leaves, scrolling under its own title.
Flickable {
    id: root
    default property alias content: column.data
    property alias column: column
    property int columnWidth: Theme.pageColumn
    readonly property real columnX: Math.max(Theme.pageGutter, (width - columnWidth) / 2)
    readonly property real usableWidth: Math.min(columnWidth, width - Theme.pageGutter * 2)

    Layout.fillWidth: true
    Layout.fillHeight: true
    contentWidth: width
    contentHeight: column.implicitHeight + 128
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    ScrollBar.vertical: AppScrollBar {}

    ColumnLayout {
        id: column
        x: root.columnX
        width: root.usableWidth
        spacing: 0
    }
}
