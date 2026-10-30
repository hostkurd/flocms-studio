#include "newproject.h"

#include "mariadbserver.h"

#include <flostudio/core/dbserver.h>
#include <flostudio/core/env.h>
#include <flostudio/core/output.h>
#include <flostudio/core/process.h>
#include <flostudio/core/wizard.h>
#include <flostudio/security/secrets.h>

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

using namespace FloStudio::Core;
namespace Security = FloStudio::Security;

namespace {

constexpr int kOutputLines = 400;

// Once the run started, the folder exists and what it was made from stays
bool fixedAfterStart(const QString &field)
{
    return field == u"name" || field == u"parentDir" || field == u"createDatabase";
}

// Fields that can still change after a step failed: they don't change
// which steps run, only what a step does (a password, the admin's email)
bool editableAfterFailure(const QString &field)
{
    return (field.startsWith(u"db") || field.startsWith(u"admin") || field == u"rememberPassword") && field != u"createDatabase";
}

} // namespace

NewProjectWizard::NewProjectWizard(const Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    reset();
}

NewProjectWizard::~NewProjectWizard()
{
    // A step still running ends with its children (its tracking)
    if (m_running) {
        QObject::disconnect(m_running, nullptr, this, nullptr);
        m_running->kill();
        m_running->waitForFinished(2000);
    }
}

bool NewProjectWizard::secureStore() const
{
    return m_settings && Security::secureStoreAvailable();
}

void NewProjectWizard::reset()
{
    if (m_running)
        return;
    m_project = NewProject{};
    m_typedName.clear();
    m_touched.clear();
    m_rememberPassword = true;
    // Where projects are: the first project folder in Settings, else ~/Sites
    if (m_settings && !m_settings->projectFolders.isEmpty())
        m_project.parentDir = m_settings->projectFolders.constFirst();
    else
        m_project.parentDir = QDir::toNativeSeparators(QDir::home().filePath(QStringLiteral("Sites")));
    m_project.port = m_settings ? m_settings->serverPort : 8000;
    m_project.dbUser = QStringLiteral("root");
    m_pickedServer.clear();
    m_steps.clear();
    m_lines.clear();
    m_plan.clear();
    m_state = QStringLiteral("editing");
    m_projectPath.clear();
    m_current = -1;
    m_madeFolder = false;
    m_skippedDatabase = false;
    m_databaseMissing = false;
    m_envSecrets.clear();
    emit formChanged();
    emit stepsChanged();
    emit linesChanged();
}

QVariantMap NewProjectWizard::form() const
{
    return {{QStringLiteral("typedName"), m_typedName},
            {QStringLiteral("name"), m_project.name},
            {QStringLiteral("parentDir"), m_project.parentDir},
            {QStringLiteral("folder"), QDir::toNativeSeparators(newProjectFolder(m_project))},
            {QStringLiteral("setup"), setupName(m_project.setup)},
            {QStringLiteral("hasAdmin"), setupHasAdmin(m_project.setup)},
            {QStringLiteral("hasSite"), setupHasSite(m_project.setup)},
            {QStringLiteral("addApi"), m_project.addApi},
            {QStringLiteral("languages"), m_project.languages},
            {QStringLiteral("defaultLanguage"), m_project.defaultLanguage},
            {QStringLiteral("appName"), m_project.appName},
            {QStringLiteral("database"), m_project.mysql ? QStringLiteral("mysql") : QStringLiteral("sqlite")},
            {QStringLiteral("dbHost"), m_project.dbHost},
            {QStringLiteral("dbPort"), m_project.dbPort},
            {QStringLiteral("dbName"), m_project.dbName},
            {QStringLiteral("dbUser"), m_project.dbUser},
            {QStringLiteral("dbPassword"), m_project.dbPassword},
            {QStringLiteral("rememberPassword"), m_rememberPassword},
            {QStringLiteral("createDatabase"), m_project.createDatabase},
            {QStringLiteral("server"), chosenServer()},
            {QStringLiteral("defaultServer"), defaultServer()},
            {QStringLiteral("adminName"), m_project.adminName},
            {QStringLiteral("adminEmail"), m_project.adminEmail},
            {QStringLiteral("adminPassword"), m_project.adminPassword}};
}

