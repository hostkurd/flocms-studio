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
    property bool iconFilled: false // a solid shape (play, stop)
    property bool iconDirectional: false // flipped in right-to-left languages (a "back" arrow)
    // A label with a `code word` is rich text. Not from `visible`: a hidden
    // button (one that measures a column) must keep its label and width
    readonly property bool richLabel: text.indexOf("`") >= 0

    implicitHeight: small ? 30 : 36
    topPadding: 0
    bottomPadding: 0
    leftPadding: small ? 10 : 14
    rightPadding: small ? 10 : 14
    font.family: Lang.uiFont
    font.pixelSize: small ? 13 : 14
    font.weight: Font.Medium
    Accessible.name: text.replace(/`/g, "")
    hoverEnabled: true

    background: Rectangle {
        radius: 8
        // Hover and pressed show, as on native buttons; disabled is the only
        // faded state
        color: control.primary
               ? (control.down ? Theme.accentPressed : (control.hovered && control.enabled ? Theme.accentHover : Theme.accent))
               : (control.down ? Theme.pressedBg : (control.checked ? Theme.selectedBg
                  : (control.hovered && control.enabled ? Theme.hoverBg : Theme.controlBg)))
        border.color: control.primary ? (control.visualFocus ? Theme.text : "transparent")
                      : (control.checked || control.visualFocus ? Theme.accent
                         : (control.hovered && control.enabled ? Theme.controlBorderHover : Theme.controlBorder))
        border.width: control.dashed && !control.visualFocus ? 0 : (control.visualFocus ? 2 : 1)
        opacity: control.enabled ? 1 : 0.45
        Behavior on color { ColorAnimation { duration: 90 } }
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
            filled: control.iconFilled
            color: control.primary ? Theme.accentText : (control.danger ? Theme.bad : Theme.text)
        }
        // Plain text, centred on the button's middle: rich text (only for a
        // label with a `code word`) lays its line out taller, low in the button
        AppText {
            visible: !control.richLabel
            ui: control.richLabel ? "" : control.text
            font: control.font
            color: control.primary ? Theme.accentText : (control.danger ? Theme.bad : Theme.text)
            Layout.alignment: Qt.AlignVCenter
        }
        RichText {
            visible: control.richLabel
            raw: control.richLabel ? control.text : ""
            font: control.font
            color: control.primary ? Theme.accentText : (control.danger ? Theme.bad : Theme.text)
            Layout.alignment: Qt.AlignVCenter
        }
        Item { Layout.fillWidth: true }
    }
}
