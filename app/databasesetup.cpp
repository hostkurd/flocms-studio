#include "databasesetup.h"

#include "mariadbserver.h"

#include <flostudio/core/commands.h>
#include <flostudio/core/dbserver.h>
#include <flostudio/core/env.h>
#include <flostudio/core/output.h>
#include <flostudio/core/process.h>
#include <flostudio/security/secrets.h>

#include <QDir>
#include <QFileInfo>
#include <QProcess>

using namespace FloStudio;
using namespace FloStudio::Core;

DatabaseSetup::DatabaseSetup(const Settings *settings, MariadbServer *mariadb, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_mariadb(mariadb)
{
}

DatabaseSetup::~DatabaseSetup()
{
    QObject::disconnect(m_waitForServer);
    if (m_running)
        stopProcess(m_running);
}

bool DatabaseSetup::useStudio() const
{
    return m_server == u"studio" && m_mariadb && m_mariadb->state() != u"none";
}

QVariantMap DatabaseSetup::form() const
{
    return {{QStringLiteral("server"), useStudio() ? QStringLiteral("studio") : QStringLiteral("other")},
            {QStringLiteral("hasOwn"), m_mariadb && m_mariadb->state() != u"none"},
            {QStringLiteral("name"), m_login.name},
            {QStringLiteral("user"), m_login.user},
            {QStringLiteral("host"), m_login.host},
            {QStringLiteral("port"), m_login.port},
            {QStringLiteral("hasPassword"), !m_login.password.isEmpty()}};
}

QList<FieldProblem> DatabaseSetup::currentProblems() const
{
    DatabaseLogin login = m_login;
    // Studio's server: the address and password are Studio's own
    if (useStudio()) {
        login.host = QStringLiteral("127.0.0.1");
        login.port = m_mariadb->port();
        login.password = QStringLiteral("made-by-studio");
    }
    return checkDatabaseLogin(login, useStudio());
}

QVariantMap DatabaseSetup::problems() const
{
    QVariantMap shown;
    if (!m_touched)
        return shown;
    for (const FieldProblem &problem : currentProblems())
        shown.insert(problem.field, problem.code);
    return shown;
}

void DatabaseSetup::reset(const QString &projectDir)
{
    // Busy (here or for another project), or waiting on the person here
    if (m_demo || m_running || m_waitForServer || m_state == u"running"
        || (m_projectDir == projectDir && active()))
        return;
    m_projectDir = projectDir;
    const QString name = databaseNameFor(projectFolderName(QFileInfo(projectDir).fileName()));
    m_login = DatabaseLogin{};
    m_login.name = name;
    // Settings' default (D-D1): Studio's server when it has one
    const QString chosen = m_settings ? m_settings->databaseServer : QStringLiteral("auto");
    const bool own = m_mariadb && m_mariadb->state() != u"none";
    m_server = (chosen == u"studio" || chosen == u"auto") && own ? QStringLiteral("studio") : QStringLiteral("other");
    if (m_server == u"studio") {
        m_login.user = name;
    } else if (m_settings && chosen == u"other") {
        m_login.host = m_settings->databaseHost;
        m_login.port = m_settings->databaseOtherPort;
        m_login.user = m_settings->databaseUser;
    } else {
        m_login.user = QStringLiteral("root");
    }
    m_touched = false;
    m_started = false;
    m_state = QStringLiteral("editing");
    m_step.clear();
    m_code.clear();
    m_message.clear();
    emit changed();
}

void DatabaseSetup::set(const QString &field, const QVariant &value)
{
    if (m_state == u"running")
        return;
    const QString text = value.toString();
    if (field == u"server") {
        const bool wasStudio = useStudio();
        m_server = text == u"studio" ? QStringLiteral("studio") : QStringLiteral("other");
        // The user follows the server: Studio's own, or the usual root
        if (useStudio() && !wasStudio)
            m_login.user = m_login.name;
        else if (!useStudio() && wasStudio)
            m_login.user = m_settings ? m_settings->databaseUser : QStringLiteral("root");
    } else if (field == u"name") {
        if (useStudio() && m_login.user == m_login.name)
            m_login.user = text;
        m_login.name = text;
    } else if (field == u"user") {
        m_login.user = text;
    } else if (field == u"host") {
        m_login.host = text;
    } else if (field == u"port") {
        m_login.port = value.toInt();
    } else if (field == u"password") {
        m_login.password = text;
    } else {
        return;
    }
    // A changed login starts the card again
    if (m_state != u"editing") {
        m_state = QStringLiteral("editing");
        m_code.clear();
        m_message.clear();
    }
    emit changed();
}

void DatabaseSetup::start()
{
    // Done: a fresh reset() comes first, for whichever project shows
    if (!m_settings || m_running || m_waitForServer || m_projectDir.isEmpty() || m_state == u"running"
        || m_state == u"done")
        return;
    m_touched = true;
    if (!currentProblems().isEmpty()) {
        emit changed();
        return;
    }
    m_started = true;
    m_state = QStringLiteral("running");
    m_code.clear();
    m_message.clear();
    if (useStudio()) {
        m_login.password = generateDatabasePassword();
        startStudio();
    } else {
        // Another server: the lines first, then db:check (C-D4)
        if (!writeEnv())
            return;
        runCheck();
    }
    emit changed();
}

void DatabaseSetup::startStudio()
{
    // Studio's server first, when it isn't running (C-D3)
    if (m_mariadb->state() != u"running") {
        m_step = QStringLiteral("starting");
        QObject::disconnect(m_waitForServer);
        m_waitForServer = connect(m_mariadb, &MariadbServer::changed, this, [this] {
            const QString state = m_mariadb->state();
            if (state == u"running") {
                QObject::disconnect(m_waitForServer);
                m_waitForServer = {};
                runSql();
            } else if (state == u"failed" || state == u"stopped") {
                QObject::disconnect(m_waitForServer);
                m_waitForServer = {};
                fail(m_mariadb->error().isEmpty() ? QStringLiteral("db_start") : m_mariadb->error(), m_mariadb->detail());
            }
        });
        m_mariadb->start();
        // A server that can't start says nothing more: don't wait for it
        if (m_waitForServer && m_mariadb->state() != u"starting" && m_mariadb->state() != u"running") {
            QObject::disconnect(m_waitForServer);
            m_waitForServer = {};
            fail(m_mariadb->error().isEmpty() ? QStringLiteral("db_start") : m_mariadb->error(), m_mariadb->detail());
            return;
        }
        emit changed();
        return;
    }
    runSql();
}

void DatabaseSetup::runSql()
{
    m_login.host = QStringLiteral("127.0.0.1");
    m_login.port = m_mariadb->port();
    NewProject project;
    project.dbName = m_login.name;
    project.dbUser = m_login.user;
    project.dbPassword = m_login.password;
    const Command command = m_mariadb->rootSqlCommand(newProjectDatabaseSql(project));
    if (command.program.isEmpty()) {
        fail(QStringLiteral("db_root"));
        return;
    }
    m_step = QStringLiteral("creating");
    emit changed();
    run(command, [this](bool ok) {
        if (!ok) {
            fail(QStringLiteral("db_create"), m_output.join(u'\n'));
            return;
        }
        if (!writeEnv())
            return;
        m_mariadb->addProject(m_projectDir);
        finish();
    });
}

void DatabaseSetup::runCheck()
{
    CommandRequest request;
    request.program = Program::Flo;
    request.projectDir = m_projectDir;
    request.arguments = {QStringLiteral("db:check")};
    const auto command = prepareCommand(*m_settings, request);
    if (!command) {
        fail(command.error().code, command.error().detail);
        return;
    }
    m_step = QStringLiteral("checking");
    emit changed();
    run(command.value(), [this](bool ok) {
        if (ok) {
            finish();
            return;
        }
        const QString message = errorMessage(m_output.join(u'\n'));
        if (isUnknownDatabase(message)) {
            m_state = QStringLiteral("missing");
            m_code.clear();
            m_message = Security::redact(message, {m_login.password});
            m_step.clear();
            emit changed();
            return;
        }
        fail({}, message.isEmpty() ? m_output.join(u'\n') : message);
    });
}

void DatabaseSetup::createDatabase()
{
    if (!m_settings || m_running || m_state != u"missing")
        return;
    const auto command = createDatabaseCommand(*m_settings, m_login, m_projectDir);
    if (!command) {
        fail(command.error().code, command.error().detail);
        return;
    }
    m_state = QStringLiteral("running");
    m_step = QStringLiteral("creating");
    m_message.clear();
    emit changed();
    run(command.value(), [this](bool ok) {
        if (ok) {
            runCheck();
            return;
        }
        // The server's words; Create stays offered (A-D4)
        m_state = QStringLiteral("missing");
        m_step.clear();
        const QString message = m_output.join(u'\n').trimmed();
        m_code = message.isEmpty() ? QStringLiteral("db_create_other") : QString();
        m_message = Security::redact(message, {m_login.password});
        emit changed();
    });
}

bool DatabaseSetup::writeEnv()
{
    const auto written = writeEnvValues(m_projectDir, databaseEnv(m_login));
    if (!written) {
        fail(written.error().code, written.error().detail);
        return false;
    }
    emit envWritten(m_projectDir);
    return true;
}

void DatabaseSetup::fail(const QString &code, const QString &message)
{
    m_state = QStringLiteral("failed");
    m_step.clear();
    m_code = code;
    m_message = Security::redact(message, {m_login.password});
    emit changed();
}

void DatabaseSetup::finish()
{
    m_state = QStringLiteral("done");
    m_step.clear();
    // A password that worked is kept for the next project on this server,
    // as the wizard does (Studio's own user's too)
    if (!m_login.password.isEmpty() && Security::secureStoreAvailable())
        Security::savePassword(Security::databaseAccount(m_login.host, m_login.port, m_login.user), m_login.password, this,
                               [](const Result<QString> &) {});
    m_login.password.clear();
    emit changed();
    emit envWritten(m_projectDir);
}

void DatabaseSetup::run(const Command &command, const std::function<void(bool ok)> &done)
{
    m_output.clear();
    m_running = startStreaming(
        command, this,
        [this](const OutputLine &line) {
            if (line.error)
                m_output << line.text;
        },
        [this, done](int exitCode, bool crashed) {
            if (m_running)
                m_running->deleteLater();
            m_running = nullptr;
            done(exitCode == 0 && !crashed);
        });
}

void DatabaseSetup::showDemo(const QVariantMap &demo)
{
    m_demo = true;
    m_projectDir = demo.value(QStringLiteral("projectDir")).toString();
    m_server = demo.value(QStringLiteral("server"), QStringLiteral("studio")).toString();
    m_login.name = demo.value(QStringLiteral("name")).toString();
    m_login.user = demo.value(QStringLiteral("user")).toString();
    m_login.host = demo.value(QStringLiteral("host"), QStringLiteral("127.0.0.1")).toString();
    m_login.port = demo.value(QStringLiteral("port"), 3306).toInt();
    m_state = demo.value(QStringLiteral("state"), QStringLiteral("editing")).toString();
    m_step = demo.value(QStringLiteral("step")).toString();
    m_code = demo.value(QStringLiteral("code")).toString();
    m_message = demo.value(QStringLiteral("message")).toString();
    emit changed();
}