QVariantMap NewProjectWizard::problems() const
{
    QVariantMap shown;
    for (const FieldProblem &problem : currentProblems()) {
        if (m_touched.contains(problem.field) || m_touched.contains(QStringLiteral("*")))
            shown.insert(problem.field, problem.code);
    }
    return shown;
}

QList<FieldProblem> NewProjectWizard::currentProblems() const
{
    QList<FieldProblem> problems = checkNewProject(m_project);
    if (m_state != u"editing")
        problems.removeIf([](const FieldProblem &problem) { return fixedAfterStart(problem.field); });
    return problems;
}

bool NewProjectWizard::ready() const
{
    return currentProblems().isEmpty();
}

QVariantList NewProjectWizard::plan() const
{
    QVariantList plan;
    for (const WizardStep &step : newProjectSteps(m_project))
        plan << QVariantMap{{QStringLiteral("id"), step.id}, {QStringLiteral("shown"), step.shown}};
    return plan;
}

void NewProjectWizard::setName(const QString &typed)
{
    if (m_state != u"editing")
        return;
    m_typedName = typed;
    const QString before = m_project.name;
    m_project.name = projectFolderName(typed);
    // The database name follows the folder until the person changes it
    if (m_project.dbName.isEmpty() || m_project.dbName == databaseNameFor(before)) {
        // Studio's own user is named after the database, and follows it
        if (m_project.createDatabase && m_project.dbUser == m_project.dbName)
            m_project.dbUser = databaseNameFor(m_project.name);
        m_project.dbName = databaseNameFor(m_project.name);
    }
    if (m_project.appName.isEmpty() || m_project.appName == before)
        m_project.appName.clear();
    emit formChanged();
}

void NewProjectWizard::setParentFolder(const QUrl &folder)
{
    set(QStringLiteral("parentDir"), QDir::toNativeSeparators(folder.toLocalFile()));
}

void NewProjectWizard::set(const QString &field, const QVariant &value)
{
    if (m_state == u"running" || m_state == u"done" || (m_state == u"failed" && !editableAfterFailure(field)))
        return;
    // A database Studio made: its name, user and password are what it made
    if (m_state == u"failed" && m_project.createDatabase && field.startsWith(u"db"))
        return;
    const QString text = value.toString();
    if (field == u"parentDir") {
        m_project.parentDir = text;
    } else if (field == u"setup") {
        Setup setup{};
        if (setupFromName(text, &setup))
            m_project.setup = setup;
        if (!setupHasSite(m_project.setup))
            m_project.addApi = false;
    } else if (field == u"addApi") {
        m_project.addApi = value.toBool() && setupHasSite(m_project.setup);
    } else if (field == u"languages") {
        m_project.languages = value.toStringList();
        if (!m_project.languages.contains(m_project.defaultLanguage) && !m_project.languages.isEmpty())
            m_project.defaultLanguage = m_project.languages.constFirst();
    } else if (field == u"defaultLanguage") {
        if (m_project.languages.contains(text))
            m_project.defaultLanguage = text;
    } else if (field == u"appName") {
        m_project.appName = text;
    } else if (field == u"database") {
        const bool wasMysql = m_project.mysql;
        m_project.mysql = text == u"mysql";
        if (m_project.mysql && !wasMysql)
            applyServerDefaults();
    } else if (field == u"createDatabase") {
        m_project.createDatabase = value.toBool() && chosenServer() == u"studio";
        if (m_project.createDatabase) {
            m_project.dbHost = QStringLiteral("127.0.0.1");
            m_project.dbPort = m_mariadb ? m_mariadb->port() : 3306;
            m_project.dbUser = m_project.dbName.isEmpty() ? databaseNameFor(m_project.name) : m_project.dbName;
            m_project.dbPassword = generateDatabasePassword();
        } else {
            m_project.dbUser = QStringLiteral("root");
            m_project.dbPassword.clear();
        }
    } else if (field == u"server") {
        // This project only; Settings keeps the default (D-D1)
        if (text != u"studio" && text != u"found" && text != u"other")
            return;
        m_pickedServer = text;
        if (m_project.mysql && chosenServer() != m_appliedServer)
            applyServerDefaults();
    } else if (field == u"dbHost") {
        m_project.dbHost = text;
    } else if (field == u"dbPort") {
        m_project.dbPort = value.toInt();
    } else if (field == u"dbName") {
        m_project.dbName = text;
    } else if (field == u"dbUser") {
        m_project.dbUser = text;
    } else if (field == u"dbPassword") {
        m_project.dbPassword = text;
    } else if (field == u"rememberPassword") {
        m_rememberPassword = value.toBool();
    } else if (field == u"adminName") {
        m_project.adminName = text;
    } else if (field == u"adminEmail") {
        m_project.adminEmail = text;
    } else if (field == u"adminPassword") {
        m_project.adminPassword = text;
    } else {
        return;
    }
    emit formChanged();
}

