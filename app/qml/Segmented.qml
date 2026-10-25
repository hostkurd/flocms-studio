import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// One choice of a few, side by side (Settings: language, theme).
// options: [{value, text, lang}] ; `lang` sets the font for a language's
// own name (العربية, کوردی).
RowLayout {
    id: group
    property var options: []
    property string value
    property string label
    signal chosen(string value)
    spacing: 6
    Accessible.role: Accessible.Grouping
    Accessible.name: label

    Repeater {
        model: group.options
        delegate: Button {
            id: option
            required property var modelData
            required property int index
            readonly property bool on: group.value === modelData.value
            checkable: true
            checked: on
            implicitHeight: 36
            topPadding: 0
            bottomPadding: 0
            hoverEnabled: true
            leftPadding: 14
            rightPadding: 14
            text: modelData.text
            font.family: modelData.lang ? "Noto Sans Arabic" : Lang.uiFont
            font.pixelSize: 14
            font.weight: on ? Font.DemiBold : Font.Normal
            Accessible.name: text
            onClicked: group.chosen(modelData.value)
            background: Rectangle {
                color: option.on ? Theme.selectedBg : (option.down ? Theme.pressedBg : (option.hovered ? Theme.hoverBg : Theme.controlBg))
                border.color: option.visualFocus || option.on ? Theme.accent : (option.hovered ? Theme.controlBorderHover : Theme.controlBorder)
                border.width: option.visualFocus ? 2 : 1
                radius: 8
            }
            contentItem: AppText {
                ui: option.text
                font: option.font
                color: option.on ? Theme.selectedText : Theme.text
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
