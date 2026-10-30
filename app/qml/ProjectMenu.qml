import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// The project header's "More" list: Open terminal (design note of 30 Oct,
// B-D1), then Remove from Studio (the files stay) and Delete project… (to
// the Recycle Bin / Trash, asked first).
Popup {
    id: menu
    signal terminalChosen()
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
        property string shortcut // shown at the end, e.g. Ctrl+`
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
            Mono {
                visible: row.shortcut !== ""
                text: row.shortcut
                color: Theme.muted
                font.pixelSize: 12
                LayoutMirroring.enabled: false
            }
        }
    }

    onOpened: terminalItem.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: 2
        Accessible.role: Accessible.PopupMenu
        LayoutMirroring.enabled: Lang.rtl
        LayoutMirroring.childrenInherit: true
        Item_ {
            id: terminalItem
            KeyNavigation.down: removeItem
            KeyNavigation.up: deleteItem
            text: qsTr("Open terminal")
            iconPath: Icons.terminal
            shortcut: Qt.platform.os === "osx" ? "\u2303`" : "Ctrl+`"
            onClicked: { menu.close(); menu.terminalChosen() }
        }
        Rectangle {
            color: Theme.rule
            implicitHeight: 1
            Layout.fillWidth: true
            Layout.topMargin: 3
            Layout.bottomMargin: 3
            Layout.leftMargin: 6
            Layout.rightMargin: 6
        }
        Item_ {
            id: removeItem
            KeyNavigation.down: deleteItem
            KeyNavigation.up: terminalItem
            text: qsTr("Remove from Studio")
            iconPath: Icons.cross
            onClicked: { menu.close(); menu.removeChosen() }
        }
        Item_ {
            id: deleteItem
            KeyNavigation.down: terminalItem
            KeyNavigation.up: removeItem
            text: qsTr("Delete project…")
            iconPath: Icons.trash
            danger: true
            onClicked: { menu.close(); menu.deleteChosen() }
        }
    }
}
