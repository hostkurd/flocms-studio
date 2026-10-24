import QtQuick
import QtQuick.Controls.Basic

// A radio button in the theme's colours.
RadioButton {
    id: control
    implicitWidth: 40
    implicitHeight: 40
    padding: 0
    indicator: Rectangle {
        x: (control.width - width) / 2
        y: (control.height - height) / 2
        width: 20
        height: 20
        radius: 10
        color: Theme.surface
        border.color: control.checked || control.visualFocus ? Theme.accent : Theme.borderStrong
        border.width: control.visualFocus ? 2 : 1.5
        Rectangle {
            anchors.centerIn: parent
            width: 10
            height: 10
            radius: 5
            color: Theme.accent
            visible: control.checked
        }
    }
    contentItem: Item {}
}
