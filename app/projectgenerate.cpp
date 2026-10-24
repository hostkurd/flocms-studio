#include "projectgenerate.h"

#include <flostudio/core/commands.h>
#include <flostudio/core/output.h>
#include <flostudio/core/preview.h>
#include <flostudio/core/process.h>

#include <QFutureWatcher>
#include <QProcess>
#include <QTimer>
#include <QtConcurrent>

#include <utility>

using namespace FloStudio::Core;

namespace {

constexpr int kOutputLines = 400;
constexpr int kFileLines = 2000; // a file's lines shown in the preview
int g_timeoutSeconds = 60;

constexpr int kCreateSeconds = 600; // Create: only for one that never ends

struct Copied
{
    QString copy;
    QStringList copied;  // the copy as it was made
    QStringList project; // the project then
    QString error;
    QString detail;
};

struct Compared
{
    QList<FileChange> changes;
    QStringList outside; // what changed in the project while the generator ran
    bool removed = true;
};

// A SQLite file (and its journal) changes when anything reads the database
bool isDatabaseFile(const QString &path)
{
    return path.contains(QStringLiteral(".sqlite")) || path.endsWith(QStringLiteral(".db"));
}

QVariantList fileLines(const FileChange &change)
{
    QVariantList lines;
    const auto add = [&lines](char kind, const QString &text) {
        lines << QVariantMap{{QStringLiteral("kind"), QString(QLatin1Char(kind))}, {QStringLiteral("text"), text}};
    };
    if (change.binary || change.tooBig)
        return lines;
    if (change.kind == u"changed") {
        for (const DiffLine &line : diffLines(QString::fromUtf8(change.before), QString::fromUtf8(change.after)))
            add(line.kind, line.text);
    } else {
        // A new file is all new, a removed one all gone
        const char kind = change.kind == u"added" ? '+' : '-';
        QString text = QString::fromUtf8(change.kind == u"added" ? change.after : change.before);
        if (text.endsWith(u'\n'))
            text.chop(1);
        for (const QString &line : text.split(u'\n'))
            add(kind, line);
    }
    if (lines.size() > kFileLines) {
        const qsizetype left = lines.size() - kFileLines;
        lines.resize(kFileLines);
        add('>', QString::number(left)); // more lines, not shown
    }
    return lines;
}

void addLine(QStringList *lines, const QString &line)
{
    *lines << line;
    if (lines->size() > kOutputLines)
        lines->remove(0, lines->size() - kOutputLines);
}

} // namespace

ProjectGenerate::ProjectGenerate(QString projectDir, const Settings *settings, bool hasAdmin, QObject *parent)
    : QObject(parent)
    , m_projectDir(std::move(projectDir))
    , m_settings(settings)
    , m_hasAdmin(hasAdmin)
    , m_generator(hasAdmin ? Generator::AdminResource : Generator::Controller)
{
}

ProjectGenerate::~ProjectGenerate()
{
    // A copy whose generator still runs: the process ends, and the copy
    // goes now (a copy still being made is removed at the next start)
    m_cancel->store(true);
    for (QProcess *process : {m_previewProcess, m_createProcess}) {
        if (process) {
            QObject::disconnect(process, nullptr, this, nullptr);
            process->kill();
            process->waitForFinished(2000);
        }
    }
    if (!m_copy.isEmpty())
        removePreviewCopy(m_copy);
}

void ProjectGenerate::setTimeoutSeconds(int seconds)
{
    g_timeoutSeconds = seconds;
}

void ProjectGenerate::setHasAdmin(bool hasAdmin)
{
    if (m_hasAdmin == hasAdmin)
        return;
    m_hasAdmin = hasAdmin;
    if (!hasAdmin && generatorNeedsAdmin(m_generator))
        choose(QStringLiteral("controller"));
    emit formChanged();
}

QString ProjectGenerate::generator() const
{
    return generatorName(m_generator);
}

QVariantMap ProjectGenerate::form() const
{
    return {{QStringLiteral("name"), m_form.name},
            {QStringLiteral("table"), m_form.table},
            {QStringLiteral("slug"), m_form.slug},
            {QStringLiteral("formMode"), m_form.formMode.isEmpty() ? QStringLiteral("page") : m_form.formMode},
            {QStringLiteral("migration"), m_form.migration},
            {QStringLiteral("reorderable"), m_form.reorderable},
            {QStringLiteral("pageKind"), m_form.pageKind},
            {QStringLiteral("view"), m_form.view},
            {QStringLiteral("group"), m_form.group},
            {QStringLiteral("resource"), m_form.resource},
            {QStringLiteral("widgetType"), m_form.widgetType},
            {QStringLiteral("controllerKind"), m_form.controllerKind},
            {QStringLiteral("model"), m_form.model},
            {QStringLiteral("apiResource"), m_form.apiResource},
            {QStringLiteral("migrationKind"), m_form.migrationKind},
            {QStringLiteral("force"), m_form.force}};
}

