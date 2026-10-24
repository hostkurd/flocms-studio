import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// What the project's files say, read without PHP (Core's projectInfo).
// The server, migrations and Doctor join it in stages 4 and 5.
ScrollView {
    id: tab
    property var info: ({})
    readonly property var db: info.database || ({})
    contentWidth: availableWidth
    clip: true

    GridLayout {
        width: tab.availableWidth
        columns: width > 900 ? 3 : 2
        columnSpacing: 14
        rowSpacing: 14

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
            Layout.columnSpan: parent.columns === 3 ? 1 : 2
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
    }
}
