import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// The Overview's Database tile (redesign boards 1-3): what .env says,
// what the database says (tables, migrations waiting), and for a project
// on Studio's own MariaDB that server's state with its Start and Stop.
// When another server (XAMPP, Laragon…) holds Studio's port, Studio
// doesn't offer to start its own: that one is in use, and Studio never
// touches it.
Card {
    id: card
    property var info: ({})
    property var startAction: function(action) {}
    property var openTab: function(name) {}
    readonly property var db: info.database || ({})
    readonly property var dbState: Studio.checks ? Studio.checks.database : ({})
    readonly property int pending: Studio.checks ? (Studio.checks.migrations.pending || 0) : 0

    // Studio's own server, for a project whose database Studio made on it
    readonly property var server: Studio.mariadb
    readonly property bool onStudioServer: {
        const host = String(db.host || "")
        const port = Number(db.port || 3306)
        return server.state !== "none" && server.usedBy(info.path || "")
               && db.driver === "mysql" && (host === "127.0.0.1" || host === "localhost") && port === server.port
    }
    readonly property var mariadbRow: (Studio.tools || []).find(t => t.tool === "mariadb") || ({})
    readonly property bool serverIdle: server.state === "stopped" || (server.state === "failed" && server.error === "db_port_busy")
    readonly property bool otherHolds: serverIdle && server.port === 3306 && mariadbRow.state === "running"
    // A port taken by something else (not on 3306, so not in the sidebar's
    // look): the Database tab explains it and offers a free port
    readonly property bool portBusy: server.state === "failed" && server.error === "db_port_busy"
    readonly property bool canStart: onStudioServer && !otherHolds && !portBusy && (server.state === "stopped" || server.state === "failed")
    readonly property bool canStop: onStudioServer && server.state === "running"

    // Studio's server just started (here, the Database tab, Settings):
    // read the database again, so the tables and migrations show. The
    // Database tab may not exist yet (tabs are made when first shown).
    property string lastServerState: server.state
    Connections {
        target: card.server
        function onChanged() {
            const was = card.lastServerState
            card.lastServerState = card.server.state
            if (card.onStudioServer && card.server.state === "running" && was !== "running"
                    && Studio.checks && card.dbState.state !== "running")
                Studio.checks.refreshDatabase()
        }
    }

    // The SQLite file from the project's folder: shorter, and the same on
    // every computer
    function shortFile(name) {
        const file = String(name || "").replace(/\\/g, "/")
        const folder = String(info.path || "").replace(/\\/g, "/").replace(/\/$/, "")
        return folder && file.toLowerCase().startsWith(folder.toLowerCase() + "/") ? file.slice(folder.length + 1) : file
    }

    SectionLabel { ui: qsTr("Database") }
    Heading {
        level: 2
        ui: !card.db.configured ? qsTr("Not set up yet")
            : (card.db.driver === "sqlite" ? "SQLite" : (card.db.driver === "mysql" ? "MySQL / MariaDB" : card.db.driver))
        elide: Text.ElideRight
        Layout.fillWidth: true
    }
    Mono {
        visible: !!card.db.configured && !card.db.usesDsn && !!card.db.name
        text: card.db.driver === "sqlite" ? card.shortFile(card.db.name)
              : card.db.name + " · " + (card.db.host || "") + ":" + (card.db.port || "")
        color: Theme.navText
        font.pixelSize: 12
        elide: Text.ElideMiddle
        Layout.fillWidth: true
        ToolTip.visible: fileHover.hovered && truncated
        ToolTip.text: text
        HoverHandler { id: fileHover }
    }
    RichText {
        visible: !!card.db.usesDsn
        raw: qsTr("from `DB_DSN`")
        color: Theme.navText
        font.pixelSize: 13
    }
    RichText {
        visible: !card.db.configured
        raw: qsTr("This project has no database yet. Studio can make one and write the `DB_` lines into `.env`.")
        color: Theme.muted
        font.pixelSize: 13
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    // What the database says (stage 5): one line that wraps in a narrow tile
    AppText {
        // On Studio's server while it is down, the server's line below says
        // why; while it runs, a failed connection is the project's own
        visible: !!card.db.configured && !(card.dbState.state === "failed" && card.onStudioServer && card.server.state !== "running")
        ui: card.dbState.state === "done"
            ? qsTr("%Ln table(s)", "", (card.dbState.tables || []).length) + " · "
              + (card.pending > 0 ? qsTr("%Ln migration(s) waiting", "", card.pending) : qsTr("migrations up to date"))
            : card.dbState.state === "failed" ? (card.onStudioServer ? "" : qsTr("Can't connect"))
            : qsTr("Checking…")
        color: card.dbState.state === "failed" ? Theme.badText : (card.pending > 0 ? Theme.warnText : Theme.muted)
        font.pixelSize: 13
        font.weight: card.pending > 0 ? Font.DemiBold : Font.Normal
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    // Studio's own server
    RowLayout {
        visible: card.onStudioServer
        spacing: 8
        Layout.fillWidth: true
        Rectangle {
            width: 8; height: 8; radius: 4
            // Level with the first line when the text wraps
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 6
            color: card.server.state === "running" ? Theme.ok
                 : (card.server.state === "starting" || card.server.state === "stopping" ? Theme.warn
                 : (card.server.state === "failed" && !card.otherHolds ? Theme.bad : Theme.idle))
        }
        AppText {
            ui: card.otherHolds ? qsTr("Another MariaDB is running on port %1.").arg(card.server.port)
                : card.server.state === "running" ? qsTr("Studio's MariaDB is running on port %1.").arg(card.server.port)
                : card.server.state === "starting" || card.server.state === "stopping" ? Texts.mariadbState(card.server.state)
                : card.server.state === "failed" ? qsTr("Studio's MariaDB didn't start.")
                : qsTr("Studio's MariaDB is stopped.")
            color: Theme.navText
            font.pixelSize: 13
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
    RichText {
        visible: (card.info.duplicateEnvKeys || []).length > 0
        raw: qsTr("`.env` sets %1 twice. The website uses the first value, some `php flo` commands the last: remove one.")
             .arg((card.info.duplicateEnvKeys || []).map(k => "`" + k + "`").join(", "))
        color: Theme.warnText
        font.pixelSize: 13
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    // The buttons sit at the bottom, level with the other tiles'
    Item { Layout.fillHeight: true }
    Flow {
        spacing: 8
        Layout.fillWidth: true
        Layout.topMargin: 4
        AppButton {
            visible: card.pending > 0
            primary: true
            text: qsTr("Run migrations")
            enabled: !!Studio.checks && !Studio.checks.busy
            onClicked: card.startAction("migrate")
        }
        AppButton {
            visible: card.canStart
            primary: card.pending === 0
            // Short: the line above says which server
            text: qsTr("Start")
            Accessible.name: qsTr("Start the database server")
            iconPath: Icons.play
            iconFilled: true
            onClicked: card.server.start()
        }
        AppButton {
            visible: card.canStop
            danger: true
            text: qsTr("Stop")
            Accessible.name: qsTr("Stop the database server")
            iconPath: Icons.stop
            iconFilled: true
            onClicked: card.server.stop()
        }
        AppButton {
            visible: !card.canStart && !card.canStop && card.pending === 0
            primary: !card.db.configured
            text: card.db.configured ? qsTr("Database tab") : qsTr("Set up database")
            onClicked: card.openTab("database")
        }
    }
}
