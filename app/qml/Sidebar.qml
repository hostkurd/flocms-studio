import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import FloStudio.Backend

// Board 1's sidebar: the projects, "New project", "Open existing folder…",
// and the tools on this computer. In Settings it lists Settings' sections.
Rectangle {
    id: sidebar
    signal downloadRequested()
    color: Theme.sidebar

    // The border sits on the end side: right in LTR, left in RTL.
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.border
    }

    FolderDialog {
        id: openFolder
        title: qsTr("Open a FloCMS project")
        onAccepted: Studio.addProject(selectedFolder)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 18
        anchors.bottomMargin: 16
        anchors.leftMargin: 14
        anchors.rightMargin: 15
        spacing: 4

        RowLayout {
            spacing: 10
            Layout.leftMargin: 6
            Layout.bottomMargin: 16
            Rectangle {
                width: 32; height: 32; radius: 8
                color: Theme.brand
                AppText {
                    anchors.centerIn: parent
                    text: "F"
                    color: Theme.brandText
                    font.family: "Bricolage Grotesque"
                    font.weight: Font.Bold
                    font.pixelSize: 17
                }
            }
            AppText { text: "FloCMS Studio"; font.family: "Bricolage Grotesque"; font.weight: Font.Bold; font.pixelSize: 17 }
        }

        // --- Projects ---------------------------------------------------
        ColumnLayout {
            visible: Studio.page !== "settings"
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4

            SectionLabel { ui: qsTr("Projects"); Layout.leftMargin: 10; Layout.bottomMargin: 6 }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.maximumHeight: contentHeight
                clip: true
                spacing: 2
                model: Studio.projects
                boundsBehavior: Flickable.StopAtBounds
                Accessible.name: qsTr("Projects")
                delegate: NavItem {
                    required property string name
                    required property string path
                    required property bool missing
                    required property bool running
                    width: ListView.view.width
                    text: name
                    selected: Studio.page === "project" && Studio.currentPath === path
                    dot: missing ? Theme.bad : (running ? Theme.ok : Theme.idle)
                    trailing: missing ? qsTr("Missing") : (running ? qsTr("Running") : "")
                    trailingColor: missing ? Theme.badText : Theme.okText
                    onClicked: Studio.selectProject(path)
                }
            }

            AppButton {
                text: qsTr("New project")
                iconPath: Icons.plus
                Layout.fillWidth: true
                Layout.topMargin: 8
                enabled: false // the wizard comes in stage 6
                ToolTip.visible: hovered
                ToolTip.text: qsTr("The new project wizard comes in a later version.")
                hoverEnabled: true
            }
            AppButton {
                text: qsTr("Open existing folder…")
                dashed: true
                Layout.fillWidth: true
                onClicked: openFolder.open()
            }
            Item { Layout.fillHeight: true }
        }

        // --- Settings' sections -----------------------------------------
        ColumnLayout {
            visible: Studio.page === "settings"
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 2

            AppButton {
                text: qsTr("Back to projects")
                iconPath: Icons.back
                small: true
                Layout.bottomMargin: 12
                iconDirectional: true
                onClicked: Studio.showProjects()
            }
            SectionLabel { ui: qsTr("Settings"); Layout.leftMargin: 10; Layout.bottomMargin: 6 }
            Repeater {
                model: [
                    { id: "general", name: qsTr("General") },
                    { id: "tools", name: qsTr("PHP and tools") },
                    { id: "server", name: qsTr("Server") },
                    { id: "editor", name: qsTr("Editor") },
                    { id: "folders", name: qsTr("Project folders") }
                ]
                delegate: NavItem {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.name
                    selected: Studio.settingsSection === modelData.id
                    onClicked: Studio.showSettings(modelData.id)
                }
            }
            Item { Layout.fillHeight: true }
            AppText {
                ui: qsTr("FloCMS Studio %1").arg(Studio.version)
                color: Theme.muted
                font.pixelSize: 12
                Layout.leftMargin: 6
            }
        }

        // --- This computer ----------------------------------------------
        ColumnLayout {
            visible: Studio.page !== "settings"
            Layout.fillWidth: true
            spacing: 2

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border; Layout.bottomMargin: 10 }
            SectionLabel { ui: qsTr("This computer"); Layout.leftMargin: 6; Layout.bottomMargin: 4 }

            Repeater {
                model: Studio.tools
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    implicitHeight: 28
                    AppText { ui: Texts.toolName(modelData.tool); Layout.fillWidth: true }
                    AppText {
                        readonly property bool bad: modelData.state === "missing" || modelData.state === "unsupported" || modelData.state === "failed"
                        ui: Texts.toolState(modelData)
                        font.family: modelData.state === "found" || modelData.state === "running" ? Theme.monoFont : Lang.uiFont
                        font.pixelSize: 13
                        color: bad ? Theme.badText : (modelData.state === "optional" ? Theme.muted : Theme.okText)
                    }
                }
            }
            AppText {
                visible: Studio.checkingTools && Studio.tools.length === 0
                ui: qsTr("Looking for PHP, Composer and MariaDB…")
                color: Theme.muted
                font.pixelSize: 13
                Layout.leftMargin: 6
            }
            AppButton {
                visible: Studio.toolsMissing
                text: qsTr("Download…")
                iconPath: Icons.download
                small: true
                Layout.leftMargin: 6
                Layout.topMargin: 4
                onClicked: sidebar.downloadRequested()
            }
            NavItem {
                Layout.fillWidth: true
                Layout.topMargin: 6
                text: qsTr("Settings")
                iconPath: Icons.gear
                onClicked: Studio.showSettings("general")
            }
        }
    }
}
