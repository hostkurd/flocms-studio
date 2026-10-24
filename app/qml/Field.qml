import QtQuick
import QtQuick.Controls.Basic
import FloStudio.Backend

TextField {
    id: field
    property bool mono: false
    implicitHeight: 40
    leftPadding: 12
    rightPadding: 12
    color: Theme.text
    placeholderTextColor: Theme.muted
    font.family: mono ? Theme.monoFont : Lang.uiFont
    font.pixelSize: mono ? 13 : 14
    horizontalAlignment: TextInput.AlignLeft
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentText
    selectByMouse: true
    background: Rectangle {
        radius: 8
        color: Theme.bg
        border.color: field.activeFocus ? Theme.accent : Theme.borderStrong
        border.width: field.activeFocus ? 2 : 1
    }
}
