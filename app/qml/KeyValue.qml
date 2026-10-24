import QtQuick
import QtQuick.Layouts

// "Name    Studio Full" on one line of a card.
RowLayout {
    property string key
    property string value
    property bool mono: false
    Layout.fillWidth: true
    implicitHeight: 28
    spacing: 12
    AppText { ui: parent.key; color: Theme.muted; font.pixelSize: 13 }
    RichText {
        visible: !parent.mono
        raw: parent.value
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
    Mono {
        visible: parent.mono
        text: parent.value
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideMiddle
        Layout.fillWidth: true
    }
}
