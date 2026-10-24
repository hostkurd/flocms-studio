#pragma once

#include <flostudio/core/settings.h>

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

class LogModel;
class QProcess;
class QTcpSocket;

// One project's site, run with `php flo serve` (stage 4, D10), and its
// log: what the server prints and storage/logs/app.log, followed every
// half second. Belongs to Studio; deleting it stops the server and its
// `php -S` child.
//
// States: stopped, starting (until the port answers), running, stopping,
// failed (it never answered, or could not start: error, detail).
class ProjectServer : public QObject
{
    Q_OBJECT

public:
    ProjectServer(QString projectDir, QString settingsDir, QObject *parent = nullptr);
    ~ProjectServer() override;
    ProjectServer(const ProjectServer &) = delete;
    ProjectServer &operator=(const ProjectServer &) = delete;

    void start(const FloStudio::Core::Settings &settings);
    // How long the port may take to answer after "[OK] Serving" (tests)
    static void setStartTimeout(int seconds);
    void stop();

    QString state() const { return m_state; }
    bool active() const { return m_state == u"starting" || m_state == u"running" || m_state == u"stopping"; }
    // {state, url, networkUrls, startedAt (ms since 1970), php, error, detail}
    QVariantMap info() const;
    QString url() const { return m_url; }
    LogModel *logs() const { return m_logs; }
    QString projectDir() const { return m_projectDir; }

    // A demo: the state and the log lines, nothing started
    void showDemo(const QVariantMap &server, const QVariantList &lines);
    // Studio stopped a server of this project left from a crash
    void noteLeftover(qint64 group);
    // Asks PHP (on a worker thread) which time zone app.log is written in
    void learnTimeZone(const FloStudio::Core::Settings &settings);
    // "+03:00", "-05:30", "Z" → seconds east of UTC; false for anything else
    static bool readUtcOffset(const QString &text, int *seconds);

signals:
    void changed();

private:
    void setState(const QString &state, const QString &error = {}, const QString &detail = {});
    void line(const QString &text, bool error);
    void probe();
    void ended(int exitCode, bool crashed);
    void followLog();

    QString m_projectDir;
    QString m_settingsDir;
    LogModel *m_logs; // a child of this
    QProcess *m_process = nullptr; // a child of this while it runs
    int m_run = 0;                 // which start the handlers belong to
    QTcpSocket *m_probe = nullptr; // a child of this while starting
    QTimer m_probeTimer;
    QDateTime m_probeDeadline;
    QString m_state = QStringLiteral("stopped");
    QString m_url;
    QString m_php;
    QString m_error;
    QString m_detail;
    QDateTime m_startedAt;
    bool m_stopping = false;
    bool m_networkHost = false; // --host=0.0.0.0
    qint64 m_group = 0;
    QStringList m_output; // what it printed before it answered, for the error
    QTimer m_follow;
    qint64 m_logOffset = -1; // -1: the log's end isn't known yet
    QByteArray m_logHead;    // its first bytes, to see it replaced
};
