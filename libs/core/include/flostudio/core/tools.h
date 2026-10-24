#pragma once

#include <flostudio/core/export.h>
#include <flostudio/core/result.h>
#include <flostudio/core/settings.h>

#include <QString>
#include <QStringList>

namespace FloStudio::Core {

// Finding PHP, Composer and the database server (D15: Studio uses what is
// installed; downloading them comes with the tools sidebar, stage 3).
//
// The detect* functions start programs and wait for them (a few seconds at
// most): call them from a worker thread, never the interface thread.

// Where Studio looks for programs: PATH, then (macOS and Linux) the login
// shell's PATH, then the usual install folders. Apps started from the
// Finder or a desktop menu get a short PATH without Homebrew, Herd or
// Composer's folder, so the shell is asked once. The same list becomes
// the PATH of every command Studio starts.
FLOSTUDIOCORE_EXPORT QStringList toolSearchPath();

// The usual install folders of PHP, Composer and MySQL/MariaDB on this
// system that exist, for a home folder (Homebrew, Herd, XAMPP, Laragon,
// Composer-Setup, ...).
FLOSTUDIOCORE_EXPORT QStringList commonToolFolders(const QString &home);

// A program by name on toolSearchPath() (php.exe, composer.bat on
// Windows), or `nameOrPath` itself when it is a path to a file. Empty when
// not found.
FLOSTUDIOCORE_EXPORT QString findTool(const QString &nameOrPath);

// How to start Composer. A composer.phar runs through PHP; Composer-Setup's
// composer.bat is a wrapper around the composer.phar next to it, which
// Studio runs directly instead (cmd.exe would read the arguments itself).
struct ComposerLaunch
{
    QString program;           // php, or the composer program
    QStringList arguments;     // [composer.phar] when it runs through PHP
    QString path;              // the Composer that was found
    // COMPOSER_BINARY for `php flo setup`, which runs [PHP, it] or a .bat
    // directly; empty when Composer is neither (setup then uses `composer`
    // from PATH).
    QString setupBinary;
    bool batchFile = false;    // a .bat/.cmd with no composer.phar beside it

    bool operator==(const ComposerLaunch &) const = default;
};

// Error codes: php_not_found, composer_not_found.
FLOSTUDIOCORE_EXPORT Result<QString> phpProgram(const Settings &settings);
FLOSTUDIOCORE_EXPORT Result<ComposerLaunch> composerLaunch(const Settings &settings);

// Whether `argument` can be passed to a .bat/.cmd file: cmd.exe reads
// " % ^ & | < > ! ( ) itself, so arguments with them are refused.
FLOSTUDIOCORE_EXPORT bool safeForBatchFile(const QString &argument);

struct PhpInfo
{
    QString version;        // 8.3.6
    QString binary;         // PHP_BINARY
    QString iniFile;        // the loaded php.ini, empty when none
    QStringList extensions; // lower case
    bool operator==(const PhpInfo &) const = default;
};

// The PHP code Studio runs with `php -r` to learn about a PHP, and its
// output read back (an empty version when the output is something else).
FLOSTUDIOCORE_EXPORT QString phpProbeCode();
FLOSTUDIOCORE_EXPORT PhpInfo parsePhpProbe(const QString &output);

// FloCMS's minimum (the skeleton's composer.json: "php": "^8.1").
FLOSTUDIOCORE_EXPORT bool phpVersionSupported(const QString &version);

// The extensions `php flo doctor` checks, missing from `extensions`:
// required ones fail Doctor, recommended ones warn. The PDO driver
// follows the database driver (pdo_sqlite for sqlite, else pdo_mysql).
struct MissingExtensions
{
    QStringList required;
    QStringList recommended;
    bool operator==(const MissingExtensions &) const = default;
};
FLOSTUDIOCORE_EXPORT MissingExtensions missingExtensions(const QStringList &extensions,
                                                        const QString &databaseDriver);

// "Composer version 2.8.12 2025-09-19 13:41:59" -> 2.8.12 (other lines,
// such as Composer's warnings when run as root, are skipped).
FLOSTUDIOCORE_EXPORT QString composerVersion(const QString &output);

struct DatabaseProduct
{
    QString name;    // MariaDB or MySQL
    QString version; // 10.11.14
    bool operator==(const DatabaseProduct &) const = default;
};

// The first line of `mariadb --version` / `mysql --version`.
FLOSTUDIOCORE_EXPORT DatabaseProduct databaseVersion(const QString &output);

struct ToolStatus
{
    bool found = false;
    QString path;
    QString version;
    QString error; // php_not_found, composer_not_found, tool_failed (found but did not answer)
    bool operator==(const ToolStatus &) const = default;
};

struct PhpStatus
{
    ToolStatus tool;
    PhpInfo info;
    bool supported = false; // phpVersionSupported
    bool operator==(const PhpStatus &) const = default;
};

struct DatabaseServerStatus
{
    DatabaseProduct product; // from the client program; empty when it is not installed
    QString clientPath;
    bool running = false;    // something answers on 127.0.0.1:3306
    bool operator==(const DatabaseServerStatus &) const = default;
};

FLOSTUDIOCORE_EXPORT PhpStatus detectPhp(const Settings &settings);
FLOSTUDIOCORE_EXPORT ToolStatus detectComposer(const Settings &settings);
FLOSTUDIOCORE_EXPORT DatabaseServerStatus detectDatabaseServer(int port = 3306);

// Whether a TCP server answers on host:port within timeoutMs.
FLOSTUDIOCORE_EXPORT bool portAnswers(const QString &host, int port, int timeoutMs);

// The editor command template (Settings::editorCommand) as a program and
// its arguments for a folder: quotes group words and are removed, {folder}
// becomes the folder, and without {folder} the folder is the last
// argument. Empty when the template has no program.
FLOSTUDIOCORE_EXPORT QStringList editorCommandLine(const QString &editorTemplate, const QString &folder);

// Opens the folder in the editor (not waiting for it). Error codes:
// editor_not_found, unsafe_argument (a .bat/.cmd editor with a folder
// cmd.exe would misread), editor_failed.
FLOSTUDIOCORE_EXPORT Result<Done> openInEditor(const Settings &settings, const QString &folder);

} // namespace FloStudio::Core