QString NewProjectWizard::defaultServer() const
{
    QString chosen = m_settings ? m_settings->databaseServer : QStringLiteral("auto");
    const bool own = m_mariadb && m_mariadb->state() != u"none";
    if (chosen == u"auto")
        return own ? QStringLiteral("studio") : QStringLiteral("found");
    if (chosen == u"studio" && !own)
        return QStringLiteral("found");
    return chosen;
}

QString NewProjectWizard::chosenServer() const
{
    if (m_pickedServer.isEmpty())
        return defaultServer();
    const bool own = m_mariadb && m_mariadb->state() != u"none";
    return m_pickedServer == u"studio" && !own ? defaultServer() : m_pickedServer;
}

void NewProjectWizard::setMariadb(MariadbServer *server)
{
    m_mariadb = server;
    connect(server, &MariadbServer::changed, this, &NewProjectWizard::serverChoiceChanged);
}

void NewProjectWizard::serverChoiceChanged()
{
    if (m_state != u"editing")
        return;
    if (m_project.mysql && chosenServer() != m_appliedServer)
        applyServerDefaults();
    // Studio's server's port may have changed in Settings
    if (m_project.mysql && m_project.createDatabase && m_mariadb && m_project.dbPort != m_mariadb->port())
        m_project.dbPort = m_mariadb->port();
    emit formChanged();
}

void NewProjectWizard::applyServerDefaults()
{
    const QString server = chosenServer();
    m_appliedServer = server;
    m_project.createDatabase = server == u"studio";
    if (server == u"studio") {
        m_project.dbHost = QStringLiteral("127.0.0.1");
        m_project.dbPort = m_mariadb ? m_mariadb->port() : 3306;
        m_project.dbUser = m_project.dbName.isEmpty() ? databaseNameFor(m_project.name) : m_project.dbName;
        m_project.dbPassword = generateDatabasePassword();
    } else if (server == u"other" && m_settings) {
        m_project.dbHost = m_settings->databaseHost;
        m_project.dbPort = m_settings->databaseOtherPort;
        m_project.dbUser = m_settings->databaseUser;
        m_project.dbPassword.clear();
    } else {
        m_project.dbHost = QStringLiteral("127.0.0.1");
        m_project.dbPort = 3306;
        m_project.dbUser = QStringLiteral("root");
        m_project.dbPassword.clear();
    }
}

void NewProjectWizard::touch(const QString &field)
{
    if (!m_touched.contains(field)) {
        m_touched << field;
        emit formChanged();
    }
}

