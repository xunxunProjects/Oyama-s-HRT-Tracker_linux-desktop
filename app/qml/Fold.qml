import QtQuick
import QtQuick.Layouts

// Something unfolding in place: what's inside grows from nothing to its own
// height under what was pressed, and folds away again. Nothing floats over
// the page. While closed it holds no room and takes no focus.
Item {
    id: root
    property bool open: false
    default property alias content: inner.data
    property alias contentHeight: inner.implicitHeight
    property real shown: open ? inner.implicitHeight : 0

    implicitWidth: inner.implicitWidth
    implicitHeight: shown
    Layout.fillWidth: true
    Layout.preferredHeight: shown
    clip: true
    visible: shown > 0.5

    Behavior on shown {
        NumberAnimation { duration: Theme.durationSlow; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeOut }
    }

    ColumnLayout {
        id: inner
        width: root.width
        spacing: 0
        enabled: root.open
    }
}
