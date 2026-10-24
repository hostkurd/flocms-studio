pragma Singleton
import QtQuick
import FloStudio.Backend

// Words used on several screens, and the messages for the error codes
// Core returns (each header lists its codes). `Lang.code` at the start of
// each function makes bindings that call it follow a language switch.
QtObject {
    function toolName(tool) {
        Lang.code
        return tool === "php" ? "PHP" : (tool === "composer" ? "Composer" : "MariaDB")
    }

    function toolState(row) {
        Lang.code
        switch (row.state) {
        case "checking": return qsTr("Looking…")
        case "found": return row.version
        case "running": return qsTr("%1 running").arg(row.version)
        case "missing": return qsTr("Missing")
        case "optional": return qsTr("Not found (optional)")
        case "unsupported": return qsTr("%1 is too old").arg(row.version)
        case "failed": return qsTr("Doesn't run")
        }
        return ""
    }

    function typeName(type) {
        Lang.code
        switch (type) {
        case "site": return qsTr("Website")
        case "siteAdmin": return qsTr("Website + admin panel")
        case "siteApi": return qsTr("Website + API")
        case "siteAdminApi": return qsTr("Website + admin panel + API")
        case "admin": return qsTr("Admin panel only")
        case "apiAdmin": return qsTr("API + admin panel")
        case "api": return qsTr("API only")
        }
        return ""
    }

    // The message for an error code from Core; `detail` is the path, the
    // command or the system's own message, shown under it.
    function problem(code) {
        Lang.code
        switch (code) {
        case "not_flocms": return qsTr("This folder is not a FloCMS project: its `composer.json` neither is the FloCMS skeleton nor requires `hostkurd/flocms-core`.")
        case "project_missing": return qsTr("The project folder is gone.")
        case "settings_read": return qsTr("Studio could not read its settings file, so it runs with the defaults and won't save over it. Close the program that holds the file, then start Studio again.")
        case "settings_write": return qsTr("Studio could not save its settings.")
        case "editor_not_found": return qsTr("The editor in Settings > Editor was not found.")
        case "editor_failed": return qsTr("The editor did not start.")
        case "unsafe_argument": return qsTr("The editor is a `.cmd` file, and the folder's name has characters `cmd.exe` would read itself ( ) & ! %. Choose the editor's `.exe` in Settings > Editor.")
        case "open_failed": return qsTr("The folder could not be opened.")
        case "tool_download": return qsTr("The download failed.")
        case "tool_hash": return qsTr("The downloaded file is not the one Studio expects (its SHA-256 differs), so it was deleted.")
        case "tool_extract": return qsTr("The download could not be unpacked.")
        case "tool_install": return qsTr("The download could not be installed.")
        case "tool_cancelled": return qsTr("Cancelled.")
        }
        return qsTr("Something went wrong (%1).").arg(code)
    }
}
