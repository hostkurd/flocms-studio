#include "logmodel.h"

#include <flostudio/core/logs.h>

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QLocale>

using namespace FloStudio::Core;

namespace {

// "Sat Oct 24 12:53:57 2026" (the built-in server) → 12:53:57
QString serverTime(const QString &time)
{
    const QStringList parts = time.split(u' ', Qt::SkipEmptyParts);
    return parts.size() >= 4 ? parts.at(3) : QString();
}

// "2026-10-24 09:53:57" (core's Logger) → 09:53:57
QString appTime(const QString &time)
{
    return time.section(u' ', 1, 1);
}

QString levelName(AppLogLevel level)
{
    switch (level) {
    case AppLogLevel::Debug:
        return QStringLiteral("debug");
    case AppLogLevel::Info:
        return QStringLiteral("info");
    case AppLogLevel::Notice:
        return QStringLiteral("notice");
    case AppLogLevel::Warning:
        return QStringLiteral("warning");
    case AppLogLevel::Error:
        return QStringLiteral("error");
    case AppLogLevel::Critical:
        return QStringLiteral("critical");
    case AppLogLevel::Alert:
        return QStringLiteral("alert");
    case AppLogLevel::Emergency:
        return QStringLiteral("emergency");
    case AppLogLevel::Unknown:
        break;
    }
    return {};
}

} // namespace

LogModel::LogModel(QObject *parent, int limit)
    : QAbstractListModel(parent)
    , m_limit(std::max(100, limit))
{
    m_batch.setInterval(50);
    m_batch.setSingleShot(true);
    connect(&m_batch, &QTimer::timeout, this, &LogModel::flush);
}

int LogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_visible.size());
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_visible.size())
        return {};
    const Entry &entry = m_entries.at(m_visible.at(index.row()));
    switch (role) {
    case TextRole:
    case Qt::DisplayRole:
        return entry.text;
    case SourceRole:
        return entry.source;
    case LevelRole:
        return entry.level;
    case TimeRole:
        return shownTime(entry);
    case ContinuationRole:
        return entry.continuation;
    default:
        return {};
    }
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    return {{TextRole, "text"}, {SourceRole, "source"}, {LevelRole, "level"}, {TimeRole, "time"}, {ContinuationRole, "continuation"}};
}

LogModel::Entry LogModel::serverEntry(const QString &line, bool *skip)
{
    const ServerLine read = readServerLine(line);
    Entry entry;
    entry.source = QStringLiteral("server");
    entry.text = read.text.isEmpty() ? line : read.text;
    entry.time = serverTime(read.time);
    *skip = read.kind == ServerLineKind::Connection || line.trimmed().isEmpty();
    switch (read.kind) {
    case ServerLineKind::Request:
        entry.level = isProblem(read) ? QStringLiteral("problem")
                      : read.status >= 400 ? QStringLiteral("warning")
                                           : QStringLiteral("request");
        break;
    case ServerLineKind::Exception:
    case ServerLineKind::PhpError:
        entry.level = isProblem(read) ? QStringLiteral("problem") : QStringLiteral("quiet");
        break;
    case ServerLineKind::PhpWarning:
        entry.level = QStringLiteral("warning");
        break;
    case ServerLineKind::Trace:
        entry.continuation = true;
        entry.level = QStringLiteral("quiet"); // takes the level of the line before (add())
        break;
    case ServerLineKind::Started:
    case ServerLineKind::Connection:
    case ServerLineKind::Other:
        entry.level = QStringLiteral("info");
        break;
    }
    return entry;
}

