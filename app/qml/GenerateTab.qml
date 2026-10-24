import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// Board 3: FloCMS's generators, a form each, the command it runs, and the
// preview: the real generator in a temporary copy of the project (D5),
// with the files it would add or change. Create runs the same command in
// the project.
ScrollView {
    id: tab
    readonly property var gen: Studio.generate
    readonly property var form: gen ? gen.form : ({})
    readonly property var problems: gen ? gen.problems : ({})
    readonly property var preview: gen ? gen.preview : ({})
    readonly property var created: gen ? gen.created : ({})
    readonly property string which: gen ? gen.generator : ""
    readonly property bool hasAdmin: !!gen && gen.hasAdmin
    readonly property var db: Studio.checks ? Studio.checks.database : ({})
    readonly property var tables: (db.tables || []).map(t => t.name)
    readonly property var resources: db.resources || []
    readonly property bool previewing: preview.state === "copying" || preview.state === "running"
    readonly property bool creating: created.state === "running"
    // The file shown in the preview
    property int shownFile: 0
    // The form and the preview side by side, as on board 3
    readonly property bool twoColumns: width > 760
    contentWidth: availableWidth
    clip: true

    // A demo can open on a file of its preview
    Component.onCompleted: if (tab.preview.selected !== undefined) tab.shownFile = tab.preview.selected
    Connections {
        target: tab.gen
        function onPreviewChanged() { if (tab.shownFile >= (tab.preview.files || []).length) tab.shownFile = 0 }
    }

    function markOf(kind) { return kind === "added" ? "+" : (kind === "removed" ? "−" : "~") }
    function colorOf(kind) { return kind === "added" ? Theme.okText : (kind === "removed" ? Theme.badText : Theme.warnText) }

    ColumnLayout {
        width: tab.availableWidth
        spacing: 16

        Choice {
            label: qsTr("Generator")
            value: tab.which
            Layout.fillWidth: true
            options: [
                { value: "admin-resource", text: qsTr("Admin resource"), enabled: tab.hasAdmin },
                { value: "admin-page", text: qsTr("Admin page"), enabled: tab.hasAdmin },
                { value: "block", text: qsTr("Block"), enabled: tab.hasAdmin },
                { value: "widget", text: qsTr("Widget"), enabled: tab.hasAdmin },
                { value: "controller", text: qsTr("Controller") },
                { value: "migration", text: qsTr("Migration") }
            ]
            onChosen: (value) => tab.gen.choose(value)
        }
        RichText {
            visible: !tab.hasAdmin
            raw: qsTr("The admin resource, page, block and widget need the admin panel (`hostkurd/flocms-admin`).")
            color: Theme.muted
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        GridLayout {
            Layout.fillWidth: true
            columns: tab.twoColumns ? 2 : 1
            columnSpacing: 16
            rowSpacing: 16

            // --- The form ---
            Card {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 1
                padding: 20
                gap: 16

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Heading {
                        ui: ({ "admin-resource": qsTr("New admin resource"), "admin-page": qsTr("New admin page"),
                               "block": qsTr("New block"), "widget": qsTr("New dashboard widget"),
                               "controller": qsTr("New controller"), "migration": qsTr("New migration") })[tab.which] || ""
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    Mono { text: (tab.gen ? tab.gen.command : "").split(" ")[2] || ""; color: Theme.muted }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: (tab.twoColumns ? (tab.width - 16) / 2 : tab.width) > 460 ? 2 : 1
                    columnSpacing: 12
                    rowSpacing: 14

                    FormField {
                        label: tab.which === "migration" ? qsTr("What it does") : qsTr("Name")
                        hint: ({ "admin-resource": qsTr("A class name, e.g. `Property` or `BlogPost`."),
                                 "admin-page": qsTr("A class name, e.g. `SalesReport`."),
                                 "block": qsTr("A class name, e.g. `LatestProjects`."),
                                 "widget": qsTr("A class name, e.g. `PostStats` or `RecentPosts`."),
                                 "controller": qsTr("E.g. `Blog` or `BlogPost`."),
                                 "migration": qsTr("E.g. `create_posts_table` or `add_slug_to_posts`.") })[tab.which] || ""
                        problem: tab.problems.name || ""
                        Layout.fillWidth: true
                        Field {
                            mono: tab.which === "migration"
                            text: tab.form.name || ""
                            Layout.fillWidth: true
                            Accessible.name: qsTr("Name")
                            onTextEdited: tab.gen.set("name", text)
                            onEditingFinished: tab.gen.touch("name")
                        }
                    }

                    // Admin resource: its table and URL
                    FormField {
                        visible: tab.which === "admin-resource"
                        label: qsTr("Table")
                        hint: qsTr("Empty: the plural of the name.")
                        problem: tab.problems.table || ""
                        Layout.fillWidth: true
                        Field { mono: true; text: tab.form.table || ""; Layout.fillWidth: true; Accessible.name: qsTr("Table"); onTextEdited: tab.gen.set("table", text); onEditingFinished: tab.gen.touch("table") }
                    }
                    FormField {
                        visible: tab.which === "admin-resource"
                        label: qsTr("URL")
                        hint: "`/admin/" + (tab.form.slug || "…") + "`"
                        problem: tab.problems.slug || ""
                        Layout.fillWidth: true
                        Field { mono: true; text: tab.form.slug || ""; placeholderText: qsTr("the table"); Layout.fillWidth: true; Accessible.name: qsTr("URL"); onTextEdited: tab.gen.set("slug", text); onEditingFinished: tab.gen.touch("slug") }
                    }

                    // Admin page: the sidebar group
                    FormField {
                        visible: tab.which === "admin-page"
                        label: qsTr("Sidebar group")
                        hint: qsTr("E.g. Reports. Empty: no group.")
                        problem: tab.problems.group || ""
                        Layout.fillWidth: true
                        Field { text: tab.form.group || ""; Layout.fillWidth: true; Accessible.name: qsTr("Sidebar group"); onTextEdited: tab.gen.set("group", text) }
                    }
                }

                // Admin resource: how Create and Edit open, and its options
                FormField {
                    visible: tab.which === "admin-resource"
                    label: qsTr("Create and Edit open as")
                    Layout.fillWidth: true
                    Choice {
                        label: qsTr("Create and Edit open as")
                        value: tab.form.formMode || "page"
                        Layout.fillWidth: true
                        options: [
                            { value: "page", text: qsTr("A full page") },
                            { value: "modal", text: qsTr("A dialog") },
                            { value: "slide-over", text: qsTr("A slide-over panel") }
                        ]
                        onChosen: (value) => tab.gen.set("formMode", value)
                    }
                }
                ColumnLayout {
                    visible: tab.which === "admin-resource"
                    spacing: 0
                    Layout.fillWidth: true
                    AppCheck { text: qsTr("Also create a migration for the table"); checked: !!tab.form.migration; onToggled: tab.gen.set("migration", checked) }
                    AppCheck { text: qsTr("Editors can drag records into order (a `sort` column)").replace(/`/g, ""); checked: !!tab.form.reorderable; onToggled: tab.gen.set("reorderable", checked) }
                }
                AppText {
                    visible: tab.which === "admin-resource"
                    ui: qsTr("The resource starts with a title field. Add the other fields in the file.")
                    color: Theme.muted
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                // Admin page: what it shows
                FormField {
                    visible: tab.which === "admin-page"
                    label: qsTr("The page shows")
                    Layout.fillWidth: true
                    Choice {
                        label: qsTr("The page shows")
                        value: tab.form.pageKind || "plain"
                        Layout.fillWidth: true
                        options: [
                            { value: "plain", text: qsTr("Content of its own") },
                            { value: "form", text: qsTr("A form with Save") },
                            { value: "table", text: qsTr("A database table") },
                            { value: "resource", text: qsTr("A resource's records") }
                        ]
                        onChosen: (value) => tab.gen.set("pageKind", value)
                    }
                }

                // Widget: its kind
                FormField {
                    visible: tab.which === "widget"
                    label: qsTr("Widget")
                    Layout.fillWidth: true
                    Choice {
                        label: qsTr("Widget")
                        value: tab.form.widgetType || "stats"
                        Layout.fillWidth: true
                        options: [
                            { value: "stats", text: qsTr("Numbers") },
                            { value: "records", text: qsTr("Latest records") },
                            { value: "chart", text: qsTr("A chart") }
                        ]
                        onChosen: (value) => tab.gen.set("widgetType", value)
                    }
                }

                // Controller: its actions
                FormField {
                    visible: tab.which === "controller"
                    label: qsTr("Actions")
                    Layout.fillWidth: true
                    Choice {
                        label: qsTr("Actions")
                        value: tab.form.controllerKind || "plain"
                        Layout.fillWidth: true
                        options: [
                            { value: "plain", text: qsTr("A page") },
                            { value: "resource", text: qsTr("Pages and admin actions (CRUD)") },
                            { value: "admin", text: qsTr("Admin actions") },
                            { value: "api", text: qsTr("JSON API") }
                        ]
                        onChosen: (value) => tab.gen.set("controllerKind", value)
                    }
                }
                AppCheck {
                    visible: tab.which === "controller" && tab.form.controllerKind === "api"
                    text: qsTr("With the resource actions: list, show, add, change, delete")
                    checked: !!tab.form.apiResource
                    onToggled: tab.gen.set("apiResource", checked)
                }
                AppCheck {
                    visible: tab.which === "controller" && tab.form.controllerKind !== "api"
                    text: qsTr("Also create its model")
                    checked: !!tab.form.model
                    onToggled: tab.gen.set("model", checked)
                }

                // Migration: a table to create or change
                FormField {
                    visible: tab.which === "migration"
                    label: qsTr("Migration")
                    Layout.fillWidth: true
                    Choice {
                        label: qsTr("Migration")
                        value: tab.form.migrationKind || "plain"
                        Layout.fillWidth: true
                        options: [
                            { value: "plain", text: qsTr("Empty") },
                            { value: "create", text: qsTr("Create a table") },
                            { value: "change", text: qsTr("Change a table") }
                        ]
                        onChosen: (value) => tab.gen.set("migrationKind", value)
                    }
                }
                RichText {
                    visible: tab.which === "migration" && (tab.form.migrationKind || "plain") === "plain"
                    raw: qsTr("A name like `create_posts_table` creates the `posts` table.")
                    color: Theme.muted
                    font.pixelSize: 13
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                // A table: for a table page, a migration
                FormField {
                    visible: (tab.which === "admin-page" && tab.form.pageKind === "table")
                             || (tab.which === "migration" && (tab.form.migrationKind || "plain") !== "plain")
                    label: qsTr("Table")
                    problem: tab.problems.table || ""
                    Layout.fillWidth: true
                    Field { mono: true; text: tab.form.table || ""; Layout.fillWidth: true; Accessible.name: qsTr("Table"); onTextEdited: tab.gen.set("table", text); onEditingFinished: tab.gen.touch("table") }
                    Flow {
                        visible: tab.tables.length > 0 && !(tab.which === "migration" && tab.form.migrationKind === "create")
                        spacing: 6
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        Repeater {
                            model: tab.tables.slice(0, 12)
                            delegate: AppButton {
                                required property string modelData
                                small: true
                                text: modelData
                                onClicked: tab.gen.set("table", modelData)
                            }
                        }
                    }
                }

                // A resource: for a resource page, a widget
                FormField {
                    visible: (tab.which === "admin-page" && tab.form.pageKind === "resource") || tab.which === "widget"
                    label: qsTr("Resource")
                    hint: tab.which === "widget" && tab.form.widgetType !== "records" ? qsTr("Optional: the resource it counts.") : ""
                    problem: tab.problems.resource || ""
                    Layout.fillWidth: true
                    Field { mono: true; text: tab.form.resource || ""; placeholderText: "Post"; Layout.fillWidth: true; Accessible.name: qsTr("Resource"); onTextEdited: tab.gen.set("resource", text); onEditingFinished: tab.gen.touch("resource") }
                    Flow {
                        visible: tab.resources.length > 0
                        spacing: 6
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        Repeater {
                            model: tab.resources
                            delegate: AppButton {
                                required property string modelData
                                small: true
                                text: modelData
                                onClicked: tab.gen.set("resource", modelData)
                            }
                        }
                    }
                }

                AppCheck {
                    visible: tab.which === "admin-page"
                    text: qsTr("Also write its view, with the panel's parts")
                    checked: !!tab.form.view
                    onToggled: tab.gen.set("view", checked)
                }
                AppCheck {
                    text: qsTr("Overwrite files that are there")
                    checked: !!tab.form.force
                    onToggled: tab.gen.set("force", checked)
                }

                // The command, as it will run
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: commandText.implicitHeight + 20
                    radius: 8
                    color: Theme.terminalBg
                    LayoutMirroring.enabled: false
                    LayoutMirroring.childrenInherit: true
                    Mono {
                        id: commandText
                        anchors.fill: parent
                        anchors.margins: 10
                        text: tab.gen ? tab.gen.command : ""
                        color: Theme.terminalText
                        wrapMode: Text.WrapAnywhere
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: qsTr("Preview")
                        enabled: !!tab.gen && !tab.gen.busy && (tab.hasAdmin || ["controller", "migration"].indexOf(tab.which) >= 0)
                        onClicked: tab.gen.runPreview()
                    }
                    AppButton {
                        primary: true
                        text: ({ "admin-resource": qsTr("Create resource"), "admin-page": qsTr("Create page"),
                                 "block": qsTr("Create block"), "widget": qsTr("Create widget"),
                                 "controller": qsTr("Create controller"), "migration": qsTr("Create migration") })[tab.which] || ""
                        iconPath: Icons.plus
                        enabled: !!tab.gen && !tab.gen.busy && (tab.hasAdmin || ["controller", "migration"].indexOf(tab.which) >= 0)
                        onClicked: tab.gen.create()
                    }
                }
                CommandOutput {
                    output: tab.created
                    Layout.fillWidth: true
                }
            }

            // --- The preview ---
            Card {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 1
                padding: 20
                gap: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Heading { ui: qsTr("Preview: what it will write"); wrapMode: Text.Wrap; Layout.fillWidth: true }
                    AppText {
                        visible: tab.preview.state === "done"
                        ui: qsTr("%Ln file(s)", "", (tab.preview.files || []).length)
                        color: Theme.muted
                    }
                }

                // Before a preview, while it runs, when it failed
                RichText {
                    visible: tab.preview.state === "idle" || !tab.preview.state
                    raw: qsTr("Preview runs the generator in a temporary copy of the project and shows the files it writes. Your project changes only with Create.")
                    color: Theme.muted
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                RowLayout {
                    visible: tab.previewing
                    spacing: 10
                    StatusIcon { status: "running" }
                    AppText {
                        ui: tab.preview.state === "copying" ? qsTr("Copying the project…") : qsTr("Running the generator in the copy…")
                        color: Theme.muted
                    }
                }
                // Why: Studio's words (the copy could not be made), or the generator's
                RichText {
                    visible: tab.preview.state === "failed" && !tab.preview.message
                    raw: tab.preview.error === "step_exit" ? qsTr("The command ended with exit code %1.").arg(Lang.num(tab.preview.detail))
                         : tab.preview.error === "preview_timeout" ? qsTr("The generator did not finish in %Ln second(s), so the preview stopped it. Is the database answering?", "", parseInt(tab.preview.detail))
                         : tab.preview.error === "preview_absolute_path" ? qsTr("`%1` in `config/admin.php` is an absolute path, so the generator would write there and not in the copy. No preview for this project; Create works.").arg(tab.preview.detail)
                         : (tab.preview.error ? Texts.problem(tab.preview.error) : "")
                    color: Theme.badText
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Mono {
                    visible: tab.preview.state === "failed" && !!(tab.preview.message || (tab.preview.detail && ["step_exit", "preview_timeout", "preview_absolute_path"].indexOf(tab.preview.error) < 0))
                    text: tab.preview.message || tab.preview.detail || ""
                    color: Theme.badText
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }
                RichText {
                    visible: tab.preview.state === "done" && !tab.preview.current
                    raw: qsTr("The form changed since this preview. Preview again to see what it writes now.")
                    color: Theme.warnText
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                // The project changed while the generator ran: maybe written
                // there (a path set as absolute), maybe something else
                RichText {
                    visible: (tab.preview.outside || []).length > 0
                    raw: qsTr("The project itself changed while the preview ran: %1. Check these files: the generator may have written there.")
                         .arg((tab.preview.outside || []).slice(0, 5).map(p => "`" + p + "`").join(", "))
                    color: Theme.badText
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                RichText {
                    visible: !!tab.preview.notRemoved
                    raw: Texts.problem("preview_remove")
                    color: Theme.warnText
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                AppText {
                    visible: tab.preview.state === "done" && (tab.preview.files || []).length === 0
                    ui: qsTr("It writes no files.")
                    color: Theme.muted
                }

                // The files
                ColumnLayout {
                    visible: tab.preview.state === "done"
                    spacing: 2
                    Layout.fillWidth: true
                    LayoutMirroring.enabled: false
                    LayoutMirroring.childrenInherit: true
                    Repeater {
                        model: tab.preview.files || []
                        delegate: ItemDelegate {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            implicitHeight: 30
                            padding: 6
                            onClicked: tab.shownFile = index
                            background: Rectangle { radius: 6; color: tab.shownFile === index ? Theme.selectedBg : "transparent" }
                            contentItem: RowLayout {
                                spacing: 8
                                Mono { text: tab.markOf(modelData.kind); color: tab.colorOf(modelData.kind); font.weight: Font.Bold }
                                Mono { text: modelData.path; elide: Text.ElideMiddle; Layout.fillWidth: true }
                            }
                        }
                    }
                }

                // The file's lines: all of a new file, the changed lines of another
                Rectangle {
                    readonly property var file: (tab.preview.files || [])[tab.shownFile] || null
                    visible: tab.preview.state === "done" && !!file
                    Layout.fillWidth: true
                    implicitHeight: Math.min(fileColumn.implicitHeight + 24, 420)
                    radius: 8
                    color: Theme.terminalBg
                    clip: true
                    LayoutMirroring.enabled: false
                    LayoutMirroring.childrenInherit: true
                    Flickable {
                        id: fileFlick
                        anchors.fill: parent
                        anchors.margins: 12
                        contentHeight: fileColumn.implicitHeight
                        contentWidth: Math.max(width, fileColumn.implicitWidth)
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                        Column {
                            id: fileColumn
                            spacing: 1
                            Repeater {
                                model: fileFlick.parent.file ? fileFlick.parent.file.lines : []
                                delegate: Item {
                                    id: lineItem
                                    required property var modelData
                                    readonly property bool skipped: modelData.kind === "~" || modelData.kind === ">"
                                    implicitWidth: skipped ? skippedText.implicitWidth : codeText.implicitWidth
                                    implicitHeight: skipped ? skippedText.implicitHeight : codeText.implicitHeight
                                    Mono {
                                        id: codeText
                                        visible: !lineItem.skipped
                                        text: lineItem.skipped ? "" : lineItem.modelData.kind + " " + lineItem.modelData.text
                                        color: lineItem.modelData.kind === "+" ? Theme.terminalOk : (lineItem.modelData.kind === "-" ? Theme.terminalBad : Theme.terminalText)
                                        font.pixelSize: 12
                                    }
                                    // A run of lines left out
                                    AppText {
                                        id: skippedText
                                        visible: lineItem.skipped
                                        ui: !lineItem.skipped ? ""
                                            : "⋯ " + (lineItem.modelData.kind === ">" ? qsTr("%Ln more line(s), not shown", "", parseInt(lineItem.modelData.text))
                                                                                      : qsTr("%Ln unchanged line(s)", "", parseInt(lineItem.modelData.text)))
                                        color: Theme.terminalMuted
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                    }
                }
                AppText {
                    readonly property var file: (tab.preview.files || [])[tab.shownFile] || null
                    visible: tab.preview.state === "done" && !!file && (!!file.binary || !!file.tooBig)
                    ui: file && file.binary ? qsTr("Not a text file.") : qsTr("Too large to show here.")
                    color: Theme.muted
                }

                // What the generator said
                CommandOutput {
                    visible: (tab.preview.lines || []).length > 0
                    bare: true
                    output: ({ state: tab.preview.state || "", lines: tab.preview.lines || [] })
                    Layout.fillWidth: true
                }
            }
        }
    }
}
