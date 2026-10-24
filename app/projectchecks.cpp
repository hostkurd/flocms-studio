#include "projectchecks.h"

#include <flostudio/core/commands.h>
#include <flostudio/core/env.h>
#include <flostudio/core/output.h>
#include <flostudio/database/actions.h>

#include <QFutureWatcher>
#include <QHash>
#include <QProcess>
#include <QtConcurrent>

using namespace FloStudio::Core;
namespace Db = FloStudio::Database;

namespace {

constexpr int kOutputLines = 500; // kept of a button's command

QString statusName(CheckStatus status)
{
    switch (status) {
    case CheckStatus::Ok:
        return QStringLiteral("ok");
    case CheckStatus::Warn:
        return QStringLiteral("warn");
    case CheckStatus::Fail:
        return QStringLiteral("fail");
    case CheckStatus::Note:
        break;
    }
    return QStringLiteral("note");
}

QString migrationState(MigrationState state)
{
    switch (state) {
    case MigrationState::Ran:
        return QStringLiteral("ran");
    case MigrationState::Pending:
        return QStringLiteral("pending");
    case MigrationState::Changed:
        return QStringLiteral("changed");
    case MigrationState::FileMissing:
        return QStringLiteral("missing");
    }
    return {};
}

// A command that could not run, or whose output Studio doesn't read
QVariantMap failure(const Captured &out, const QString &code)
{
    const QString message = errorMessage(out.err);
    return {{QStringLiteral("state"), QStringLiteral("failed")},
            {QStringLiteral("error"), out.started ? code : QStringLiteral("command_failed")},
            {QStringLiteral("detail"), message.isEmpty() ? out.err.trimmed().section(u'\n', 0, 0) : message},
            {QStringLiteral("output"), (out.out + out.err).trimmed()}};
}

QString now()
{
    return QDateTime::currentDateTime().toString(Qt::ISODate);
}

} // namespace

ProjectChecks::ProjectChecks(QString projectDir, const Settings *settings, bool hasAdmin, QObject *parent)
    : QObject(parent)
    , m_projectDir(std::move(projectDir))
    , m_settings(settings)
    , m_hasAdmin(hasAdmin)
{
}

ProjectChecks::~ProjectChecks()
{
    // A button's command still running ends with its children (its tracking)
    if (m_running) {
        QObject::disconnect(m_running, nullptr, this, nullptr);
        m_running->kill();
        m_running->waitForFinished(2000);
    }
}

bool ProjectChecks::production() const
{
    return isProduction(parseEnv(readEnvFile(m_projectDir)));
}

// The analyzer misses the watcher's parent (see Studio::refreshTools), and
// reports it in every function that calls capture()
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void ProjectChecks::capture(const QStringList &arguments, Finished onDone)
{
    CommandRequest request;
    request.program = Program::Flo;
    request.projectDir = m_projectDir;
    request.arguments = arguments;
    const auto command = prepareCommand(*m_settings, request);
    if (!command) {
        Captured out;
        out.err = command.error().code + QStringLiteral(": ") + command.error().detail;
        out.exitCode = -1;
        onDone(out);
        return;
    }
    auto *watcher = new QFutureWatcher<Captured>(this); // a child of this, deleted when it finishes
    connect(watcher, &QFutureWatcher<Captured>::finished, this, [watcher, onDone = std::move(onDone)] {
        const Captured out = watcher->result();
        watcher->deleteLater();
        onDone(out);
    });
    watcher->setFuture(QtConcurrent::run([command = command.value()] { return runCaptured(command, 120000); }));
}

void ProjectChecks::refreshAll()
{
    runDoctor();
    refreshDatabase();
}

void ProjectChecks::runDoctor()
{
    if (demo() || m_doctor.value(QStringLiteral("state")).toString() == u"running")
        return;
    m_doctor.insert(QStringLiteral("state"), QStringLiteral("running"));
    if (m_hasAdmin)
        m_adminDoctor.insert(QStringLiteral("state"), QStringLiteral("running"));
    emit doctorChanged();

    capture({QStringLiteral("doctor"), QStringLiteral("--format=json")}, [this](const Captured &out) { takeDoctor(out); });
    if (!m_hasAdmin)
        return;
    capture({QStringLiteral("admin:doctor")}, [this](const Captured &out) { takeAdminDoctor(out); });
}

