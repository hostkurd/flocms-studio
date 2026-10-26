import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T
import FloStudio.Backend

// "Delete project…": the folder goes to the Recycle Bin / Trash, so it can
// be brought back; the database on Studio's own MariaDB only when ticked
// (off by default: that one can't be brought back).
Popup {
    id: dialog
    modal: true
    focus: true
    width: Math.min(540, parent ? parent.width - 48 : 540)
    padding: 24
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string path
    property string name
    property var choices: ({})
    signal accepted(bool database)

    function ask(projectPath, projectName) {
        path = projectPath
        name = projectName
        choices = Studio.deleteChoices(projectPath)
        database.checked = false
        open()
    }

    readonly property string trashName: Qt.platform.os === "windows" ? qsTr("Recycle Bin") : qsTr("Trash")

    background: Rectangle { color: Theme.surface; radius: 14; border.color: Theme.border }
    T.Overlay.modal: Rectangle { color: Theme.overlay }

    contentItem: ColumnLayout {
        spacing: 14
        LayoutMirroring.enabled: Lang.rtl
        LayoutMirroring.childrenInherit: true

        Heading { ui: qsTr("Delete %1?").arg(dialog.name); wrapMode: Text.Wrap; Layout.fillWidth: true }
        AppText {
            ui: qsTr("Its folder goes to the %1, so you can still bring it back from there. Its website server stops, and the project leaves Studio.").arg(dialog.trashName)
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Mono {
            text: dialog.path
            color: Theme.muted
            wrapMode: Text.WrapAnywhere
            Layout.fillWidth: true
        }
        ColumnLayout {
            visible: !!dialog.choices.onStudioServer
            spacing: 4
            Layout.fillWidth: true
            AppCheck {
                id: database
                text: qsTr("Also delete its database on Studio's MariaDB")
                Layout.fillWidth: true
            }
            RichText {
                raw: (dialog.choices.userShared
                      ? qsTr("The database `%1`, with all its data. The user `%2` stays: another project uses it.")
                      : qsTr("The database `%1`, with all its data, and its user `%2`."))
                         .arg(dialog.choices.database || "").arg(dialog.choices.user || "")
                     + " " + qsTr("This can't be undone.")
                color: database.checked ? Theme.badText : Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 8
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("Cancel"); onClicked: dialog.close() }
            AppButton {
                text: qsTr("Delete project")
                iconPath: Icons.trash
                danger: true
                onClicked: { dialog.close(); dialog.accepted(database.checked) }
            }
        }
    }
}
