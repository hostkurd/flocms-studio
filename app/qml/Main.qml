import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T
import FloStudio.Backend

// The window: the sidebar and the page it shows (welcome, a project,
// Settings), the problem bar, and the downloads dialog. The whole layout
// mirrors for Arabic and Kurdish (LayoutMirroring on the root item, D9).
ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    color: Theme.bg
    title: Studio.page === "project" && Studio.current.name
           ? qsTr("%1 – FloCMS Studio").arg(Studio.current.name) : "FloCMS Studio"

    // Quitting while a migration runs would stop it halfway: ask first
    property bool quitAnyway: false
    onClosing: (close) => {
        if (Studio.commandRunning && !window.quitAnyway) {
            close.accepted = false
            quitQuestion.ask({
                title: qsTr("A command is still running"),
                message: qsTr("Quitting now stops it halfway, which can leave the database half migrated."),
                confirmText: qsTr("Quit anyway"),
                danger: true
            })
        }
    }
    ConfirmDialog {
        id: quitQuestion
        parent: T.Overlay.overlay
        anchors.centerIn: parent
        onAccepted: { window.quitAnyway = true; window.close() }
    }

    Item {
        id: root
        anchors.fill: parent
        LayoutMirroring.enabled: Lang.rtl
        LayoutMirroring.childrenInherit: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Sidebar {
                Layout.preferredWidth: 240
                Layout.fillHeight: true
                onDownloadRequested: downloads.open()
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                ProblemBar {
                    Layout.fillWidth: true
                    Layout.margins: 16
                    Layout.bottomMargin: 0
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: ({ project: 1, settings: 2, new: 3, about: 4 })[Studio.page] || 0
                    // Each page is made the first time it is shown, then kept;
                    // `index` must match currentIndex above
                    ShownOnce { index: 0; sourceComponent: Component { WelcomePage { onDownloadRequested: downloads.open() } } }
                    ShownOnce { index: 1; sourceComponent: Component { ProjectPage {} } }
                    ShownOnce { index: 2; sourceComponent: Component { SettingsPage { onDownloadRequested: downloads.open() } } }
                    ShownOnce { index: 3; sourceComponent: Component { NewProjectPage {} } }
                    ShownOnce { index: 4; sourceComponent: Component { AboutPage {} } }
                }
            }
        }

        DownloadsDialog {
            id: downloads
            anchors.centerIn: parent
        }
    }

    Component.onCompleted: {
        // A demo can open on the downloads dialog (screenshots of board 7)
        if (Studio.demoDialog === "downloads")
            downloads.open()
    }
}
