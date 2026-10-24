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
        case "server_failed": return qsTr("The server did not start.")
        case "server_no_answer": return qsTr("The server said it started, but nothing answers at its address. Another program may hold the port: try another port in Settings > Server.")
        case "server_ended": return qsTr("The server stopped by itself.")
        case "php_not_found": return qsTr("PHP was not found. Download it in This computer, or choose it in Settings > PHP and tools.")
        case "php_unsafe": return qsTr("PHP is a `.bat` or `.cmd` file, and the project folder's name has characters `cmd.exe` would read itself ( ) & ! %. Choose `php.exe` in Settings > PHP and tools.")
        case "no_launcher": return qsTr("The project has no `flo` launcher yet: run `composer install` in it.")
        case "no_log_file": return qsTr("There is no `storage/logs/app.log` yet. FloCMS writes it at the first message.")
        }
        return qsTr("Something went wrong (%1).").arg(code)
    }
}
