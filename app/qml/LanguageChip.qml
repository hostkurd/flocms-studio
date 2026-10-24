import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// A language of the new project (board 6): chosen or not, and the default.
AbstractButton {
    id: chip
    property string name
    property bool chosen: false
    property bool isDefault: false
    implicitHeight: 36
    leftPadding: 12
    rightPadding: 12
    implicitWidth: row.implicitWidth + leftPadding + rightPadding
    Accessible.role: Accessible.CheckBox
    Accessible.checked: chosen
    Accessible.name: name + (isDefault ? " · " + qsTr("default") : "")
    background: Rectangle {
        radius: height / 2
        color: chip.chosen ? Theme.selectedBg : Theme.surface
        border.color: chip.visualFocus ? Theme.accent : (chip.chosen ? Theme.accent : Theme.borderStrong)
        border.width: chip.visualFocus ? 2 : 1
    }
    contentItem: RowLayout {
        id: row
        spacing: 6
        Icon { visible: chip.chosen; path: Icons.check; size: 14; color: Theme.selectedText }
        AppText { text: chip.name; color: chip.chosen ? Theme.selectedText : Theme.text; font.family: "Noto Sans Arabic" }
        AppText { visible: chip.isDefault; ui: qsTr("default"); color: Theme.muted; font.pixelSize: 12 }
    }
}
