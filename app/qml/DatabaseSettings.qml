import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// MariaDB boards 1 and 2 (design note D9, D6–D8, D12): the server new
// projects use, and Studio's own MariaDB with its folder, port, root's
// password and start/stop. Servers from XAMPP and the like are listed and
// used, never started or stopped.
ColumnLayout {
    id: section
    signal downloadRequested()
    spacing: 16

    readonly property var server: Studio.mariadb
    readonly property bool hasOwn: server.programs.length > 0
    readonly property bool active: server.state === "starting" || server.state === "running" || server.state === "stopping"
    // The server this computer has besides Studio's (XAMPP's, say)
    readonly property var found: {
        const row = Studio.tools.find(t => t.tool === "mariadb")
        if (!row || !row.product || !row.path)
            return null
        const own = String(Studio.toolsFolder).toLowerCase().replace(/\\/g, "/")
        return String(row.path).toLowerCase().replace(/\\/g, "/").startsWith(own) ? null : row
    }
    // "auto", or a choice that no longer exists (Studio's removed, nothing
    // found any more), shows what the wizard will use
    readonly property string choice: Studio.databaseServer === "other" ? "other"
        : (Studio.databaseServer === "studio" && hasOwn) ? "studio"
        : (Studio.databaseServer === "found" && found) ? "found"
        : (hasOwn ? "studio" : (found ? "found" : "other"))
    // Minutes it has been running, kept fresh
    property real now: Date.now()
    Timer { interval: 30000; repeat: true; running: section.server.state === "running"; onTriggered: section.now = Date.now() }
    readonly property int minutes: server.startedAt > 0 ? Math.max(0, Math.floor((now - server.startedAt) / 60000)) : 0

    function stateColors(state) {
        return state === "running" ? [Theme.okBg, Theme.okText]
             : state === "failed" ? [Theme.badBg, Theme.badText]
             : (state === "starting" || state === "stopping") ? [Theme.warnBg, Theme.warnText]
             : [Theme.rule, Theme.muted]
    }
    function foundHome(path) {
        const p = String(path).toLowerCase()
        if (p.indexOf("xampp") >= 0) return "XAMPP"
        if (p.indexOf("laragon") >= 0) return "Laragon"
        return ""
    }

    ConfirmDialog {
        id: confirm
        onAccepted: (typed) => section.server.remove(confirm.deleteData)
        property bool deleteData: false
    }

    // --- Which server ---
    Card {
        Layout.fillWidth: true
        Heading { ui: qsTr("Database server") }
        AppText { ui: qsTr("Default server for new projects"); font.weight: Font.DemiBold }
        ButtonGroup { id: serverGroup }
        OptionRow {
            visible: section.hasOwn
            Layout.fillWidth: true
            radio: true
            group: serverGroup
            checked: section.choice === "studio"
            title: qsTr("Studio's MariaDB %1").arg(section.server.series)
            note: qsTr("Started by Studio · port %1").arg(section.server.port)
            badge: Texts.mariadbState(section.server.state)
            badgeColor: section.stateColors(section.server.state)[0]
            badgeText: section.stateColors(section.server.state)[1]
            onToggled: (on) => { if (on) Studio.setDatabaseServer("studio") }
        }
        OptionRow {
            visible: !!section.found
            Layout.fillWidth: true
            radio: true
            group: serverGroup
            checked: section.choice === "found"
            title: section.found ? (section.foundHome(section.found.path) !== "" ? section.foundHome(section.found.path) + " · " : "")
                                   + section.found.product + " " + section.found.version : ""
            note: !section.found ? ""
                  : section.foundHome(section.found.path) !== ""
                    ? qsTr("%1 · start and stop it in %2").arg(section.found.path).arg(section.foundHome(section.found.path))
                    : qsTr("%1 · Studio doesn't start or stop it").arg(section.found.path)
            badge: section.found ? (section.found.state === "running" ? qsTr("Running") : qsTr("Stopped")) : ""
            badgeColor: section.found && section.found.state === "running" ? Theme.okBg : Theme.rule
            badgeText: section.found && section.found.state === "running" ? Theme.okText : Theme.muted
            onToggled: (on) => { if (on) Studio.setDatabaseServer("found", section.found.path) }
        }
        OptionRow {
            Layout.fillWidth: true
            radio: true
            group: serverGroup
            checked: section.choice === "other"
            title: qsTr("Another server")
            note: qsTr("Host, port and user you give")
            onToggled: (on) => { if (on) Studio.setDatabaseServer("other") }
        }
        GridLayout {
            visible: section.choice === "other"
            columns: 3
            columnSpacing: 12
            rowSpacing: 6
            Layout.fillWidth: true
            AppText { ui: qsTr("Host"); font.weight: Font.DemiBold }
            AppText { ui: qsTr("Port"); font.weight: Font.DemiBold }
            AppText { ui: qsTr("User"); font.weight: Font.DemiBold }
            Field {
                id: otherHost
                mono: true
                text: Studio.databaseHost
                Layout.fillWidth: true
                Accessible.name: qsTr("Host")
                onEditingFinished: Studio.setOtherDatabase(text, otherPort.acceptableInput ? Number(otherPort.text) : Studio.databaseOtherPort, otherUser.text)
            }
            Field {
                id: otherPort
                mono: true
                implicitWidth: 100
                text: String(Studio.databaseOtherPort)
                validator: IntValidator { bottom: 1; top: 65535 }
                inputMethodHints: Qt.ImhDigitsOnly
                Accessible.name: qsTr("Port")
                onEditingFinished: if (acceptableInput) Studio.setOtherDatabase(otherHost.text, Number(text), otherUser.text)
            }
            Field {
                id: otherUser
                mono: true
                text: Studio.databaseUser
                Layout.fillWidth: true
                Accessible.name: qsTr("User")
                onEditingFinished: Studio.setOtherDatabase(otherHost.text, otherPort.acceptableInput ? Number(otherPort.text) : Studio.databaseOtherPort, text)
            }
        }
        RichText {
            raw: qsTr("Each project can pick its own when it is made or set up. Existing projects keep the server in their `.env`.")
            color: Theme.muted
            font.pixelSize: 13
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        // No server of Studio's own yet
        RowLayout {
            visible: !section.hasOwn
            Layout.fillWidth: true
            spacing: 12
            RichText {
                raw: Studio.mariadbFromHomebrew
                     ? qsTr("Studio can run its own MariaDB from Homebrew: install it with `brew install mariadb`, then search again.")
                     : qsTr("Studio can download its own MariaDB and start and stop it for you. Its databases stay in a folder for your user.")
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            AppButton {
                visible: Studio.mariadbFromHomebrew
                text: qsTr("Search again")
                iconPath: Icons.refresh
                onClicked: Studio.refreshTools()
            }
            AppButton {
                visible: !Studio.mariadbFromHomebrew
                text: qsTr("Download MariaDB…")
                iconPath: Icons.download
                onClicked: section.downloadRequested()
            }
        }
    }

    // --- Studio's own MariaDB ---
    Card {
        visible: section.hasOwn
        Layout.fillWidth: true
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Heading { ui: qsTr("Studio's MariaDB") }
            AppText {
                text: section.server.version
                      + (section.server.origin === "homebrew" ? " · Homebrew" : "")
                color: Theme.muted
                font.pixelSize: 13
            }
            AppText {
                visible: section.server.state === "running"
                ui: section.minutes < 1 ? qsTr("running since just now") : qsTr("running for %Ln min", "", section.minutes)
                color: Theme.muted
                font.pixelSize: 13
            }
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("Open log"); onClicked: section.server.openLog() }
            AppButton {
                visible: section.server.state !== "running" && section.server.state !== "stopping"
                text: section.server.state === "starting" ? qsTr("Starting…") : qsTr("Start")
                iconPath: Icons.play
                iconFilled: true
                enabled: section.server.state !== "starting"
                onClicked: section.server.start()
            }
            AppButton {
                visible: section.server.state === "running" || section.server.state === "stopping"
                text: section.server.state === "stopping" ? qsTr("Stopping…") : qsTr("Stop")
                iconPath: Icons.stop
                iconFilled: true
                danger: true
                enabled: section.server.state === "running"
                onClicked: section.server.stop()
            }
        }
        MariadbProblem { inSettings: true; Layout.fillWidth: true }

        // More than one series downloaded: which one runs
        RowLayout {
            visible: section.server.programs.length > 1
            spacing: 12
            AppText { ui: qsTr("Version"); font.weight: Font.DemiBold; Layout.preferredWidth: 170 }
            Segmented {
                label: qsTr("Version")
                enabled: !section.active
                value: section.server.series
                options: section.server.programs.map(p => ({ value: p.series, text: "MariaDB " + p.series }))
                onChosen: (value) => section.server.setSeries(value)
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            AppText { ui: qsTr("Databases folder"); font.weight: Font.DemiBold; Layout.preferredWidth: 170 }
            Mono { text: section.server.dataFolder; elide: Text.ElideMiddle; Layout.fillWidth: true }
            AppButton { text: qsTr("Open folder"); small: true; onClicked: section.server.openDataFolder() }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            AppText { ui: qsTr("Port"); font.weight: Font.DemiBold; Layout.preferredWidth: 170 }
            Field {
                mono: true
                implicitWidth: 110
                text: String(section.server.port) // a port, not a number: no 3,306
                enabled: !section.active
                validator: IntValidator { bottom: 1; top: 65535 }
                inputMethodHints: Qt.ImhDigitsOnly
                Accessible.name: qsTr("Port")
                onEditingFinished: if (acceptableInput) section.server.setPort(Number(text))
            }
            RichText {
                raw: qsTr("Used from the next start. Only this computer can connect (`127.0.0.1`).")
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            RichText { raw: qsTr("User `root`"); font.weight: Font.DemiBold; Layout.preferredWidth: 170 }
            Mono {
                text: section.server.shownPassword !== "" ? section.server.shownPassword : "••••••••••••"
                color: section.server.shownPassword !== "" ? Theme.text : Theme.muted
                Accessible.name: section.server.shownPassword !== "" ? text : qsTr("Hidden password")
            }
            AppButton {
                small: true
                visible: section.server.state === "running"
                text: section.server.shownPassword !== "" ? qsTr("Hide") : qsTr("Show")
                onClicked: section.server.showPassword(section.server.shownPassword === "")
            }
            AppButton {
                small: true
                text: qsTr("Copy")
                enabled: section.server.initialized
                onClicked: section.server.copyPassword()
            }
            AppText {
                ui: qsTr("Kept in %1. For HeidiSQL or phpMyAdmin.").arg(Texts.secureStoreName())
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        AppCheck {
            checked: section.server.autoStart
            text: qsTr("Start the server when Studio opens")
            Layout.fillWidth: true
            onToggled: section.server.setAutoStart(checked)
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.rule }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            AppText {
                ui: qsTr("Studio stops the server cleanly when it closes.")
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            AppButton {
                text: qsTr("Delete its databases…")
                danger: true
                small: true
                enabled: !section.active
                onClicked: {
                    confirm.deleteData = true
                    confirm.ask({ title: section.server.origin === "studio" ? qsTr("Delete Studio's MariaDB and its databases?") : qsTr("Delete the databases of Studio's MariaDB?"),
                                  message: section.server.origin === "studio"
                                           ? qsTr("The program and every database in `%1` are deleted, with the password of `root`. Projects that use them stop working.").arg(section.server.dataFolder)
                                           : qsTr("Every database in `%1` is deleted, with the password of `root`. Homebrew's MariaDB stays. Projects that use them stop working.").arg(section.server.dataFolder),
                                  confirmText: qsTr("Delete") })
                }
            }
            AppButton {
                visible: section.server.origin === "studio"
                text: qsTr("Remove MariaDB…")
                danger: true
                small: true
                enabled: !section.active
                onClicked: {
                    confirm.deleteData = false
                    confirm.ask({ title: qsTr("Remove Studio's MariaDB %1?").arg(section.server.series),
                                  message: qsTr("The program is deleted. The databases stay in `%1`; download MariaDB %2 again to use them.").arg(section.server.dataFolder).arg(section.server.series),
                                  confirmText: qsTr("Remove") })
                }
            }
        }
    }
}
