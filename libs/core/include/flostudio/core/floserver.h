#pragma once

#include <flostudio/core/export.h>
#include <flostudio/core/settings.h>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace FloStudio::Core {

// FloServer and Studio working together (design D14). FloServer (HostKurd's
// local server: PHP per project, MariaDB, https://<name>.localhost) writes
// integration.json at the top of its data folder:
//
//   {"schema": 1, "floserver_version": "0.1.0",
//    "cli": "<full path of flo(.exe)>", "shims": "<folder of its php,
//    composer, node... shims>", "runtimes": [{"name": "php", "version":
//    "8.5.12"}, ...], "databases": [{"series": "11.4", "version":
//    "11.4.13", "port": 3306}]}
//
// No secrets. An unknown schema, or a file that is missing, unreadable or
// not this shape, means FloServer isn't there. `cli` may be empty.
//
// With Settings::useFloServer on and FloServer found, PHP and Composer are
// FloServer's shims (each project gets the PHP in its floserver.json) and
// the shims folder goes first on the PATH of the commands Studio starts.
// Settings::phpPath and composerPath are kept, so turning it off brings
// them back. Studio's database server choice is not changed by it.

struct FloServerRuntime
{
    QString name;    // php, composer, mariadb, node, python...
    QString version; // 8.5.12
    bool operator==(const FloServerRuntime &) const = default;
};

struct FloServerDatabase
{
    QString series;  // 11.4
    QString version; // 11.4.13, empty when FloServer didn't say
    int port = 0;    // the port projects connect to
    bool operator==(const FloServerDatabase &) const = default;
};

struct FloServerInfo
{
    QString version; // FloServer's own
    QString cli;     // flo(.exe), native separators; empty when not known
    QString shims;   // the shims folder, native separators
    QList<FloServerRuntime> runtimes;
    QList<FloServerDatabase> databases;
    bool operator==(const FloServerInfo &) const = default;
};

// FloServer's data folder: FLOSERVER_DATA_DIR (an absolute path) when set,
// else %LOCALAPPDATA%\FloServer (Windows), ~/Library/Application
// Support/FloServer (macOS), $XDG_DATA_HOME/floserver or
// ~/.local/share/floserver (Linux). Empty when none can be made.
FLOSTUDIOCORE_EXPORT QString floServerDataFolder();

// integration.json's text read back; nothing when it isn't schema 1 in the
// shape above. Entries of runtimes and databases that aren't are left out.
FLOSTUDIOCORE_EXPORT std::optional<FloServerInfo> parseFloServerIntegration(const QByteArray &json);
// dataFolder/integration.json (up to 1 MB).
FLOSTUDIOCORE_EXPORT std::optional<FloServerInfo> readFloServerIntegration(const QString &dataFolder);

// FloServer, when the settings use it and it is found (read each call: the
// file is small, and FloServer may be installed or removed while Studio
// runs). Nothing otherwise.
FLOSTUDIOCORE_EXPORT std::optional<FloServerInfo> floServerInUse(const Settings &settings);

// A shim's program: shims/php (php.exe on Windows). Empty when it isn't
// there.
FLOSTUDIOCORE_EXPORT QString floServerShim(const FloServerInfo &info, const QString &tool);
// Where that shim would be, there or not (for messages).
FLOSTUDIOCORE_EXPORT QString floServerShimPath(const FloServerInfo &info, const QString &tool);

// The PATH for commands Studio starts: FloServer's shims first when the
// settings use FloServer, then toolSearchPath().
FLOSTUDIOCORE_EXPORT QString commandSearchPath(const Settings &settings);

// The installed versions of a runtime (php, composer...), newest first.
FLOSTUDIOCORE_EXPORT QStringList floServerVersions(const FloServerInfo &info, const QString &runtime);

} // namespace FloStudio::Core
