import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// A quick command on the Overview (redesign board 1): an icon, the
// command's name and one line on what it does or which `php flo` command
// it runs. Looks like AppButton, two lines tall.
Button {
    id: tile
    property string iconPath: ""
    property string detail: ""
    property bool detailMono: false // the detail is a command, in the code font

    implicitHeight: Math.max(58, row.implicitHeight + topPadding + bottomPadding)
    implicitWidth: row.implicitWidth + leftPadding + rightPadding
    topPadding: 10
    bottomPadding: 10
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: true
    font.family: Lang.uiFont
    font.pixelSize: 14
    Accessible.name: text + (detail ? ", " + detail : "")

    background: Rectangle {
        radius: 10
        color: tile.down ? Theme.pressedBg : (tile.hovered && tile.enabled ? Theme.hoverBg : Theme.controlBg)
        border.color: tile.visualFocus ? Theme.accent : (tile.hovered && tile.enabled ? Theme.controlBorderHover : Theme.controlBorder)
        border.width: tile.visualFocus ? 2 : 1
        opacity: tile.enabled ? 1 : 0.45
        Behavior on color { ColorAnimation { duration: 90 } }
    }

    contentItem: RowLayout {
        id: row
        spacing: 12
        opacity: tile.enabled ? 1 : 0.6
        Icon {
            path: tile.iconPath
            size: 20
            color: Theme.link
            Layout.alignment: Qt.AlignVCenter
        }
        ColumnLayout {
            spacing: 2
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            AppText {
                ui: tile.text
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            AppText {
                visible: !tile.detailMono && tile.detail !== ""
                ui: tile.detail
                color: Theme.muted
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Mono {
                visible: tile.detailMono && tile.detail !== ""
                text: tile.detail
                color: Theme.muted
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }
}
