#pragma once

// Shared by the Core tests: the captured fixtures and the helper program.

#include <QDir>
#include <QFile>
#include <QString>

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
#endif

namespace TestUtil {

// tests/fixtures/<relative>
inline QString fixturePath(const QString &relative)
{
    return QDir(QStringLiteral(FLOSTUDIO_FIXTURES)).filePath(relative);
}

inline QString fixture(const QString &relative)
{
    QFile file(fixturePath(relative));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("missing fixture %s", qUtf8Printable(relative));
    return QString::fromUtf8(file.readAll());
}

// The exit code a captured command had (NAME.code).
inline int fixtureCode(const QString &name)
{
    return fixture(QStringLiteral("flo/%1.code").arg(name)).trimmed().toInt();
}

inline QString helperProgram()
{
    return QStringLiteral(FLOSTUDIO_TEST_HELPER);
}

inline bool processAlive(qint64 pid)
{
    if (pid <= 0)
        return false;
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!process)
        return false;
    const bool alive = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    CloseHandle(process);
    return alive;
#else
    if (::kill(static_cast<pid_t>(pid), 0) != 0)
        return false;
#ifdef Q_OS_LINUX
    // A process that ended but wasn't collected yet (a zombie) counts as
    // gone: in a container, process 1 may never collect orphans.
    QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
    if (stat.open(QIODevice::ReadOnly)) {
        const QByteArray text = stat.readAll();
        const qsizetype close = text.lastIndexOf(')');
        if (close > 0 && text.size() > close + 2 && text.at(close + 2) == 'Z')
            return false;
    }
#endif
    return true;
#endif
}

} // namespace TestUtil
