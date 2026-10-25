#pragma once

#include <flostudio/core/settings.h>
#include <flostudio/core/wizard.h>

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class MariadbServer;
class QProcess;

// The new project wizard (stage 6, board 6): the choices, what they are
// missing, the commands they will run, and the run itself, step by step
// with the output shown live (passwords and keys never shown). Belongs to
// Studio, which outlives it, so the settings pointer stays valid.
//
// state: editing, running, failed (a step failed: retry runs it and what
// follows), done (projectPath is the new project).
class NewProjectWizard : public QObject
{
    Q_OBJECT
    // The choices: name, parentDir, setup (site-admin, site, admin,
    // api-admin, api), addApi, languages, defaultLanguage, appName,
    // database (sqlite, mysql), dbHost, dbPort, dbName, dbUser,
    // dbPassword, rememberPassword, adminName, adminEmail, adminPassword,
    // createDatabase (Studio makes it on its own MariaDB, D10) and
    // server (studio, found, other: Settings' choice, D9)
    Q_PROPERTY(QVariantMap form READ form NOTIFY formChanged)
    // field → problem code (Core's checkNewProject), for what was touched
    Q_PROPERTY(QVariantMap problems READ problems NOTIFY formChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY formChanged)
    // What will run: [{id, shown}]
    Q_PROPERTY(QVariantList plan READ plan NOTIFY formChanged)
    // The run: [{id, shown, state (waiting, running, done, failed), message}]
    Q_PROPERTY(QVariantList steps READ steps NOTIFY stepsChanged)
    Q_PROPERTY(QStringList lines READ lines NOTIFY linesChanged)
    Q_PROPERTY(QString state READ state NOTIFY stepsChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY stepsChanged)
    Q_PROPERTY(bool secureStore READ secureStore CONSTANT)

public:
    NewProjectWizard(const FloStudio::Core::Settings *settings, QObject *parent = nullptr);
    ~NewProjectWizard() override;
    NewProjectWizard(const NewProjectWizard &) = delete;
    NewProjectWizard &operator=(const NewProjectWizard &) = delete;

    QVariantMap form() const;
    QVariantMap problems() const;
    bool ready() const;
    QVariantList plan() const;
    QVariantList steps() const { return m_steps; }
    QStringList lines() const { return m_lines; }
    QString state() const { return m_state; }
    QString projectPath() const { return m_projectPath; }
    bool secureStore() const;

    // A fresh form (the folder from Settings' project folders).
    Q_INVOKABLE void reset();
    Q_INVOKABLE void set(const QString &field, const QVariant &value);
    // The project name as typed: the folder name is made from it.
    Q_INVOKABLE void setName(const QString &typed);
    Q_INVOKABLE void setParentFolder(const QUrl &folder);
    // A field shows its problem once touched (or once Create was tried).
    Q_INVOKABLE void touch(const QString &field);
    // Fills dbPassword from the secure store, for this host, port and user.
    Q_INVOKABLE void fillSavedPassword();

    Q_INVOKABLE void start();
    // From the step that failed.
    Q_INVOKABLE void retry();
    // Stops the step that runs; what was made stays (the folder).
    Q_INVOKABLE void cancel();
    // After a failure: deletes the folder this run made, and the form
    // starts again.
    Q_INVOKABLE void startOver();

    // The demo: the form and the run from the demo file.
    void showDemo(const QVariantMap &demo);
    // Studio's own MariaDB (it outlives the wizard)
    void setMariadb(MariadbServer *server) { m_mariadb = server; }

signals:
    void formChanged();
    void stepsChanged();
    void linesChanged();
    // The project is made: Studio lists and opens it
    void created(const QString &path);
    // The saved password could not be read (keychain code)
    void problem(const QString &code, const QString &detail);

private:
    void runFrom(int index);
    void runStep(int index);
    void runCreateDatabase(int index);
    // The server MySQL projects use by default (Settings, D9): studio, found, other
    QString defaultServer() const;
    void applyServerDefaults();
    // Why a step failed: a code Texts.problem knows (with its detail), or
    // what the command itself said
    struct StepProblem
    {
        QString code;
        QString detail;
        QString message;
    };
    void stepDone(int index, bool ok, const StepProblem &problem);
    void addLine(const QString &line);
    void setStep(int index, const QString &state, const StepProblem &problem = {});
    QStringList secrets() const;
    void readEnvSecrets();
    QList<FloStudio::Core::FieldProblem> currentProblems() const;

    const FloStudio::Core::Settings *m_settings; // Studio's; null for a demo
    MariadbServer *m_mariadb = nullptr;          // Studio's
    QMetaObject::Connection m_waitForServer;     // db-create waits for it to start
    FloStudio::Core::NewProject m_project;
    QString m_typedName;
    bool m_rememberPassword = true;
    QStringList m_touched;
    QList<FloStudio::Core::WizardStep> m_plan; // made when the run starts, again on retry
    bool m_madeFolder = false;                 // the folder is this run's
    QStringList m_envSecrets;                  // the new .env's, read after create and env
    QVariantList m_steps;
    QStringList m_lines;
    QString m_state = QStringLiteral("editing");
    QString m_projectPath;
    int m_current = -1;
    QProcess *m_running = nullptr; // a child of this while a step runs
    QStringList m_output;          // the step's stderr, for its message
};
