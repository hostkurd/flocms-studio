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
//   - is tracked with what it starts (its own process group on macOS and
//     Linux, its own job object on Windows), so stopping it also stops its
//     children: `php flo serve` runs `php -S` as a child;
//   - ends with its children when its QProcess is deleted (the owner goes,
//     Studio quits), and on Windows also when Studio crashes;
//   - takes what it started with it when it ends: children still running
//     then are ended at once. On Linux a
//     crash ends the program itself only; on macOS neither.

struct Command
{
    QString program;
    QStringList arguments;
    QString workingDir;
    QMap<QString, QString> environment; // added to (or replacing) Studio's own
    QStringList removeEnvironment;      // names taken out of Studio's own first
    // Written to the program's input, which is then closed. Passwords go
    // here (user:create --password-stdin), never into the arguments or
    // `shown`. Copies of it live as long as the Command and the process.
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
// exitCode -1 and crashed = true when the program did not end on its own:
// it crashed, could not start, or was stopped with stopProcess().
using ExitHandler = std::function<void(int exitCode, bool crashed)>;

// Starts the command; lines arrive as they are printed, the exit once at
// the end (also when it could not start: then exitCode -1, after an error
// line saying why). The handlers run on `owner`'s thread. The returned
// QProcess belongs to `owner` (Qt deletes it with the owner); pass it to
// stopProcess() to stop the program early.
FLOSTUDIOCORE_EXPORT QProcess *startStreaming(const Command &command, QObject *owner, LineHandler onLine,
                                              ExitHandler onExit);

// Stops the program and everything it started: asks first (SIGTERM to the
// group on macOS/Linux) and ends whatever is left after graceMs. Windows
// has no polite way for console programs without a window, so there they
// are ended at once. Called while the program is still starting, it stops
// it as soon as it has started.
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
