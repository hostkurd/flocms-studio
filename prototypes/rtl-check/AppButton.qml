import QtQuick
import QtQuick.Controls
import RtlCheck.Backend

Button {
    id: control
    property bool primary: false
    property bool danger: false
    property bool small: false

    implicitHeight: small ? 34 : 40
    leftPadding: small ? 12 : 14
    rightPadding: small ? 12 : 14
    font.family: Lang.uiFont
    Accessible.name: text.replace(/`/g, "")
    font.pixelSize: small ? 13 : 14

    background: Rectangle {
        radius: 8
        color: control.primary ? Theme.accent
             : (control.checked ? Theme.selectedBg : Theme.surface)
        border.color: control.primary ? Theme.accent
                    : (control.checked ? Theme.accent : Theme.borderStrong)
        border.width: 1
        opacity: control.down ? 0.8 : 1
    }

    contentItem: RichText {
        raw: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        color: control.primary ? Theme.accentText
             : (control.danger ? Theme.bad : Theme.text)
    }
}