void ProjectChecks::takeDoctor(const Captured &out)
{
    {
        // Exit code 1 when a check fails: the report is still there
        const auto report = parseDoctor(out.out);
        if (!report) {
            m_doctor = failure(out, QStringLiteral("doctor_unreadable"));
        } else {
            QVariantList checks;
            int ok = 0;
            for (const DoctorCheck &check : report->checks) {
                ok += check.status == CheckStatus::Ok ? 1 : 0;
                checks << QVariantMap{{QStringLiteral("status"), statusName(check.status)},
                                      {QStringLiteral("check"), check.check},
                                      {QStringLiteral("detail"), check.detail}};
            }
            m_doctor = {{QStringLiteral("state"), QStringLiteral("done")},
                        {QStringLiteral("failed"), report->failed},
                        {QStringLiteral("warnings"), report->warnings},
                        {QStringLiteral("ok"), ok},
                        {QStringLiteral("checks"), checks}};
        }
        m_doctor.insert(QStringLiteral("checkedAt"), now());
        emit doctorChanged();
    }
}

void ProjectChecks::takeAdminDoctor(const Captured &out)
{
    {
        const auto report = parseAdminDoctor(out.out, out.err);
        if (!report) {
            m_adminDoctor = failure(out, QStringLiteral("doctor_unreadable"));
        } else {
            QVariantList sections;
            for (const ReportSection &section : report->sections) {
                QVariantList lines;
                for (const ReportLine &line : section.lines)
                    lines << QVariantMap{{QStringLiteral("status"), statusName(line.status)}, {QStringLiteral("text"), line.text}};
                sections << QVariantMap{{QStringLiteral("title"), section.title}, {QStringLiteral("lines"), lines}};
            }
            m_adminDoctor = {{QStringLiteral("state"), QStringLiteral("done")},
                             {QStringLiteral("heading"), report->heading},
                             {QStringLiteral("problems"), report->problems},
                             {QStringLiteral("warnings"), report->warnings},
                             {QStringLiteral("summary"), report->summary},
                             {QStringLiteral("sections"), sections},
                             {QStringLiteral("output"), (out.out + out.err).trimmed()}};
        }
        m_adminDoctor.insert(QStringLiteral("checkedAt"), now());
        emit doctorChanged();
    }
}

void ProjectChecks::refreshDatabase()
{
    if (demo())
        return;
    if (m_databaseReads > 0) {
        m_refreshAgain = true; // once these end
        return;
    }
    m_database.insert(QStringLiteral("state"), QStringLiteral("running"));
    m_migrations.insert(QStringLiteral("state"), QStringLiteral("running"));
    emit databaseChanged();
    m_databaseReads = m_hasAdmin ? 3 : 2;
    auto adminUrls = std::make_shared<QHash<QString, QString>>(); // table → /admin/... (admin:list)
    auto tables = std::make_shared<QVariantList>();
    const auto done = [this, adminUrls, tables] {
        if (--m_databaseReads > 0)
            return;
        // The links once both db:show and admin:list are in
        linkTables(*tables, *adminUrls);
        emit databaseChanged();
        if (std::exchange(m_refreshAgain, false))
            refreshDatabase();
    };

    capture(Db::showArguments(), [this, tables, done](const Captured &out) {
        takeShow(out, tables.get());
        done();
    });
    capture(Db::statusArguments(), [this, done](const Captured &out) {
        takeStatus(out);
        done();
    });
    if (m_hasAdmin) {
        capture({QStringLiteral("admin:list")}, [adminUrls, done](const Captured &out) {
            takeAdminList(out, adminUrls.get());
            done();
        });
    }
}

// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

