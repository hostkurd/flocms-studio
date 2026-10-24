import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

Button {
    id: control
    property bool primary: false
    property bool danger: false
    property bool small: false
    property bool dashed: false
    property string iconPath: ""
    property bool iconDirectional: false // flipped in right-to-left languages (a "back" arrow)

    implicitHeight: small ? 34 : 40
    leftPadding: small ? 12 : 14
    rightPadding: small ? 12 : 14
    font.family: Lang.uiFont
    font.pixelSize: small ? 13 : 14
    font.weight: Font.Medium
    Accessible.name: text.replace(/`/g, "")
    hoverEnabled: true

    background: Rectangle {
        radius: 8
        color: control.primary ? Theme.accent : (control.checked ? Theme.selectedBg : Theme.surface)
        border.color: control.primary ? Theme.accent : (control.checked || control.visualFocus ? Theme.accent : Theme.borderStrong)
        border.width: control.dashed && !control.visualFocus ? 0 : (control.visualFocus ? 2 : 1)
        opacity: !control.enabled ? 0.45 : (control.down ? 0.8 : 1)
        DashedBox {
            visible: control.dashed && !control.visualFocus
            anchors.fill: parent
        }
    }

    contentItem: RowLayout {
        spacing: 8
        opacity: control.enabled ? 1 : 0.6
        Item { Layout.fillWidth: true }
        Icon {
            visible: control.iconPath !== ""
            path: control.iconPath
            size: control.small ? 14 : 16
            directional: control.iconDirectional
            color: control.primary ? Theme.accentText : (control.danger ? Theme.bad : Theme.text)
        }
        RichText {
            raw: control.text
            font: control.font
            color: control.primary ? Theme.accentText : (control.danger ? Theme.bad : Theme.text)
        }
        Item { Layout.fillWidth: true }
    }
}
