#pragma once

#include <flostudio/core/export.h>
#include <flostudio/core/process.h>
#include <flostudio/core/result.h>

#include <QString>
#include <QStringList>

#include <functional>

class QObject;

namespace FloStudio::Core {

// Studio's own MariaDB server (MariaDB design note D1–D12).
//
// Studio starts and stops only its own server: a MariaDB it downloaded (or
// Homebrew's mariadbd on macOS) running on Studio's own data folder, one
// per series because MariaDB can't go back a version on the same data:
//   Windows  %LOCALAPPDATA%\FloCMS Studio\mariadb\<series>
//   macOS    ~/Library/Application Support/FloCMS Studio/mariadb/<series>
//   Linux    ~/.local/share/FloCMS Studio/mariadb/<series>
// The folder holds data/, Studio's config (my.ini on Windows, my.cnf
// elsewhere), the error log, the pid file and (macOS, Linux) the socket.
// XAMPP, Laragon, Herd and system services are never started, stopped or
// changed.
//
// The server listens on 127.0.0.1 only. root logs in with a password
// (never by OS user name); Studio keeps it in the system's secure store
// (FloStudioSecurity, in the app) and hands it to these functions, which
// pass it to MariaDB's programs in their environment (MYSQL_PWD) or on
// their input, never as an argument.

struct DatabaseServerFiles
{
    QString series;     // 12.3
    QString server;     // .../bin/mariadbd(.exe)
    QString installDb;  // mariadb-install-db(.exe): bin/ or scripts/
    QString admin;      // mariadb-admin(.exe)
    QString client;     // mariadb(.exe)
    QString baseDir;    // the install: the folder holding bin/
    QString folder;     // Studio's folder for this series (above)
    QString dataDir;    // folder/data
    QString config;     // folder/my.ini or my.cnf
    QString errorLog;   // folder/mariadbd.err
    QString pidFile;    // folder/mariadbd.pid
    QString socket;     // folder/mysqld.sock (empty on Windows)
    QString platform;   // windows-x86_64, linux-x86_64, macos-arm64, ...
    bool operator==(const DatabaseServerFiles &) const = default;
};

// Studio's folder for its servers' data: <AppLocalDataLocation>/mariadb.
FLOSTUDIOCORE_EXPORT QString databaseServersFolder();

// The files of a server program for a series. server is mariadbd's path;
// its folder's parent is the install. mariadb-install-db is looked for in
// bin/ (Windows, Homebrew) and scripts/ (Linux bintar).
FLOSTUDIOCORE_EXPORT DatabaseServerFiles databaseServerFiles(const QString &server, const QString &series,
                                                             const QString &platform,
                                                             const QString &serversFolder = databaseServersFolder());

// "12.3.2" -> "12.3"; "" when it isn't a version.
FLOSTUDIOCORE_EXPORT QString mariadbSeries(const QString &version);

// Studio's config for the server (D6): datadir, port, bind-address
// 127.0.0.1, pid file, error log, socket, utf8mb4 / utf8mb4_unicode_ci,
// innodb_buffer_pool_size 128M, and a [client] section with the port.
// Without networking (the first start on macOS and Linux, while root has
// no password yet) only the socket in Studio's folder answers.
// Paths are written with forward slashes and quoted (MariaDB reads \ as an
// escape in option files).
FLOSTUDIOCORE_EXPORT QString databaseServerConfig(const DatabaseServerFiles &files, int port, bool networking = true);

// The data folder has been set up (its mysql/ system database exists).
FLOSTUDIOCORE_EXPORT bool databaseServerInitialized(const DatabaseServerFiles &files);

// root's password has been set on this data folder (Studio's marker file
// beside the config).
FLOSTUDIOCORE_EXPORT bool databaseRootPasswordSet(const DatabaseServerFiles &files);

// The commands, for runCaptured / startStreaming and for the screen.
// Setting up the data folder, once: no test database (anonymous users are
// dropped when root's password is set);
// on macOS and Linux root logs in with a password, not by OS user name. On
// Windows the tool sets root's password itself (an argument: the only way
// it takes one; `shown` hides it).
FLOSTUDIOCORE_EXPORT Command databaseInstallCommand(const DatabaseServerFiles &files, int port,
                                                    const QString &rootPassword = {});
// The server itself, with Studio's config only (--defaults-file).
FLOSTUDIOCORE_EXPORT Command databaseServerCommand(const DatabaseServerFiles &files);
// A clean shutdown through mariadb-admin over TCP.
FLOSTUDIOCORE_EXPORT Command databaseShutdownCommand(const DatabaseServerFiles &files, int port,
                                                     const QString &rootPassword);
// SQL on the input of the mariadb client, as user (root for Studio's own
// work), over TCP to 127.0.0.1:port. --batch, stops at the first error.
FLOSTUDIOCORE_EXPORT Command databaseSqlCommand(const DatabaseServerFiles &files, int port, const QString &user,
                                                const QString &password, const QString &sql);
// `mariadbd --version`: shows whether the program runs at all here.
FLOSTUDIOCORE_EXPORT Command databaseVersionCommand(const DatabaseServerFiles &files);

// A password of letters and digits only (no quoting needed anywhere),
// from the system's secure random source.
FLOSTUDIOCORE_EXPORT QString generateDatabasePassword(int length = 24);

// SQL text: an identifier in backticks, a string in single quotes.
FLOSTUDIOCORE_EXPORT QString sqlIdentifier(const QString &name);
FLOSTUDIOCORE_EXPORT QString sqlString(const QString &text);

// "Delete project" on Studio's own server: DROP the project's database,
// and its user (the two accounts the wizard made, @localhost and
// @127.0.0.1) when dropUser. Empty when the name is empty or one of
// MariaDB's own databases; the user is never root or a MariaDB account.
FLOSTUDIOCORE_EXPORT QString dropProjectDatabaseSql(const QString &database, const QString &user, bool dropUser);

// The first free port on 127.0.0.1 from `from`, trying `tries` ports;
// 0 when none is free. "Free" = nothing answers and Studio can listen.
FLOSTUDIOCORE_EXPORT int freeDatabasePort(int from = 3306, int tries = 20);

// The shared library a program could not load, from its error output
// ("error while loading shared libraries: libaio.so.1: cannot open ...");
// empty when the output says nothing of the kind.
FLOSTUDIOCORE_EXPORT QString missingSharedLibrary(const QString &output);

// Linux (MariaDB design note D4, Q5): when the bintar wants libaio.so.1
// and the system only has Ubuntu 24.04's libaio.so.1t64, a link to it is
// made inside the install's own lib/ folder (never in a system folder:
// not for an install in /usr and the like, nor one this user can't write).
// Returns the link, or an empty string when the system file isn't there.
// libraryDirs: where to look (the default is the usual system folders).
FLOSTUDIOCORE_EXPORT QString linkLibaioT64(const DatabaseServerFiles &files,
                                           const QStringList &libraryDirs = {});

// The last lines of the error log, for the screen when a start fails;
// from `from` (the log's size when that start began) on.
FLOSTUDIOCORE_EXPORT QStringList databaseErrorLogTail(const DatabaseServerFiles &files, int lines = 12, qint64 from = 0);

// Starting and stopping (D6–D8). startDatabaseServer:
//  1. refuses when something already answers on the port: db_port_busy
//     (detail: the port);
//  2. checks the program runs (`--version`), making the libaio link when
//     that is all that's missing; otherwise db_library (detail: the
//     library) or db_program;
//  3. the first time, sets up the data folder (db_init): a folder left
//     half-way is moved aside (data.failed-<time>), never deleted; on
//     Windows the tool sets root's password;
//  4. writes the config and starts mariadbd, checking the port is still
//     free, and waits until it is ready and proven ours (our process wrote
//     the pid file): db_start (it ended; detail: this start's log lines),
//     db_timeout after timeoutMs;
//  5. macOS and Linux, the first time: the server runs without networking,
//     root's password is set through its socket, and it starts again with
//     networking; then (every time) root's password is checked over TCP:
//     db_root.
// rootPassword comes from the secure store (the app saves a new one there
// before the first start). Lines (commands run and their output) arrive
// in onLine; onStarted once, with the result (db_cancelled when it was
// stopped while starting); onExit when the server ends later (stopped,
// crashed). All on owner's thread from its event loop. The returned object
// belongs to owner; deleting it ends the server at once (its job object or
// process group), so stop it with stopDatabaseServer() first.
struct DatabaseServerStart
{
    DatabaseServerFiles files;
    int port = 3306;
    QString rootPassword;
    int timeoutMs = 60000;
};
using DatabaseStartedHandler = std::function<void(const Result<Done> &started)>;
FLOSTUDIOCORE_EXPORT QObject *startDatabaseServer(const DatabaseServerStart &start, QObject *owner,
                                                  LineHandler onLine, DatabaseStartedHandler onStarted,
                                                  ExitHandler onExit);

// A clean shutdown: mariadb-admin shutdown, then waits up to graceMs for
// the server to end, then ends it. While it is still starting, the start
// stops instead (set-up ended, a server that runs ended politely). done
// runs once the server has ended (also when it was not running). server
// is startDatabaseServer's object.
FLOSTUDIOCORE_EXPORT void stopDatabaseServer(QObject *server, const QString &rootPassword,
                                             std::function<void()> done, int graceMs = 30000);

// The same, blocking, for when Studio closes: mariadb-admin shutdown, then
// waits up to graceMs for the server to end, then ends it. onExit is not
// called.
FLOSTUDIOCORE_EXPORT void stopDatabaseServerNow(QObject *server, const QString &rootPassword, int graceMs = 20000);

// The server started by startDatabaseServer is running (started and not
// ended).
FLOSTUDIOCORE_EXPORT bool databaseServerRunning(const QObject *server);

} // namespace FloStudio::Core
