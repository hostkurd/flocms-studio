import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// The project header's "More" list: Remove from Studio (the files stay)
// and Delete project… (to the Recycle Bin / Trash, asked first).
Popup {
    id: menu
    signal removeChosen()
    signal deleteChosen()
    padding: 6
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: Theme.surface; radius: 10; border.color: Theme.border }

    component Item_: Button {
        id: row
        property string iconPath
        property bool danger: false
        Layout.fillWidth: true
        implicitHeight: 36
        leftPadding: 10
        rightPadding: 14
        hoverEnabled: true
        font.family: Lang.uiFont
        font.pixelSize: 14
        Accessible.name: text
        Accessible.role: Accessible.MenuItem
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        background: Rectangle {
            radius: 6
            color: row.down ? Theme.pressedBg : (row.hovered || row.visualFocus ? Theme.hoverBg : "transparent")
        }
        contentItem: RowLayout {
            spacing: 10
            Icon { path: row.iconPath; size: 16; color: row.danger ? Theme.badText : Theme.navText }
            AppText { ui: row.text; color: row.danger ? Theme.badText : Theme.text; Layout.fillWidth: true }
        }
    }

    onOpened: removeItem.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: 2
        Accessible.role: Accessible.PopupMenu
        LayoutMirroring.enabled: Lang.rtl
        LayoutMirroring.childrenInherit: true
        Item_ {
            id: removeItem
            KeyNavigation.down: deleteItem
            KeyNavigation.up: deleteItem
            text: qsTr("Remove from Studio")
            iconPath: Icons.cross
            onClicked: { menu.close(); menu.removeChosen() }
        }
        Item_ {
            id: deleteItem
            KeyNavigation.down: removeItem
            KeyNavigation.up: removeItem
            text: qsTr("Delete project…")
            iconPath: Icons.trash
            danger: true
            onClicked: { menu.close(); menu.deleteChosen() }
        }
    }
}
