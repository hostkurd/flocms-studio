#pragma once

#include <flostudio/core/settings.h>
#include <flostudio/core/wizard.h>

#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include <functional>

class MariadbServer;
class QProcess;

// Setting up a database for a project that has none (design note of 30
// Oct, part C): the Database tab's set-up card. With Studio's MariaDB it
// starts the server when it is stopped, makes the database and a user just
// for it, writes the DB_ lines into .env and keeps the password in the
// system's secure store (C-D3). With another server it writes the lines and
// runs db:check; a missing database can then be created (C-D4, A-D2).
//
// state: editing, running, missing (the server has no such database: offer
// to create it), failed, done.
class DatabaseSetup : public QObject
{
    Q_OBJECT
    // server (studio, other), name, user, host, port, hasOwn (Studio has
    // its MariaDB); the password is never handed out
    Q_PROPERTY(QVariantMap form READ form NOTIFY changed)
    Q_PROPERTY(QVariantMap problems READ problems NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString projectDir READ projectDir NOTIFY changed)
    // What runs now ("starting", "creating", "checking"), and why it failed:
    // a code Texts.problem knows, or the server's own words
    Q_PROPERTY(QString step READ step NOTIFY changed)
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    // Set up was clicked for projectDir and it isn't done: the card stays
    // on its tab even once .env has the DB_ lines
    Q_PROPERTY(bool active READ active NOTIFY changed)

public:
    DatabaseSetup(const FloStudio::Core::Settings *settings, MariadbServer *mariadb, QObject *parent = nullptr);
    ~DatabaseSetup() override;
    DatabaseSetup(const DatabaseSetup &) = delete;
    DatabaseSetup &operator=(const DatabaseSetup &) = delete;

    QVariantMap form() const;
    QVariantMap problems() const;
    QString state() const { return m_state; }
    QString projectDir() const { return m_projectDir; }
    QString step() const { return m_step; }
    QString code() const { return m_code; }
    QString message() const { return m_message; }
    bool active() const { return m_started && m_state != u"done"; }

    // A fresh card for the project: names from its folder, the server from
    // Settings' default (D-D1). Does nothing while one runs.
    Q_INVOKABLE void reset(const QString &projectDir);
    Q_INVOKABLE void set(const QString &field, const QVariant &value);
    Q_INVOKABLE void start();
    // After `missing`: creates it with the login, then checks again
    Q_INVOKABLE void createDatabase();

    void showDemo(const QVariantMap &demo);

signals:
    void changed();
    // The DB_ lines are in .env: Studio reloads the project and its tab
    void envWritten(const QString &projectDir);

private:
    bool useStudio() const;
    QList<FloStudio::Core::FieldProblem> currentProblems() const;
    void startStudio();
    void runSql();
    void runCheck();
    bool writeEnv();
    void fail(const QString &code, const QString &message = {});
    void finish();
    void run(const FloStudio::Core::Command &command, const std::function<void(bool ok)> &done);

    const FloStudio::Core::Settings *m_settings; // Studio's; null for a demo
    MariadbServer *m_mariadb;                    // Studio's
    QMetaObject::Connection m_waitForServer;
    QString m_projectDir;
    QString m_server = QStringLiteral("studio");
    FloStudio::Core::DatabaseLogin m_login;
    bool m_touched = false;
    QString m_state = QStringLiteral("editing");
    QString m_step;
    QString m_code;
    QString m_message;
    QProcess *m_running = nullptr;
    QStringList m_output;
    bool m_demo = false;
    bool m_started = false; // start() since the last reset()
};
