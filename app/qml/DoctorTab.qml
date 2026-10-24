import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// Board 7: what `php flo doctor` and `php flo admin:doctor` say. The
// check names are translated; their details are FloCMS's own words.
ScrollView {
    id: tab
    readonly property var checks: Studio.checks
    readonly property var doctor: checks ? checks.doctor : ({})
    readonly property var admin: checks ? checks.adminDoctor : ({})
    readonly property bool hasAdmin: ["siteAdmin", "siteAdminApi", "admin", "apiAdmin"].indexOf(Studio.current.type) >= 0
    readonly property var notOk: (doctor.checks || []).filter(c => c.status !== "ok")
    readonly property var allOk: (doctor.checks || []).filter(c => c.status === "ok")
    property bool showAll: false
    property bool showOutput: false
    // Minutes since the last run, kept fresh
    property real now: Date.now()
    Timer { interval: 30000; repeat: true; running: tab.visible; onTriggered: tab.now = Date.now() }
    readonly property int minutes: doctor.checkedAt ? Math.max(0, Math.floor((now - Date.parse(doctor.checkedAt)) / 60000)) : -1
    readonly property int problems: (doctor.failed || 0) + (admin.problems || 0)
    readonly property int warnings: (doctor.warnings || 0) + (admin.warnings || 0)
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: tab.availableWidth
        spacing: 16

        // --- The summary ---
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Pill {
                visible: tab.doctor.state === "done"
                text: qsTr("%Ln problem(s)", "", tab.problems)
                color: tab.problems > 0 ? Theme.badBg : Theme.rule
                fg: tab.problems > 0 ? Theme.badText : Theme.muted
                implicitHeight: 28
            }
            Pill {
                visible: tab.doctor.state === "done"
                text: qsTr("%Ln warning(s)", "", tab.warnings)
                color: tab.warnings > 0 ? Theme.warnBg : Theme.rule
                fg: tab.warnings > 0 ? Theme.warnText : Theme.muted
                implicitHeight: 28
            }
            Pill {
                visible: tab.doctor.state === "done"
                text: qsTr("%Ln OK", "", tab.doctor.ok || 0)
                color: Theme.okBg
                fg: Theme.okText
                implicitHeight: 28
            }
            AppText {
                ui: tab.doctor.state === "running" ? qsTr("Checking…")
                    : tab.minutes < 0 ? ""
                    : tab.minutes < 1 ? qsTr("Last checked: just now")
                    : qsTr("Last checked: %Ln min ago", "", tab.minutes)
                color: Theme.muted
                font.pixelSize: 13
            }
            Item { Layout.fillWidth: true }
            AppButton {
                text: qsTr("Run again")
                iconPath: Icons.refresh
                enabled: !!tab.checks && tab.doctor.state !== "running"
                onClicked: tab.checks.runDoctor()
            }
        }

        GridLayout {
            id: cards
            Layout.fillWidth: true
            columns: tab.availableWidth > 900 ? 2 : 1
            columnSpacing: 16
            rowSpacing: 16

            // --- The framework: php flo doctor ---
            Card {
                Layout.fillWidth: true
                Layout.columnSpan: tab.hasAdmin ? 1 : cards.columns
                // Fixed shares of the row, whatever the content asks for
                Layout.preferredWidth: cards.columns === 1 || !tab.hasAdmin ? tab.availableWidth : (tab.availableWidth - cards.columnSpacing) / 2
                Layout.maximumWidth: cards.columns === 1 || !tab.hasAdmin ? tab.availableWidth : (tab.availableWidth - cards.columnSpacing) / 2
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignTop
                RowLayout {
                    Layout.fillWidth: true
                    Heading { ui: qsTr("Framework checks"); Layout.fillWidth: true }
                    Mono { text: "php flo doctor"; color: Theme.muted; elide: Text.ElideLeft; Layout.maximumWidth: 180 }
                }
                ProblemText { report: tab.doctor }
                AppText {
                    visible: tab.doctor.state === "running" && !tab.doctor.checks
                    ui: qsTr("Checking…")
                    color: Theme.muted
                }
                Repeater {
                    // What needs a look first, then the rest when asked
                    model: tab.showAll ? tab.notOk.concat(tab.allOk) : tab.notOk.concat(tab.allOk.slice(0, Math.max(0, 8 - tab.notOk.length)))
                    delegate: CheckRow {
                        required property var modelData
                        status: modelData.status
                        name: Texts.checkName(modelData.check)
                        detail: modelData.detail
                        Layout.fillWidth: true
                    }
                }
                AppButton {
                    readonly property int hidden: tab.allOk.length - Math.max(0, 8 - tab.notOk.length)
                    visible: hidden > 0
                    small: true
                    text: tab.showAll ? qsTr("Show fewer") : qsTr("%Ln more check(s), all OK", "", hidden)
                    onClicked: tab.showAll = !tab.showAll
                }
            }

            // --- The admin panel: php flo admin:doctor ---
            Card {
                visible: tab.hasAdmin
                Layout.fillWidth: true
                // Fixed shares of the row, whatever the content asks for
                Layout.preferredWidth: cards.columns === 1 ? tab.availableWidth : (tab.availableWidth - cards.columnSpacing) * 1 / 2
                Layout.maximumWidth: cards.columns === 1 ? tab.availableWidth : (tab.availableWidth - cards.columnSpacing) * 1 / 2
                Layout.minimumWidth: 0
                Layout.alignment: Qt.AlignTop
                RowLayout {
                    Layout.fillWidth: true
                    Heading { ui: qsTr("Admin panel checks"); Layout.fillWidth: true }
                    Mono { text: "php flo admin:doctor"; color: Theme.muted; elide: Text.ElideLeft; Layout.maximumWidth: 180 }
                }
                ProblemText { report: tab.admin }
                Mono { visible: !!tab.admin.heading; text: tab.admin.heading || ""; color: Theme.muted; elide: Text.ElideRight; Layout.fillWidth: true }
                Repeater {
                    model: tab.admin.sections || []
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        // Never wider than the card, in either direction
                        Layout.maximumWidth: parent ? parent.width : 0
                        spacing: 4
                        // The panel's own words, left to right
                        LayoutMirroring.enabled: false
                        LayoutMirroring.childrenInherit: true
                        Mono {
                            visible: !!modelData.title
                            text: modelData.title
                            font.weight: Font.DemiBold
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                        }
                        Repeater {
                            model: modelData.lines
                            delegate: Rectangle {
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: row.implicitHeight + 12
                                radius: 6
                                color: modelData.status === "fail" ? Theme.badBg : (modelData.status === "warn" ? Theme.warnBg : "transparent")
                                RowLayout {
                                    id: row
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 10
                                    StatusIcon { status: modelData.status; Layout.alignment: Qt.AlignTop }
                                    AppText {
                                        text: modelData.text
                                        textFormat: Text.PlainText
                                        font.pixelSize: 13
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }
                Mono {
                    visible: !!tab.admin.summary
                    text: tab.admin.summary || ""
                    color: Theme.muted
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                AppButton {
                    visible: !!tab.admin.output
                    small: true
                    text: tab.showOutput ? qsTr("Hide the full output") : qsTr("Show the full output")
                    onClicked: tab.showOutput = !tab.showOutput
                }
                CommandOutput {
                    visible: tab.showOutput
                    output: ({ state: "done", shown: "php flo admin:doctor --no-ansi", lines: (tab.admin.output || "").split("\n") })
                    Layout.fillWidth: true
                }
            }
        }
    }
}
