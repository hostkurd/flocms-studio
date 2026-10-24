import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
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
                    currentIndex: Studio.page === "settings" ? 2 : (Studio.page === "project" ? 1 : 0)
                    WelcomePage { onDownloadRequested: downloads.open() }
                    ProjectPage {}
                    SettingsPage { onDownloadRequested: downloads.open() }
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
