#include <flostudio/core/floserver.h>
#include <flostudio/core/tools.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVersionNumber>

#include <algorithm>

namespace FloStudio::Core {

namespace {

constexpr int kSchema = 1;
constexpr qint64 kIntegrationLimit = qint64(1024) * 1024;

// An absolute folder from an environment variable, else nothing
QString envFolder(const char *name)
{
    const QString value = qEnvironmentVariable(name);
    return !value.isEmpty() && QDir::isAbsolutePath(value) ? QDir::cleanPath(value) : QString();
}

QString text(const QJsonObject &json, const char *key)
{
    const QJsonValue value = json.value(QLatin1String(key));
    return value.isString() ? value.toString() : QString();
}

bool newerFirst(const QString &a, const QString &b)
{
    return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
}

} // namespace

QString floServerDataFolder()
{
    if (const QString custom = envFolder("FLOSERVER_DATA_DIR"); !custom.isEmpty())
        return QDir::toNativeSeparators(custom);
#if defined(Q_OS_WIN)
    const QString local = envFolder("LOCALAPPDATA");
    return local.isEmpty() ? QString() : QDir::toNativeSeparators(local + QStringLiteral("/FloServer"));
#elif defined(Q_OS_MACOS)
    const QString home = envFolder("HOME");
    return home.isEmpty() ? QString() : home + QStringLiteral("/Library/Application Support/FloServer");
#else
    if (const QString xdg = envFolder("XDG_DATA_HOME"); !xdg.isEmpty())
        return xdg + QStringLiteral("/floserver");
    const QString home = envFolder("HOME");
    return home.isEmpty() ? QString() : home + QStringLiteral("/.local/share/floserver");
#endif
}

std::optional<FloServerInfo> parseFloServerIntegration(const QByteArray &json)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonObject root = document.object();
    const QJsonValue schema = root.value(QStringLiteral("schema"));
    if (!schema.isDouble() || schema.toDouble() != kSchema)
        return std::nullopt;
    const QJsonValue shims = root.value(QStringLiteral("shims"));
    const QJsonValue cli = root.value(QStringLiteral("cli"));
    if (!shims.isString() || !QDir::isAbsolutePath(shims.toString()) || (!cli.isUndefined() && !cli.isString()))
        return std::nullopt;
    FloServerInfo info;
    info.version = text(root, "floserver_version");
    info.shims = QDir::toNativeSeparators(shims.toString());
    // A relative `flo` would mean wherever Studio runs: not used
    if (QDir::isAbsolutePath(cli.toString()))
        info.cli = QDir::toNativeSeparators(cli.toString());
    for (const auto &item : root.value(QStringLiteral("runtimes")).toArray()) {
        const QJsonObject runtime = item.toObject();
        const QString name = text(runtime, "name");
        const QString version = text(runtime, "version");
        if (!name.isEmpty() && !version.isEmpty())
            info.runtimes << FloServerRuntime{name, version};
    }
    static const QRegularExpression series(QStringLiteral(R"(^\d+\.\d+$)"));
    for (const auto &item : root.value(QStringLiteral("databases")).toArray()) {
        const QJsonObject database = item.toObject();
        const QString name = text(database, "series");
        const QJsonValue port = database.value(QStringLiteral("port"));
        const int number = port.isDouble() ? port.toInt(0) : 0;
        if (!series.match(name).hasMatch() || number < 1 || number > 65535)
            continue;
        info.databases << FloServerDatabase{name, text(database, "version"), number};
    }
    return info;
}

std::optional<FloServerInfo> readFloServerIntegration(const QString &dataFolder)
{
    if (dataFolder.isEmpty())
        return std::nullopt;
    QFile file(QDir(dataFolder).filePath(QStringLiteral("integration.json")));
    if (file.size() > kIntegrationLimit || !file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return parseFloServerIntegration(file.read(kIntegrationLimit));
}

std::optional<FloServerInfo> floServerInUse(const Settings &settings)
{
    if (!settings.useFloServer)
        return std::nullopt;
    return readFloServerIntegration(floServerDataFolder());
}

QString floServerShimPath(const FloServerInfo &info, const QString &tool)
{
#ifdef Q_OS_WIN
    const QString name = tool + QStringLiteral(".exe");
#else
    const QString &name = tool;
#endif
    return QDir::toNativeSeparators(QDir(QDir::fromNativeSeparators(info.shims)).filePath(name));
}

QString floServerShim(const FloServerInfo &info, const QString &tool)
{
    const QString path = floServerShimPath(info, tool);
    const QFileInfo file(path);
    return file.isFile() && file.isExecutable() ? path : QString();
}

QString commandSearchPath(const Settings &settings)
{
    QStringList folders = toolSearchPath();
    if (const auto info = floServerInUse(settings)) {
        folders.removeAll(info->shims);
        folders.prepend(info->shims);
    }
    return folders.join(QDir::listSeparator());
}

QStringList floServerVersions(const FloServerInfo &info, const QString &runtime)
{
    QStringList versions;
    for (const FloServerRuntime &item : info.runtimes) {
        if (item.name == runtime && !versions.contains(item.version))
            versions << item.version;
    }
    std::sort(versions.begin(), versions.end(), newerFirst);
    return versions;
}

} // namespace FloStudio::Core
