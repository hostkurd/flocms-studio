#pragma once

#include <flostudio/core/export.h>

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

#include <functional>

class QObject;
class QProcess;

namespace FloStudio::Core {

// Starting programs: short ones are captured and waited for (Doctor,
// migrate:status), long ones stream their lines and can be stopped
// (migrate, composer create-project, php flo serve).
//
// Every program Studio starts:
//   - gets PATH = toolSearchPath(), so `php flo setup` finds Composer;
//   - opens no console window on Windows;
//   - starts its own process group (macOS, Linux) or is tracked with its
//     children (Windows), so stopping it also stops what it started:
//     `php flo serve` runs `php -S` as a child;
//   - on Windows, is ended with Studio, even if Studio crashes.

struct Command
{
    QString program;
    QStringList arguments;
    QString workingDir;
    QMap<QString, QString> environment; // added to (or replacing) Studio's own
    // Written to the program's input, which is then closed. Passwords go
    // here (user:create --password-stdin), never into the arguments.
    QByteArray input;
    // The command as a person would type it, for the screen.
    QString shown;
};

struct Captured
{
    int exitCode = -1;
    bool started = false;  // the program could be started at all
    bool timedOut = false; // stopped after the time limit
    bool crashed = false;
    QString out; // stdout, UTF-8
    QString err; // stderr, UTF-8
    QString shown;

    bool succeeded() const { return started && !timedOut && !crashed && exitCode == 0; }
};

// Runs the command and waits for it, up to timeoutMs (then it is stopped
// with its children). Blocks: use a worker thread.
FLOSTUDIOCORE_EXPORT Captured runCaptured(const Command &command, int timeoutMs = 120000);

struct OutputLine
{
    bool error = false; // stderr
    QString text;       // cleanLine()d
};

using LineHandler = std::function<void(const OutputLine &)>;
// exitCode -1 with crashed (or not started) when the program did not end
// on its own.
using ExitHandler = std::function<void(int exitCode, bool crashed)>;

// Starts the command; lines arrive as they are printed, the exit once at
// the end (also when it could not start: then exitCode -1, after an error
// line saying why). The handlers run on `owner`'s thread. The returned
// QProcess belongs to `owner` (Qt deletes it with the owner); pass it to
// stopProcess() to stop the program early.
FLOSTUDIOCORE_EXPORT QProcess *startStreaming(const Command &command, QObject *owner, LineHandler onLine,
                                              ExitHandler onExit);

// Stops the program and everything it started: asks first (SIGTERM on
// macOS/Linux), ends them after graceMs. Windows has no polite way for
// console programs without a window, so there they are ended at once.
FLOSTUDIOCORE_EXPORT void stopProcess(QProcess *process, int graceMs = 3000);

// A line of output as a person reads it: colour and cursor codes removed
// (ESC [ ... ), backspaces applied, other control characters removed (tabs
// stay).
FLOSTUDIOCORE_EXPORT QString cleanLine(const QString &line);

// Takes the finished lines out of `pending` after `chunk` is added. A line
// ends at \n; a \r inside it is a redraw (Composer's progress bars), so
// only what follows the last \r is kept, as a terminal would show it. With
// atEnd, what is left becomes the last line. Blank redraws are dropped.
FLOSTUDIOCORE_EXPORT QStringList takeLines(QString &pending, const QString &chunk, bool atEnd);

// The command line for the screen: arguments with spaces or nothing in
// them are quoted.
FLOSTUDIOCORE_EXPORT QString shownCommandLine(const QString &head, const QStringList &arguments);

} // namespace FloStudio::Core
