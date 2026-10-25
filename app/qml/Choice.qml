import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// One choice of several, as buttons that wrap to the next line when they
// don't fit (the Generate tab's generators and kinds). options:
// [{value, text, enabled}]
Flow {
    id: group
    property var options: []
    property string value
    property string label
    signal chosen(string value)
    spacing: 8
    Accessible.role: Accessible.Grouping
    Accessible.name: label

    Repeater {
        model: group.options
        delegate: Button {
            id: option
            required property var modelData
            readonly property bool on: group.value === modelData.value
            checkable: true
            checked: on
            enabled: modelData.enabled !== false
            implicitHeight: 36
            topPadding: 0
            bottomPadding: 0
            hoverEnabled: true
            leftPadding: 14
            rightPadding: 14
            text: modelData.text
            font.family: Lang.uiFont
            font.pixelSize: 14
            font.weight: on ? Font.DemiBold : Font.Normal
            Accessible.name: text
            onClicked: group.chosen(modelData.value)
            background: Rectangle {
                color: option.on ? Theme.selectedBg : (option.down ? Theme.pressedBg : (option.hovered ? Theme.hoverBg : Theme.controlBg))
                border.color: option.on || option.visualFocus ? Theme.accent : (option.hovered ? Theme.controlBorderHover : Theme.controlBorder)
                border.width: option.visualFocus ? 2 : 1
                radius: 8
                opacity: option.enabled ? 1 : 0.5
            }
            contentItem: AppText {
                ui: option.text
                font: option.font
                color: option.on ? Theme.selectedText : (option.enabled ? Theme.text : Theme.muted)
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
