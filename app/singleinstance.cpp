#include "singleinstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QThread>

#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr QByteArrayView kShow = "show\n";

} // namespace

SingleInstance::SingleInstance(const QString &lockPath, QString name, QObject *parent)
    : QObject(parent)
    , m_name(std::move(name))
    , m_lock(lockPath)
{
    // A Studio that is busy stays the owner; only a lock whose process is
    // gone is taken over
    m_lock.setStaleLockTime(0);
}

SingleInstance::~SingleInstance()
{
    if (m_server)
        m_server->close();
}

QString SingleInstance::nameForUser()
{
    // The user's home folder tells users apart on every system; hashed so
    // the name is short and has no path characters
    const QByteArray home = QDir::homePath().toUtf8();
    const QByteArray hash = QCryptographicHash::hash(home, QCryptographicHash::Sha256).toHex().left(16);
    return QStringLiteral("flocms-studio-") + QString::fromLatin1(hash);
}

bool SingleInstance::claim()
{
    QDir().mkpath(QFileInfo(m_lock.fileName()).path());
    if (!m_lock.tryLock(0)) {
        // Taken by a copy that is starting or running: ask it to show
        // itself. One that is still starting may not listen yet.
        for (int attempt = 0; attempt < 20; ++attempt) {
            if (askRunningCopy())
                return false;
            QThread::msleep(150);
        }
        // It runs but doesn't answer (stuck): still only one copy
        return false;
    }
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    // A socket file left on macOS and Linux by a Studio that crashed
    QLocalServer::removeServer(m_name);
    if (!m_server->listen(m_name))
        return true; // still the only copy; a second one just won't be able to call it forward
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            const auto read = [this, socket] {
                if (!socket->canReadLine())
                    return;
                if (socket->readLine() == kShow.toByteArray())
                    emit activateRequested();
                socket->disconnectFromServer();
            };
            connect(socket, &QLocalSocket::readyRead, this, read);
            read(); // it may have arrived already
        }
    });
    return true;
}

bool SingleInstance::askRunningCopy()
{
#ifdef Q_OS_WIN
    // Windows lets a program bring itself to the front only when the one
    // the user just started allows it
    AllowSetForegroundWindow(ASFW_ANY);
#endif
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(500))
        return false;
    socket.write(kShow.data(), kShow.size());
    const bool sent = socket.waitForBytesWritten(500);
    socket.waitForDisconnected(500);
    return sent;
}
