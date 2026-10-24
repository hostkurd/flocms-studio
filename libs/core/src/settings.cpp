#include <flostudio/core/settings.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace FloStudio::Core {

namespace {

QString settingsFileName()
{
    return QStringLiteral("settings.json");
}

const QStringList &knownKeys()
{
    static const QStringList keys{
        QStringLiteral("language"),       QStringLiteral("theme"),          QStringLiteral("phpPath"),
        QStringLiteral("composerPath"),   QStringLiteral("editorCommand"),  QStringLiteral("projectFolders"),
        QStringLiteral("projects"),       QStringLiteral("serverPort"),     QStringLiteral("serverNetwork"),
    };
    return keys;
}

QStringList cleanList(const QStringList &list)
{
    QStringList out;
    for (const QString &item : list) {
        const QString trimmed = item.trimmed();
        if (!trimmed.isEmpty() && !out.contains(trimmed))
            out << trimmed;
    }
    return out;
}

QStringList stringList(const QJsonValue &value)
{
    QStringList out;
    const QJsonArray array = value.toArray();
    for (const auto &item : array) {
        if (item.isString())
            out << item.toString();
    }
    return out;
}

} // namespace

QString settingsFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

Settings normalized(Settings settings)
{
    const Settings defaults;
    static const QStringList languages{QStringLiteral("system"), QStringLiteral("en"), QStringLiteral("ar"),
                                       QStringLiteral("ckb")};
    static const QStringList themes{QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")};
    if (!languages.contains(settings.language))
        settings.language = defaults.language;
    if (!themes.contains(settings.theme))
        settings.theme = defaults.theme;
    if (settings.serverPort < 1 || settings.serverPort > 65535)
        settings.serverPort = defaults.serverPort;
    settings.editorCommand = settings.editorCommand.trimmed();
    if (settings.editorCommand.isEmpty())
        settings.editorCommand = defaults.editorCommand;
    settings.phpPath = settings.phpPath.trimmed();
    settings.composerPath = settings.composerPath.trimmed();
    settings.projectFolders = cleanList(settings.projectFolders);
    settings.projects = cleanList(settings.projects);
    return settings;
}

QJsonObject toJson(const Settings &settings)
{
    QJsonObject json = settings.unknown;
    json.insert(QStringLiteral("language"), settings.language);
    json.insert(QStringLiteral("theme"), settings.theme);
    json.insert(QStringLiteral("phpPath"), settings.phpPath);
    json.insert(QStringLiteral("composerPath"), settings.composerPath);
    json.insert(QStringLiteral("editorCommand"), settings.editorCommand);
    json.insert(QStringLiteral("projectFolders"), QJsonArray::fromStringList(settings.projectFolders));
    json.insert(QStringLiteral("projects"), QJsonArray::fromStringList(settings.projects));
    json.insert(QStringLiteral("serverPort"), settings.serverPort);
    json.insert(QStringLiteral("serverNetwork"), settings.serverNetwork);
    return json;
}

Settings settingsFromJson(const QJsonObject &json)
{
    Settings settings;
    const auto text = [&json](const char *key, QString &out) {
        const QJsonValue value = json.value(QLatin1String(key));
        if (value.isString())
            out = value.toString();
    };
    text("language", settings.language);
    text("theme", settings.theme);
    text("phpPath", settings.phpPath);
    text("composerPath", settings.composerPath);
    text("editorCommand", settings.editorCommand);
    settings.projectFolders = stringList(json.value(QStringLiteral("projectFolders")));
    settings.projects = stringList(json.value(QStringLiteral("projects")));
    const QJsonValue port = json.value(QStringLiteral("serverPort"));
    if (port.isDouble())
        settings.serverPort = port.toInt(0); // 0 (not a whole number) becomes the default
    const QJsonValue network = json.value(QStringLiteral("serverNetwork"));
    if (network.isBool())
        settings.serverNetwork = network.toBool();
    for (auto it = json.begin(); it != json.end(); ++it) {
        if (!knownKeys().contains(it.key()))
            settings.unknown.insert(it.key(), it.value());
    }
    return normalized(settings);
}

Result<Settings> loadSettings(const QString &dir)
{
    QFile file(QDir(dir).filePath(settingsFileName()));
    if (!file.exists())
        return Settings{};
    if (!file.open(QIODevice::ReadOnly))
        return Error{QStringLiteral("settings_read"), file.errorString()};
    const QByteArray data = file.readAll();
    file.close();
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        // Kept aside under a name of its own (an earlier one is never replaced)
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        QString broken = QDir(dir).filePath(QStringLiteral("settings.broken-%1.json").arg(stamp));
        for (int n = 2; QFile::exists(broken); ++n)
            broken = QDir(dir).filePath(QStringLiteral("settings.broken-%1-%2.json").arg(stamp).arg(n));
        if (!QFile::rename(file.fileName(), broken))
            return Error{QStringLiteral("settings_read"), file.fileName()};
        return Settings{};
    }
    return settingsFromJson(document.object());
}

Result<Done> saveSettings(const Settings &settings, const QString &dir)
{
    if (!QDir().mkpath(dir))
        return Error{QStringLiteral("settings_write"), dir};
    QSaveFile file(QDir(dir).filePath(settingsFileName()));
    if (!file.open(QIODevice::WriteOnly))
        return Error{QStringLiteral("settings_write"), file.errorString()};
    const QByteArray data = QJsonDocument(toJson(normalized(settings))).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit())
        return Error{QStringLiteral("settings_write"), file.errorString()};
    return Done{};
}

} // namespace FloStudio::Core
