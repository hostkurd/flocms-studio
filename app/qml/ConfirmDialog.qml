import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T
import FloStudio.Backend

// Asks before a command that changes the database (stage 5). For
// migrate:fresh the person types the database's name first.
Popup {
    id: dialog
    modal: true
    focus: true
    width: Math.min(520, parent ? parent.width - 48 : 520)
    padding: 24
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string title
    property string message   // may hold `code` words
    property string command
    property string confirmText
    property bool danger: true
    property string typeName  // non-empty: this must be typed first
    signal accepted(string typed)

    function ask(options) {
        title = options.title
        message = options.message
        command = options.command || ""
        confirmText = options.confirmText
        danger = options.danger !== false
        typeName = options.typeName || ""
        typed.text = ""
        open()
    }

    background: Rectangle { color: Theme.surface; radius: 14; border.color: Theme.border }
    T.Overlay.modal: Rectangle { color: Theme.overlay }

    contentItem: ColumnLayout {
        spacing: 14
        LayoutMirroring.enabled: Lang.rtl
        LayoutMirroring.childrenInherit: true

        Heading { ui: dialog.title; wrapMode: Text.Wrap; Layout.fillWidth: true }
        RichText { raw: dialog.message; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Mono {
            visible: dialog.command !== ""
            text: dialog.command
            color: Theme.muted
            wrapMode: Text.WrapAnywhere
            Layout.fillWidth: true
        }
        ColumnLayout {
            visible: dialog.typeName !== ""
            spacing: 6
            Layout.fillWidth: true
            RichText { raw: qsTr("Type the database's name, `%1`, to go on:").arg(dialog.typeName); wrapMode: Text.Wrap; Layout.fillWidth: true }
            Field {
                id: typed
                mono: true
                Layout.fillWidth: true
                Accessible.name: qsTr("Database name")
                onAccepted: if (go.enabled) go.clicked()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 8
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("Cancel"); onClicked: dialog.close() }
            AppButton {
                id: go
                text: dialog.confirmText
                primary: !dialog.danger
                danger: dialog.danger
                enabled: dialog.typeName === "" || typed.text.trim() === dialog.typeName.trim()
                onClicked: { dialog.close(); dialog.accepted(typed.text) }
            }
        }
    }
}