void ProjectChecks::takeShow(const Captured &out, QVariantList *tables)
{
    {
        const auto show = parseDatabaseShow(out.out);
        if (!show) {
            m_database = failure(out, QStringLiteral("database_unreadable"));
        } else {
            for (const DatabaseTable &table : show->tables)
                *tables << QVariantMap{{QStringLiteral("name"), table.name}, {QStringLiteral("rows"), table.rows}};
            m_database = {{QStringLiteral("state"), QStringLiteral("done")},
                          {QStringLiteral("driver"), show->driver},
                          {QStringLiteral("server"), show->server},
                          {QStringLiteral("host"), show->host},
                          {QStringLiteral("port"), show->port},
                          {QStringLiteral("database"), show->database},
                          {QStringLiteral("username"), show->username}};
        }
    }
}

void ProjectChecks::takeStatus(const Captured &out)
{
    {
        const auto rows = parseMigrateStatus(out.out);
        if (!rows) {
            m_migrations = failure(out, QStringLiteral("migrations_unreadable"));
        } else {
            QVariantList list;
            // Newest first, as on board 5: what waits is on top
            for (auto it = rows->crbegin(); it != rows->crend(); ++it)
                list << QVariantMap{{QStringLiteral("name"), it->name},
                                    {QStringLiteral("package"), it->package},
                                    {QStringLiteral("batch"), it->batch},
                                    {QStringLiteral("state"), migrationState(it->state)}};
            m_migrations = {{QStringLiteral("state"), QStringLiteral("done")},
                            {QStringLiteral("pending"), pendingMigrations(rows.value())},
                            {QStringLiteral("rows"), list}};
        }
    }
}

void ProjectChecks::takeAdminList(const Captured &out, QHash<QString, QString> *adminUrls)
{
    if (const auto resources = parseAdminList(out.out)) {
        for (const AdminResource &resource : resources.value()) {
            if (!resource.table.isEmpty() && resource.url.startsWith(u'/'))
                adminUrls->insert(resource.table, resource.url);
        }
    }
}

void ProjectChecks::linkTables(const QVariantList &tables, const QHash<QString, QString> &adminUrls)
{
    QVariantList linked;
    for (const QVariant &row : tables) {
        QVariantMap table = row.toMap();
        table.insert(QStringLiteral("adminUrl"), adminUrls.value(table.value(QStringLiteral("name")).toString()));
        linked << table;
    }
    if (m_database.value(QStringLiteral("state")).toString() == u"done")
        m_database.insert(QStringLiteral("tables"), linked);
}

QVariantMap ProjectChecks::plan(const QString &action) const
{
    Db::Action which{};
    if (!Db::actionFromName(action, &which))
        return {};
    const bool onProduction = !demo() && production();
    const Db::ActionPlan plan = Db::planAction(which, onProduction);
    const QString confirm = plan.confirm == Db::Confirm::Ask           ? QStringLiteral("ask")
                            : plan.confirm == Db::Confirm::TypeDatabase ? QStringLiteral("typeDatabase")
                                                                        : QStringLiteral("none");
    return {{QStringLiteral("action"), action},
            {QStringLiteral("command"), QStringLiteral("php flo ") + plan.arguments.join(u' ')},
            {QStringLiteral("confirm"), confirm},
            {QStringLiteral("force"), plan.force},
            {QStringLiteral("production"), onProduction},
            {QStringLiteral("database"), m_database.value(QStringLiteral("database"))},
            {QStringLiteral("pending"), m_migrations.value(QStringLiteral("pending"), 0)}};
}

void ProjectChecks::runAction(const QString &action, const QString &typed)
{
    Db::Action which{};
    if (demo() || busy() || !Db::actionFromName(action, &which))
        return;
    const Db::ActionPlan plan = Db::planAction(which, production());
    // Fresh drops every table: only with the database's name typed
    if (plan.confirm == Db::Confirm::TypeDatabase
        && !Db::typedNameMatches(typed, m_database.value(QStringLiteral("database")).toString()))
        return;
    stream(plan.arguments, action, plan.changesData);
}

void ProjectChecks::runQuick(const QString &command)
{
    static const QStringList quick{QStringLiteral("cache:clear"), QStringLiteral("view:clear"), QStringLiteral("optimize"),
                                   QStringLiteral("optimize:clear")};
    if (demo() || busy() || !quick.contains(command))
        return;
    stream({command}, command, false);
}

