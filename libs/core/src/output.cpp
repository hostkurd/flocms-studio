#include <flostudio/core/output.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace FloStudio::Core {

namespace {

QStringList outputLines(const QString &text)
{
    return QString(text).remove(u'\r').split(u'\n');
}

std::optional<QJsonObject> jsonObject(const QString &json)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    return document.object();
}

// A title line is underlined by a line of = (the heading) or - (a section).
bool isUnderline(const QString &line, QChar mark)
{
    const QString trimmed = line.trimmed();
    return !trimmed.isEmpty() && std::all_of(trimmed.begin(), trimmed.end(), [mark](QChar c) { return c == mark; });
}

// "| a | b |" -> {a, b}
QStringList tableCells(const QString &line)
{
    QStringList cells = line.trimmed().split(u'|');
    if (cells.size() < 3)
        return {};
    cells.removeFirst();
    cells.removeLast();
    for (QString &cell : cells)
        cell = cell.trimmed();
    return cells;
}

bool isBorder(const QString &line)
{
    const QString trimmed = line.trimmed();
    return trimmed.startsWith(u'+') && trimmed.endsWith(u'+')
           && std::all_of(trimmed.begin(), trimmed.end(), [](QChar c) { return c == u'+' || c == u'-'; });
}

} // namespace

std::optional<DoctorReport> parseDoctor(const QString &json)
{
    const auto object = jsonObject(json);
    if (!object || !object->value(QStringLiteral("checks")).isArray())
        return std::nullopt;
    DoctorReport report;
    report.ok = object->value(QStringLiteral("ok")).toBool();
    report.failed = object->value(QStringLiteral("failed")).toInt();
    report.warnings = object->value(QStringLiteral("warnings")).toInt();
    const QJsonArray checks = object->value(QStringLiteral("checks")).toArray();
    for (const auto &item : checks) {
        const QJsonObject check = item.toObject();
        const QString status = check.value(QStringLiteral("status")).toString();
        DoctorCheck entry;
        entry.status = status == u"ok"     ? CheckStatus::Ok
                       : status == u"warn" ? CheckStatus::Warn
                       : status == u"fail" ? CheckStatus::Fail
                                           : CheckStatus::Note;
        entry.check = check.value(QStringLiteral("check")).toString();
        entry.detail = check.value(QStringLiteral("detail")).toVariant().toString();
        report.checks << entry;
    }
    return report;
}

std::optional<DatabaseShow> parseDatabaseShow(const QString &json)
{
    const auto object = jsonObject(json);
    if (!object || !object->value(QStringLiteral("tables")).isArray())
        return std::nullopt;
    const QJsonObject connection = object->value(QStringLiteral("connection")).toObject();
    DatabaseShow show;
    show.driver = connection.value(QStringLiteral("driver")).toString();
    show.host = connection.value(QStringLiteral("host")).toString();
    show.port = connection.value(QStringLiteral("port")).toVariant().toInt();
    show.database = connection.value(QStringLiteral("database")).toString();
    show.username = connection.value(QStringLiteral("username")).toString();
    show.server = connection.value(QStringLiteral("server")).toString();
    const QJsonArray tables = object->value(QStringLiteral("tables")).toArray();
    for (const auto &item : tables) {
        const QJsonObject table = item.toObject();
        const QJsonValue rows = table.value(QStringLiteral("rows"));
        show.tables << DatabaseTable{table.value(QStringLiteral("table")).toString(),
                                     rows.isDouble() ? static_cast<qint64>(rows.toDouble()) : -1};
    }
    return show;
}

std::optional<QList<Migration>> parseMigrateStatus(const QString &out)
{
    if (out.contains(QStringLiteral("No migrations found")))
        return QList<Migration>{};
    const QList<TextTable> tables = parseTables(out);
    const auto table = std::find_if(tables.begin(), tables.end(), [](const TextTable &t) {
        return t.headers == QStringList{QStringLiteral("Migration"), QStringLiteral("Package"),
                                        QStringLiteral("Batch"), QStringLiteral("Status")};
    });
    if (table == tables.end())
        return std::nullopt;
    QList<Migration> migrations;
    for (const QStringList &row : table->rows) {
        Migration migration;
        migration.name = row.value(0);
        migration.package = row.value(1);
        migration.batch = row.value(2).toInt();
        const QString status = row.value(3);
        // MigrateStatusCommand: Ran, Ran (changed since), Ran (file missing), Pending
        migration.state = status.contains(QStringLiteral("file missing")) ? MigrationState::FileMissing
                          : status.contains(QStringLiteral("changed"))    ? MigrationState::Changed
                          : status.startsWith(QStringLiteral("Ran"))      ? MigrationState::Ran
                                                                          : MigrationState::Pending;
        migrations << migration;
    }
    return migrations;
}

int pendingMigrations(const QList<Migration> &migrations)
{
    return static_cast<int>(std::count_if(migrations.begin(), migrations.end(),
                                          [](const Migration &m) { return m.state == MigrationState::Pending; }));
}

