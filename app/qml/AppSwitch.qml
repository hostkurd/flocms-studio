import QtQuick
import QtQuick.Controls.Basic
import FloStudio.Backend

// A switch in the theme's colours (Settings > FloServer); the label wraps
// and sits on the far side of the track (the left in RTL).
Switch {
    id: control
    implicitHeight: Math.max(40, label.implicitHeight)
    padding: 0
    spacing: 12
    indicator: Rectangle {
        x: control.mirrored ? control.width - width : 0
        y: (control.height - height) / 2
        width: 40
        height: 22
        radius: 11
        color: control.checked ? Theme.accent : Theme.controlBorder
        opacity: control.enabled ? 1 : 0.45
        border.color: control.visualFocus ? Theme.text : "transparent"
        border.width: 2
        Rectangle {
            // The knob: at the end when on, mirrored in RTL
            x: (control.checked !== control.mirrored) ? parent.width - width - 3 : 3
            y: 3
            width: 16
            height: 16
            radius: 8
            color: control.checked ? Theme.accentText : Theme.surface
            Behavior on x { NumberAnimation { duration: 90 } }
        }
    }
    contentItem: AppText {
        id: label
        ui: control.text
        opacity: control.enabled ? 1 : 0.6
        font.weight: Font.DemiBold
        leftPadding: control.mirrored ? 0 : control.indicator.width + control.spacing
        rightPadding: control.mirrored ? control.indicator.width + control.spacing : 0
        wrapMode: Text.Wrap
    }
}
