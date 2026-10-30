import QtQuick
import QtQuick.Layouts

// A short heading over a run of rows: 13px, semibold, muted, sentence case.
UiText {
    Layout.fillWidth: true
    Layout.topMargin: 20
    Layout.bottomMargin: 4
    size: Theme.caption
    color: Theme.textMuted
    font.weight: Font.DemiBold
    elide: Text.ElideRight
}