void NewProjectWizard::fillSavedPassword()
{
    if (!secureStore())
        return;
    const QString account = Security::databaseAccount(m_project.dbHost, m_project.dbPort, m_project.dbUser);
    Security::readPassword(account, this, [this, account](const Result<QString> &secret) {
        // The answer can come late (the system may ask first): it only
        // fills the form it was asked for
        if (m_state == u"running" || m_state == u"done"
            || account != Security::databaseAccount(m_project.dbHost, m_project.dbPort, m_project.dbUser))
            return;
        if (!secret) {
            emit problem(secret.error().code, secret.error().detail);
            return;
        }
        m_project.dbPassword = secret.value();
        emit formChanged();
    });
}

QStringList NewProjectWizard::secrets() const
{
    QStringList secrets{m_project.dbPassword, m_project.adminPassword};
    // The new project's APP_KEY, once create-project wrote it
    secrets += m_envSecrets;
    return secrets;
}

void NewProjectWizard::readEnvSecrets()
{
    m_envSecrets = Security::envSecrets(readEnvFile(newProjectFolder(m_project)));
}

void NewProjectWizard::addLine(const QString &line)
{
    m_lines << Security::redact(line, secrets());
    if (m_lines.size() > kOutputLines)
        m_lines.remove(0, m_lines.size() - kOutputLines);
    emit linesChanged();
}

void NewProjectWizard::setStep(int index, const QString &state, const StepProblem &problem)
{
    if (index < 0 || index >= m_steps.size())
        return;
    QVariantMap step = m_steps.at(index).toMap();
    step.insert(QStringLiteral("state"), state);
    step.insert(QStringLiteral("code"), problem.code);
    step.insert(QStringLiteral("detail"), Security::redact(problem.detail, secrets()));
    step.insert(QStringLiteral("message"), Security::redact(problem.message, secrets()));
    m_steps[index] = step;
    emit stepsChanged();
}

void NewProjectWizard::start()
{
    if (!m_settings || m_running || m_state != u"editing")
        return;
    m_touched << QStringLiteral("*");
    if (!ready()) {
        emit formChanged();
        return;
    }
    if (!QDir().mkpath(m_project.parentDir)) {
        m_touched << QStringLiteral("parentDir");
        emit problem(QStringLiteral("parent_invalid"), m_project.parentDir);
        return;
    }
    m_madeFolder = true; // checkNewProject: it wasn't there
    m_envSecrets.clear();
    m_plan = newProjectSteps(m_project);
    m_steps.clear();
    for (const WizardStep &step : std::as_const(m_plan))
        m_steps << QVariantMap{{QStringLiteral("id"), step.id},
                               {QStringLiteral("shown"), step.shown},
                               {QStringLiteral("state"), QStringLiteral("waiting")}};
    m_lines.clear();
    emit linesChanged();
    runFrom(0);
}