QVariantMap ProjectGenerate::problems() const
{
    QVariantMap shown;
    for (const FieldProblem &problem : checkGenerator(m_generator, m_form)) {
        if (m_touched.contains(problem.field) || m_touched.contains(QStringLiteral("*")))
            shown.insert(problem.field, problem.code);
    }
    return shown;
}

bool ProjectGenerate::ready() const
{
    return (m_hasAdmin || !generatorNeedsAdmin(m_generator)) && checkGenerator(m_generator, m_form).isEmpty();
}

QStringList ProjectGenerate::arguments() const
{
    return generatorArguments(m_generator, m_form);
}

QString ProjectGenerate::command() const
{
    return shownCommand(Program::Flo, arguments());
}

QVariantMap ProjectGenerate::preview() const
{
    QVariantMap preview = m_preview;
    preview.insert(QStringLiteral("current"), m_previewArguments == arguments());
    return preview;
}

bool ProjectGenerate::busy() const
{
    const QString state = m_preview.value(QStringLiteral("state")).toString();
    return state == u"copying" || state == u"running" || m_createProcess != nullptr;
}

void ProjectGenerate::choose(const QString &generator)
{
    Generator chosen{};
    if (!generatorFromName(generator, &chosen) || chosen == m_generator)
        return;
    m_generator = chosen;
    m_form = GeneratorForm{};
    m_touched.clear();
    emit formChanged();
    emit previewChanged();
}

void ProjectGenerate::set(const QString &field, const QVariant &value)
{
    const QString text = value.toString();
    if (field == u"name")
        m_form.name = text;
    else if (field == u"table")
        m_form.table = text;
    else if (field == u"slug")
        m_form.slug = text;
    else if (field == u"formMode")
        m_form.formMode = text;
    else if (field == u"migration")
        m_form.migration = value.toBool();
    else if (field == u"reorderable")
        m_form.reorderable = value.toBool();
    else if (field == u"pageKind")
        m_form.pageKind = text;
    else if (field == u"view")
        m_form.view = value.toBool();
    else if (field == u"group")
        m_form.group = text;
    else if (field == u"resource")
        m_form.resource = text;
    else if (field == u"widgetType")
        m_form.widgetType = text;
    else if (field == u"controllerKind")
        m_form.controllerKind = text;
    else if (field == u"model")
        m_form.model = value.toBool();
    else if (field == u"apiResource")
        m_form.apiResource = value.toBool();
    else if (field == u"migrationKind")
        m_form.migrationKind = text;
    else if (field == u"force")
        m_form.force = value.toBool();
    else
        return;
    emit formChanged();
    emit previewChanged(); // its `current`
}

void ProjectGenerate::touch(const QString &field)
{
    if (!m_touched.contains(field)) {
        m_touched << field;
        emit formChanged();
    }
}

void ProjectGenerate::previewFailed(const QString &error, const QString &detail)
{
    m_preview.insert(QStringLiteral("state"), QStringLiteral("failed"));
    m_preview.insert(QStringLiteral("error"), error);
    m_preview.insert(QStringLiteral("detail"), detail);
    emit previewChanged();
    emit busyChanged();
}

// The analyzer misses the watchers' parent and reports them at the end of
// the function, where a line NOLINT can't reach
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void ProjectGenerate::runPreview()
{
    if (demo() || busy())
        return;
    m_touched << QStringLiteral("*");
    if (!ready()) {
        emit formChanged();
        return;
    }
    const int run = ++m_previewRun;
    m_cancel = std::make_shared<std::atomic<bool>>(false);
    m_previewArguments = arguments();
    m_previewOut.clear();
    m_previewErr.clear();
    m_preview = {{QStringLiteral("state"), QStringLiteral("copying")},
                 {QStringLiteral("shown"), shownCommand(Program::Flo, m_previewArguments)}};
    emit previewChanged();
    emit busyChanged();

    ++m_copying;
    auto *watcher = new QFutureWatcher<Copied>(this); // a child of this, deleted when it finishes
    connect(watcher, &QFutureWatcher<Copied>::finished, this, [this, watcher, run] {
        const Copied copied = watcher->result();
        watcher->deleteLater();
        --m_copying;
        m_copied = copied.copied;
        m_projectBefore = copied.project;
        previewCopied(run, copied.copy, copied.error, copied.detail);
    });
    watcher->setFuture(QtConcurrent::run([project = m_projectDir, cancel = m_cancel] {
        Copied copied;
        auto made = makePreviewCopy(project, previewsFolder(), cancel.get());
        if (!made) {
            copied.error = made.error().code;
            copied.detail = made.error().detail;
            return copied;
        }
        // A generator set to write somewhere absolute would write outside
        // the copy: no preview then
        if (const QString setting = absoluteConfigPath(made.value()); !setting.isEmpty()) {
            removePreviewCopy(made.value());
            copied.error = QStringLiteral("preview_absolute_path");
            copied.detail = setting;
            return copied;
        }
        copied.copy = made.value();
        copied.copied = fileSnapshot(copied.copy);
        copied.project = fileSnapshot(project);
        return copied;
    }));
}

