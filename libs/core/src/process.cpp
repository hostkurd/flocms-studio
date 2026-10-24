#include <flostudio/core/process.h>
#include <flostudio/core/tools.h>

#include <QDir>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringDecoder>
#include <QTimer>

#include <algorithm>
#include <memory>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <csignal>
#include <sys/types.h>
#include <unistd.h>
#endif
#ifdef Q_OS_LINUX
#include <sys/prctl.h>
#endif

namespace FloStudio::Core {

namespace {

// Each started program has a Tracker, a child of its QProcess: it knows
// how to reach everything the program started, and ends what is left when
// the QProcess goes (Studio closes a project or quits): ~QProcess only
// ends the program itself, not `php -S` started by `php flo serve`.
//
//   Windows: the program runs in its own job object (KILL_ON_JOB_CLOSE).
//     Its children join the job, so ending the job ends them all, and
//     when Studio ends, even by a crash, Windows closes the handle and
//     ends them too. The program joins the job when QProcess reports it
//     started, which is right after CreateProcess: what it starts before
//     that moment (nothing, for php) would be outside the job.
//   macOS, Linux: the program leads its own process group (setpgid), so a
//     signal to the group reaches its children. On Linux the program also
//     gets SIGTERM if Studio crashes (PR_SET_PDEATHSIG); its children do
//     not, and on macOS there is no such thing.
class Tracker : public QObject
{
public:
    explicit Tracker(QProcess *process)
        : QObject(process)
    {
        setObjectName(QStringLiteral("flostudio-tracker"));
        // When the program ends, what it left running ends too, at once:
        // while one of them lives, its group or job is surely still ours.
        // The id is then forgotten, so a later group with the same number,
        // someone else's, is never signalled when the QProcess goes.
        connect(process, &QProcess::finished, this, [this] {
            endAll();
            m_pid = 0;
        });
    }
    Tracker(const Tracker &) = delete;
    Tracker &operator=(const Tracker &) = delete;
    ~Tracker() override { endAll(); }

    static Tracker *of(const QProcess *process)
    {
        return process ? dynamic_cast<Tracker *>(process->findChild<QObject *>(
                             QStringLiteral("flostudio-tracker"), Qt::FindDirectChildrenOnly))
                       : nullptr;
    }

    // Right after the program started.
    void started(qint64 pid)
    {
        m_pid = pid;
#ifdef Q_OS_WIN
        m_job = CreateJobObjectW(nullptr, nullptr);
        if (!m_job)
            return;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
        if (!SetInformationJobObject(m_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) || !process
            || !AssignProcessToJobObject(m_job, process))
            qWarning("Could not put a started program into its job object.");
        if (process)
            CloseHandle(process);
#endif
    }

    // Asks everything to end (macOS, Linux); ends it at once on Windows.
    void stop()
    {
        m_stopped = true;
#ifdef Q_OS_WIN
        endAll();
#else
        signalGroup(SIGTERM);
#endif
    }

    // Ends whatever is still running, without asking.
    void endAll()
    {
#ifdef Q_OS_WIN
        if (m_job) {
            TerminateJobObject(m_job, 1);
            CloseHandle(m_job);
            m_job = nullptr;
        }
#else
        // The group outlives its leader while a child is still in it.
        if (m_pid > 0 && ::kill(static_cast<pid_t>(-m_pid), 0) == 0)
            signalGroup(SIGKILL);
#endif
    }

    bool stopped() const { return m_stopped; }
    bool stopWhenStarted = false; // stopProcess() came before the program started

private:
#ifndef Q_OS_WIN
    void signalGroup(int signal) const
    {
        if (m_pid > 0)
            ::kill(static_cast<pid_t>(-m_pid), signal); // ESRCH: already gone
    }
#endif

