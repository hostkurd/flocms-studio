#include "singleinstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSysInfo>
#include <QThread>
#include <QTimer>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

constexpr QByteArrayView kShow = "show\n";
constexpr QByteArrayView kOpen = "open ";
constexpr QByteArrayView kShown = "ok\n";
// A line longer than this is no message of ours: the connection is closed
constexpr qint64 kMaxLine = 8192;
// A copy that hasn't sent its line by then is closed
constexpr int kReadTimeoutMs = 2000;

QByteArray openMessage(const QString &folder)
{
    return kOpen.toByteArray() + folder.toUtf8() + '\n';
}

// A random secret kept in the settings folder (created the first time)
QByteArray secretIn(const QString &folder)
{
    const QString path = QDir(folder).filePath(QStringLiteral("studio.instance"));
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray secret = file.readAll().trimmed();
        if (secret.size() >= 16)
            return secret;
    }
    QByteArray secret;
    for (int i = 0; i < 4; ++i)
        secret += QByteArray::number(QRandomGenerator::system()->generate64(), 16);
    QSaveFile out(path);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(secret);
        out.commit();
    }
    // Written or not, read back what is there (another copy may have been
    // first); when the folder can't be written, the folder's path
    QFile again(path);
    if (again.open(QIODevice::ReadOnly)) {
        const QByteArray stored = again.readAll().trimmed();
        if (stored.size() >= 16)
            return stored;
    }
    return folder.toUtf8();
}

} // namespace

SingleInstance::SingleInstance(const QString &folder, QObject *parent)
    : QObject(parent)
    , m_folder(folder)
    , m_lock(QDir(folder).filePath(QStringLiteral("studio.lock")))
{
    // Never stale by age (QLockFile would count a Studio running for long
    // as gone); a lock whose process is gone is taken over by QLockFile
    // itself, one from another computer by claim()
    m_lock.setStaleLockTime(0);
    QDir().mkpath(folder);
    QByteArray seed = secretIn(folder);
#ifdef Q_OS_WIN
    DWORD session = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    seed += '/' + QByteArray::number(static_cast<qulonglong>(session));
#else
    seed += '/' + QByteArray::number(static_cast<qulonglong>(getuid()));
#endif
    m_name = QStringLiteral("flocms-studio-")
             + QString::fromLatin1(QCryptographicHash::hash(seed, QCryptographicHash::Sha256).toHex().left(24));
}

SingleInstance::~SingleInstance()
{
    release();
}

void SingleInstance::release()
{
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
    m_lock.unlock();
}

bool SingleInstance::canHandOver(const QString &folder)
{
    return !folder.isEmpty() && !folder.contains(u'\n') && !folder.contains(u'\r') && !folder.contains(QChar(0))
           && QDir::isAbsolutePath(folder) && openMessage(folder).size() <= kMaxLine;
}

bool SingleInstance::claim(const QString &openFolder)
{
    // A folder that can't go in one message: a running Studio is only
    // asked to show itself (main.cpp says the folder wasn't opened), never
    // a second Studio started for it
    const QString handOver = canHandOver(openFolder) ? openFolder : QString();
    // About three seconds: a Studio still starting (not listening yet) or
    // closing (about to let go of the lock)
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (m_lock.tryLock(0)) {
            listen();
            return true;
        }
        // Only "another Studio holds it" means one may be running; a
        // folder that can't be written doesn't stop Studio
        if (m_lock.error() != QLockFile::LockFailedError)
            return true;
        if (askRunningCopy(handOver) == Answer::Shown)
            return false;
        // Left by a Studio on another computer sharing this home folder
        // (QLockFile can't see that one's process): nothing here answers
        qint64 pid = 0;
        QString host;
        QString app;
        if (m_lock.getLockInfo(&pid, &host, &app) && !host.isEmpty() && host != QSysInfo::machineHostName()) {
            m_lock.removeStaleLockFile();
            continue;
        }
        QThread::msleep(150);
    }
    // Held, but nothing answers: run anyway (better two copies than none)
    return true;
}

void SingleInstance::listen()
{
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    // A socket file left on macOS and Linux by a Studio that crashed (this
    // copy holds the lock, so no running Studio owns it)
    QLocalServer::removeServer(m_name);
    if (!m_server->listen(m_name))
        return; // still runs; a second copy just won't be able to call it forward
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            // Nothing (or no whole line) in time: closed, so a copy that
            // stays connected doesn't keep its socket
            QTimer::singleShot(kReadTimeoutMs, socket, [socket] { socket->disconnectFromServer(); });
            const auto read = [this, socket] {
                if (!socket->canReadLine()) {
                    if (socket->bytesAvailable() > kMaxLine)
                        socket->disconnectFromServer();
                    return;
                }
                const QByteArray line = socket->readLine(kMaxLine + 1);
                // open <absolute path>: one line of UTF-8; Studio checks
                // the folder before it opens anything
                QString folder;
                if (line.startsWith(kOpen.toByteArray()) && line.endsWith('\n')) {
                    folder = QString::fromUtf8(line.mid(kOpen.size()).chopped(1));
                    if (folder.isEmpty() || folder.contains(u'\r') || folder.contains(QChar(0)) || !QDir::isAbsolutePath(folder))
                        folder.clear();
                }
                if (line == kShow.toByteArray() || !folder.isEmpty()) {
                    // Answered first: the window may not exist yet (main.cpp
                    // shows it once it does)
                    socket->write(kShown.data(), kShown.size());
                    socket->flush();
                    if (!folder.isEmpty())
                        emit openRequested(folder);
                    emit activateRequested();
                }
                socket->disconnectFromServer();
            };
            connect(socket, &QLocalSocket::readyRead, this, read);
            read(); // it may have arrived already
        }
    });
}

SingleInstance::Answer SingleInstance::askRunningCopy(const QString &openFolder)
{
#ifdef Q_OS_WIN
    // Windows lets a program bring itself to the front only when the one
    // the user just started allows it
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(300))
        return Answer::NoAnswer;
    // claim() checked the folder (canHandOver)
    const QByteArray message = openFolder.isEmpty() ? kShow.toByteArray() : openMessage(openFolder);
    socket.write(message);
    if (!socket.waitForBytesWritten(300))
        return Answer::NoAnswer;
    // Shown only when the running Studio says so (one that is closing
    // reads nothing)
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(1000))
            return Answer::NoAnswer;
    }
    return socket.readLine() == kShown.toByteArray() ? Answer::Shown : Answer::NoAnswer;
}
