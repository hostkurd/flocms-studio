#pragma once

#include <flostudio/core/process.h>
#include <flostudio/core/settings.h>

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class QProcess;

// One project's Doctor and Database tabs (stage 5): what `php flo doctor`,
// `admin:doctor`, `db:show` and `migrate:status` say, read on worker
// threads, and the commands that change the database or the caches, run
// one at a time with their output shown live (board 1's Quick commands
// panel). Belongs to Studio, which outlives it, so the settings pointer
// stays valid.
//
// Each report is a map with `state`: idle (not asked yet), running, done,
// failed (the command could not run, or its output was not what Studio
// reads: `error`, `detail`, and the raw `output`).
class ProjectChecks : public QObject
{
    Q_OBJECT
    // {state, checkedAt, failed, warnings, ok, checks: [{status, check, detail}], output, error, detail}
    Q_PROPERTY(QVariantMap doctor READ doctor NOTIFY doctorChanged)
    // {state, checkedAt, heading, problems, warnings, summary,
    //  sections: [{title, lines: [{status, text}]}], output, error, detail}
    Q_PROPERTY(QVariantMap adminDoctor READ adminDoctor NOTIFY doctorChanged)
    // {state, driver, server, host, port, database, username,
    //  tables: [{name, rows, adminUrl}], resources: [Post, ...], error, detail}
    Q_PROPERTY(QVariantMap database READ database NOTIFY databaseChanged)
    // {state, pending, rows: [{name, package, batch, state}], error, detail}
    Q_PROPERTY(QVariantMap migrations READ migrations NOTIFY databaseChanged)
    // The last command run from a button: {command, shown, state (running,
    // done, failed), lines, exitCode, message, error}
    Q_PROPERTY(QVariantMap output READ output NOTIFY outputChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY outputChanged)

public:
    ProjectChecks(QString projectDir, const FloStudio::Core::Settings *settings, bool hasAdmin, QObject *parent = nullptr);
    ~ProjectChecks() override;
    ProjectChecks(const ProjectChecks &) = delete;
    ProjectChecks &operator=(const ProjectChecks &) = delete;

    QVariantMap doctor() const { return m_doctor; }
    QVariantMap adminDoctor() const { return m_adminDoctor; }
    QVariantMap database() const { return m_database; }
    QVariantMap migrations() const { return m_migrations; }
    QVariantMap output() const { return m_output; }
    bool busy() const { return m_running != nullptr; }
    // A read is still running (tests wait for this before deleting a folder)
    bool reading() const { return m_databaseReads > 0 || m_doctorReads > 0; }

    // Runs `php flo doctor` (and `admin:doctor` with the admin panel).
    Q_INVOKABLE void runDoctor();
    // Runs `db:show` and `migrate:status` (and `admin:list` for the links).
    Q_INVOKABLE void refreshDatabase();
    // What a database action asks first: {action, command, confirm (none,
    // ask, typeDatabase), force, database, production, pending}.
    Q_INVOKABLE QVariantMap plan(const QString &action) const;
    // Runs it; the confirmation (if plan() asked for one) was given for
    // plan()'s `production`. `typed` is the database name for typeDatabase.
    // Refused (false) when .env no longer matches what was asked: then
    // the person is asked again.
    Q_INVOKABLE bool runAction(const QString &action, const QString &typed, bool plannedProduction);
    // cache:clear, view:clear, optimize, optimize:clear.
    Q_INVOKABLE void runQuick(const QString &command);
    Q_INVOKABLE void clearOutput();

    // Asked once, when the project is first opened.
    void refreshAll();
    // A demo: everything from the demo file.
    void showDemo(const QVariantMap &demo);
    void setHasAdmin(bool hasAdmin) { m_hasAdmin = hasAdmin; }

signals:
    void doctorChanged();
    void databaseChanged();
    void outputChanged();
    // A command that changed the database finished: Overview refreshes
    void changedData();

private:
    using Finished = std::function<void(const FloStudio::Core::Captured &)>;
    void capture(const QStringList &arguments, Finished onDone);
    void stream(const QStringList &arguments, const QString &command, bool changesData);
    bool production() const;
    QString configuredDatabase() const;
    // Reading each command's output (a real run, or a demo's saved output)
    void takeDoctor(const FloStudio::Core::Captured &out);
    void takeAdminDoctor(const FloStudio::Core::Captured &out);
    static QVariantMap takeShow(const FloStudio::Core::Captured &out, QVariantList *tables);
    static QVariantMap takeStatus(const FloStudio::Core::Captured &out);
    static void takeAdminList(const FloStudio::Core::Captured &out, QHash<QString, QString> *adminUrls, QStringList *classes);
    void linkTables(const QVariantList &tables, const QHash<QString, QString> &adminUrls, const QStringList &resources);
    bool demo() const { return m_settings == nullptr; }

    QString m_projectDir;
    const FloStudio::Core::Settings *m_settings; // Studio's; null for a demo
    bool m_hasAdmin;
    QVariantMap m_doctor{{QStringLiteral("state"), QStringLiteral("idle")}};
    QVariantMap m_adminDoctor{{QStringLiteral("state"), QStringLiteral("idle")}};
    QVariantMap m_database{{QStringLiteral("state"), QStringLiteral("idle")}};
    QVariantMap m_migrations{{QStringLiteral("state"), QStringLiteral("idle")}};
    QVariantMap m_output;
    QStringList m_lines;
    QProcess *m_running = nullptr; // a child of this while a button's command runs
    bool m_refreshAgain = false;
    int m_databaseReads = 0; // db:show, migrate:status, admin:list still running
    bool m_doctorAgain = false;
    int m_doctorReads = 0;   // doctor, admin:doctor still running
};