    qint64 m_pid = 0;
    bool m_stopped = false;
#ifdef Q_OS_WIN
    HANDLE m_job = nullptr;
#endif
};

void prepare(QProcess &process, const Command &command)
{
    process.setProgram(command.program);
    process.setArguments(command.arguments);
    if (!command.workingDir.isEmpty())
        process.setWorkingDirectory(command.workingDir);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    for (const QString &name : command.removeEnvironment)
        environment.remove(name);
    environment.insert(QStringLiteral("PATH"), toolSearchPath().join(QDir::listSeparator()));
    for (auto it = command.environment.cbegin(); it != command.environment.cend(); ++it)
        environment.insert(it.key(), it.value());
    process.setProcessEnvironment(environment);

    if (command.input.isEmpty())
        process.setStandardInputFile(QProcess::nullDevice());

    new Tracker(&process); // a child of the process: Qt deletes it with the process

#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
    });
#else
    process.setChildProcessModifier([] {
        // Runs in the child between fork and exec: only async-signal-safe calls.
        ::setpgid(0, 0);
#ifdef Q_OS_LINUX
        ::prctl(PR_SET_PDEATHSIG, SIGTERM); // ends with Studio
#endif
    });
#endif
}

// Right after the program started: its tracker learns the process id,
// and the input (a password) is written and the channel closed.
void afterStart(QProcess &process, const QByteArray &input)
{
    Tracker *tracker = Tracker::of(&process);
    if (tracker)
        tracker->started(process.processId());
    if (!input.isEmpty()) {
        process.write(input);
        process.closeWriteChannel();
    }
    if (tracker && tracker->stopWhenStarted)
        tracker->stop();
}

// Blocking stop for runCaptured(): ask, wait, then end everything.
void stopAndWait(QProcess &process, int graceMs)
{
    if (Tracker *tracker = Tracker::of(&process)) {
        tracker->stop();
        process.waitForFinished(graceMs);
        tracker->endAll();
    } else {
        process.kill();
    }
    process.waitForFinished(5000);
}

} // namespace

Captured runCaptured(const Command &command, int timeoutMs)
{
    Captured result;
    result.shown = command.shown;
    QProcess process;
    prepare(process, command);
    // started() comes from inside start() on Windows, from waitForStarted()
    // elsewhere: either way before the program can do much.
    QObject::connect(&process, &QProcess::started, &process,
                     [&process, &command] { afterStart(process, command.input); });
    process.start();
    if (!process.waitForStarted(30000)) {
        result.err = process.errorString();
        return result;
    }
    result.started = true;
    if (!process.waitForFinished(timeoutMs > 0 ? timeoutMs : -1)) {
        result.timedOut = true;
        stopAndWait(process, 2000);
    }
    result.out = QString::fromUtf8(process.readAllStandardOutput());
    result.err = QString::fromUtf8(process.readAllStandardError());
    result.crashed = !result.timedOut && process.exitStatus() == QProcess::CrashExit;
    result.exitCode = result.timedOut || result.crashed ? -1 : process.exitCode();
    return result;
}

QProcess *startStreaming(const Command &command, QObject *owner, LineHandler onLine, ExitHandler onExit)
{
    struct State
    {
        QStringDecoder out{QStringDecoder::Utf8};
        QStringDecoder err{QStringDecoder::Utf8};
        QString pendingOut;
        QString pendingErr;
        bool ended = false;
        LineHandler onLine;
        ExitHandler onExit;
    };
    auto state = std::make_shared<State>();
    state->onLine = std::move(onLine);
    state->onExit = std::move(onExit);

    auto *process = new QProcess(owner); // owned by owner (Qt parent)
    prepare(*process, command);

    const auto deliver = [state](bool error, const QStringList &lines) {
        if (!state->onLine)
            return;
        for (const QString &line : lines)
            state->onLine(OutputLine{error, line});
    };
    const auto readOut = [process, state, deliver](bool atEnd) {
        deliver(false, takeLines(state->pendingOut, state->out.decode(process->readAllStandardOutput()), atEnd));
    };
    const auto readErr = [process, state, deliver](bool atEnd) {
        deliver(true, takeLines(state->pendingErr, state->err.decode(process->readAllStandardError()), atEnd));
    };
    const auto end = [state](int code, bool crashed) {
        if (state->ended)
            return;
        state->ended = true;
        if (state->onExit)
            state->onExit(code, crashed);
    };

    QObject::connect(process, &QProcess::readyReadStandardOutput, owner, [readOut] { readOut(false); });
    QObject::connect(process, &QProcess::readyReadStandardError, owner, [readErr] { readErr(false); });
    QObject::connect(process, &QProcess::finished, owner,
                     [process, readOut, readErr, end](int code, QProcess::ExitStatus status) {
                         readOut(true);
                         readErr(true);
                         // Stopped by Studio: the same on every system (Windows
                         // reports a stopped program as exit code 1)
                         const Tracker *tracker = Tracker::of(process);
                         const bool crashed = status == QProcess::CrashExit || (tracker && tracker->stopped());
                         end(crashed ? -1 : code, crashed);
                     });
    QObject::connect(process, &QProcess::errorOccurred, owner,
                     [process, deliver, end](QProcess::ProcessError error) {
                         if (error != QProcess::FailedToStart)
                             return; // the others end in finished()
                         deliver(true, {process->errorString()});
                         end(-1, true);
                     });
    QObject::connect(process, &QProcess::started, owner,
                     [process, input = command.input] { afterStart(*process, input); });
    process->start();
    return process;
}

