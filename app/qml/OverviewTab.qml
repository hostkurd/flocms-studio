import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// The project at a glance (Overview redesign, approved 26 Oct): three
// tiles of the same height for the server, the database and Doctor; then
// two columns, each card only as tall as what it holds: the project and
// the quick commands, and the packages. No grid rows across columns, so
// no gaps between cards of different heights.
ScrollView {
    id: tab
    property var info: ({})
    // From ProjectPage: runs a database action, asking first when it should
    property var startAction: function(action) {}
    property var openTab: function(name) {}
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        id: page
        width: tab.availableWidth
        spacing: 14
        readonly property bool wide: width >= 640

        // --- Server, database, Doctor: one row, the same height ---
        GridLayout {
            id: tiles
            Layout.fillWidth: true
            columns: page.wide ? 3 : 1
            columnSpacing: 14
            rowSpacing: 14
            readonly property real tileWidth: page.wide ? (width - 2 * columnSpacing) / 3 : width

            ServerCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: tiles.tileWidth
                Layout.minimumWidth: 0
            }
            DatabaseCard {
                info: tab.info
                startAction: tab.startAction
                openTab: tab.openTab
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: tiles.tileWidth
                Layout.minimumWidth: 0
            }
            DoctorCard {
                openTab: tab.openTab
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: tiles.tileWidth
                Layout.minimumWidth: 0
            }
        }

        // --- Two columns, each as tall as its cards ---
        GridLayout {
            id: lower
            Layout.fillWidth: true
            columns: page.wide ? 2 : 1
            columnSpacing: 14
            rowSpacing: 14
            readonly property real columnWidth: page.wide ? (width - columnSpacing) / 2 : width

            ColumnLayout {
                spacing: 14
                Layout.alignment: Qt.AlignTop
                Layout.fillWidth: true
                Layout.preferredWidth: lower.columnWidth
                Layout.minimumWidth: 0

                // --- The project ---
                Card {
                    Layout.fillWidth: true
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

                // --- Quick commands: two by two, each with what it does ---
                Card {
                    Layout.fillWidth: true
                    gap: 12
                    SectionLabel { ui: qsTr("Quick commands") }
                    GridLayout {
                        id: commands
                        Layout.fillWidth: true
                        // From the column's width, not its own: a layout
                        // that sizes itself from its columns would loop
                        columns: lower.columnWidth >= 400 ? 2 : 1
                        columnSpacing: 10
                        rowSpacing: 10
                        readonly property real tileWidth: columns === 2 ? (lower.columnWidth - 2 * 18 - columnSpacing) / 2 : lower.columnWidth - 2 * 18
                        // To the Generate tab, with that generator
                        CommandTile {
                            text: qsTr("Admin resource…")
                            detail: qsTr("A page in the admin panel")
                            iconPath: Icons.table
                            enabled: !!Studio.generate && Studio.generate.hasAdmin
                            Layout.fillWidth: true
                            Layout.preferredWidth: commands.tileWidth
                            Layout.minimumWidth: 0
                            onClicked: { Studio.generate.choose("admin-resource"); tab.openTab("generate") }
                        }
                        CommandTile {
                            text: qsTr("Migration…")
                            detail: qsTr("A new table or a change to one")
                            iconPath: Icons.database
                            enabled: !!Studio.generate
                            Layout.fillWidth: true
                            Layout.preferredWidth: commands.tileWidth
                            Layout.minimumWidth: 0
                            onClicked: { Studio.generate.choose("migration"); tab.openTab("generate") }
                        }
                        CommandTile {
                            text: qsTr("Clear cache")
                            detail: "php flo cache:clear"
                            detailMono: true
                            iconPath: Icons.trash
                            enabled: !!Studio.checks && !Studio.checks.busy
                            Layout.fillWidth: true
                            Layout.preferredWidth: commands.tileWidth
                            Layout.minimumWidth: 0
                            onClicked: Studio.checks.runQuick("cache:clear")
                        }
                        CommandTile {
                            text: qsTr("Optimize")
                            detail: "php flo optimize"
                            detailMono: true
                            iconPath: Icons.bolt
                            enabled: !!Studio.checks && !Studio.checks.busy
                            Layout.fillWidth: true
                            Layout.preferredWidth: commands.tileWidth
                            Layout.minimumWidth: 0
                            onClicked: Studio.checks.runQuick("optimize")
                        }
                    }
                    CommandOutput {
                        output: Studio.checks ? Studio.checks.output : ({})
                        visible: ["cache:clear", "view:clear", "optimize", "optimize:clear"].indexOf(output.command) >= 0
                        Layout.fillWidth: true
                    }
                }
            }

            // --- The packages, from composer.lock ---
            Card {
                Layout.alignment: Qt.AlignTop
                Layout.fillWidth: true
                Layout.preferredWidth: lower.columnWidth
                Layout.minimumWidth: 0
                RowLayout {
                    Layout.fillWidth: true
                    SectionLabel { ui: qsTr("Packages"); Layout.fillWidth: true }
                    AppButton {
                        small: true
                        text: qsTr("Packages tab")
                        onClicked: tab.openTab("packages")
                    }
                }
                Repeater {
                    model: tab.info.packages || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 26
                        Mono { text: modelData.name.replace("hostkurd/", ""); color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true }
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
}