LogModel::Entry LogModel::appLogEntry(const QString &line)
{
    const AppLogLine read = readAppLogLine(line);
    Entry entry;
    entry.source = QStringLiteral("app");
    if (read.continuation) {
        entry.text = line;
        entry.continuation = true;
        entry.level = QStringLiteral("quiet");
        return entry;
    }
    entry.time = appTime(read.time);
    entry.appStamp = QDateTime::fromString(read.time, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    entry.text = levelName(read.level) + QStringLiteral(": ") + read.message
                 + (read.context.isEmpty() ? QString() : u' ' + read.context);
    if (isProblem(read))
        entry.level = QStringLiteral("problem");
    else if (read.level == AppLogLevel::Warning)
        entry.level = QStringLiteral("warning");
    else if (read.clientErrorStatus > 0 || read.level == AppLogLevel::Debug)
        entry.level = QStringLiteral("quiet"); // a page not found is not the site's problem
    else
        entry.level = QStringLiteral("info");
    return entry;
}

void LogModel::addServerLine(const QString &line)
{
    bool skip = false;
    Entry entry = serverEntry(line, &skip);
    if (!skip)
        add(std::move(entry));
}

void LogModel::addAppLogLine(const QString &line)
{
    if (!line.trimmed().isEmpty())
        add(appLogEntry(line));
}

void LogModel::addStudioLine(const QString &text, const QString &level)
{
    Entry entry;
    entry.source = QStringLiteral("studio");
    entry.text = text;
    entry.level = level;
    entry.time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    add(std::move(entry));
}

void LogModel::add(Entry entry)
{
    // A stack trace line belongs to the line before it from the same log
    if (entry.continuation) {
        const Entry *before = nullptr;
        for (auto it = m_pending.crbegin(); it != m_pending.crend() && !before; ++it)
            before = it->source == entry.source ? &*it : nullptr;
        for (auto it = m_entries.crbegin(); it != m_entries.crend() && !before; ++it)
            before = it->source == entry.source ? &*it : nullptr;
        if (before)
            entry.level = before->level;
    }
    m_pending.append(std::move(entry));
    if (!m_batch.isActive())
        m_batch.start();
}

void LogModel::flush()
{
    m_batch.stop();
    if (m_pending.isEmpty())
        return;
    const QList<Entry> pending = std::exchange(m_pending, {});
    // Over the limit: the oldest go, a tenth at a time (one reset, not one per line)
    if (m_entries.size() + pending.size() > m_limit) {
        beginResetModel();
        for (const Entry &entry : pending)
            m_entries.append(entry);
        const qsizetype drop = m_entries.size() - m_limit + m_limit / 10;
        if (drop > 0)
            m_entries.remove(0, std::min(drop, m_entries.size()));
        m_problems = static_cast<int>(std::count_if(m_entries.cbegin(), m_entries.cend(), [](const Entry &e) {
            return e.level == u"problem" && !e.continuation;
        }));
        m_visible.clear();
        for (qsizetype i = 0; i < m_entries.size(); ++i) {
            if (accepts(m_entries.at(i)))
                m_visible.append(i);
        }
        endResetModel();
        emit countsChanged();
        return;
    }
    QList<qsizetype> added;
    for (const Entry &entry : pending) {
        m_entries.append(entry);
        if (entry.level == u"problem" && !entry.continuation)
            ++m_problems;
        if (accepts(entry))
            added.append(m_entries.size() - 1);
    }
    if (!added.isEmpty()) {
        const int first = static_cast<int>(m_visible.size());
        beginInsertRows({}, first, first + static_cast<int>(added.size()) - 1);
        m_visible.append(added);
        endInsertRows();
    }
    emit countsChanged();
}

bool LogModel::accepts(const Entry &entry) const
{
    if (m_source != u"all" && entry.source != m_source && entry.source != u"studio")
        return false;
    if (m_problemsOnly && entry.level != u"problem")
        return false;
    return m_search.isEmpty() || entry.text.contains(m_search, Qt::CaseInsensitive);
}

void LogModel::refilter()
{
    beginResetModel();
    m_visible.clear();
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (accepts(m_entries.at(i)))
            m_visible.append(i);
    }
    endResetModel();
    emit filterChanged();
    emit countsChanged();
}

void LogModel::setSource(const QString &source)
{
    if (source == m_source)
        return;
    m_source = source;
    refilter();
}

void LogModel::setProblemsOnly(bool only)
{
    if (only == m_problemsOnly)
        return;
    m_problemsOnly = only;
    refilter();
}

void LogModel::setSearch(const QString &search)
{
    if (search == m_search)
        return;
    m_search = search;
    refilter();
}

QString LogModel::text() const
{
    QStringList lines;
    lines.reserve(m_visible.size());
    for (const qsizetype index : m_visible) {
        const Entry &entry = m_entries.at(index);
        const QString time = shownTime(entry);
        lines << (time.isEmpty() ? entry.text : time + u' ' + entry.text);
    }
    return lines.join(u'\n');
}

void LogModel::copy() const
{
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text());
}

QString LogModel::shownTime(const Entry &entry) const
{
    if (!m_appOffsetKnown || !entry.appStamp.isValid())
        return entry.time;
    QDateTime stamp = entry.appStamp;
    stamp.setOffsetFromUtc(m_appOffset);
    return stamp.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
}

void LogModel::setAppTimeOffset(int secondsEastOfUtc)
{
    if (m_appOffsetKnown && m_appOffset == secondsEastOfUtc)
        return;
    m_appOffsetKnown = true;
    m_appOffset = secondsEastOfUtc;
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1), {TimeRole});
}

void LogModel::clear()
{
    beginResetModel();
    m_entries.clear();
    m_visible.clear();
    m_pending.clear();
    m_problems = 0;
    endResetModel();
    emit countsChanged();
}