void stopProcess(QProcess *process, int graceMs)
{
    Tracker *tracker = Tracker::of(process);
    if (!process || !tracker || process->state() == QProcess::NotRunning)
        return;
    if (process->state() == QProcess::Starting) {
        tracker->stopWhenStarted = true;
        return;
    }
    tracker->stop();
    // Then end whatever is left: a child can outlive its leader.
    QTimer::singleShot(graceMs, tracker, [tracker] { tracker->endAll(); });
}

QString cleanLine(const QString &line)
{
    QString out;
    out.reserve(line.size());
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        const char16_t code = c.unicode();
        if (code == 0x1B) { // ESC
            const QChar next = i + 1 < line.size() ? line.at(i + 1) : QChar();
            if (next == u'[') { // CSI: parameters, then a final byte @ to ~
                i += 2;
                while (i < line.size() && (line.at(i).unicode() < 0x40 || line.at(i).unicode() > 0x7E))
                    ++i;
            } else if (next == u']') { // OSC: up to BEL or ESC backslash
                i += 2;
                while (i < line.size() && line.at(i).unicode() != 0x07
                       && !(line.at(i).unicode() == 0x1B && i + 1 < line.size() && line.at(i + 1) == u'\\'))
                    ++i;
                if (i < line.size() && line.at(i).unicode() == 0x1B)
                    ++i;
            } else {
                ++i; // ESC and one character
            }
            continue;
        }
        if (code == 0x08) { // backspace: remove the character before
            if (!out.isEmpty()) {
                const bool pair = out.size() >= 2 && out.back().isLowSurrogate() && out.at(out.size() - 2).isHighSurrogate();
                out.chop(pair ? 2 : 1);
            }
            continue;
        }
        if (code == u'\t' || !(code < 0x20 || code == 0x7F || (code >= 0x80 && code <= 0x9F)))
            out.append(c);
    }
    return out;
}

QStringList takeLines(QString &pending, const QString &chunk, bool atEnd)
{
    pending += chunk;
    QStringList lines;
    const auto finish = [&lines](QString raw) {
        if (raw.endsWith(u'\r'))
            raw.chop(1);
        const qsizetype redraw = raw.lastIndexOf(u'\r');
        const QString text = cleanLine(redraw >= 0 ? raw.mid(redraw + 1) : raw);
        if (redraw >= 0 && text.trimmed().isEmpty())
            return;
        lines << text;
    };
    qsizetype start = 0;
    for (qsizetype end = pending.indexOf(u'\n'); end >= 0; end = pending.indexOf(u'\n', start)) {
        finish(pending.mid(start, end - start));
        start = end + 1;
    }
    pending.remove(0, start);
    if (atEnd && !pending.isEmpty()) {
        finish(pending);
        pending.clear();
    }
    return lines;
}

QString shownCommandLine(const QString &head, const QStringList &arguments)
{
    QStringList words{head};
    for (const QString &argument : arguments) {
        const bool plain = !argument.isEmpty()
                           && std::none_of(argument.begin(), argument.end(),
                                           [](QChar c) { return c.isSpace() || c == u'"' || c == u'\''; });
        words << (plain ? argument : QStringLiteral("\"%1\"").arg(QString(argument).replace(u'"', QStringLiteral("\\\""))));
    }
    return words.join(u' ');
}

} // namespace FloStudio::Core