std::optional<AdminDoctorReport> parseAdminDoctor(const QString &out, const QString &err)
{
    AdminDoctorReport report;
    ReportSection section;
    bool started = false;
    const QStringList lines = outputLines(out);
    static const QRegularExpression mark(QStringLiteral(R"(^\s*(✓|!|✗)\s+(.*)$)"));
    static const QRegularExpression done(QStringLiteral(R"(^\s*\[OK\]\s+(.*)$)"));
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        if (line.trimmed().isEmpty())
            continue;
        const QString next = lines.value(i + 1);
        if (isUnderline(next, u'=')) {
            report.heading = line.trimmed();
            started = true;
            ++i;
            continue;
        }
        if (isUnderline(next, u'-')) {
            if (!section.title.isEmpty() || !section.lines.isEmpty())
                report.sections << section;
            section = ReportSection{line.trimmed(), {}};
            started = true;
            ++i;
            continue;
        }
        if (const auto ok = done.match(line); ok.hasMatch()) {
            report.summary = ok.captured(1);
            continue;
        }
        if (const auto marked = mark.match(line); marked.hasMatch()) {
            const QString symbol = marked.captured(1);
            const CheckStatus status = symbol == u"✓" ? CheckStatus::Ok
                                       : symbol == u"!" ? CheckStatus::Warn
                                                        : CheckStatus::Fail;
            report.warnings += status == CheckStatus::Warn ? 1 : 0;
            report.problems += status == CheckStatus::Fail ? 1 : 0;
            section.lines << ReportLine{status, marked.captured(2)};
            continue;
        }
        section.lines << ReportLine{CheckStatus::Note, line.trimmed()};
    }
    if (!section.title.isEmpty() || !section.lines.isEmpty())
        report.sections << section;
    if (const QString error = errorMessage(err); !error.isEmpty())
        report.summary = error;
    if (!started)
        return std::nullopt;
    return report;
}

QList<TextTable> parseTables(const QString &out)
{
    QList<TextTable> tables;
    const QStringList lines = outputLines(out);
    QString title;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);
        if (isUnderline(lines.value(i + 1), u'-') || isUnderline(lines.value(i + 1), u'=')) {
            if (!isBorder(lines.value(i + 1)))
                title = line.trimmed();
            continue;
        }
        if (!isBorder(line))
            continue;
        // +---+ / | headers | / +---+ / | row | ... / +---+
        const QStringList headers = tableCells(lines.value(i + 1));
        if (headers.isEmpty() || !isBorder(lines.value(i + 2)))
            continue;
        TextTable table{title, headers, {}};
        qsizetype row = i + 3;
        for (; row < lines.size() && !isBorder(lines.at(row)); ++row) {
            QStringList cells = tableCells(lines.at(row));
            if (cells.isEmpty())
                break;
            while (cells.size() < headers.size())
                cells << QString();
            table.rows << cells;
        }
        tables << table;
        title.clear();
        i = row; // the closing border
    }
    return tables;
}

std::optional<QList<AdminResource>> parseAdminList(const QString &out)
{
    const QList<TextTable> tables = parseTables(out);
    for (const TextTable &table : tables) {
        const qsizetype name = table.headers.indexOf(QStringLiteral("Resource"));
        const qsizetype url = table.headers.indexOf(QStringLiteral("URL"));
        const qsizetype tableColumn = table.headers.indexOf(QStringLiteral("Table"));
        if (name < 0 || url < 0 || tableColumn < 0)
            continue;
        const qsizetype permissions = table.headers.indexOf(QStringLiteral("Permissions"));
        const qsizetype className = table.headers.indexOf(QStringLiteral("Class"));
        QList<AdminResource> resources;
        for (const QStringList &row : table.rows) {
            resources << AdminResource{row.value(name), row.value(url), row.value(tableColumn),
                                       permissions >= 0 ? row.value(permissions) : QString(),
                                       className >= 0 ? row.value(className) : QString()};
        }
        return resources;
    }
    return std::nullopt;
}

QString errorMessage(const QString &err)
{
    const QStringList lines = outputLines(err);
    static const QRegularExpression block(QStringLiteral(R"(^ \[ERROR\] (.*)$)"));
    static const QRegularExpression single(QStringLiteral(R"(^ ERROR  (.*)$)"));
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (const auto match = block.match(lines.at(i)); match.hasMatch()) {
            // ErrorRenderer::block(): later lines are indented by the width
            // of " [ERROR] "
            QStringList parts{match.captured(1).trimmed()};
            const QString indent(9, u' ');
            for (qsizetype j = i + 1; j < lines.size() && lines.at(j).startsWith(indent)
                                      && !lines.at(j).trimmed().isEmpty();
                 ++j)
                parts << lines.at(j).trimmed();
            return parts.join(u' ');
        }
        if (const auto match = single.match(lines.at(i)); match.hasMatch())
            return match.captured(1).trimmed();
    }
    return {};
}

QString successMessage(const QString &out)
{
    static const QRegularExpression ok(QStringLiteral(R"(^\[OK\] (.*)$)"));
    QString message;
    for (const QString &line : outputLines(out)) {
        if (const auto match = ok.match(line); match.hasMatch())
            message = match.captured(1).trimmed();
    }
    return message;
}

QString serveUrl(const QString &line)
{
    static const QRegularExpression serving(QStringLiteral(R"(^\[OK\] Serving .* at (https?://\S+) \(Ctrl\+C to stop\)\s*$)"));
    return serving.match(line).captured(1);
}

} // namespace FloStudio::Core