void ProjectGenerate::previewCopied(int run, const QString &copy, const QString &error, const QString &detail)
{
    if (run != m_previewRun) {
        // An older preview's copy: not wanted any more
        if (!copy.isEmpty())
            (void)QtConcurrent::run([copy] { removePreviewCopy(copy); });
        return;
    }
    if (copy.isEmpty()) {
        previewFailed(error, detail);
        return;
    }
    CommandRequest request;
    request.program = Program::Flo;
    request.projectDir = copy;
    request.arguments = m_previewArguments;
    const auto command = prepareCommand(*m_settings, request);
    if (!command) {
        (void)QtConcurrent::run([copy] { removePreviewCopy(copy); });
        previewFailed(command.error().code, command.error().detail);
        return;
    }
    m_copy = copy;
    m_previewTimedOut = false;
    m_preview.insert(QStringLiteral("state"), QStringLiteral("running"));
    emit previewChanged();
    m_previewProcess = startStreaming(
        command.value(), this,
        [this](const OutputLine &line) {
            addLine(line.error ? &m_previewErr : &m_previewOut, line.text);
            QStringList lines = m_preview.value(QStringLiteral("lines")).toStringList();
            addLine(&lines, line.text);
            m_preview.insert(QStringLiteral("lines"), lines);
            emit previewChanged();
        },
        [this, run, copy](int exitCode, bool crashed) { previewRan(run, copy, exitCode, crashed); });
    // A generator that waits on something (a database that doesn't
    // answer); cancelled with the process (it is the timer's context)
    QTimer::singleShot(g_timeoutSeconds * 1000, m_previewProcess, [this, process = m_previewProcess] {
        m_previewTimedOut = true;
        stopProcess(process);
    });
}

