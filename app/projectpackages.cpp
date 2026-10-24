#include "projectpackages.h"

#include <flostudio/core/commands.h>
#include <flostudio/core/output.h>
#include <flostudio/core/process.h>

#include <QDateTime>
#include <QFutureWatcher>
#include <QProcess>
#include <QtConcurrent>

#include <utility>

using namespace FloStudio::Core;

namespace {

constexpr int kOutputLines = 400;
constexpr int kCheckMs = 120000; // composer outdated asks Packagist for every package

// The packages the tab lists even when they aren't there: Studio can add them
const QStringList &addable()
{
    static const QStringList names{QStringLiteral("hostkurd/flocms-admin"), QStringLiteral("hostkurd/flocms-api")};
    return names;
}

QString shown(const PackageStep &step)
{
    return shownCommand(step.program, step.arguments);
}

} // namespace

ProjectPackages::ProjectPackages(QString projectDir, const Settings *settings, QObject *parent)
    : QObject(parent)
    , m_projectDir(std::move(projectDir))
    , m_settings(settings)
{
}

ProjectPackages::~ProjectPackages()
{
    // A step still running ends with its children (its tracking)
    if (m_process) {
        QObject::disconnect(m_process, nullptr, this, nullptr);
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

void ProjectPackages::setInstalled(const QVariantList &packages, const QVariantList &others)
{
    m_installed = packages;
    m_others.clear();
    for (const QVariant &value : others) {
        const QVariantMap package = value.toMap();
        m_others << package.value(QStringLiteral("name")).toString() + u' ' + package.value(QStringLiteral("version")).toString();
    }
    emit packagesChanged();
}

QVariantList ProjectPackages::packages() const
{
    QVariantList rows;
    QStringList listed;
    const auto statusOf = [this](const QString &name) -> const PackageStatus * {
        for (const PackageStatus &status : m_status) {
            if (status.name == name)
                return &status;
        }
        return nullptr;
    };
    for (const QVariant &value : m_installed) {
        const QVariantMap package = value.toMap();
        const QString name = package.value(QStringLiteral("name")).toString();
        const QString version = package.value(QStringLiteral("version")).toString();
        QVariantMap row{{QStringLiteral("name"), name},
                        {QStringLiteral("version"), version},
                        {QStringLiteral("status"), QStringLiteral("unknown")},
                        {QStringLiteral("upgrading"), hasUpgradeNotes(name)},
                        {QStringLiteral("repository"), QStringLiteral("https://github.com/") + name}};
        if (const PackageStatus *status = statusOf(name)) {
            row.insert(QStringLiteral("latest"), status->latest);
            row.insert(QStringLiteral("description"), status->description);
            row.insert(QStringLiteral("abandoned"), status->abandoned);
            if (!status->repository.isEmpty())
                row.insert(QStringLiteral("repository"), status->repository);
            // The lock is newer than what Composer last saw: wait for a check
            const bool sameVersion = status->version == version;
            row.insert(QStringLiteral("status"), !sameVersion ? QStringLiteral("unknown")
                                                 : status->status == u"up-to-date" ? QStringLiteral("current")
                                                 : status->status == u"semver-safe-update" ? QStringLiteral("update")
                                                                                           : QStringLiteral("major"));
        } else if (m_check.value(QStringLiteral("state")).toString() == u"done") {
            // Not in composer.json itself (a package's own dependency)
            row.insert(QStringLiteral("status"), QStringLiteral("indirect"));
        }
        rows << row;
        listed << name;
    }
    for (const QString &name : addable()) {
        if (!listed.contains(name))
            rows << QVariantMap{{QStringLiteral("name"), name},
                                {QStringLiteral("status"), QStringLiteral("missing")},
                                {QStringLiteral("canAdd"), true},
                                {QStringLiteral("repository"), QStringLiteral("https://github.com/") + name}};
    }
    return rows;
}

// The analyzer misses the watcher's parent and reports it at the end of
// the function, where a line NOLINT can't reach
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void ProjectPackages::checkUpdates()
{
    if (demo() || m_check.value(QStringLiteral("state")).toString() == u"running")
        return;
    CommandRequest request;
    request.program = Program::Composer;
    request.projectDir = m_projectDir;
    request.arguments = outdatedArguments();
    const auto command = prepareCommand(*m_settings, request);
    if (!command) {
        m_check = {{QStringLiteral("state"), QStringLiteral("failed")},
                   {QStringLiteral("error"), command.error().code},
                   {QStringLiteral("detail"), command.error().detail}};
        emit packagesChanged();
        return;
    }
    const int run = ++m_checkRun;
    m_check.insert(QStringLiteral("state"), QStringLiteral("running"));
    emit packagesChanged();
    emit runChanged(); // busy
    auto *watcher = new QFutureWatcher<Captured>(this); // a child of this, deleted when it finishes
    connect(watcher, &QFutureWatcher<Captured>::finished, this, [this, watcher, run] {
        const Captured out = watcher->result();
        watcher->deleteLater();
        if (run != m_checkRun)
            return;
        takeOutdated(out.out, out.timedOut ? QStringLiteral("timed out") : out.err, out.timedOut ? -1 : out.exitCode);
        emit runChanged();
    });
    watcher->setFuture(QtConcurrent::run([command = command.value()] { return runCaptured(command, kCheckMs); }));
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

void ProjectPackages::takeOutdated(const QString &out, const QString &err, int exitCode)
{
    const auto parsed = parseOutdated(out);
    if (exitCode != 0 || !parsed) {
        // No network, no Packagist: Composer says why on stderr
        QStringList lines = err.split(u'\n');
        lines.removeIf([](const QString &line) { return line.trimmed().isEmpty(); });
        m_check = {{QStringLiteral("state"), QStringLiteral("failed")},
                   {QStringLiteral("error"), QStringLiteral("outdated_failed")},
                   {QStringLiteral("message"), lines.isEmpty() ? QString() : lines.constLast().trimmed()}};
        emit packagesChanged();
        return;
    }
    m_status = parsed.value();
    m_check = {{QStringLiteral("state"), QStringLiteral("done")},
               {QStringLiteral("checkedAt"), QDateTime::currentDateTime().toString(Qt::ISODate)}};
    emit packagesChanged();
}

QVariantMap ProjectPackages::planUpdate(const QString &name) const
{
    for (const QVariant &value : packages()) {
        const QVariantMap row = value.toMap();
        if (row.value(QStringLiteral("name")).toString() != name || row.value(QStringLiteral("latest")).toString().isEmpty())
            continue;
        QStringList commands;
        for (const PackageStep &step : updateSteps(name, row.value(QStringLiteral("latest")).toString()))
            commands << shown(step);
        const QString repository = row.value(QStringLiteral("repository")).toString();
        const QString tag = QStringLiteral("/blob/v") + row.value(QStringLiteral("latest")).toString();
        return {{QStringLiteral("kind"), QStringLiteral("update")},
                {QStringLiteral("name"), name},
                {QStringLiteral("from"), row.value(QStringLiteral("version"))},
                {QStringLiteral("to"), row.value(QStringLiteral("latest"))},
                {QStringLiteral("major"), row.value(QStringLiteral("status")).toString() == u"major"},
                {QStringLiteral("steps"), commands},
                {QStringLiteral("changelog"), repository + tag + QStringLiteral("/CHANGELOG.md")},
                {QStringLiteral("upgrading"), hasUpgradeNotes(name) ? repository + tag + QStringLiteral("/UPGRADING.md") : QString()}};
    }
    return {};
}

QVariantMap ProjectPackages::planAdd(const QString &name) const
{
    const QList<PackageStep> steps = addSteps(name);
    if (steps.isEmpty())
        return {};
    QStringList commands;
    for (const PackageStep &step : steps)
        commands << shown(step);
    return {{QStringLiteral("kind"), QStringLiteral("add")}, {QStringLiteral("name"), name}, {QStringLiteral("steps"), commands}};
}

void ProjectPackages::update(const QString &name)
{
    const QVariantMap plan = planUpdate(name);
    if (plan.isEmpty())
        return;
    start(QStringLiteral("update"), name, plan.value(QStringLiteral("from")).toString(), plan.value(QStringLiteral("to")).toString(),
          updateSteps(name, plan.value(QStringLiteral("to")).toString()));
}

void ProjectPackages::add(const QString &name)
{
    // Only one that isn't there yet
    for (const QVariant &value : m_installed) {
        if (value.toMap().value(QStringLiteral("name")).toString() == name)
            return;
    }
    start(QStringLiteral("add"), name, {}, {}, addSteps(name));
}

void ProjectPackages::start(const QString &kind, const QString &name, const QString &from, const QString &to,
                            const QList<PackageStep> &steps)
{
    if (demo() || m_process || steps.isEmpty())
        return;
    m_steps = steps;
    m_lines.clear();
    QVariantList shownSteps;
    for (const PackageStep &step : steps)
        shownSteps << QVariantMap{{QStringLiteral("shown"), shown(step)}, {QStringLiteral("state"), QStringLiteral("waiting")}};
    m_run = {{QStringLiteral("kind"), kind},        {QStringLiteral("name"), name},
             {QStringLiteral("from"), from},        {QStringLiteral("to"), to},
             {QStringLiteral("steps"), shownSteps}, {QStringLiteral("state"), QStringLiteral("running")}};
    runStep(0);
}

void ProjectPackages::setStepState(int index, const QString &state)
{
    QVariantList steps = m_run.value(QStringLiteral("steps")).toList();
    if (index < 0 || index >= steps.size())
        return;
    QVariantMap step = steps.at(index).toMap();
    step.insert(QStringLiteral("state"), state);
    steps[index] = step;
    m_run.insert(QStringLiteral("steps"), steps);
}

void ProjectPackages::runStep(int index)
{
    if (index >= m_steps.size()) {
        m_run.insert(QStringLiteral("state"), QStringLiteral("done"));
        emit runChanged();
        // composer.lock is new: what is newer is asked again
        emit changedProject();
        checkUpdates();
        return;
    }
    const PackageStep &step = m_steps.at(index);
    CommandRequest request;
    request.program = step.program;
    request.projectDir = m_projectDir;
    request.arguments = step.arguments;
    const auto command = prepareCommand(*m_settings, request);
    setStepState(index, QStringLiteral("running"));
    m_lines << QStringLiteral("› ") + shown(step);
    m_run.insert(QStringLiteral("lines"), m_lines);
    if (!command) {
        setStepState(index, QStringLiteral("failed"));
        m_run.insert(QStringLiteral("state"), QStringLiteral("failed"));
        m_run.insert(QStringLiteral("error"), command.error().code);
        m_run.insert(QStringLiteral("detail"), command.error().detail);
        emit runChanged();
        if (index > 0)
            emit changedProject(); // what ran before changed the project
        return;
    }
    m_errors.clear();
    emit runChanged();
    m_process = startStreaming(
        command.value(), this,
        [this](const OutputLine &line) {
            if (line.error)
                m_errors << line.text;
            m_lines << line.text;
            if (m_lines.size() > kOutputLines)
                m_lines.remove(0, m_lines.size() - kOutputLines);
            m_run.insert(QStringLiteral("lines"), m_lines);
            emit runChanged();
        },
        [this, index](int exitCode, bool crashed) {
            if (m_process)
                m_process->deleteLater();
            m_process = nullptr;
            if (exitCode == 0 && !crashed) {
                setStepState(index, QStringLiteral("done"));
                runStep(index + 1);
                return;
            }
            setStepState(index, QStringLiteral("failed"));
            m_run.insert(QStringLiteral("state"), QStringLiteral("failed"));
            m_run.insert(QStringLiteral("exitCode"), exitCode);
            if (crashed)
                m_run.insert(QStringLiteral("error"), QStringLiteral("step_stopped"));
            m_run.insert(QStringLiteral("message"), errorMessage(m_errors.join(u'\n')));
            emit runChanged();
            // Composer may have changed composer.json before it failed
            emit changedProject();
        });
}

void ProjectPackages::clearRun()
{
    if (m_process)
        return;
    m_run.clear();
    emit runChanged();
}

void ProjectPackages::showDemo(const QVariantMap &demo)
{
    if (demo.contains(QStringLiteral("outdated"))) {
        const QVariantMap raw = demo.value(QStringLiteral("outdated")).toMap();
        takeOutdated(raw.value(QStringLiteral("out")).toString(), raw.value(QStringLiteral("err")).toString(),
                     raw.value(QStringLiteral("code"), 0).toInt());
        const int minutes = demo.value(QStringLiteral("checkedMinutesAgo"), -1).toInt();
        if (minutes >= 0)
            m_check.insert(QStringLiteral("checkedAt"), QDateTime::currentDateTime().addSecs(-60LL * minutes).toString(Qt::ISODate));
    }
    if (demo.contains(QStringLiteral("run")))
        m_run = demo.value(QStringLiteral("run")).toMap();
    emit packagesChanged();
    emit runChanged();
}
