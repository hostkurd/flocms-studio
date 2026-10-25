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

    // Studio's MariaDB (MariaDB design note D11)
    function mariadbState(state) {
        Lang.code
        switch (state) {
        case "running": return qsTr("Running")
        case "starting": return qsTr("Starting…")
        case "stopping": return qsTr("Stopping…")
        case "failed": return qsTr("Didn't start")
        }
        return qsTr("Stopped")
    }

    // Where the password of root is kept, by system
    function secureStoreName() {
        Lang.code
        if (Studio.platform.startsWith("windows")) return qsTr("Windows Credential Manager")
        if (Studio.platform.startsWith("macos")) return qsTr("the macOS Keychain")
        return qsTr("the system's keyring")
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
    // A `php flo doctor` check by its English name (flocms-cli's
    // DoctorCommand): translated, with code words in backticks
    function checkName(name) {
        Lang.code
        const extension = /^Extension (\S+)$/.exec(name)
        if (extension)
            return qsTr("Extension `%1`").arg(extension[1])
        if (/^[\w./-]+\/$/.test(name) && name !== "public/.htaccess")
            return qsTr("Folder `%1`").arg(name)
        switch (name) {
        case "PHP version": return qsTr("PHP version")
        case "Composer dependencies": return qsTr("Composer packages installed")
        case ".env file": return qsTr("File `.env`")
        case ".env keys": return qsTr("Keys in `.env`")
        case "APP_KEY": return "`APP_KEY`"
        case "APP_URL": return "`APP_URL`"
        case "APP_DEBUG": return qsTr("Debug mode (`APP_DEBUG`)")
        case "HTTPS": return "HTTPS"
        case "Database": return qsTr("Database")
        case "Migrations": return qsTr("Migrations")
        case "public/.htaccess": return qsTr("Rewrite rules (`public/.htaccess`)")
        case "Document root": return qsTr("Document root")
        case "Maintenance mode": return qsTr("Maintenance mode")
        }
        return "`" + name + "`"
    }

    // What is wrong with a wizard field (Core's checkNewProject)
    function fieldProblem(code) {
        Lang.code
        switch (code) {
        case "name_invalid": return qsTr("Use letters, digits, `-` and `_` (the folder's name).")
        case "folder_exists": return qsTr("That folder is there already. Choose another name or folder.")
        case "parent_invalid": return qsTr("Choose a folder on this computer.")
        case "app_name_invalid": return qsTr("One line only.")
        case "languages_empty": return qsTr("Choose at least one language.")
        case "default_language": return qsTr("The default must be one of the chosen languages.")
        case "db_name_invalid": return qsTr("Letters, digits and `_` only, up to 64.")
        case "db_user_empty": return qsTr("Enter the database user (XAMPP: `root`).")
        case "db_user_invalid": return qsTr("Letters, digits and `_` only, and not `root`.")
        case "db_port_invalid": return qsTr("A port between 1 and 65535.")
        case "db_host_invalid": return qsTr("Enter the database server (usually `127.0.0.1`).")
        case "db_password_invalid": return qsTr("One line only.")
        case "admin_name_empty": return qsTr("Enter a name.")
        case "email_invalid": return qsTr("Enter an email address.")
        case "password_short": return qsTr("Use at least 8 characters.")
        case "password_invalid": return qsTr("One line only.")
        // The Generate tab
        case "name_empty": return qsTr("Enter a name.")
        case "class_name": return qsTr("A class name in StudlyCase: a capital first, then letters and digits (`BlogPost`).")
        case "controller_name": return qsTr("Letters, digits, `-` and `_`, starting with a letter.")
        case "migration_name": return qsTr("Letters, digits and `_`, starting with a letter (`add_slug_to_posts`).")
        case "table_name": return qsTr("A table name: letters, digits and `_`, up to 64.")
        case "table_needed": return qsTr("Enter the table.")
        case "slug_name": return qsTr("Lower case letters and digits, with `-` between words.")
        case "resource_name": return qsTr("A resource's class name, e.g. `Post`.")
        case "resource_needed": return qsTr("Enter the resource it lists, e.g. `Post`.")
        case "group_invalid": return qsTr("One line only.")
        case "block_name_long": return qsTr("Too long: its template name would be over 41 characters.")
        }
        return problem(code)
    }

    // The same without the backticks, for plain text
    function problemPlain(code) {
        return problem(code).replace(/`/g, "")
    }

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
        case "db_create": return qsTr("Studio could not make the database and its user on its MariaDB.")
        case "db_port_busy": return qsTr("The port is in use by another program, so Studio's MariaDB can't start on it.")
        case "db_library": return qsTr("MariaDB needs a library this system doesn't have.")
        case "db_program": return qsTr("Studio's MariaDB doesn't run on this computer.")
        case "db_init": return qsTr("Studio could not set up the databases folder.")
        case "db_start": return qsTr("MariaDB stopped while starting. The end of its log:")
        case "db_timeout": return qsTr("MariaDB didn't answer in time. The end of its log:")
        case "db_root": return qsTr("Studio could not set the password of MariaDB's `root` user.")
        case "db_root_lost": return qsTr("The password of `root` is missing from the system's secure store, so Studio can't use these databases. Delete the databases in Settings to start again.")
        case "db_keychain": return qsTr("Studio could not use the system's secure store for the password of `root`.")
        case "db_crashed": return qsTr("MariaDB stopped unexpectedly. The end of its log:")
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
        case "parent_invalid": return qsTr("The folder for the new project could not be made.")
        case "folder_remove": return qsTr("Studio could not delete the folder it had made. Close what uses it (an editor, a terminal), then try again.")
        case "env_write": return qsTr("Studio could not write the project's `.env` file.")
        case "env_key":
        case "env_value": return qsTr("A value for `.env` has a character FloCMS can't read there (a line break).")
        case "step_stopped": return qsTr("Stopped.")
        case "outdated_failed": return qsTr("Composer could not say what is newer. It asks Packagist, so this needs the internet.")
        case "no_vendor": return qsTr("The project has no `vendor/` folder yet: run `composer install` in it.")
        case "preview_too_big": return qsTr("The project is too large to copy for a preview (over 400 MB or 40,000 files, without uploads and logs). Create works without one.")
        case "preview_copy": return qsTr("Studio could not copy the project for the preview.")
        case "preview_cancelled": return qsTr("The preview was stopped.")
        case "preview_remove": return qsTr("Studio could not remove the preview's copy. It tries again when it next starts.")
        case "not_allowed": return qsTr("Studio does not run this command.")
        case "keychain_unavailable": return qsTr("There is no secure store for passwords on this computer.")
        case "keychain_denied": return qsTr("The secure store did not give the password.")
        case "keychain_missing": return qsTr("No password is saved for this database user.")
        case "keychain_failed": return qsTr("The secure store did not answer.")
        case "composer_not_found": return qsTr("Composer was not found. Download it in This computer, or choose it in Settings > PHP and tools.")
        case "project_busy": return qsTr("A command is still running in this project. Remove it from the list when it has finished.")
        case "no_env_file": return qsTr("The project has no `.env` file. Copy `.env.example` to `.env`.")
        case "command_failed": return qsTr("The command could not run.")
        case "doctor_unreadable": return qsTr("Studio could not read what the check printed.")
        case "database_unreadable": return qsTr("Studio could not read the database. Is the database server running, and are the settings in `.env` right?")
        case "migrations_unreadable": return qsTr("Studio could not read the migrations.")
        case "no_log_file": return qsTr("There is no `storage/logs/app.log` yet. FloCMS writes it at the first message.")
        }
        return qsTr("Something went wrong (%1).").arg(code)
    }
}
