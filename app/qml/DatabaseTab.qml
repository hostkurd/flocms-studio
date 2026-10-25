import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// Board 5: the connection, the migrations with their buttons, the tables.
// Everything through `php flo` (db:show, migrate:status, migrate, ...);
// the commands that change data ask first (ConfirmDialog).
ScrollView {
    id: tab
    readonly property var checks: Studio.checks
    readonly property var db: checks ? checks.database : ({})
    readonly property var migrations: checks ? checks.migrations : ({})
    readonly property int pending: migrations.pending || 0
    readonly property bool anyRan: (migrations.rows || []).some(m => m.batch > 0)
    readonly property bool busy: !!checks && checks.busy
    contentWidth: availableWidth
    clip: true

    // "10.11.14-MariaDB-0ubuntu0.24.04.1" → "MariaDB 10.11.14"
    function serverName(driver, server) {
        if (driver === "sqlite")
            return "SQLite" + (server ? " " + server : "")
        const version = (server || "").split("-")[0]
        const maria = (server || "").toLowerCase().indexOf("mariadb") >= 0
        return (maria ? "MariaDB" : "MySQL") + (version ? " " + version : "")
    }

    // The project uses Studio's own MariaDB, which isn't running (D11):
    // the connection fails because of that, so the tab offers to start it
    readonly property var server: Studio.mariadb
    readonly property bool onStudioServer: {
        const db = Studio.current.database || {}
        const host = String(db.host || "")
        const port = Number(db.port || 3306)
        return db.driver === "mysql" && (host === "127.0.0.1" || host === "localhost") && port === server.port
               && server.state !== "none"
    }
    readonly property bool offerStart: onStudioServer && server.state !== "running" && tab.db.state === "failed"
    property bool waitingForServer: false
    Connections {
        target: tab.server
        function onChanged() {
            if (tab.waitingForServer && tab.server.state === "running" && tab.checks) {
                tab.waitingForServer = false
                tab.checks.refreshDatabase()
            } else if (tab.server.state === "failed" || tab.server.state === "stopped") {
                tab.waitingForServer = false
            }
        }
    }

    // From ProjectPage: runs a database action, asking first when it should
    property var startAction: function(action) {}

    ColumnLayout {
        width: tab.availableWidth
        spacing: 16

        // --- The connection ---
        Card {
            Layout.fillWidth: true
            RowLayout {
                Layout.fillWidth: true
                spacing: 28
                ColumnLayout {
                    spacing: 4
                    SectionLabel { ui: qsTr("Connection") }
                    RowLayout {
                        spacing: 8
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: tab.db.state === "done" ? Theme.ok : (tab.db.state === "failed" ? Theme.bad : Theme.idle)
                        }
                        AppText {
                            ui: tab.db.state === "done" ? tab.serverName(tab.db.driver, tab.db.server)
                                : (tab.db.state === "failed" ? qsTr("Can't connect") : qsTr("Checking…"))
                            font.weight: Font.DemiBold
                        }
                    }
                }
                ColumnLayout {
                    visible: !!tab.db.database
                    spacing: 4
                    SectionLabel { ui: qsTr("Database") }
                    Mono { text: tab.db.database || "" }
                }
                ColumnLayout {
                    visible: tab.db.driver !== "sqlite" && !!tab.db.host
                    spacing: 4
                    SectionLabel { ui: qsTr("Host") }
                    Mono { text: (tab.db.host || "") + (tab.db.port ? ":" + tab.db.port : "") }
                }
                ColumnLayout {
                    spacing: 4
                    SectionLabel { ui: qsTr("From") }
                    Mono { text: ".env" }
                }
                Item { Layout.fillWidth: true }
                AppButton {
                    small: true
                    text: qsTr("Edit in `.env`")
                    onClicked: Studio.openEnvFile()
                }
                AppButton {
                    small: true
                    text: qsTr("Refresh")
                    iconPath: Icons.refresh
                    enabled: tab.db.state !== "running"
                    onClicked: tab.checks.refreshDatabase()
                }
            }
            Rectangle {
                visible: tab.offerStart
                Layout.fillWidth: true
                implicitHeight: startRow.implicitHeight + 24
                radius: 10
                color: Theme.optionSelectedBg
                border.color: Theme.accent
                RowLayout {
                    id: startRow
                    anchors.fill: parent
                    anchors.margins: 12
                    anchors.leftMargin: 14
                    spacing: 12
                    RichText {
                        raw: qsTr("This project uses Studio's MariaDB (`127.0.0.1:%1`), which is stopped.").arg(tab.server.port)
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    AppButton {
                        primary: true
                        text: tab.server.state === "starting" ? qsTr("Starting…") : qsTr("Start the database server")
                        iconPath: Icons.play
                        iconFilled: true
                        enabled: tab.server.state !== "starting" && tab.server.state !== "stopping"
                        onClicked: { tab.waitingForServer = true; tab.server.start() }
                    }
                }
            }
            MariadbProblem { visible: tab.onStudioServer && code !== ""; Layout.fillWidth: true }
            ProblemText { report: tab.db; visible: !tab.offerStart && (tab.db.state === "failed") }
        }

        GridLayout {
            id: cards
            Layout.fillWidth: true
            columns: tab.availableWidth > 900 ? 2 : 1
            columnSpacing: 16
            rowSpacing: 16

            // --- Migrations ---
            Card {
                Layout.fillWidth: true
                // Fixed shares of the row, whatever the content asks for
                Layout.preferredWidth: cards.columns === 1 ? tab.availableWidth : (tab.availableWidth - cards.columnSpacing) * 3 / 5
                Layout.maximumWidth: cards.columns === 1 ? tab.availableWidth : (tab.availableWidth - cards.columnSpacing) * 3 / 5
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignTop
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Heading { ui: qsTr("Migrations"); Layout.fillWidth: true }
                    AppButton {
                        text: qsTr("Roll back last batch")
                        enabled: tab.anyRan && !tab.busy
                        onClicked: tab.startAction("rollback")
                    }
                    AppButton {
                        primary: true
                        text: tab.pending > 0 ? qsTr("Run %Ln migration(s)", "", tab.pending) : qsTr("Up to date")
                        enabled: tab.pending > 0 && !tab.busy
                        onClicked: tab.startAction("migrate")
                    }
                }
                ProblemText { report: tab.migrations }
                AppText {
                    visible: tab.migrations.state === "done" && (tab.migrations.rows || []).length === 0
                    ui: qsTr("No migrations yet.")
                    color: Theme.muted
                }

                // The table: status, migration, package, batch
                GridLayout {
                    visible: (tab.migrations.rows || []).length > 0
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 8
                    SectionLabel { ui: qsTr("Status") }
                    SectionLabel { ui: qsTr("Migration"); Layout.fillWidth: true }
                    SectionLabel { ui: qsTr("Package") }
                    SectionLabel { ui: qsTr("Batch") }
                    Repeater {
                        model: (tab.migrations.rows || []).length * 4
                        delegate: Loader {
                            required property int index
                            readonly property var row: tab.migrations.rows[Math.floor(index / 4)]
                            readonly property int column: index % 4
                            Layout.fillWidth: column === 1
                            sourceComponent: column === 0 ? statusCell : (column === 1 ? nameCell : (column === 2 ? packageCell : batchCell))
                        }
                    }
                }
                Component {
                    id: statusCell
                    Pill {
                        text: parent.row.state === "pending" ? qsTr("Waiting")
                              : parent.row.state === "changed" ? qsTr("Ran · changed since")
                              : parent.row.state === "missing" ? qsTr("Ran · file gone")
                              : qsTr("Ran")
                        color: parent.row.state === "pending" ? Theme.warnBg : (parent.row.state === "ran" ? Theme.okBg : Theme.badBg)
                        fg: parent.row.state === "pending" ? Theme.warnText : (parent.row.state === "ran" ? Theme.okText : Theme.badText)
                    }
                }
                Component { id: nameCell; Mono { text: parent.row.name; elide: Text.ElideMiddle } }
                Component { id: packageCell; Mono { text: parent.row.package; color: Theme.muted } }
                Component { id: batchCell; AppText { text: parent.row.batch > 0 ? Lang.num(parent.row.batch) : "—"; color: Theme.muted } }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    spacing: 8
                    RichText { raw: qsTr("Runs: %1").arg("`php flo migrate`"); color: Theme.muted; Layout.fillWidth: true; elide: Text.ElideRight }
                    AppButton { small: true; text: qsTr("Seed…"); enabled: !tab.busy && tab.db.state === "done"; onClicked: tab.startAction("seed") }
                    AppButton { small: true; danger: true; text: qsTr("Drop all and migrate…"); enabled: !tab.busy && tab.db.state === "done"; onClicked: tab.startAction("fresh") }
                }
                AppText {
                    ui: qsTr("On a production site Studio asks first, then runs the command with --force.")
                    color: Theme.muted
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                CommandOutput {
                    output: tab.checks ? tab.checks.output : ({})
                    visible: ["migrate", "rollback", "fresh", "seed"].indexOf(output.command) >= 0
                    Layout.fillWidth: true
                }
            }

            // --- Tables ---
            Card {
                Layout.fillWidth: true
                // Fixed shares of the row, whatever the content asks for
                Layout.preferredWidth: cards.columns === 1 ? tab.availableWidth : (tab.availableWidth - cards.columnSpacing) * 2 / 5
                Layout.maximumWidth: cards.columns === 1 ? tab.availableWidth : (tab.availableWidth - cards.columnSpacing) * 2 / 5
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignTop
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Heading { ui: qsTr("Tables"); Layout.fillWidth: true }
                    Field {
                        id: filter
                        placeholderText: qsTr("Table name")
                        Accessible.name: qsTr("Filter the tables")
                        Layout.preferredWidth: 160
                    }
                }
                Repeater {
                    model: (tab.db.tables || []).filter(t => filter.text === "" || t.name.toLowerCase().indexOf(filter.text.toLowerCase()) >= 0)
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 30
                        spacing: 12
                        Mono { text: modelData.name; Layout.fillWidth: true; elide: Text.ElideRight }
                        AppText { text: modelData.rows >= 0 ? Lang.num(modelData.rows) : "—"; color: Theme.muted }
                        AppButton {
                            visible: !!modelData.adminUrl
                            small: true
                            text: qsTr("Open in admin")
                            enabled: Studio.server.state === "running"
                            onClicked: Studio.openUrl(Studio.server.url + modelData.adminUrl)
                        }
                    }
                }
                AppText {
                    visible: tab.db.state === "done"
                    ui: qsTr("%Ln table(s) and their row counts. Edit data in the admin panel.", "", (tab.db.tables || []).length)
                    color: Theme.muted
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }
    }
}