void NewProjectWizard::retry()
{
    if (!m_settings || m_running || m_state != u"failed" || m_current < 0 || m_current >= m_plan.size())
        return;
    m_touched << QStringLiteral("*");
    if (!ready()) {
        emit formChanged();
        return;
    }
    // What the person fixed since: the same steps, with the new values
    const QList<WizardStep> plan = newProjectSteps(m_project);
    if (plan.size() != m_plan.size())
        return; // the fields that change the steps are fixed after a start
    m_plan = plan;
    for (int i = 0; i < m_plan.size(); ++i) {
        QVariantMap step = m_steps.at(i).toMap();
        step.insert(QStringLiteral("shown"), m_plan.at(i).shown);
        m_steps[i] = step;
    }
    int from = m_current;
    const QString folder = newProjectFolder(m_project);
    if (m_plan.at(from).id == u"create" && QFileInfo::exists(folder)) {
        // create-project refuses a folder that isn't empty: one it finished
        // is kept, one it left half made (stopped, no network) goes
        if (QFileInfo::exists(folder + QStringLiteral("/composer.json"))
            && QFileInfo::exists(folder + QStringLiteral("/vendor/autoload.php"))) {
            setStep(from, QStringLiteral("done"));
            ++from;
        } else if (!m_madeFolder || !QDir(folder).removeRecursively()) {
            setStep(from, QStringLiteral("failed"), {QStringLiteral("folder_remove"), QDir::toNativeSeparators(folder), {}});
            emit stepsChanged();
            return;
        }
    }
    // .env was already written: it gets the database values fixed since
    const int env = static_cast<int>(std::find_if(m_plan.cbegin(), m_plan.cend(), [](const WizardStep &step) {
                                         return step.kind == StepKind::WriteEnv;
                                     }) - m_plan.cbegin());
    if (env < from) {
        const auto written = writeEnvValues(folder, newProjectEnv(m_project));
        if (!written) {
            setStep(from, QStringLiteral("failed"), {written.error().code, written.error().detail, {}});
            emit stepsChanged();
            return;
        }
        readEnvSecrets();
    }
    runFrom(from);
}

void NewProjectWizard::startOver()
{
    if (m_running || m_state != u"failed")
        return;
    // Only the folder this run made, and nothing when it can't go
    const QString folder = newProjectFolder(m_project);
    if (m_madeFolder && QFileInfo::exists(folder) && !QDir(folder).removeRecursively()) {
        emit problem(QStringLiteral("folder_remove"), QDir::toNativeSeparators(folder));
        return;
    }
    reset();
}

void NewProjectWizard::cancel()
{
    if (m_running) {
        stopProcess(m_running);
    } else if (m_waitForServer) {
        // Waiting for Studio's server: the step stops (the server goes on)
        QObject::disconnect(m_waitForServer);
        m_waitForServer = {};
        stepDone(m_current, false, {QStringLiteral("step_stopped"), {}, {}});
    }
}

void NewProjectWizard::runFrom(int index)
{
    m_state = QStringLiteral("running");
    m_databaseMissing = false;
    for (int i = index; i < m_steps.size(); ++i)
        setStep(i, QStringLiteral("waiting"));
    emit formChanged(); // problems() and ready() depend on the state
    runStep(index);
}

void NewProjectWizard::runStep(int index)
{
    m_current = index;
    if (index >= m_plan.size()) {
        finish(true);
        return;
    }
    const WizardStep &step = m_plan.at(index);
    setStep(index, QStringLiteral("running"));
    addLine(QStringLiteral("› ") + step.shown);

    if (step.kind == StepKind::CreateDatabase) {
        runCreateDatabase(index);
        return;
    }
    if (step.kind == StepKind::WriteEnv) {
        const auto written = writeEnvValues(newProjectFolder(m_project), newProjectEnv(m_project));
        stepDone(index, written.ok(), written ? StepProblem{} : StepProblem{written.error().code, written.error().detail, {}});
        return;
    }
    const auto command = prepareCommand(*m_settings, step.request);
    if (!command) {
        stepDone(index, false, {command.error().code, command.error().detail, {}});
        return;
    }
    m_output.clear();
    m_running = startStreaming(
        command.value(), this,
        [this](const OutputLine &line) {
            if (line.error)
                m_output << line.text;
            addLine(line.text);
        },
        [this, index](int exitCode, bool crashed) {
            if (m_running)
                m_running->deleteLater();
            m_running = nullptr;
            const bool ok = exitCode == 0 && !crashed;
            // What the command said, else why it ended
            const QString message = errorMessage(m_output.join(u'\n'));
            StepProblem problem;
            if (!ok && message.isEmpty() && crashed)
                problem.code = QStringLiteral("step_stopped");
            else if (!ok && message.isEmpty())
                problem = {QStringLiteral("step_exit"), QString::number(exitCode), {}};
            else if (!ok)
                problem.message = message;
            stepDone(index, ok, problem);
        });
}

