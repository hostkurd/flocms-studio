#pragma once

#include <QLockFile>
#include <QObject>
#include <QString>

#include <memory>

class QLocalServer;

// One Studio per user: the first copy holds a lock file and listens on a
// local socket only this user can open; a second copy finds the lock
// taken, tells the first to show its window, and quits. A lock left by
// a Studio that crashed is taken over (QLockFile checks its process).
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    // `lockPath`: the lock file (in Studio's settings folder); `name`: the
    // local socket's name (one per user)
    SingleInstance(const QString &lockPath, QString name, QObject *parent = nullptr);
    ~SingleInstance() override;
    SingleInstance(const SingleInstance &) = delete;
    SingleInstance &operator=(const SingleInstance &) = delete;

    // True: this is the only copy (it now listens). False: another copy
    // runs and was asked to show itself; this one should quit.
    bool claim();

    // The name for this user: the same for every copy this user starts,
    // another for another user on the same computer
    static QString nameForUser();

signals:
    // A second copy was started: show the window
    void activateRequested();

private:
    bool askRunningCopy();

    QString m_name;
    QLockFile m_lock;
    QLocalServer *m_server = nullptr; // a child of this
};
