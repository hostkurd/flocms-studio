import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import FloStudio.Backend

// No project yet: add the folder the projects are in, or one project.
Item {
    id: page
    signal downloadRequested()

    FolderDialog {
        id: projectsFolder
        title: qsTr("The folder your FloCMS projects are in")
        onAccepted: Studio.addProjectFolder(selectedFolder)
    }
    FolderDialog {
        id: oneProject
        title: qsTr("Open a FloCMS project")
        onAccepted: Studio.addProject(selectedFolder)
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(560, parent.width - 64)
        spacing: 16

        Heading { level: 1; ui: qsTr("Welcome to FloCMS Studio"); Layout.fillWidth: true }
        AppText {
            ui: qsTr("Studio finds your FloCMS projects, runs them and shows what they need. Show it where your projects are.")
            color: Theme.muted
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        RowLayout {
            spacing: 10
            AppButton { text: qsTr("Add a projects folder…"); primary: true; iconPath: Icons.folder; onClicked: projectsFolder.open() }
            AppButton { text: qsTr("Open one project…"); onClicked: oneProject.open() }
        }

        Card {
            visible: Studio.toolsMissing
            Layout.fillWidth: true
            Layout.topMargin: 12
            RowLayout {
                spacing: 12
                Icon { path: Icons.warning; color: Theme.warn; size: 20 }
                RichText {
                    raw: qsTr("Studio could not find PHP or Composer on this computer, so projects can't run yet.")
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                AppButton { text: qsTr("Download…"); iconPath: Icons.download; onClicked: page.downloadRequested() }
            }
        }
    }
}
