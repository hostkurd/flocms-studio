import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Templates as T
import FloStudio.Backend

// A project: board 1's header and tabs. Overview reads the project's
// files and shows its server, database, Doctor and quick commands; Logs
// follows the server (stage 4); Database and Doctor (stage 5). The
// questions before commands that change the database are asked here, for
// Overview and Database alike.
Item {
    id: page
    readonly property var info: Studio.current
    readonly property string serverState: Studio.server.state || "stopped"
    readonly property bool running: serverState === "running"
    // Asks what plan() says, then runs it
    function startAction(action) {
        const checks = Studio.checks
        const plan = checks.plan(action)
        if (plan.confirm === "none") {
            // .env changed between plan and run: ask with what it says now
            if (!checks.runAction(action, "", plan.production) && !checks.busy)
                Qt.callLater(page.startAction, action)
            return
        }
        const texts = {
            migrate: {
                title: qsTr("Run the migrations on this production site?"),
                message: qsTr("`APP_ENV` says this is a production site. %Ln migration(s) will change its database.", "", plan.pending),
                confirm: qsTr("Run migrations"), danger: false
            },
            rollback: {
                title: qsTr("Roll back the last batch?"),
                message: qsTr("This undoes the migrations of the last batch. Tables or columns they made, and the data in them, are dropped."),
                confirm: qsTr("Roll back"), danger: true
            },
            seed: {
                title: qsTr("Seed the database?"),
                message: qsTr("This runs `database/seeders/DatabaseSeeder`, which adds rows to the tables."),
                confirm: qsTr("Seed"), danger: false
            },
            fresh: {
                title: qsTr("Drop every table and migrate again?"),
                message: qsTr("`migrate:fresh` deletes every table in this database, with all its data, then runs every migration again. This can't be undone."),
                confirm: qsTr("Drop and migrate"), danger: true
            }
        }[action]
        // The answer goes to this project's checks, as planned
        confirm.action = action
        confirm.checks = checks
        confirm.production = plan.production
        confirm.ask({
            title: texts.title,
            message: texts.message + (plan.production && action !== "migrate" ? " " + qsTr("This is a production site.") : ""),
            command: plan.command,
            confirmText: texts.confirm,
            danger: texts.danger,
            typeName: plan.confirm === "typeDatabase" ? (plan.database || "") : ""
        })
    }

    ConfirmDialog {
        id: confirm
        property string action
        property var checks: null
        property bool production: false
        parent: T.Overlay.overlay
        anchors.centerIn: parent
        onAccepted: (typed) => {
            // Refused when .env changed while the question was open (production,
            // or the database fresh would drop): ask again with what it says now
            if (checks && !checks.runAction(action, typed, production) && !checks.busy && checks === Studio.checks)
                Qt.callLater(page.startAction, action)
        }
    }


    // A demo can open on one of the questions (screenshots)
    Component.onCompleted: {
        if (["migrate", "rollback", "seed", "fresh"].indexOf(Studio.demoDialog) >= 0)
            Qt.callLater(page.startAction, Studio.demoDialog)
    }

    function openTab(name) {
        page.tab = Math.max(0, ["overview", "generate", "database", "logs", "packages", "doctor"].indexOf(name))
    }

    readonly property bool hasAdmin: ["siteAdmin", "siteAdminApi", "admin", "apiAdmin"].indexOf(info.type) >= 0
    property int tab: Math.max(0, ["overview", "generate", "database", "logs", "packages", "doctor"].indexOf(Studio.demoTab))

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        anchors.topMargin: 24
        spacing: 18

        // Header: name and folder, then the actions; the actions go below
        // the name when both don't fit on one line (longer labels in Kurdish)
        GridLayout {
            id: header
            Layout.fillWidth: true
            // From the page's width (minus the margins), not the header's
            // own: the wrapping buttons change the header's width
            readonly property real room: Math.max(0, page.width - 56)
            columns: nameColumn.implicitWidth + actions.oneLine + columnSpacing <= room ? 2 : 1
            columnSpacing: 16
            rowSpacing: 12
            ColumnLayout {
                id: nameColumn
                spacing: 4
                Layout.fillWidth: true
                Layout.minimumWidth: 160
                Heading { level: 1; text: page.info.name || ""; elide: Text.ElideRight; Layout.fillWidth: true }
                Mono { text: page.info.path || ""; color: Theme.muted; elide: Text.ElideMiddle; Layout.fillWidth: true }
            }
            // On their own line they wrap, so long labels (Kurdish) never
            // push the page past the window's edge
            Flow {
                id: actions
                spacing: 8
                // Their width on one line
                readonly property real oneLine: {
                    let total = 0
                    let shown = 0
                    for (let i = 0; i < children.length; ++i) {
                        if (children[i].visible) {
                            total += children[i].implicitWidth
                            shown += 1
                        }
                    }
                    return total + Math.max(0, shown - 1) * spacing
                }
                Layout.alignment: header.columns === 2 ? Qt.AlignTop : Qt.AlignLeft
                Layout.preferredWidth: header.columns === 2 ? oneLine : header.room
                Layout.maximumWidth: Layout.preferredWidth
                AppButton {
                    visible: !page.info.error
                    text: qsTr("Open site")
                    iconPath: Icons.globe
                    enabled: page.running
                    onClicked: Studio.openSite()
                }
                AppButton {
                    visible: !page.info.error && page.hasAdmin
                    text: qsTr("Open admin")
                    enabled: page.running
                    onClicked: Studio.openAdmin()
                }
                AppButton {
                    text: qsTr("Open folder")
                    iconPath: Icons.folder
                    onClicked: Studio.openFolder(page.info.path)
                }
                AppButton {
                    text: qsTr("Open in editor")
                    iconPath: Icons.code
                    enabled: !page.info.error
                    onClicked: Studio.openInEditor(page.info.path)
                }
                AppButton {
                    visible: !page.info.error
                    readonly property bool on: page.running || page.serverState === "starting"
                    text: on ? qsTr("Stop server") : qsTr("Start server")
                    iconPath: on ? Icons.stop : Icons.play
                    iconFilled: true
                    primary: !on && page.serverState !== "stopping"
                    danger: on
                    enabled: page.serverState !== "stopping"
                    onClicked: on ? Studio.stopServer() : Studio.startServer()
                }
            }
        }

        // The project is gone or no longer FloCMS
        Card {
            visible: !!page.info.error
            Layout.fillWidth: true
            RichText {
                raw: page.info.error ? Texts.problem(page.info.error) : ""
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            AppButton {
                text: qsTr("Remove from the list")
                iconPath: Icons.trash
                onClicked: Studio.removeProject(page.info.path)
            }
        }

        TabBar {
            id: tabs
            visible: !page.info.error
            Layout.fillWidth: true
            spacing: 22
            currentIndex: page.tab
            onCurrentIndexChanged: page.tab = currentIndex
            background: Rectangle {
                color: "transparent"
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
            }
            Repeater {
                model: [qsTr("Overview"), qsTr("Generate"), qsTr("Database"), qsTr("Logs"), qsTr("Packages"), qsTr("Doctor")]
                delegate: TabButton {
                    id: tabButton
                    required property string modelData
                    required property int index
                    text: modelData
                    width: implicitWidth
                    implicitHeight: 40
                    leftPadding: 4
                    rightPadding: 4
                    topPadding: 0
                    bottomPadding: 0
                    hoverEnabled: true
                    font.family: Lang.uiFont
                    font.pixelSize: 14
                    font.weight: tabs.currentIndex === index ? Font.DemiBold : Font.Medium
                    background: Rectangle {
                        color: "transparent"
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 2
                            color: tabs.currentIndex === tabButton.index ? Theme.accent : (tabButton.hovered ? Theme.controlBorder : "transparent")
                        }
                    }
                    contentItem: AppText {
                        ui: tabButton.text
                        font: tabButton.font
                        color: tabs.currentIndex === tabButton.index ? Theme.selectedText : (tabButton.hovered ? Theme.text : Theme.muted)
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }

        StackLayout {
            visible: !page.info.error
            Layout.fillWidth: true
            Layout.fillHeight: true
            id: tabStack
            currentIndex: page.tab
            // Each tab is made the first time it is shown, then kept (what
            // was typed or scrolled stays): fewer objects in memory and a
            // quicker start for the tabs never opened
            ShownOnce { index: 0; sourceComponent: Component { OverviewTab { info: page.info; startAction: page.startAction; openTab: page.openTab } } }
            ShownOnce { index: 1; sourceComponent: Component { GenerateTab {} } }
            ShownOnce { index: 2; sourceComponent: Component { DatabaseTab { startAction: page.startAction } } }
            ShownOnce { index: 3; sourceComponent: Component { LogsTab {} } }
            ShownOnce { index: 4; sourceComponent: Component { PackagesTab {} } }
            ShownOnce { index: 5; sourceComponent: Component { DoctorTab {} } }
        }
    }
}
