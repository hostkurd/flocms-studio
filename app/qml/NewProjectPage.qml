import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import FloStudio.Backend

// Board 6: a new FloCMS project in four steps: the project, its database,
// its first admin (with the admin panel), then Studio makes it and shows
// each command as it runs. Passwords are never shown in the output.
Item {
    id: page
    readonly property var wizard: Studio.wizard
    readonly property var form: wizard.form
    readonly property var problems: wizard.problems
    readonly property bool hasAdmin: !!form.hasAdmin
    // 0 project, 1 database, 2 admin, 3 create
    property int step: 0
    readonly property var stepNames: [qsTr("Project"), qsTr("Database"), qsTr("First admin"), qsTr("Create")]
    readonly property bool running: wizard.state === "running"
    readonly property bool started: wizard.state !== "editing"
    readonly property bool failed: wizard.state === "failed"

    // The fields each step holds (to tell which step a problem is on)
    readonly property var stepFields: [
        ["name", "parentDir", "languages", "defaultLanguage", "appName"],
        ["dbName", "dbUser", "dbPort", "dbHost", "dbPassword"],
        ["adminName", "adminEmail", "adminPassword"]
    ]
    function stepHasProblems(index) {
        return stepFields[index].some(f => page.problems[f] !== undefined)
    }
    function next() {
        for (const field of stepFields[page.step]) wizard.touch(field)
        if (page.stepHasProblems(page.step)) return
        page.step = page.step === 1 && !page.hasAdmin ? 3 : page.step + 1
    }
    function back() {
        page.step = page.step === 3 && !page.hasAdmin ? 1 : Math.max(0, page.step - 1)
    }
    // A demo can open on a step
    Component.onCompleted: {
        const steps = ["project", "database", "admin", "create"]
        if (steps.indexOf(Studio.demoTab) >= 0) page.step = steps.indexOf(Studio.demoTab)
    }
    // A run shows its steps; a new form starts at the first step. After a
    // failure the person can go back to fix the database or the admin.
    property string shownState: "editing"
    Connections {
        target: page.wizard
        function onStepsChanged() {
            if (page.wizard.state === page.shownState) return
            page.shownState = page.wizard.state
            page.step = page.wizard.state === "editing" ? 0 : 3
        }
    }
    function create() {
        if (page.started) page.wizard.retry(); else page.wizard.start()
        // Not started: show the first step with a problem
        if (page.wizard.state === "editing" || page.failed) {
            for (let i = 0; i < 3; ++i) {
                if (page.stepHasProblems(i)) { page.step = i; return }
            }
        }
    }

    ConfirmDialog {
        id: startOverDialog
        onAccepted: page.wizard.startOver()
    }

    FolderDialog {
        id: parentFolder
        title: qsTr("Where to make the project")
        onAccepted: page.wizard.setParentFolder(selectedFolder)
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: Math.min(860, parent.width - 48)
            x: (parent.width - width) / 2
            spacing: 22

            Item { implicitHeight: 8 }

            // --- Title ---
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Image {
                    source: "qrc:/icons/logo-128.png"
                    sourceSize: Qt.size(64, 64)
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    mipmap: true
                }
                Heading { level: 1; ui: qsTr("New FloCMS project"); Layout.fillWidth: true }
                AppButton {
                    text: page.wizard.state === "done" ? qsTr("Close") : qsTr("Cancel")
                    enabled: !page.running
                    onClicked: Studio.showProjects()
                }
            }

            // --- The steps ---
            RowLayout {
                spacing: 20
                Repeater {
                    model: page.stepNames
                    delegate: RowLayout {
                        id: stepItem
                        required property string modelData
                        required property int index
                        readonly property bool now: page.step === index
                        readonly property bool skipped: index === 2 && !page.hasAdmin
                        opacity: skipped ? 0.45 : 1
                        spacing: 8
                        Rectangle {
                            width: 26; height: 26; radius: 13
                            color: stepItem.now ? Theme.accent : Theme.surface
                            border.color: stepItem.now ? Theme.accent : (page.stepHasProblems(Math.min(index, 2)) && index < 3 ? Theme.bad : Theme.borderStrong)
                            AppText {
                                anchors.centerIn: parent
                                text: Lang.num(stepItem.index + 1)
                                color: stepItem.now ? Theme.accentText : Theme.text
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }
                        AppText {
                            ui: stepItem.modelData
                            color: stepItem.now ? Theme.text : Theme.muted
                            font.weight: stepItem.now ? Font.DemiBold : Font.Normal
                        }
                    }
                }
            }

            // --- The card ---
            Card {
                Layout.fillWidth: true
                padding: 28
                gap: 22

                // Step 1: the project (fixed once the folder is made)
                ColumnLayout {
                    visible: page.step === 0
                    enabled: !page.started
                    spacing: 22
                    Layout.fillWidth: true

                    GridLayout {
                        Layout.fillWidth: true
                        columns: page.width > 900 ? 2 : 1
                        columnSpacing: 18
                        rowSpacing: 18
                        FormField {
                            label: qsTr("Project name")
                            hint: page.form.name && page.form.name !== page.form.typedName
                                  ? qsTr("The folder will be `%1`.").arg(page.form.name)
                                  : qsTr("Lower case, used for the folder and the database.")
                            problem: page.problems.name || ""
                            Layout.fillWidth: true
                            Field {
                                text: page.form.typedName
                                placeholderText: "my-shop"
                                Layout.fillWidth: true
                                Accessible.name: qsTr("Project name")
                                onTextEdited: page.wizard.setName(text)
                                onEditingFinished: page.wizard.touch("name")
                            }
                        }
                        FormField {
                            label: qsTr("Folder")
                            hint: qsTr("Made in: %1").arg("`" + page.form.parentDir + "`")
                            problem: page.problems.parentDir || ""
                            Layout.fillWidth: true
                            RowLayout {
                                spacing: 8
                                Layout.fillWidth: true
                                Field {
                                    mono: true
                                    text: page.form.folder
                                    readOnly: true
                                    Layout.fillWidth: true
                                    Accessible.name: qsTr("Folder")
                                }
                                AppButton { text: qsTr("Browse…"); onClicked: parentFolder.open() }
                            }
                        }
                    }

                    FormField {
                        label: qsTr("Languages")
                        hint: qsTr("Sets `LANGUAGES` and `DEFAULT_LANG` in `.env`. Click a chosen language again to make it the default. Right-to-left languages get the right-to-left styles automatically.")
                        problem: page.problems.languages || page.problems.defaultLanguage || ""
                        Layout.fillWidth: true
                        Flow {
                            spacing: 8
                            Layout.fillWidth: true
                            Repeater {
                                // FloCMS's codes (ku for Kurdish)
                                model: [
                                    { code: "en", name: "English" }, { code: "ar", name: "العربية" },
                                    { code: "ku", name: "کوردی" }, { code: "el", name: "Ελληνικά" },
                                    { code: "tr", name: "Türkçe" }, { code: "fa", name: "فارسی" },
                                    { code: "de", name: "Deutsch" }, { code: "fr", name: "Français" }
                                ]
                                delegate: LanguageChip {
                                    required property var modelData
                                    name: modelData.name
                                    chosen: (page.form.languages || []).indexOf(modelData.code) >= 0
                                    isDefault: page.form.defaultLanguage === modelData.code
                                    onClicked: {
                                        const chosenNow = (page.form.languages || []).slice()
                                        if (!chosen) {
                                            chosenNow.push(modelData.code)
                                            page.wizard.set("languages", chosenNow)
                                        } else if (!isDefault) {
                                            page.wizard.set("defaultLanguage", modelData.code)
                                        } else if (chosenNow.length > 1) {
                                            page.wizard.set("languages", chosenNow.filter(c => c !== modelData.code))
                                        }
                                    }
                                }
                            }
                        }
                    }

                    FormField {
                        label: qsTr("What are you building?")
                        hint: qsTr("You can add the admin panel or the API later from Packages.")
                        Layout.fillWidth: true
                        GridLayout {
                            Layout.fillWidth: true
                            columns: page.width > 900 ? 2 : 1
                            columnSpacing: 10
                            rowSpacing: 10
                            ButtonGroup { id: setups }
                            Repeater {
                                model: [
                                    { value: "site-admin", title: qsTr("Website + admin panel"), note: qsTr("Your website, with the admin panel to manage its pages and content.") },
                                    { value: "site", title: qsTr("Website only"), note: qsTr("Your website with the default theme. Nothing to add.") },
                                    { value: "admin", title: qsTr("Admin panel only"), note: qsTr("No website: the home page opens the admin panel.") },
                                    { value: "api-admin", title: qsTr("API + admin panel"), note: qsTr("A JSON API for apps, with the admin panel to manage the data.") },
                                    { value: "api", title: qsTr("API only"), note: qsTr("A JSON API for apps. The home page answers in JSON.") }
                                ]
                                delegate: OptionRow {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    radio: true
                                    group: setups
                                    checked: page.form.setup === modelData.value
                                    title: modelData.title
                                    note: modelData.note
                                    onToggled: (on) => { if (on) page.wizard.set("setup", modelData.value) }
                                }
                            }
                        }
                        OptionRow {
                            visible: !!page.form.hasSite
                            Layout.fillWidth: true
                            checked: !!page.form.addApi
                            title: qsTr("Also add the JSON API")
                            note: qsTr("For apps or a JavaScript front end.")
                            onToggled: (on) => page.wizard.set("addApi", on)
                        }
                    }
                }

                // Step 2: the database
                ColumnLayout {
                    visible: page.step === 1
                    spacing: 18
                    Layout.fillWidth: true
                    Segmented {
                        enabled: !page.started
                        label: qsTr("Database")
                        value: page.form.database
                        options: [
                            { value: "sqlite", text: qsTr("SQLite: a file in the project") },
                            { value: "mysql", text: qsTr("MySQL / MariaDB") }
                        ]
                        onChosen: (value) => page.wizard.set("database", value)
                    }
                    RichText {
                        visible: page.form.database === "sqlite"
                        raw: qsTr("The database is the file `storage/database.sqlite`, made by the first migration. Nothing to install; good for trying things and small sites.")
                        color: Theme.muted
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    GridLayout {
                        visible: page.form.database === "mysql"
                        Layout.fillWidth: true
                        columns: page.width > 900 ? 2 : 1
                        columnSpacing: 18
                        rowSpacing: 18
                        FormField {
                            label: qsTr("Database name")
                            hint: qsTr("It must exist already: create it in phpMyAdmin, HeidiSQL or `mysql`.")
                            problem: page.problems.dbName || ""
                            Layout.fillWidth: true
                            Field { mono: true; text: page.form.dbName; Layout.fillWidth: true; Accessible.name: qsTr("Database name"); onTextEdited: page.wizard.set("dbName", text); onEditingFinished: page.wizard.touch("dbName") }
                        }
                        FormField {
                            label: qsTr("Server")
                            problem: page.problems.dbHost || page.problems.dbPort || ""
                            Layout.fillWidth: true
                            RowLayout {
                                spacing: 8
                                Layout.fillWidth: true
                                Field { mono: true; text: page.form.dbHost; Layout.fillWidth: true; Accessible.name: qsTr("Server"); onTextEdited: page.wizard.set("dbHost", text); onEditingFinished: page.wizard.touch("dbHost") }
                                Field {
                                    mono: true
                                    text: String(page.form.dbPort)
                                    validator: IntValidator { bottom: 1; top: 65535 }
                                    Layout.preferredWidth: 90
                                    Accessible.name: qsTr("Port")
                                    // Only a whole port: clearing the field to type another keeps the old one
                                    onTextEdited: if (acceptableInput && parseInt(text) !== page.form.dbPort) page.wizard.set("dbPort", parseInt(text))
                                    onEditingFinished: page.wizard.touch("dbPort")
                                }
                            }
                        }
                        FormField {
                            label: qsTr("User")
                            problem: page.problems.dbUser || ""
                            Layout.fillWidth: true
                            Field { mono: true; text: page.form.dbUser; Layout.fillWidth: true; Accessible.name: qsTr("User"); onTextEdited: page.wizard.set("dbUser", text); onEditingFinished: page.wizard.touch("dbUser") }
                        }
                        FormField {
                            label: qsTr("Password")
                            hint: qsTr("Written to `DB_PASSWORD` in `.env`, where FloCMS reads it. Empty for XAMPP's root.")
                            problem: page.problems.dbPassword || ""
                            Layout.fillWidth: true
                            RowLayout {
                                spacing: 8
                                Layout.fillWidth: true
                                Field { text: page.form.dbPassword; echoMode: TextInput.Password; Layout.fillWidth: true; Accessible.name: qsTr("Password"); onTextEdited: page.wizard.set("dbPassword", text) }
                                AppButton { visible: page.wizard.secureStore; small: true; text: qsTr("Use saved"); onClicked: page.wizard.fillSavedPassword() }
                            }
                        }
                    }
                    OptionRow {
                        visible: page.form.database === "mysql" && page.wizard.secureStore
                        Layout.fillWidth: true
                        checked: !!page.form.rememberPassword
                        title: Qt.platform.os === "osx" ? qsTr("Remember the password in the Keychain")
                               : (Qt.platform.os === "windows" ? qsTr("Remember the password in Windows Credential Manager")
                                  : qsTr("Remember the password in the system's keyring"))
                        note: qsTr("For your next project on this database server. Studio keeps no copy of its own.")
                        onToggled: (on) => page.wizard.set("rememberPassword", on)
                    }
                }

                // Step 3: the first admin
                ColumnLayout {
                    visible: page.step === 2
                    spacing: 18
                    Layout.fillWidth: true
                    AppText {
                        ui: qsTr("The first user of the admin panel, with every right (super admin).")
                        color: Theme.muted
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: page.width > 900 ? 2 : 1
                        columnSpacing: 18
                        rowSpacing: 18
                        FormField {
                            label: qsTr("Name")
                            problem: page.problems.adminName || ""
                            Layout.fillWidth: true
                            Field { text: page.form.adminName; Layout.fillWidth: true; Accessible.name: qsTr("Name"); onTextEdited: page.wizard.set("adminName", text); onEditingFinished: page.wizard.touch("adminName") }
                        }
                        FormField {
                            label: qsTr("Email")
                            hint: qsTr("Also the login name.")
                            problem: page.problems.adminEmail || ""
                            Layout.fillWidth: true
                            Field { mono: true; text: page.form.adminEmail; Layout.fillWidth: true; Accessible.name: qsTr("Email"); onTextEdited: page.wizard.set("adminEmail", text); onEditingFinished: page.wizard.touch("adminEmail") }
                        }
                        FormField {
                            label: qsTr("Password")
                            hint: qsTr("At least 8 characters. It goes to FloCMS on its input, never on a command line or in a log.")
                            problem: page.problems.adminPassword || ""
                            Layout.fillWidth: true
                            Field { text: page.form.adminPassword; echoMode: TextInput.Password; Layout.fillWidth: true; Accessible.name: qsTr("Password"); onTextEdited: page.wizard.set("adminPassword", text); onEditingFinished: page.wizard.touch("adminPassword") }
                        }
                    }
                }

                // Step 4: create
                ColumnLayout {
                    visible: page.step === 3
                    spacing: 14
                    Layout.fillWidth: true
                    Repeater {
                        model: page.started ? page.wizard.steps : page.wizard.plan
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 10
                            StatusIcon {
                                status: modelData.state === "done" ? "ok" : (modelData.state === "failed" ? "fail" : (modelData.state === "running" ? "running" : "note"))
                                Layout.alignment: Qt.AlignTop
                            }
                            ColumnLayout {
                                spacing: 2
                                Layout.fillWidth: true
                                Mono {
                                    text: modelData.id === "env" ? "" : modelData.shown
                                    visible: modelData.id !== "env"
                                    color: modelData.state === "waiting" || !modelData.state ? Theme.muted : Theme.text
                                    wrapMode: Text.WrapAnywhere
                                    Layout.fillWidth: true
                                }
                                RichText {
                                    visible: modelData.id === "env"
                                    raw: qsTr("Studio writes `.env`: name, address, languages, database")
                                    color: modelData.state === "waiting" || !modelData.state ? Theme.muted : Theme.text
                                    Layout.fillWidth: true
                                }
                                // Why it failed: Studio's own words, or what the command said
                                RichText {
                                    visible: !!modelData.code
                                    raw: modelData.code === "step_exit" ? qsTr("The command ended with exit code %1.").arg(Lang.num(parseInt(modelData.detail) || 0))
                                                                        : (modelData.code ? Texts.problem(modelData.code) : "")
                                    color: Theme.badText
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Mono {
                                    visible: !!modelData.message || (!!modelData.detail && modelData.code !== "step_exit")
                                    text: modelData.message || modelData.detail || ""
                                    color: Theme.badText
                                    font.pixelSize: 12
                                    wrapMode: Text.WrapAnywhere
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                    CommandOutput {
                        visible: page.started && page.wizard.lines.length > 0
                        bare: true
                        output: ({ state: page.wizard.state, lines: page.wizard.lines })
                        Layout.fillWidth: true
                    }
                    RichText {
                        visible: page.wizard.state === "done"
                        raw: qsTr("`%1` is ready. Open it to start its server.").arg(page.form.name)
                        color: Theme.okText
                        font.weight: Font.DemiBold
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    RichText {
                        visible: page.wizard.state === "failed"
                        raw: qsTr("A step failed. Fix what it says (Back leads to the database and admin settings), then try again: Studio continues from that step.")
                        color: Theme.badText
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }

                // --- Buttons ---
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Item { visible: !page.failed; Layout.fillWidth: true }
                    AppButton {
                        visible: page.failed
                        text: qsTr("Start over")
                        danger: true
                        onClicked: startOverDialog.ask({
                            title: qsTr("Start over?"),
                            message: qsTr("Studio deletes the folder it made, `%1`, and everything in it. The form keeps nothing.").arg(page.form.folder),
                            confirmText: qsTr("Delete and start over")
                        })
                    }
                    Item { visible: page.failed; Layout.fillWidth: true }
                    AppButton {
                        visible: page.step > 0 && (!page.started || page.failed)
                        text: qsTr("Back")
                        iconPath: Icons.back
                        iconDirectional: true
                        onClicked: page.back()
                    }
                    AppButton {
                        visible: page.step < 3
                        primary: true
                        text: page.step === 0 ? qsTr("Next: Database") : (page.step === 1 && page.hasAdmin ? qsTr("Next: First admin") : qsTr("Next: Create"))
                        onClicked: page.next()
                    }
                    AppButton {
                        visible: page.step === 3 && page.wizard.state === "editing"
                        primary: true
                        text: qsTr("Create the project")
                        iconPath: Icons.plus
                        onClicked: page.create()
                    }
                    AppButton {
                        visible: page.running
                        text: qsTr("Stop")
                        iconPath: Icons.stop
                        iconFilled: true
                        danger: true
                        onClicked: page.wizard.cancel()
                    }
                    AppButton {
                        visible: page.failed && page.step === 3
                        primary: true
                        text: qsTr("Try again")
                        iconPath: Icons.refresh
                        onClicked: page.create()
                    }
                    AppButton {
                        visible: page.wizard.state === "done"
                        primary: true
                        text: qsTr("Open the project")
                        onClicked: Studio.openNewProject()
                    }
                }
            }

            Item { implicitHeight: 24 }
        }
    }
}
