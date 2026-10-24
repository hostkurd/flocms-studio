import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// What the project's files say, read without PHP (Core's projectInfo).
// The server (stage 4), the database, Doctor and quick commands
// (stage 5).
ScrollView {
    id: tab
    property var info: ({})
    // From ProjectPage: runs a database action, asking first when it should
    property var startAction: function(action) {}
    property var openTab: function(name) {}
    readonly property var doctor: Studio.checks ? Studio.checks.doctor : ({})
    readonly property var dbState: Studio.checks ? Studio.checks.database : ({})
    readonly property int pending: Studio.checks ? (Studio.checks.migrations.pending || 0) : 0
    readonly property var db: info.database || ({})
    contentWidth: availableWidth
    clip: true

    GridLayout {
        width: tab.availableWidth
        columns: width > 900 ? 3 : 2
        columnSpacing: 14
        rowSpacing: 14

        // --- The server (stage 4) ---
        ServerCard {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
        }

        // --- The database ---
        Card {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            SectionLabel { ui: qsTr("Database") }
            Heading {
                level: 2
                ui: !tab.db.configured ? qsTr("Not set up yet")
                    : (tab.db.driver === "sqlite" ? "SQLite" : (tab.db.driver === "mysql" ? "MySQL / MariaDB" : tab.db.driver))
                Layout.fillWidth: true
            }
            KeyValue { visible: tab.db.driver !== "sqlite" && !tab.db.usesDsn; key: qsTr("Server"); value: (tab.db.host || "") + ":" + (tab.db.port || ""); mono: true }
            KeyValue { visible: !!tab.db.name; key: tab.db.driver === "sqlite" ? qsTr("File") : qsTr("Database"); value: tab.db.name || ""; mono: true }
            KeyValue { visible: !!tab.db.usesDsn; key: qsTr("Connection"); value: qsTr("from `DB_DSN`") }
            RichText {
                visible: !tab.db.configured
                raw: qsTr("Set `DB_NAME` (and `DB_USERNAME` for MySQL) in `.env`. The Database tab will do this for you.")
                color: Theme.muted
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            // What the database says (stage 5)
            Flow {
                visible: tab.dbState.state === "done"
                spacing: 6
                Layout.fillWidth: true
                AppText { ui: qsTr("%Ln table(s)", "", (tab.dbState.tables || []).length) }
                AppText { visible: tab.pending > 0; text: "·"; color: Theme.muted }
                AppText {
                    visible: tab.pending > 0
                    ui: qsTr("%Ln migration(s) waiting", "", tab.pending)
                    color: Theme.warnText
                    font.weight: Font.DemiBold
                }
            }
            AppButton {
                visible: tab.pending > 0
                primary: true
                text: qsTr("Run migrations")
                enabled: !!Studio.checks && !Studio.checks.busy
                onClicked: tab.startAction("migrate")
            }
            RichText {
                visible: (tab.info.duplicateEnvKeys || []).length > 0
                raw: qsTr("`.env` sets %1 twice. The website uses the first value, some `php flo` commands the last: remove one.")
                     .arg((tab.info.duplicateEnvKeys || []).map(k => "`" + k + "`").join(", "))
                color: Theme.warnText
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        // --- The packages, from composer.lock ---
        Card {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            SectionLabel { ui: qsTr("Packages") }
            Repeater {
                model: tab.info.packages || []
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 26
                    Mono { text: modelData.name.replace("hostkurd/", ""); color: Theme.text; Layout.fillWidth: true }
                    Mono { text: modelData.version }
                }
            }
            AppText {
                visible: (tab.info.otherPackages || 0) > 0
                ui: qsTr("and %Ln other package(s)", "", tab.info.otherPackages || 0)
                color: Theme.muted
                font.pixelSize: 13
            }
            RichText {
                visible: tab.info.name !== undefined && !tab.info.hasLock
                raw: qsTr("No `composer.lock` yet: run `composer install` in the project.")
                color: Theme.warnText
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            RichText {
                visible: !!tab.info.hasLock && !tab.info.hasVendor
                raw: qsTr("The packages are not installed (no `vendor/`): run `composer install`.")
                color: Theme.warnText
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        // --- Doctor (stage 5) ---
        Card {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            Layout.columnSpan: parent.columns === 3 ? 2 : 1
            RowLayout {
                Layout.fillWidth: true
                Heading { ui: qsTr("Doctor"); Layout.fillWidth: true }
                AppButton {
                    small: true
                    text: qsTr("Run again")
                    enabled: !!Studio.checks && tab.doctor.state !== "running"
                    onClicked: Studio.checks.runDoctor()
                }
            }
            ProblemText { report: tab.doctor }
            AppText {
                visible: tab.doctor.state === "running" && !tab.doctor.checks
                ui: qsTr("Checking…")
                color: Theme.muted
            }
            Repeater {
                // What needs a look first
                model: (tab.doctor.checks || []).filter(c => c.status !== "ok").concat((tab.doctor.checks || []).filter(c => c.status === "ok")).slice(0, 4)
                delegate: CheckRow {
                    required property var modelData
                    status: modelData.status
                    name: Texts.checkName(modelData.check)
                    detail: modelData.detail
                    Layout.fillWidth: true
                }
            }
            AppButton {
                visible: (tab.doctor.checks || []).length > 4
                small: true
                text: qsTr("All checks")
                onClicked: tab.openTab("doctor")
            }
        }

        // --- Quick commands (stage 5) ---
        Card {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            Heading { ui: qsTr("Quick commands") }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 8
                rowSpacing: 8
                // To the Generate tab, with that generator
                AppButton {
                    text: qsTr("Admin resource…")
                    enabled: !!Studio.generate && Studio.generate.hasAdmin
                    Layout.fillWidth: true
                    onClicked: { Studio.generate.choose("admin-resource"); tab.openTab("generate") }
                }
                AppButton {
                    text: qsTr("Migration…")
                    enabled: !!Studio.generate
                    Layout.fillWidth: true
                    onClicked: { Studio.generate.choose("migration"); tab.openTab("generate") }
                }
                AppButton {
                    text: qsTr("Clear cache")
                    enabled: !!Studio.checks && !Studio.checks.busy
                    Layout.fillWidth: true
                    onClicked: Studio.checks.runQuick("cache:clear")
                }
                AppButton {
                    text: qsTr("Optimize")
                    enabled: !!Studio.checks && !Studio.checks.busy
                    Layout.fillWidth: true
                    onClicked: Studio.checks.runQuick("optimize")
                }
            }
            CommandOutput {
                output: Studio.checks ? Studio.checks.output : ({})
                visible: ["cache:clear", "view:clear", "optimize", "optimize:clear"].indexOf(output.command) >= 0
                Layout.fillWidth: true
            }
        }

        // --- The project ---
        Card {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            SectionLabel { ui: qsTr("Project") }
            Heading { level: 2; ui: Texts.typeName(tab.info.type); wrapMode: Text.Wrap; Layout.fillWidth: true }
            KeyValue { key: qsTr("Name"); value: tab.info.appName || "—" }
            KeyValue {
                key: qsTr("Environment")
                value: tab.info.appEnv ? tab.info.appEnv : qsTr("production (not set)")
                mono: !!tab.info.appEnv
            }
            KeyValue { key: qsTr("Address"); value: tab.info.appUrl || "—"; mono: !!tab.info.appUrl }
            KeyValue {
                key: qsTr("Started from")
                value: tab.info.skeletonVersion ? qsTr("skeleton %1").arg(tab.info.skeletonVersion) : "—"
            }
            ColumnLayout {
                visible: (tab.info.languages || []).length > 0
                spacing: 6
                AppText { ui: qsTr("Languages"); color: Theme.muted; font.pixelSize: 13 }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: tab.info.languages || []
                        delegate: Pill {
                            required property string modelData
                            text: modelData === tab.info.defaultLanguage ? qsTr("%1 · default").arg(modelData) : modelData
                            color: modelData === tab.info.defaultLanguage ? Theme.selectedBg : Theme.rule
                            fg: modelData === tab.info.defaultLanguage ? Theme.selectedText : Theme.navText
                        }
                    }
                }
            }
        }

    }
}
