import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FloStudio.Backend

// The Database tab's set-up for a project with no database (design note
// of 30 Oct, part C): the server for this project (D-D1), the database and
// its user, one button. Studio's MariaDB is started when stopped; another
// server's missing database can be created (C-D4).
ColumnLayout {
    id: form
    property var setup
    readonly property var values: setup ? setup.form : ({})
    readonly property var problems: setup ? setup.problems : ({})
    readonly property bool studio: values.server === "studio"
    readonly property bool busy: !!setup && setup.state === "running"
    spacing: 14

    RichText {
        raw: qsTr("This project has no database yet: no `DB_NAME` in `.env`. Studio can make one and write the `DB_` lines for you.")
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    Segmented {
        visible: !!form.values.hasOwn
        enabled: !form.busy
        label: qsTr("Server")
        value: form.studio ? "studio" : "other"
        options: [
            { value: "studio", text: qsTr("Studio's MariaDB %1").arg(Studio.mariadb.series) },
            { value: "other", text: qsTr("Another server") }
        ]
        onChosen: (value) => form.setup.set("server", value)
    }
    GridLayout {
        Layout.fillWidth: true
        columns: form.width > 640 ? 2 : 1
        columnSpacing: 18
        rowSpacing: 12
        FormField {
            label: qsTr("Database")
            problem: form.problems.dbName || ""
            Layout.fillWidth: true
            Field { mono: true; enabled: !form.busy; text: form.values.name || ""; Layout.fillWidth: true; Accessible.name: qsTr("Database"); onTextEdited: form.setup.set("name", text) }
        }
        FormField {
            label: qsTr("User")
            problem: form.problems.dbUser || ""
            Layout.fillWidth: true
            Field { mono: true; enabled: !form.busy; text: form.values.user || ""; Layout.fillWidth: true; Accessible.name: qsTr("User"); onTextEdited: form.setup.set("user", text) }
        }
        FormField {
            visible: !form.studio
            label: qsTr("Server")
            problem: form.problems.dbHost || form.problems.dbPort || ""
            Layout.fillWidth: true
            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                Field { mono: true; enabled: !form.busy; text: form.values.host || ""; Layout.fillWidth: true; Accessible.name: qsTr("Server"); onTextEdited: form.setup.set("host", text) }
                Field {
                    mono: true
                    enabled: !form.busy
                    text: String(form.values.port || "")
                    validator: IntValidator { bottom: 1; top: 65535 }
                    Layout.preferredWidth: 90
                    Accessible.name: qsTr("Port")
                    onTextEdited: if (acceptableInput) form.setup.set("port", parseInt(text))
                }
            }
        }
        FormField {
            visible: !form.studio
            label: qsTr("Password")
            hint: qsTr("Written to `DB_PASSWORD` in `.env`, where FloCMS reads it. Empty for XAMPP's root.")
            problem: form.problems.dbPassword || ""
            Layout.fillWidth: true
            Field { enabled: !form.busy; echoMode: TextInput.Password; Layout.fillWidth: true; Accessible.name: qsTr("Password"); onTextEdited: form.setup.set("password", text) }
        }
    }
    RowLayout {
        spacing: 12
        Layout.fillWidth: true
        RichText {
            raw: form.studio
                 ? (Studio.mariadb.state === "running"
                    ? qsTr("On Studio's MariaDB (`127.0.0.1:%1`). Studio makes the user's password and keeps it in %2.").arg(Studio.mariadb.port).arg(Texts.secureStoreName())
                    : qsTr("Studio's MariaDB is stopped: Studio starts it first (port `%1`). It makes the user's password and keeps it in %2.").arg(Studio.mariadb.port).arg(Texts.secureStoreName()))
                 : qsTr("The database may exist already; if not, Studio offers to create it.")
            color: Theme.muted
            font.pixelSize: 13
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        AppButton {
            primary: true
            enabled: !form.busy && form.setup.state !== "missing"
            text: form.busy ? qsTr("Setting up…") : qsTr("Set up database")
            onClicked: form.setup.start()
        }
    }
    // What runs now
    AppText {
        visible: form.busy && form.setup.step !== ""
        ui: form.setup.step === "starting" ? qsTr("Starting Studio's MariaDB…")
            : (form.setup.step === "creating" ? qsTr("Creating the database…") : qsTr("Checking the connection…"))
        color: Theme.muted
        Layout.fillWidth: true
    }
    // The server has no such database (C-D4)
    Rectangle {
        visible: !!form.setup && form.setup.state === "missing"
        Layout.fillWidth: true
        implicitHeight: missingColumn.implicitHeight + 28
        radius: 8
        color: Theme.optionSelectedBg
        border.color: Theme.border
        ColumnLayout {
            id: missingColumn
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10
            RichText {
                raw: qsTr("The server on `%1:%2` answered, but it has no database called `%3`. Studio can create it with this login.")
                         .arg(form.values.host || "").arg(form.values.port || "").arg(form.values.name || "")
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            AppButton {
                primary: true
                text: qsTr("Create database “%1”").arg(form.values.name || "")
                onClicked: form.setup.createDatabase()
            }
        }
    }
    // Why it failed
    RichText {
        visible: !!form.setup && form.setup.code !== ""
        raw: form.setup && form.setup.code ? Texts.problem(form.setup.code) : ""
        color: Theme.badText
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
    Mono {
        visible: !!form.setup && form.setup.message !== "" && form.setup.state !== "running"
        text: form.setup ? form.setup.message : ""
        color: Theme.badText
        font.pixelSize: 12
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        Layout.fillWidth: true
    }
}