void ProjectChecks::clearOutput()
{
    if (busy())
        return;
    m_output.clear();
    m_lines.clear();
    emit outputChanged();
}

void ProjectChecks::stream(const QStringList &arguments, const QString &command, bool changesData)
{
    CommandRequest request;
    request.program = Program::Flo;
    request.projectDir = m_projectDir;
    request.arguments = arguments;
    const auto prepared = prepareCommand(*m_settings, request);
    m_lines.clear();
    m_output = {{QStringLiteral("command"), command}};
    if (!prepared) {
        m_output.insert(QStringLiteral("state"), QStringLiteral("failed"));
        m_output.insert(QStringLiteral("error"), prepared.error().code);
        m_output.insert(QStringLiteral("message"), prepared.error().detail);
        emit outputChanged();
        return;
    }
    m_output.insert(QStringLiteral("shown"), prepared->shown);
    m_output.insert(QStringLiteral("state"), QStringLiteral("running"));
    auto errors = std::make_shared<QStringList>();
    auto outs = std::make_shared<QStringList>();
    m_running = startStreaming(
        prepared.value(), this,
        [this, errors, outs](const OutputLine &line) {
            (line.error ? *errors : *outs) << line.text;
            m_lines << line.text;
            if (m_lines.size() > kOutputLines)
                m_lines.remove(0, m_lines.size() - kOutputLines);
            m_output.insert(QStringLiteral("lines"), m_lines);
            emit outputChanged();
        },
        [this, errors, outs, changesData](int exitCode, bool crashed) {
            m_running = nullptr;
            const bool ok = exitCode == 0 && !crashed;
            m_output.insert(QStringLiteral("state"), ok ? QStringLiteral("done") : QStringLiteral("failed"));
            m_output.insert(QStringLiteral("exitCode"), exitCode);
            m_output.insert(QStringLiteral("message"), ok ? successMessage(outs->join(u'\n')) : errorMessage(errors->join(u'\n')));
            m_output.insert(QStringLiteral("lines"), m_lines);
            emit outputChanged();
            if (changesData) {
                refreshDatabase();
                runDoctor(); // its Migrations check
                emit changedData();
            }
        });
    emit outputChanged();
}

void ProjectChecks::showDemo(const QVariantMap &demo)
{
    // The demo holds what the commands print (the real captured output),
    // read here as a real run reads it
    const auto captured = [&demo](const QString &key) {
        const QVariantMap raw = demo.value(key).toMap();
        Captured out;
        out.started = true;
        out.exitCode = raw.value(QStringLiteral("code"), 0).toInt();
        out.out = raw.value(QStringLiteral("out")).toString();
        out.err = raw.value(QStringLiteral("err")).toString();
        return out;
    };
    if (demo.contains(QStringLiteral("doctor")))
        takeDoctor(captured(QStringLiteral("doctor")));
    if (demo.contains(QStringLiteral("adminDoctor")))
        takeAdminDoctor(captured(QStringLiteral("adminDoctor")));
    QVariantList tables;
    QHash<QString, QString> adminUrls;
    if (demo.contains(QStringLiteral("show")))
        takeShow(captured(QStringLiteral("show")), &tables);
    if (demo.contains(QStringLiteral("status")))
        takeStatus(captured(QStringLiteral("status")));
    if (demo.contains(QStringLiteral("adminList")))
        takeAdminList(captured(QStringLiteral("adminList")), &adminUrls);
    linkTables(tables, adminUrls);
    m_output = demo.value(QStringLiteral("output")).toMap();
    // "checkedMinutesAgo": a time relative to now
    const int minutes = demo.value(QStringLiteral("checkedMinutesAgo"), -1).toInt();
    if (minutes >= 0) {
        const QString when = QDateTime::currentDateTime().addSecs(-60LL * minutes).toString(Qt::ISODate);
        m_doctor.insert(QStringLiteral("checkedAt"), when);
        m_adminDoctor.insert(QStringLiteral("checkedAt"), when);
    }
    emit doctorChanged();
    emit databaseChanged();
    emit outputChanged();
}