void ProjectGenerate::previewRan(int run, const QString &copy, int exitCode, bool crashed)
{
    if (m_previewProcess)
        m_previewProcess->deleteLater();
    m_previewProcess = nullptr;
    const bool ok = exitCode == 0 && !crashed;
    const bool timedOut = m_previewTimedOut;
    // The worker removes the copy from here on
    m_copy.clear();
    // What it wrote, then the copy goes, on a worker (a copy is many files)
    ++m_copying;
    auto *watcher = new QFutureWatcher<Compared>(this); // a child of this, deleted when it finishes
    connect(watcher, &QFutureWatcher<Compared>::finished, this, [this, watcher, run, ok, exitCode, crashed, timedOut] {
        const Compared compared = watcher->result();
        watcher->deleteLater();
        --m_copying;
        if (run != m_previewRun)
            return;
        // Things it can't help: shown with the result
        m_preview.insert(QStringLiteral("outside"), compared.outside);
        m_preview.insert(QStringLiteral("notRemoved"), !compared.removed);
        if (!ok) {
            const QString message = errorMessage(m_previewErr.join(u'\n'));
            m_preview.insert(QStringLiteral("message"), message);
            previewFailed(timedOut ? QStringLiteral("preview_timeout")
                                   : (crashed ? QStringLiteral("step_stopped") : QStringLiteral("step_exit")),
                          timedOut ? QString::number(g_timeoutSeconds) : QString::number(exitCode));
            return;
        }
        QVariantList files;
        for (const FileChange &change : compared.changes)
            files << QVariantMap{{QStringLiteral("path"), change.path},
                                 {QStringLiteral("kind"), change.kind},
                                 {QStringLiteral("binary"), change.binary},
                                 {QStringLiteral("tooBig"), change.tooBig},
                                 {QStringLiteral("lines"), fileLines(change)}};
        m_preview.insert(QStringLiteral("files"), files);
        m_preview.insert(QStringLiteral("state"), QStringLiteral("done"));
        emit previewChanged();
        emit busyChanged();
    });
    watcher->setFuture(QtConcurrent::run([project = m_projectDir, copy, ok, copied = m_copied, before = m_projectBefore] {
        Compared compared;
        if (ok)
            compared.changes = previewChanges(project, copy, copied);
        compared.removed = removePreviewCopy(copy).ok();
        // Written outside the copy (a path FloCMS was given as absolute)?
        // A database file changes whenever something reads it: not counted
        for (const QString &path : changedSince(before, fileSnapshot(project))) {
            if (!isDatabaseFile(path))
                compared.outside << path;
        }
        return compared;
    }));
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

void ProjectGenerate::create()
{
    if (demo() || busy())
        return;
    m_touched << QStringLiteral("*");
    if (!ready()) {
        emit formChanged();
        return;
    }
    CommandRequest request;
    request.program = Program::Flo;
    request.projectDir = m_projectDir;
    request.arguments = arguments();
    const auto command = prepareCommand(*m_settings, request);
    m_createOut.clear();
    m_createErr.clear();
    m_created = {{QStringLiteral("shown"), shownCommand(Program::Flo, request.arguments)}};
    if (!command) {
        m_created.insert(QStringLiteral("state"), QStringLiteral("failed"));
        m_created.insert(QStringLiteral("error"), command.error().code);
        m_created.insert(QStringLiteral("detail"), command.error().detail);
        emit createdChanged();
        return;
    }
    m_created.insert(QStringLiteral("state"), QStringLiteral("running"));
    m_createProcess = startStreaming(
        command.value(), this,
        [this](const OutputLine &line) {
            addLine(line.error ? &m_createErr : &m_createOut, line.text);
            QStringList lines = m_created.value(QStringLiteral("lines")).toStringList();
            addLine(&lines, line.text);
            m_created.insert(QStringLiteral("lines"), lines);
            emit createdChanged();
        },
        [this](int exitCode, bool crashed) {
            if (m_createProcess)
                m_createProcess->deleteLater();
            m_createProcess = nullptr;
            const bool ok = exitCode == 0 && !crashed;
            m_created.insert(QStringLiteral("state"), ok ? QStringLiteral("done") : QStringLiteral("failed"));
            m_created.insert(QStringLiteral("exitCode"), exitCode);
            if (!ok) {
                if (crashed)
                    m_created.insert(QStringLiteral("error"), QStringLiteral("step_stopped"));
                m_created.insert(QStringLiteral("message"), errorMessage(m_createErr.join(u'\n')));
            }
            emit createdChanged();
            emit busyChanged();
            // It wrote into the project: a preview from before is old now
            m_preview = {{QStringLiteral("state"), QStringLiteral("idle")}};
            m_previewArguments.clear();
            emit previewChanged();
            emit changedProject();
        });
    // Only one that never ends (it writes into the project: not stopped
    // half way for being slow); cancelled with the process
    QTimer::singleShot(kCreateSeconds * 1000, m_createProcess, [process = m_createProcess] { stopProcess(process); });
    emit createdChanged();
    emit busyChanged();
}

void ProjectGenerate::showDemo(const QVariantMap &demo)
{
    choose(demo.value(QStringLiteral("generator"), generator()).toString());
    const QVariantMap form = demo.value(QStringLiteral("form")).toMap();
    for (auto it = form.begin(); it != form.end(); ++it)
        set(it.key(), it.value());
    if (demo.contains(QStringLiteral("preview"))) {
        m_preview = demo.value(QStringLiteral("preview")).toMap();
        // A demo's preview gives the files as they are; their lines are
        // worked out as for a real one
        QVariantList files;
        for (const QVariant &value : m_preview.value(QStringLiteral("files")).toList()) {
            QVariantMap file = value.toMap();
            FileChange change;
            change.path = file.value(QStringLiteral("path")).toString();
            change.kind = file.value(QStringLiteral("kind")).toString();
            change.before = file.value(QStringLiteral("before")).toString().toUtf8();
            change.after = file.value(QStringLiteral("after")).toString().toUtf8();
            file.insert(QStringLiteral("lines"), fileLines(change));
            file.remove(QStringLiteral("before"));
            file.remove(QStringLiteral("after"));
            files << file;
        }
        m_preview.insert(QStringLiteral("files"), files);
        m_previewArguments = arguments();
    }
    if (demo.contains(QStringLiteral("created")))
        m_created = demo.value(QStringLiteral("created")).toMap();
    emit previewChanged();
    emit createdChanged();
}
