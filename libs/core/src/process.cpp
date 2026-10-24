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
#include <tlhelp32.h>

#include <vector>
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

#ifdef Q_OS_WIN
// A job object every started program joins: when Studio ends (also by a
// crash) Windows closes the handle and ends the programs and their
// children. Created once, never closed by Studio.
HANDLE studioJob()
{
    static const HANDLE job = [] {
        HANDLE handle = CreateJobObjectW(nullptr, nullptr);
        if (handle) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (!SetInformationJobObject(handle, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
                qWarning("Could not set up the job object for started programs.");
        }
        return handle;
    }();
    return job;
}

void joinStudioJob(qint64 pid)
{
    const HANDLE job = studioJob();
    if (!job || pid <= 0)
        return;
    HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (!process)
        return;
    if (!AssignProcessToJobObject(job, process))
        qWarning("Could not add a started program to Studio's job object.");
    CloseHandle(process);
}

// Ends the process and every process it started, children first.
void endTree(qint64 root)
{
    if (root <= 0)
        return;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return;
    std::vector<std::pair<DWORD, DWORD>> parents; // (process, parent)
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL more = Process32FirstW(snapshot, &entry); more; more = Process32NextW(snapshot, &entry))
        parents.emplace_back(entry.th32ProcessID, entry.th32ParentProcessID);
    CloseHandle(snapshot);

    std::vector<DWORD> tree{static_cast<DWORD>(root)};
    for (std::size_t i = 0; i < tree.size(); ++i) {
        for (const auto &[pid, parent] : parents) {
            if (parent == tree[i] && pid != tree[i] && std::find(tree.begin(), tree.end(), pid) == tree.end())
                tree.push_back(pid);
        }
    }
    for (auto it = tree.rbegin(); it != tree.rend(); ++it) {
        HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, *it);
        if (process) {
            TerminateProcess(process, 1);
            CloseHandle(process);
        }
    }
}
#else
void signalGroup(qint64 pid, int signal)
{
    // The process leads its own group (see prepare()): -pid is the group.
    if (pid > 0)
        ::kill(static_cast<pid_t>(-pid), signal);
}
#endif

void prepare(QProcess &process, const Command &command)
{
    process.setProgram(command.program);
    process.setArguments(command.arguments);
    if (!command.workingDir.isEmpty())
        process.setWorkingDirectory(command.workingDir);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"), toolSearchPath().join(QDir::listSeparator()));
    for (auto it = command.environment.cbegin(); it != command.environment.cend(); ++it)
        environment.insert(it.key(), it.value());
    process.setProcessEnvironment(environment);

    if (command.input.isEmpty())
        process.setStandardInputFile(QProcess::nullDevice());

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

// Joins the job (Windows) and writes the input, which is then cleared so a
// password is not kept around.
void afterStart(QProcess &process, QByteArray &input)
{
#ifdef Q_OS_WIN
    joinStudioJob(process.processId());
#endif
    if (!input.isEmpty()) {
        process.write(input);
        process.closeWriteChannel();
        input.fill('\0');
        input.clear();
    }
}

// Blocking stop for runCaptured().
void stopAndWait(QProcess &process, int graceMs)
{
#ifdef Q_OS_WIN
    Q_UNUSED(graceMs)
    endTree(process.processId());
#else
    const qint64 pid = process.processId();
    signalGroup(pid, SIGTERM);
    if (!process.waitForFinished(graceMs))
        signalGroup(pid, SIGKILL);
#endif
    process.waitForFinished(5000);
}

} // namespace

Captured runCaptured(const Command &command, int timeoutMs)
{
    Captured result;
    result.shown = command.shown;
    QProcess process;
    prepare(process, command);
    process.start();
    if (!process.waitForStarted(30000)) {
        result.err = process.errorString();
        return result;
    }
    result.started = true;
    QByteArray input = command.input;
    afterStart(process, input);
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
                     [readOut, readErr, end](int code, QProcess::ExitStatus status) {
                         readOut(true);
                         readErr(true);
                         const bool crashed = status == QProcess::CrashExit;
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
                     [process, input = command.input]() mutable { afterStart(*process, input); });
    process->start();
    return process;
}

void stopProcess(QProcess *process, int graceMs)
{
    if (!process || process->state() == QProcess::NotRunning)
        return;
#ifdef Q_OS_WIN
    Q_UNUSED(graceMs)
    endTree(process->processId());
#else
    const qint64 pid = process->processId();
    signalGroup(pid, SIGTERM);
    QTimer::singleShot(graceMs, process, [process = QPointer<QProcess>(process), pid] {
        if (process && process->state() != QProcess::NotRunning && process->processId() == pid)
            signalGroup(pid, SIGKILL);
    });
#endif
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
