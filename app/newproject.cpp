#include "newproject.h"

#include <flostudio/core/env.h>
#include <flostudio/core/output.h>
#include <flostudio/core/process.h>
#include <flostudio/security/secrets.h>

#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

using namespace FloStudio::Core;
namespace Security = FloStudio::Security;

namespace {

constexpr int kOutputLines = 400;

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
    m_steps.clear();
    m_lines.clear();
    m_plan.clear();
    m_state = QStringLiteral("editing");
    m_projectPath.clear();
    m_current = -1;
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
            {QStringLiteral("adminName"), m_project.adminName},
            {QStringLiteral("adminEmail"), m_project.adminEmail},
            {QStringLiteral("adminPassword"), m_project.adminPassword}};
}

QVariantMap NewProjectWizard::problems() const
{
    QVariantMap shown;
    for (const FieldProblem &problem : checkNewProject(m_project)) {
        if (m_touched.contains(problem.field) || m_touched.contains(QStringLiteral("*")))
            shown.insert(problem.field, problem.code);
    }
    return shown;
}

bool NewProjectWizard::ready() const
{
    return checkNewProject(m_project).isEmpty();
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
    if (m_running || m_state == u"done")
        return;
    m_typedName = typed;
    const QString before = m_project.name;
    m_project.name = projectFolderName(typed);
    // The database name follows the folder until the person changes it
    if (m_project.dbName.isEmpty() || m_project.dbName == databaseNameFor(before))
        m_project.dbName = databaseNameFor(m_project.name);
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
    if (m_running || m_state == u"done")
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
        m_project.mysql = text == u"mysql";
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
    Security::readPassword(Security::databaseAccount(m_project.dbHost, m_project.dbPort, m_project.dbUser), this,
                           [this](const Result<QString> &secret) {
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
    secrets += Security::envSecrets(readEnvFile(newProjectFolder(m_project)));
    return secrets;
}

void NewProjectWizard::addLine(const QString &line)
{
    m_lines << Security::redact(line, secrets());
    if (m_lines.size() > kOutputLines)
        m_lines.remove(0, m_lines.size() - kOutputLines);
    emit linesChanged();
}

void NewProjectWizard::setStep(int index, const QString &state, const QString &message)
{
    if (index < 0 || index >= m_steps.size())
        return;
    QVariantMap step = m_steps.at(index).toMap();
    step.insert(QStringLiteral("state"), state);
    step.insert(QStringLiteral("message"), Security::redact(message, secrets()));
    m_steps[index] = step;
    emit stepsChanged();
}

void NewProjectWizard::start()
{
    if (!m_settings || m_running || m_state == u"running" || m_state == u"done")
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
    if (m_state != u"failed" || m_current < 0)
        return;
    runFrom(m_current);
}

void NewProjectWizard::cancel()
{
    if (m_running)
        stopProcess(m_running);
}

void NewProjectWizard::runFrom(int index)
{
    m_state = QStringLiteral("running");
    for (int i = index; i < m_steps.size(); ++i)
        setStep(i, QStringLiteral("waiting"));
    runStep(index);
}

void NewProjectWizard::runStep(int index)
{
    m_current = index;
    if (index >= m_plan.size()) {
        m_state = QStringLiteral("done");
        m_projectPath = QDir::toNativeSeparators(newProjectFolder(m_project));
        // A password that worked is saved, if the person wanted that
        if (m_project.mysql && m_rememberPassword && !m_project.dbPassword.isEmpty() && secureStore())
            Security::savePassword(Security::databaseAccount(m_project.dbHost, m_project.dbPort, m_project.dbUser),
                                   m_project.dbPassword, this, [](const Result<QString> &) {});
        emit stepsChanged();
        emit created(m_projectPath);
        return;
    }
    const WizardStep &step = m_plan.at(index);
    setStep(index, QStringLiteral("running"));
    addLine(QStringLiteral("› ") + step.shown);

    if (step.kind == StepKind::WriteEnv) {
        const auto written = writeEnvValues(newProjectFolder(m_project), newProjectEnv(m_project));
        stepDone(index, written.ok(), written ? QString() : written.error().code + QStringLiteral(": ") + written.error().detail);
        return;
    }
    const auto command = prepareCommand(*m_settings, step.request);
    if (!command) {
        stepDone(index, false, command.error().code + QStringLiteral(": ") + command.error().detail);
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
            const QString message = errorMessage(m_output.join(u'\n'));
            stepDone(index, ok,
                     ok ? QString()
                        : (message.isEmpty() ? (crashed ? QStringLiteral("stopped") : QStringLiteral("exit code %1").arg(exitCode))
                                             : message));
        });
}

void NewProjectWizard::stepDone(int index, bool ok, const QString &message)
{
    setStep(index, ok ? QStringLiteral("done") : QStringLiteral("failed"), message);
    if (!ok) {
        m_state = QStringLiteral("failed");
        emit stepsChanged();
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
    emit formChanged();
    emit stepsChanged();
    emit linesChanged();
}