void NewProjectWizard::finish(bool saveDatabaseLogin)
{
    m_state = QStringLiteral("done");
    m_projectPath = QDir::toNativeSeparators(newProjectFolder(m_project));
    // Studio's server: the project is marked as its, for the Database tab
    if (m_project.mysql && m_project.createDatabase && m_mariadb)
        m_mariadb->addProject(m_projectPath);
    // A password that worked is saved, if the person wanted that; the
    // password Studio made is always kept (its checkbox isn't shown)
    if (saveDatabaseLogin && m_project.mysql && (m_rememberPassword || m_project.createDatabase)
        && !m_project.dbPassword.isEmpty() && secureStore())
        Security::savePassword(Security::databaseAccount(m_project.dbHost, m_project.dbPort, m_project.dbUser),
                               m_project.dbPassword, this, [](const Result<QString> &) {});
    emit stepsChanged();
    emit formChanged();
    emit created(m_projectPath);
}

bool NewProjectWizard::failedAtDatabaseCheck() const
{
    return m_state == u"failed" && !m_running && m_current >= 0 && m_current < m_plan.size()
           && m_plan.at(m_current).id == u"db-check";
}

bool NewProjectWizard::canSkipDatabase() const
{
    if (m_demo)
        return m_demoCanSkip;
    return failedAtDatabaseCheck();
}

bool NewProjectWizard::missingDatabase() const
{
    if (m_demo)
        return m_demoMissing;
    // Stays offered when creating it failed, so it can be tried again (A-D4)
    return failedAtDatabaseCheck() && m_project.mysql && !m_project.createDatabase && m_databaseMissing;
}

void NewProjectWizard::createDatabase()
{
    if (!m_settings || !missingDatabase())
        return;
    const int index = m_current;
    const auto command = createDatabaseCommand(*m_settings, databaseLogin(m_project), newProjectFolder(m_project));
    m_state = QStringLiteral("running");
    setStep(index, QStringLiteral("running"));
    emit formChanged();
    if (!command) {
        stepDone(index, false, {command.error().code, command.error().detail, {}});
        return;
    }
    addLine(QStringLiteral("› ") + command.value().shown);
    m_output.clear();
    m_running = startStreaming(
        command.value(), this,
        [this](const OutputLine &line) {
            if (line.error)
                m_output << line.text;
            addLine(line.text);
        },
        [this, index](int exitCode, bool crashed) {
            if (m_running)
                m_running->deleteLater();
            m_running = nullptr;
            if (exitCode == 0 && !crashed) {
                runFrom(index); // db:check again, then the rest (A-D3)
                return;
            }
            // The server's own words in place of db:check's (A-D4)
            const QString message = m_output.join(u'\n').trimmed();
            stepDone(index, false, message.isEmpty() ? StepProblem{QStringLiteral("db_create_other"), {}, {}}
                                                     : StepProblem{{}, {}, message});
        });
}

void NewProjectWizard::skipDatabaseSteps()
{
    if (!canSkipDatabase())
        return;
    for (int i = m_current; i < m_plan.size(); ++i) {
        const QString &id = m_plan.at(i).id;
        if (id == u"db-check" || id == u"migrate" || id == u"admin")
            setStep(i, QStringLiteral("skipped"));
    }
    m_skippedDatabase = true;
    // The password never worked: nothing is saved (A-D5)
    finish(false);
}

