import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// Board 8: hostkurd's packages, installed (composer.lock) and latest
// (composer outdated), an update with what it runs, and adding the API or
// the admin panel. Nothing runs before the person clicks Update or Add.
ScrollView {
    id: tab
    readonly property var pk: Studio.packagesTab
    readonly property var rows: pk ? pk.packages : []
    readonly property var check: pk ? pk.check : ({})
    readonly property var run: pk && pk.run ? pk.run : ({})
    readonly property bool running: run.state === "running"
    readonly property bool production: !!(Studio.current && Studio.current.production)
    // The package whose update (or addition) is shown on the right
    property string chosen: ""
    property string chosenKind: ""
    // Read again whenever the rows change (a check found something newer)
    readonly property var plan: (rows, !pk || chosen === "" ? ({})
                                       : (chosenKind === "add" ? pk.planAdd(chosen) : pk.planUpdate(chosen)))
    property real now: Date.now()
    readonly property int minutes: check.checkedAt ? Math.max(0, Math.floor((now - Date.parse(check.checkedAt)) / 60000)) : -1
    readonly property int updates: rows.filter(r => r.status === "update" || r.status === "major").length
    // The table's columns
    readonly property int versionWidth: 90
    // The widths the header and every row use. In a narrow window (960 px,
    // long labels) the version and status columns give up space, down to
    // these, before the package name goes below 120 px
    readonly property int versionMinimum: 74
    readonly property int statusMinimum: 96
    readonly property real tableWidth: tableHeader.width
    readonly property real columnRoom: tableWidth - 20 - 4 * 14 - actionWidth - 120
    readonly property real columnShare: Math.max(0, Math.min(1, columnRoom / (2 * versionWidth + statusWidth)))
    readonly property int versionColumn: Math.max(versionMinimum, Math.floor(versionWidth * columnShare))
    readonly property int statusColumn: Math.max(statusMinimum, Math.floor(statusWidth * columnShare))
    readonly property int statusWidth: Math.max(measureCurrent.implicitWidth, measureUpdate.implicitWidth, measureMajor.implicitWidth,
                                                measureZero.implicitWidth, measureThrough.implicitWidth, measureAbandoned.implicitWidth)
    // The update's card beside the table: only when one is chosen and the
    // window is wide enough
    readonly property bool showPlan: tab.chosen !== "" && !!tab.plan.name
    readonly property bool sideBySide: tab.width > 1150 && showPlan
    readonly property int actionWidth: Math.max(measureAdmin.implicitWidth, measureApi.implicitWidth, measureReview.implicitWidth)
    contentWidth: availableWidth
    clip: true

    Timer { interval: 30000; repeat: true; running: tab.visible; onTriggered: tab.now = Date.now() }
    // Asked once per project, the first time its tab is shown: Composer
    // asks Packagist
    function checkOnce() { if (visible && tab.pk && tab.check.state === "idle") tab.pk.checkUpdates() }
    onVisibleChanged: checkOnce()
    // Another project: its own run (one still going, or one that ended)
    property var shownPk: null
    onPkChanged: {
        if (tab.pk === tab.shownPk) return
        tab.shownPk = tab.pk
        // Read from the new project itself: tab.run may not follow yet
        const run = tab.pk ? tab.pk.run : null
        tab.chosen = run && run.name ? run.name : ""
        tab.chosenKind = run && run.kind ? run.kind : ""
        checkOnce()
    }
    // A demo can open on an update
    Component.onCompleted: {
        tab.shownPk = tab.pk
        if (tab.run.name) { tab.chosen = tab.run.name; tab.chosenKind = tab.run.kind }
        else if (Studio.demoDialog) { tab.chosen = Studio.demoDialog; tab.chosenKind = tab.rows.some(r => r.name === Studio.demoDialog && r.canAdd) ? "add" : "update" }
    }
    // An isolated left-to-right run: versions read the same in Arabic
    function ltr(text) { return "\u2066" + text + "\u2069" }
    // Measures for the action column: as wide as its widest button
    AppButton { id: measureAdmin; visible: false; small: true; text: qsTr("Add the admin panel") }
    AppButton { id: measureApi; visible: false; small: true; text: qsTr("Add the API") }
    AppButton { id: measureReview; visible: false; small: true; text: qsTr("Review update") }
    // ... and the status column as wide as its widest label
    Pill { id: measureCurrent; visible: false; text: qsTr("Up to date") }
    Pill { id: measureUpdate; visible: false; text: qsTr("Update") }
    Pill { id: measureMajor; visible: false; text: qsTr("New major version") }
    Pill { id: measureZero; visible: false; text: qsTr("New 0.x version") }
    Pill { id: measureThrough; visible: false; text: qsTr("Through another package") }
    Pill { id: measureAbandoned; visible: false; text: qsTr("Abandoned") }

    function shortName(name) { return name.replace("hostkurd/", "") }
    function choose(name, kind) {
        if (tab.running) return
        if (tab.pk) tab.pk.clearRun()
        tab.chosen = name
        tab.chosenKind = kind
    }

    GridLayout {
        width: tab.availableWidth
        columns: tab.sideBySide ? 2 : 1
        columnSpacing: 20
        rowSpacing: 20

        // --- The packages ---
        Card {
            // Side by side as on board 8; on a narrower window the update
            // comes first, above the table
            Layout.row: tab.sideBySide || !tab.showPlan ? 0 : 1
            Layout.column: 0
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: tab.sideBySide ? (tab.availableWidth - 20) * 2 / 3 : tab.availableWidth
            Layout.maximumWidth: Layout.preferredWidth
            Layout.minimumWidth: 0
            padding: 20
            gap: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Heading { ui: qsTr("FloCMS packages"); Layout.fillWidth: true }
                AppButton {
                    small: true
                    text: tab.check.state === "running" ? qsTr("Checking…") : qsTr("Check for updates")
                    enabled: !!tab.pk && tab.check.state !== "running" && !tab.running
                    onClicked: tab.pk.checkUpdates()
                }
            }
            AppText {
                visible: tab.check.state === "done"
                ui: (tab.updates > 0 ? qsTr("%Ln update(s).", "", tab.updates) : qsTr("Everything is up to date."))
                    + " " + (tab.minutes < 1 ? qsTr("Checked just now.") : qsTr("Checked %Ln min ago.", "", tab.minutes))
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            ColumnLayout {
                visible: tab.check.state === "failed"
                spacing: 4
                Layout.fillWidth: true
                RichText {
                    raw: tab.check.error ? Texts.problem(tab.check.error) : ""
                    color: Theme.badText
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Mono {
                    visible: !!(tab.check.message || tab.check.detail)
                    text: tab.check.message || tab.check.detail || ""
                    color: Theme.badText
                    font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }
            }

            // The table: package, installed, latest, status, action. Laid
            // out like a row below (inside an item as wide as the card), so
            // the columns line up
            Item {
                id: tableHeader
                Layout.fillWidth: true
                implicitHeight: header.implicitHeight
                RowLayout {
                    id: header
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 14
                    SectionLabel { ui: qsTr("Package"); Layout.fillWidth: true; Layout.minimumWidth: 80 }
                    SectionLabel { ui: qsTr("Installed"); elide: Text.ElideRight; Layout.preferredWidth: tab.versionColumn }
                    SectionLabel { ui: qsTr("Latest"); elide: Text.ElideRight; Layout.preferredWidth: tab.versionColumn }
                    SectionLabel { ui: qsTr("Status"); elide: Text.ElideRight; Layout.preferredWidth: tab.statusColumn }
                    Item { Layout.preferredWidth: tab.actionWidth }
                }
            }
            // A row per package, its columns as wide as the header's
            Repeater {
                model: tab.rows
                delegate: Rectangle {
                    id: row
                    required property var modelData
                    readonly property bool hasUpdate: modelData.status === "update" || modelData.status === "major"
                    readonly property bool selected: tab.chosen === modelData.name
                    Layout.fillWidth: true
                    implicitHeight: rowLayout.implicitHeight + 16
                    radius: 8
                    color: selected ? Theme.selectedBg : (hasUpdate ? Theme.warnBg : "transparent")
                    RowLayout {
                        id: rowLayout
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 14
                        Mono {
                            // Without the "hostkurd/" every name has, when the
                            // columns had to give up space
                            text: tab.columnShare < 1 ? tab.shortName(row.modelData.name) : row.modelData.name
                            color: row.modelData.status === "missing" ? Theme.muted : Theme.text
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            Layout.minimumWidth: 80
                        }
                        Mono {
                            visible: row.modelData.status !== "missing"
                            text: row.modelData.version || ""
                            elide: Text.ElideRight
                            Layout.preferredWidth: tab.versionColumn
                        }
                        AppText {
                            visible: row.modelData.status === "missing"
                            ui: qsTr("Not installed")
                            color: Theme.muted
                            elide: Text.ElideRight
                            Layout.preferredWidth: tab.versionColumn
                        }
                        Mono {
                            text: row.modelData.latest || "—"
                            color: row.hasUpdate ? Theme.text : Theme.muted
                            elide: Text.ElideRight
                            Layout.preferredWidth: tab.versionColumn
                        }
                        Item {
                            Layout.preferredWidth: tab.statusColumn
                            implicitHeight: statusPill.implicitHeight
                        Pill {
                            id: statusPill
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            // Cut at the column in the smallest window; the
                            // whole label in the tooltip
                            width: Math.min(implicitWidth, parent.width)
                            clip: true
                            ToolTip.visible: statusHover.hovered && width < implicitWidth
                            ToolTip.text: text
                            HoverHandler { id: statusHover }
                            visible: row.modelData.status !== "missing" && row.modelData.status !== "unknown"
                            text: row.modelData.abandoned ? qsTr("Abandoned")
                                  : row.modelData.status === "current" ? qsTr("Up to date")
                                  : row.modelData.status === "update" ? qsTr("Update")
                                  : row.modelData.status === "major" ? ((row.modelData.version || "").startsWith("0.") ? qsTr("New 0.x version") : qsTr("New major version"))
                                  : qsTr("Through another package")
                            color: row.modelData.abandoned ? Theme.badBg : row.modelData.status === "current" ? Theme.okBg : (row.hasUpdate ? Theme.warnBg : Theme.border)
                            fg: row.modelData.abandoned ? Theme.badText : row.modelData.status === "current" ? Theme.okText : (row.hasUpdate ? Theme.warnText : Theme.muted)
                        }
                        }
                        Item {
                            Layout.preferredWidth: tab.actionWidth
                            implicitHeight: actions.implicitHeight
                        RowLayout {
                            id: actions
                            anchors.right: parent.right
                            spacing: 0
                        AppButton {
                            visible: row.hasUpdate
                            small: true
                            text: qsTr("Review update")
                            enabled: !tab.running
                            onClicked: tab.choose(row.modelData.name, "update")
                        }
                        AppButton {
                            visible: !!row.modelData.canAdd
                            small: true
                            text: row.modelData.name === "hostkurd/flocms-api" ? qsTr("Add the API") : qsTr("Add the admin panel")
                            enabled: !tab.running
                            onClicked: tab.choose(row.modelData.name, "add")
                        }
                        }
                        }
                    }
                }
            }

            // The rest of composer.lock
            ColumnLayout {
                visible: !!tab.pk && tab.pk.others.length > 0
                spacing: 6
                Layout.fillWidth: true
                AppButton {
                    id: othersButton
                    property bool open: false
                    small: true
                    text: qsTr("Other packages (%1)").arg(Lang.num(tab.pk ? tab.pk.others.length : 0))
                    iconPath: Icons.arrowDown
                    onClicked: open = !open
                }
                Flow {
                    visible: othersButton.open
                    spacing: 12
                    Layout.fillWidth: true
                    LayoutMirroring.enabled: false
                    Repeater {
                        model: tab.pk ? tab.pk.others : []
                        delegate: Mono { required property string modelData; text: modelData; color: Theme.muted; font.pixelSize: 12 }
                    }
                }
            }
        }

        // --- An update, or an addition ---
        Card {
            visible: tab.showPlan
            Layout.row: 0
            Layout.column: tab.sideBySide ? 1 : 0
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: tab.sideBySide ? (tab.availableWidth - 20) / 3 : tab.availableWidth
            Layout.maximumWidth: Layout.preferredWidth
            Layout.minimumWidth: 0
            padding: 20
            gap: 14
            border.color: Theme.accent

            SectionLabel {
                ui: tab.chosenKind === "add"
                    ? (tab.chosen === "hostkurd/flocms-api" ? qsTr("Add the JSON API") : qsTr("Add the admin panel"))
                    : qsTr("Update %1").arg(tab.shortName(tab.chosen))
            }
            Mono {
                visible: tab.chosenKind === "update"
                text: tab.ltr((tab.plan.from || "") + " → " + (tab.plan.to || ""))
                font.family: Lang.displayFont
                font.pixelSize: 22
                font.weight: Font.DemiBold
            }
            Mono {
                visible: tab.chosenKind === "add"
                text: tab.chosen
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            RichText {
                visible: !!tab.plan.major
                raw: (tab.plan.from || "").startsWith("0.")
                     ? qsTr("Before 1.0, a new minor version may change how things work. Read the changelog first.")
                     : qsTr("A new major version: it may change how things work. Read the changelog first.")
                color: Theme.warnText
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            ColumnLayout {
                visible: tab.chosenKind === "update"
                spacing: 2
                AppButton { small: true; text: qsTr("Read the changelog"); iconPath: Icons.external; onClicked: Studio.openRepository(tab.plan.changelog) }
                AppButton { visible: !!tab.plan.upgrading; small: true; text: qsTr("Read UPGRADING.md"); iconPath: Icons.external; onClicked: Studio.openRepository(tab.plan.upgrading) }
            }

            // What happens after
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: afterText.implicitHeight + 24
                radius: 8
                color: Theme.warnBg
                ColumnLayout {
                    id: afterText
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6
                    AppText { ui: tab.chosenKind === "add" ? qsTr("After Composer adds it") : qsTr("After the update"); font.weight: Font.DemiBold; color: Theme.warnText }
                    RichText {
                        raw: tab.chosen === "hostkurd/flocms-admin"
                             ? qsTr("Studio runs `admin:install`, which only adds what is missing, and then `migrate`.")
                             : tab.chosen === "hostkurd/flocms-api"
                               ? qsTr("Studio runs `api:install`, which only adds what is missing, and then `migrate`.")
                               : qsTr("Studio runs `migrate` for the package's new tables.")
                        color: Theme.warnText
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    RichText {
                        visible: !!tab.plan.noDatabase
                        raw: qsTr("`.env` names no database, so `migrate` doesn't run. Run it from the Database tab once one is set.")
                        color: Theme.warnText
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    RichText {
                        visible: tab.production
                        raw: qsTr("`APP_ENV` says this is a production site: Composer leaves out the development packages (`--update-no-dev`), and `migrate` changes its database.")
                        color: Theme.warnText
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
            }

            // The commands: as they will run, then how each went
            SectionLabel { ui: qsTr("Commands") }
            ColumnLayout {
                spacing: 8
                Layout.fillWidth: true
                Repeater {
                    model: tab.run.name === tab.chosen && tab.run.steps ? tab.run.steps : (tab.plan.steps || []).map(s => ({ shown: s }))
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8
                        StatusIcon {
                            visible: !!modelData.state
                            status: modelData.state === "done" ? "ok" : (modelData.state === "failed" ? "fail" : (modelData.state === "running" ? "running" : "note"))
                            Layout.alignment: Qt.AlignTop
                        }
                        Mono {
                            text: modelData.shown
                            color: modelData.state === "waiting" ? Theme.muted : Theme.text
                            wrapMode: Text.WrapAnywhere
                            Layout.fillWidth: true
                        }
                    }
                }
            }
            CommandOutput {
                visible: tab.run.name === tab.chosen && (tab.run.lines || []).length > 0
                bare: true
                output: ({ state: tab.run.state || "", lines: tab.run.lines || [] })
                Layout.fillWidth: true
            }
            // How it ended
            RichText {
                visible: tab.run.name === tab.chosen && tab.run.state === "done"
                raw: tab.run.kind === "add" ? qsTr("Added. The project's tabs show it now.") : qsTr("Updated to %1.").arg(tab.run.to || "")
                color: Theme.okText
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            ColumnLayout {
                visible: tab.run.name === tab.chosen && tab.run.state === "failed"
                spacing: 4
                Layout.fillWidth: true
                RichText {
                    raw: tab.run.error ? Texts.problem(tab.run.error)
                                       : qsTr("A command failed (exit code %1). What ran before it stays done.").arg(Lang.num(tab.run.exitCode || 0))
                    color: Theme.badText
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Mono {
                    visible: !!(tab.run.message || tab.run.detail)
                    text: tab.run.message || tab.run.detail || ""
                    color: Theme.badText
                    font.pixelSize: 12
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                AppButton {
                    text: tab.run.name === tab.chosen && tab.run.state && !tab.running ? qsTr("Close") : qsTr("Cancel")
                    enabled: !tab.running
                    onClicked: { tab.pk.clearRun(); tab.chosen = ""; tab.chosenKind = "" }
                }
                AppButton {
                    visible: !(tab.run.name === tab.chosen && tab.run.state)
                    primary: true
                    text: tab.chosenKind === "add" ? qsTr("Add") : qsTr("Update")
                    enabled: !!tab.pk && !tab.pk.busy
                    onClicked: tab.chosenKind === "add" ? tab.pk.add(tab.chosen) : tab.pk.update(tab.chosen, tab.plan.to)
                }
            }
        }
    }
}
