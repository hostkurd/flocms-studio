#pragma once

#include <flostudio/core/export.h>

#include <QString>
#include <QStringList>

namespace FloStudio::Core {

// Running a project's site with `php flo serve` (stage 4, D10) and
// following its log, storage/logs/app.log.
//
// `php flo serve` picks the next free port itself when the one asked for
// is taken (up to --tries further ports) and prints "[OK] Serving <folder>
// at http://<host>:<port>" before the server listens (serveUrl() reads it;
// wait for portAnswers()). It then runs `php -S` as a child, which
// startStreaming()'s tracking stops with it.

// The arguments for prepareCommand(): serve, --port, and --host: 127.0.0.1,
// or 0.0.0.0 when the site should be reachable from the network.
FLOSTUDIOCORE_EXPORT QStringList serveArguments(int port, bool network);

// The address to open on this computer: 0.0.0.0 and :: become 127.0.0.1.
FLOSTUDIOCORE_EXPORT QString localUrl(const QString &serveUrl);

// The same site from another device on the network: one address per IPv4
// address of this computer that is up and not a loopback (192.168.1.20).
FLOSTUDIOCORE_EXPORT QStringList networkUrls(const QString &serveUrl);

// The port in a serve URL; 0 when there is none.
FLOSTUDIOCORE_EXPORT int urlPort(const QString &url);

// --- storage/logs/app.log ------------------------------------------------

FLOSTUDIOCORE_EXPORT QString appLogPath(const QString &projectDir);

struct LogRead
{
    QStringList lines; // complete lines, without their line ends
    qint64 offset = 0; // where the next read starts
    bool restarted = false; // the file got shorter or was replaced: read from the start
    bool missing = false;   // no such file (yet)
    QByteArray head;        // its first bytes, to tell the next read whether it is the same file
};

// The last `count` lines of a log, read from the end (a large log is fine),
// and the offset after them.
FLOSTUDIOCORE_EXPORT LogRead readLogTail(const QString &path, int count);

// The lines added since `offset`. A line still being written (no line end
// yet) is left for the next read. At most maxBytes are read at once; the
// rest comes with the next read. With the `head` of the read before, a
// file that was replaced (rotated, cleared and written again) is read from
// its start even when it has grown past the old offset.
FLOSTUDIOCORE_EXPORT LogRead readLogFrom(const QString &path, qint64 offset, qint64 maxBytes = qint64{4} * 1024 * 1024,
                                         const QByteArray &head = {});

// --- Servers left from a Studio that crashed -----------------------------
//
// On Windows a server ends with Studio even when Studio crashes (its job
// object). On macOS and Linux it can outlive a crash, so Studio notes each
// server it starts (its process group, and which Studio started it) in a
// file and, at the next start, stops the groups still running that
// project's PHP built-in server. A Studio that still runs keeps its
// servers: a second Studio window never stops the first one's.

// servers.json in the settings folder.
FLOSTUDIOCORE_EXPORT QString serverRecordFile(const QString &settingsDir);

// Notes (or, with group 0, forgets) the server of a project.
FLOSTUDIOCORE_EXPORT void recordServer(const QString &settingsDir, const QString &projectDir, qint64 group);

struct LeftoverServer
{
    QString projectDir;
    qint64 group = 0;
};

// The noted servers whose Studio is gone and whose process group still
// has the project's `php -S` in it, and stops them (SIGTERM to the group).
// Only the notes of Studios still running stay in the file. Nothing on
// Windows. Blocks briefly (asks `ps`): use a worker thread.
FLOSTUDIOCORE_EXPORT QList<LeftoverServer> stopLeftoverServers(const QString &settingsDir);

// One line of `ps -A -o pgid=,command=` as (group, command); the group is 0
// when the line isn't one.
struct PsLine
{
    qint64 group = 0;
    QString command;
    bool operator==(const PsLine &) const = default;
};
FLOSTUDIOCORE_EXPORT PsLine readPsLine(const QString &line);

// A PHP built-in server's command line: php ... -S host:port ...
FLOSTUDIOCORE_EXPORT bool isBuiltInServer(const QString &command);
// ... and the one `php flo serve` starts for this project (-t <project>/public).
FLOSTUDIOCORE_EXPORT bool isBuiltInServerOf(const QString &command, const QString &projectDir);

} // namespace FloStudio::Core
