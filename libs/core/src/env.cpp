#include <flostudio/core/env.h>

#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <optional>

namespace FloStudio::Core {

namespace {

struct Line
{
    QString key;
    QString value;
};

// PHP's trim(): space, tab, \n, \r, \0 and \v (not other Unicode spaces).
QString phpTrim(const QString &text)
{
    const auto isSpace = [](QChar c) {
        return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == QChar(0) || c == u'\x0B';
    };
    qsizetype start = 0;
    qsizetype end = text.size();
    while (start < end && isSpace(text.at(start)))
        ++start;
    while (end > start && isSpace(text.at(end - 1)))
        --end;
    return text.mid(start, end - start);
}

// One line as both readers split it; nullopt for blank lines, comments and
// lines without "=".
std::optional<Line> splitLine(const QString &raw)
{
    const QString line = phpTrim(raw);
    if (line.isEmpty() || line.at(0) == u'#')
        return std::nullopt;
    const qsizetype equals = line.indexOf(u'=');
    if (equals < 0)
        return std::nullopt;
    return Line{phpTrim(line.left(equals)), phpTrim(line.mid(equals + 1))};
}

// Env::load: (starts with " and ends with ") or (' and '), even a single
// quote character, which becomes the empty value.
QString unquoteForSite(const QString &value)
{
    const bool quoted = (value.startsWith(u'"') && value.endsWith(u'"'))
                        || (value.startsWith(u'\'') && value.endsWith(u'\''));
    return quoted ? value.mid(1, value.size() - 2) : value;
}

// Project::dotenv: at least two characters, the same quote at both ends.
QString unquoteForCommands(const QString &value)
{
    const bool quoted = value.size() >= 2 && (value.at(0) == u'"' || value.at(0) == u'\'')
                        && value.back() == value.at(0);
    return quoted ? value.mid(1, value.size() - 2) : value;
}

// PHP's file() splits on \n; trim() then drops a \r. A lone \r is not a
// line break for either reader.
QStringList fileLines(const QString &text)
{
    return text.split(u'\n');
}

} // namespace

QMap<QString, QString> parseEnv(const QString &text)
{
    QMap<QString, QString> values;
    for (const QString &raw : fileLines(text)) {
        const auto line = splitLine(raw);
        if (line && !values.contains(line->key))
            values.insert(line->key, unquoteForSite(line->value));
    }
    return values;
}

QMap<QString, QString> parseEnvForCommands(const QString &text)
{
    QMap<QString, QString> values;
    for (const QString &raw : fileLines(text)) {
        if (const auto line = splitLine(raw))
            values.insert(line->key, unquoteForCommands(line->value));
    }
    return values;
}

QStringList duplicateKeys(const QString &text)
{
    QStringList seen;
    QStringList twice;
    for (const QString &raw : fileLines(text)) {
        const auto line = splitLine(raw);
        if (!line)
            continue;
        if (seen.contains(line->key)) {
            if (!twice.contains(line->key))
                twice << line->key;
        } else {
            seen << line->key;
        }
    }
    return twice;
}

QString envText(const QByteArray &file)
{
    // QString::fromUtf8 drops a byte order mark; PHP keeps it as part of the
    // first key, so it is kept here too (as U+FEFF, which trim leaves alone).
    if (file.startsWith("\xEF\xBB\xBF"))
        return QChar(0xFEFF) + QString::fromUtf8(file.mid(3));
    return QString::fromUtf8(file);
}

QString readEnvFile(const QString &dir)
{
    QFile file(QDir(dir).filePath(QStringLiteral(".env")));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return envText(file.readAll());
}

bool hasByteOrderMark(const QString &text)
{
    return text.startsWith(QChar(0xFEFF));
}

bool envFlag(const QMap<QString, QString> &env, const QString &key, bool fallback)
{
    const auto it = env.constFind(key);
    if (it == env.constEnd())
        return fallback;
    // PHP's filter_var(FILTER_VALIDATE_BOOLEAN), as core's ErrorHandler
    // reads APP_DEBUG
    const QString value = phpTrim(it.value()).toLower();
    if (value == u"1" || value == u"true" || value == u"on" || value == u"yes")
        return true;
    if (value.isEmpty() || value == u"0" || value == u"false" || value == u"off" || value == u"no")
        return false;
    return fallback;
}

bool isProduction(const QMap<QString, QString> &env)
{
    // Project::environment(): a missing APP_ENV, or "null" (cast to null),
    // is production; an empty one is not
    QString value = env.value(QStringLiteral("APP_ENV"), QStringLiteral("production")).toLower();
    if (value == u"null")
        value = QStringLiteral("production");
    return value == u"production" || value == u"prod" || value == u"live";
}

const QStringList &writableEnvKeys()
{
    static const QStringList keys{
        QStringLiteral("APP_NAME"),    QStringLiteral("APP_ENV"),         QStringLiteral("APP_DEBUG"),
        QStringLiteral("APP_URL"),     QStringLiteral("SITE_NAME"),       QStringLiteral("SITE_NAME_ADMIN"),
        QStringLiteral("LANGUAGES"),   QStringLiteral("DEFAULT_LANG"),    QStringLiteral("DB_TYPE"),
        QStringLiteral("DB_HOST"),     QStringLiteral("DB_PORT"),         QStringLiteral("DB_NAME"),
        QStringLiteral("DB_USERNAME"), QStringLiteral("DB_PASSWORD"),
    };
    return keys;
}

Result<QString> envLineValue(const QString &value)
{
    if (value.contains(u'\n') || value.contains(u'\r'))
        return Error{QStringLiteral("env_value"), value};
    // Plain is safe when trimming keeps it and no reader would take its
    // first and last characters for quotes. Quoting with " always works:
    // both readers remove exactly one outer pair.
    const bool trimmed = phpTrim(value) == value;
    const bool looksQuoted = !value.isEmpty() && (value.at(0) == u'"' || value.at(0) == u'\'')
                             && value.back() == value.at(0);
    if (trimmed && !looksQuoted)
        return value;
    return QStringLiteral("\"%1\"").arg(value);
}

Result<QByteArray> setEnvValues(const QByteArray &file, const QList<QPair<QString, QString>> &values)
{
    // Line by line, as bytes: lines Studio doesn't write stay exactly as
    // they were (whatever their encoding or line ending).
    const bool crlf = file.contains("\r\n");
    QList<QByteArray> lines = file.split('\n');
    if (!lines.isEmpty() && lines.back().isEmpty())
        lines.removeLast(); // the final line break
    const auto keyOf = [](const QByteArray &line) {
        QByteArray text = line;
        if (text.endsWith('\r'))
            text.chop(1);
        const auto split = splitLine(QString::fromUtf8(text));
        return split ? split->key : QString();
    };

    for (const auto &[key, value] : values) {
        if (!writableEnvKeys().contains(key))
            return Error{QStringLiteral("env_key"), key};
        const auto written = envLineValue(value);
        if (!written)
            return written.error();
        const QByteArray newLine = (key + u'=' + written.value()).toUtf8();
        bool placed = false;
        for (qsizetype i = 0; i < lines.size();) {
            if (keyOf(lines.at(i)) != key) {
                ++i;
                continue;
            }
            if (placed) {
                lines.removeAt(i); // a later line of the same key
                continue;
            }
            lines[i] = lines.at(i).endsWith('\r') ? newLine + '\r' : newLine;
            placed = true;
            ++i;
        }
        if (!placed)
            lines << (crlf ? newLine + '\r' : newLine);
    }
    // Each line carries its own \r: the last one only needs its \n (and a
    // \r when the file uses CRLF and its last line had no break at all)
    QByteArray out = lines.join('\n');
    if (crlf && !out.endsWith('\r'))
        out += '\r';
    out += '\n';
    return out;
}

Result<Done> writeEnvValues(const QString &dir, const QList<QPair<QString, QString>> &values)
{
    const QDir project(dir);
    QByteArray text;
    QFile env(project.filePath(QStringLiteral(".env")));
    if (env.exists()) {
        if (!env.open(QIODevice::ReadOnly))
            return Error{QStringLiteral("env_write"), env.errorString()};
        text = env.readAll();
        env.close();
    } else {
        QFile example(project.filePath(QStringLiteral(".env.example")));
        if (example.open(QIODevice::ReadOnly))
            text = example.readAll();
    }
    const auto next = setEnvValues(text, values);
    if (!next)
        return next.error();
    QSaveFile file(env.fileName());
    const QByteArray &data = next.value();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit())
        return Error{QStringLiteral("env_write"), file.errorString()};
    return Done{};
}

} // namespace FloStudio::Core
