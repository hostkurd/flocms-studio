import QtQuick
import QtQuick.Controls.Basic
import FloStudio.Backend

TextField {
    id: field
    property bool mono: false
    // As wide as its place in the layout, never as wide as what is typed
    implicitWidth: 160
    implicitHeight: 36
    hoverEnabled: true
    leftPadding: 12
    rightPadding: 12
    color: Theme.text
    placeholderTextColor: Theme.muted
    font.family: mono ? Theme.monoFont : Lang.uiFont
    font.pixelSize: mono ? 13 : 14
    horizontalAlignment: TextInput.AlignLeft
    verticalAlignment: TextInput.AlignVCenter
    topPadding: 0
    bottomPadding: 0
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentText
    selectByMouse: true
    background: Rectangle {
        radius: 8
        color: field.readOnly ? Theme.rule : Theme.bg
        border.color: field.activeFocus ? Theme.accent : (field.hovered && field.enabled ? Theme.controlBorderHover : Theme.controlBorder)
        border.width: field.activeFocus ? 2 : 1
    }
}