void NewProjectWizard::runCreateDatabase(int index)
{
    if (!m_mariadb || m_mariadb->state() == u"none") {
        stepDone(index, false, {QStringLiteral("db_program"), {}, {}});
        return;
    }
    // Studio's server first, if it isn't running (D10): the step waits for it
    if (m_mariadb->state() != u"running") {
        QObject::disconnect(m_waitForServer);
        m_waitForServer = connect(m_mariadb, &MariadbServer::changed, this, [this, index] {
            const QString state = m_mariadb->state();
            if (state == u"running") {
                QObject::disconnect(m_waitForServer);
                m_waitForServer = {};
                runCreateDatabase(index);
            } else if (state == u"failed" || state == u"stopped") {
                QObject::disconnect(m_waitForServer);
                m_waitForServer = {};
                stepDone(index, false, {m_mariadb->error().isEmpty() ? QStringLiteral("db_start") : m_mariadb->error(),
                                        m_mariadb->detail(), {}});
            }
        });
        addLine(QStringLiteral("› ") + tr("Starting Studio's MariaDB %1").arg(m_mariadb->series()));
        m_mariadb->start();
        return;
    }
    // Where the server really runs (it may have started on another port,
    // say "Start on 3307"): .env follows
    if (m_project.dbHost != u"127.0.0.1" || m_project.dbPort != m_mariadb->port()) {
        m_project.dbHost = QStringLiteral("127.0.0.1");
        m_project.dbPort = m_mariadb->port();
        const auto written = writeEnvValues(newProjectFolder(m_project), newProjectEnv(m_project));
        if (!written) {
            stepDone(index, false, {written.error().code, written.error().detail, {}});
            return;
        }
        readEnvSecrets();
        emit formChanged();
    }
    const Command command = m_mariadb->rootSqlCommand(newProjectDatabaseSql(m_project));
    if (command.program.isEmpty()) {
        stepDone(index, false, {QStringLiteral("db_root"), {}, {}});
        return;
    }
    m_output.clear();
    m_running = startStreaming(
        command, this,
        [this](const OutputLine &line) {
            if (line.error)
                m_output << line.text;
            addLine(line.text);
        },
        [this, index](int exitCode, bool crashed) {
            if (m_running)
                m_running->deleteLater();
            m_running = nullptr;
            const bool ok = exitCode == 0 && !crashed;
            StepProblem problem;
            if (!ok)
                problem = {QStringLiteral("db_create"), m_output.join(u'\n'), {}};
            stepDone(index, ok, problem);
        });
}

void NewProjectWizard::stepDone(int index, bool ok, const StepProblem &problem)
{
    // The new project's .env secrets, once there is one
    if (index < m_plan.size() && (m_plan.at(index).id == u"create" || m_plan.at(index).kind == StepKind::WriteEnv))
        readEnvSecrets();
    setStep(index, ok ? QStringLiteral("done") : QStringLiteral("failed"), problem);
    if (!ok && index < m_plan.size() && m_plan.at(index).id == u"db-check"
        && isUnknownDatabase(problem.message + u'\n' + problem.detail))
        m_databaseMissing = true;
    if (!ok) {
        m_state = QStringLiteral("failed");
        emit stepsChanged();
        emit formChanged();
        return;
    }
    runStep(index + 1);
}

void NewProjectWizard::showDemo(const QVariantMap &demo)
{
    const QVariantMap form = demo.value(QStringLiteral("form")).toMap();
    for (auto it = form.begin(); it != form.end(); ++it) {
        if (it.key() == u"name")
            setName(it.value().toString());
        else
            set(it.key(), it.value());
    }
    for (const QVariant &field : demo.value(QStringLiteral("touched")).toList())
        m_touched << field.toString();
    m_steps = demo.value(QStringLiteral("steps")).toList();
    m_lines = demo.value(QStringLiteral("lines")).toStringList();
    m_state = demo.value(QStringLiteral("state"), QStringLiteral("editing")).toString();
    m_projectPath = demo.value(QStringLiteral("projectPath")).toString();
    m_demo = true;
    m_demoMissing = demo.value(QStringLiteral("missingDatabase")).toBool();
    m_demoCanSkip = demo.value(QStringLiteral("canSkipDatabase")).toBool();
    m_skippedDatabase = demo.value(QStringLiteral("skippedDatabase")).toBool();
    emit formChanged();
    emit stepsChanged();
    emit linesChanged();
}
