import QtQuick
import QtQuick.Controls.Basic
import FloStudio.Backend

// A checkbox in the theme's colours; text is optional.
CheckBox {
    id: control
    implicitHeight: 40
    padding: 0
    spacing: 10
    indicator: Rectangle {
        x: control.text === "" ? (control.width - width) / 2 : (control.mirrored ? control.width - width - 10 : 10)
        y: (control.height - height) / 2
        width: 20
        height: 20
        radius: 5
        color: control.checked ? Theme.accent : Theme.surface
        border.color: control.checked || control.visualFocus ? Theme.accent : Theme.borderStrong
        border.width: control.visualFocus ? 2 : 1.5
        Icon {
            anchors.centerIn: parent
            visible: control.checked
            path: Icons.check
            color: Theme.accentText
            stroke: 3
            size: 14
        }
    }
    contentItem: AppText {
        visible: control.text !== ""
        ui: control.text
        // Room for the box, on the side it is on (the right in RTL)
        leftPadding: control.mirrored ? 0 : control.indicator.width + control.spacing + 10
        rightPadding: control.mirrored ? control.indicator.width + control.spacing + 10 : 0
        wrapMode: Text.Wrap
    }
}
