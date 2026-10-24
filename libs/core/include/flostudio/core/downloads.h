#pragma once

#include <flostudio/core/export.h>
#include <flostudio/core/result.h>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>

class QObject;

namespace FloStudio::Core {

// Downloading PHP, Composer and MariaDB when they are missing (D15).
//
// Studio installs only the files listed in its tool list (tools.json,
// built into Core), each with its exact SHA-256 and size: no address is
// ever taken from a page or a file. tools/tool_manifest.py makes the list
// in CI, checking each hash against the one the source publishes where it
// publishes one (windows.php.net, getcomposer.org, MariaDB); static-php-cli
// publishes none, so its hashes are the ones CI recorded. Everything goes
// into Studio's own folder for the user (no administrator rights):
//   Windows  %LOCALAPPDATA%\FloCMS Studio\tools\<id>
//   macOS    ~/Library/Application Support/FloCMS Studio/tools/<id>
//   Linux    ~/.local/share/FloCMS Studio/tools/<id>

struct ToolPackage
{
    QString id;        // php-8.5, composer, mariadb-11.8
    QString tool;      // php, composer, mariadb
    QString version;   // 8.5.1
    QString platform;  // windows-x86_64, macos-arm64, macos-x86_64, linux-x86_64, linux-arm64, any
    QStringList urls;  // HTTPS, tried in order (a newer release moves a file to an archive folder)
    QString sha256;    // lower-case hex
    qint64 size = 0;   // bytes
    QString archive;   // zip, tar.gz, phar
    QString source;    // shown to the user: windows.php.net, static-php.dev, ...
    bool operator==(const ToolPackage &) const = default;
};

// This computer, in the tool list's terms.
FLOSTUDIOCORE_EXPORT QString currentPlatform();

// The packages of a tool list for a platform ("any" packages always),
// newest PHP first. Entries that aren't HTTPS, or have no valid SHA-256,
// are left out. Error codes: tools_list (not a tool list).
FLOSTUDIOCORE_EXPORT Result<QList<ToolPackage>> toolPackages(const QByteArray &json, const QString &platform);

// The list built into Studio, for this computer.
FLOSTUDIOCORE_EXPORT QList<ToolPackage> bundledToolPackages();

// Studio's tools folder (see above), and where a package goes in it.
FLOSTUDIOCORE_EXPORT QString toolsFolder();
FLOSTUDIOCORE_EXPORT QString installFolder(const ToolPackage &package, const QString &toolsDir = toolsFolder());

// The program a package gives once installed: php(.exe), composer.phar,
// or MariaDB's bin/mariadbd(.exe). Empty when it isn't installed (the
// folder's marker file holds the package's SHA-256).
FLOSTUDIOCORE_EXPORT QString installedProgram(const ToolPackage &package, const QString &toolsDir = toolsFolder());

// The installed programs in Studio's tools folder, for toolSearchPath()
// and Settings ("downloaded by Studio").
FLOSTUDIOCORE_EXPORT QStringList installedToolFolders(const QString &toolsDir = toolsFolder());

// php.ini for the Windows PHP zips, which come without one: the extensions
// FloCMS needs (doctor's list) and Composer's (zip, openssl), from
// extensionDir (absolute: PHP reads a relative one from the current
// folder, not from php.exe's), and sensible limits for development.
FLOSTUDIOCORE_EXPORT QString windowsPhpIni(const QString &extensionDir);

// The SHA-256 of a file as lower-case hex; empty when it can't be read.
FLOSTUDIOCORE_EXPORT QString sha256OfFile(const QString &path);

// Unpacks a zip or tar.gz into a new folder with the system's tar (bsdtar
// on Windows 10+ and macOS reads zip too). When everything is inside one
// top folder (MariaDB's archives), its contents are moved up. Blocks.
// Error codes: tool_extract.
FLOSTUDIOCORE_EXPORT Result<Done> extractArchive(const QString &archive, const QString &type, const QString &folder);

// Installs an already downloaded and checked file into a staging folder
// (<id>.new): unpack (or copy the phar), refuse links that point outside
// it, write php.ini on Windows, make the programs executable, write the
// marker; then swap it in, moving the old install aside (<id>.old) and
// back if the swap fails. A failure leaves the old install as it was.
// cancelled (may be null) is checked before the swap. Blocks. Returns the
// program. Error codes: tool_extract, tool_install, tool_cancelled.
FLOSTUDIOCORE_EXPORT Result<QString> installDownloaded(const ToolPackage &package, const QString &file,
                                                       const QString &toolsDir = toolsFolder(),
                                                       const std::atomic<bool> *cancelled = nullptr);

// Downloads, checks and installs a package. Progress arrives while it
// downloads; done once at the end with the installed program, or an
// error: tool_download (every address failed; the detail says why),
// tool_hash (the file is not the one in the tool list: it is deleted),
// tool_cancelled, tool_extract, tool_install. Handlers run on owner's
// thread, always from its event loop (never inside cancelToolInstall).
// The returned object belongs to owner; cancelToolInstall() stops it.
using DownloadProgressHandler = std::function<void(qint64 received, qint64 total)>;
using InstallDoneHandler = std::function<void(const Result<QString> &program)>;
FLOSTUDIOCORE_EXPORT QObject *startToolInstall(const ToolPackage &package, QObject *owner,
                                               DownloadProgressHandler onProgress, InstallDoneHandler onDone,
                                               const QString &toolsDir = toolsFolder());
FLOSTUDIOCORE_EXPORT void cancelToolInstall(QObject *install);

} // namespace FloStudio::Core
