#include <flostudio/core/env.h>
#include <flostudio/core/project.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace FloStudio::Core {

namespace {

QJsonObject readJsonObject(const QString &file)
{
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(in.readAll()).object();
}

bool requiresPackage(const QJsonObject &composer, const QString &package)
{
    return composer.value(QStringLiteral("require")).toObject().contains(package)
           || composer.value(QStringLiteral("require-dev")).toObject().contains(package);
}

// A .env value as `php flo` uses it in a string context: Project::env()
// casts "null" to null and "true"/"false" to booleans, and PHP's (string)
// turns those into "", "1" and "". Missing: fallback.
QString commandString(const QMap<QString, QString> &env, const QString &key, const QString &fallback = {})
{
    const auto it = env.constFind(key);
    if (it == env.constEnd())
        return fallback;
    const QString lower = it.value().toLower();
    if (lower == u"null" || lower == u"false")
        return {};
    if (lower == u"true")
        return QStringLiteral("1");
    return it.value();
}

QString canonical(const QString &dir)
{
    const QString path = QFileInfo(dir).canonicalFilePath();
    return path.isEmpty() ? QDir::cleanPath(QFileInfo(dir).absoluteFilePath()) : path;
}

} // namespace

bool isProject(const QString &dir)
{
    const QString file = QDir(dir).filePath(QStringLiteral("composer.json"));
    if (!QFileInfo(file).isFile())
        return false;
    const QJsonObject composer = readJsonObject(file);
    return composer.value(QStringLiteral("name")).toString() == QLatin1String(kSkeletonPackage)
           || requiresPackage(composer, QLatin1String(kCorePackage));
}

QString floLauncher(const QString &dir)
{
    const QDir project(dir);
    for (const QString &relative : {QStringLiteral("flo"), QStringLiteral("vendor/bin/flo")}) {
        if (QFileInfo(project.filePath(relative)).isFile())
            return QDir::toNativeSeparators(project.filePath(relative));
    }
    return {};
}

Home parseHome(const QString &configPhp)
{
    // SetupCommand::currentHome()
    static const QRegularExpression pattern(QStringLiteral(R"(Config::set\('home',\s*'(site|admin|api)'\))"));
    const QString value = pattern.match(configPhp).captured(1);
    if (value == u"admin")
        return Home::Admin;
    if (value == u"api")
        return Home::Api;
    return Home::Site;
}

ProjectType projectType(Home home, bool hasAdmin, bool hasApi)
{
    switch (home) {
    case Home::Api:
        return hasAdmin ? ProjectType::ApiAdmin : ProjectType::Api;
    case Home::Admin:
        return hasApi ? ProjectType::ApiAdmin : ProjectType::Admin;
    case Home::Site:
        break;
    }
    if (hasAdmin && hasApi)
        return ProjectType::SiteAdminApi;
    if (hasAdmin)
        return ProjectType::SiteAdmin;
    if (hasApi)
        return ProjectType::SiteApi;
    return ProjectType::Site;
}

ProjectType projectType(const ProjectSummary &summary)
{
    return projectType(summary.home, summary.hasAdmin, summary.hasApi);
}

QString setupChoice(ProjectType type)
{
    switch (type) {
    case ProjectType::Site:
        return QStringLiteral("site");
    case ProjectType::SiteAdmin:
        return QStringLiteral("site-admin");
    case ProjectType::Admin:
        return QStringLiteral("admin");
    case ProjectType::ApiAdmin:
        return QStringLiteral("api-admin");
    case ProjectType::Api:
        return QStringLiteral("api");
    case ProjectType::SiteApi:
    case ProjectType::SiteAdminApi:
        break;
    }
    return {};
}

QPair<QList<Package>, QList<Package>> lockedPackages(const QByteArray &composerLock)
{
    QList<Package> ours;
    QList<Package> others;
    const QJsonObject lock = QJsonDocument::fromJson(composerLock).object();
    for (const QString &key : {QStringLiteral("packages"), QStringLiteral("packages-dev")}) {
        const QJsonArray list = lock.value(key).toArray();
        for (const auto &item : list) {
            const QJsonObject package = item.toObject();
            const QString name = package.value(QStringLiteral("name")).toString();
            if (name.isEmpty())
                continue;
            QString version = package.value(QStringLiteral("version")).toString();
            if (version.startsWith(u'v') && version.size() > 1 && version.at(1).isDigit())
                version.remove(0, 1);
            (name.startsWith(QStringLiteral("hostkurd/")) ? ours : others).append(Package{name, version});
        }
    }
    const auto byName = [](const Package &a, const Package &b) { return a.name < b.name; };
    std::sort(ours.begin(), ours.end(), byName);
    std::sort(others.begin(), others.end(), byName);
    return {ours, others};
}

DatabaseConfig databaseConfig(const QMap<QString, QString> &env, const QString &dir)
{
    DatabaseConfig db;
    const QString dsn = commandString(env, QStringLiteral("DB_DSN"));
    if (!dsn.isEmpty()) {
        const qsizetype colon = dsn.indexOf(u':');
        db.driver = colon > 0 ? dsn.left(colon).toLower() : QStringLiteral("unknown");
        db.usesDsn = true;
        db.configured = true;
        db.username = commandString(env, QStringLiteral("DB_USERNAME"));
        return db;
    }
    // DB_TYPE ?? DB_CONNECTION ?? mysql: an empty DB_TYPE still counts
    if (env.contains(QStringLiteral("DB_TYPE")) && env.value(QStringLiteral("DB_TYPE")).toLower() != u"null")
        db.driver = commandString(env, QStringLiteral("DB_TYPE")).toLower();
    else
        db.driver = commandString(env, QStringLiteral("DB_CONNECTION"), QStringLiteral("mysql")).toLower();
    db.name = commandString(env, QStringLiteral("DB_NAME"));
    if (db.driver == u"sqlite") {
        // ConnectionFactory::sqlitePath(): relative to the project
        static const QRegularExpression absolute(QStringLiteral(R"(^([A-Za-z]:)?[/\\])"));
        if (!db.name.isEmpty() && db.name != u":memory:" && !absolute.match(db.name).hasMatch())
            db.name = QDir::cleanPath(QDir(dir).filePath(db.name));
        db.configured = !db.name.isEmpty();
        return db;
    }
    db.host = commandString(env, QStringLiteral("DB_HOST"), QStringLiteral("localhost"));
    bool number = false;
    const int port = commandString(env, QStringLiteral("DB_PORT"), QStringLiteral("3306")).toInt(&number);
    db.port = number ? port : 0; // PHP's (int) of a non-number is 0
    db.username = commandString(env, QStringLiteral("DB_USERNAME"));
    db.configured = !db.name.isEmpty() && !db.username.isEmpty();
    return db;
}

ProjectSummary projectSummary(const QString &dir)
{
    ProjectSummary summary;
    summary.path = QDir::toNativeSeparators(QDir::cleanPath(dir));
    summary.folderName = QFileInfo(QDir::cleanPath(dir)).fileName();
    if (summary.folderName.isEmpty())
        summary.folderName = summary.path;
    if (!isProject(dir)) {
        summary.missing = true;
        return summary;
    }
    const QJsonObject composer = readJsonObject(QDir(dir).filePath(QStringLiteral("composer.json")));
    summary.hasAdmin = requiresPackage(composer, QLatin1String(kAdminPackage));
    summary.hasApi = requiresPackage(composer, QLatin1String(kApiPackage));
    QFile config(QDir(dir).filePath(QStringLiteral("config/config.php")));
    if (config.open(QIODevice::ReadOnly))
        summary.home = parseHome(QString::fromUtf8(config.readAll()));
    return summary;
}

Result<ProjectInfo> projectInfo(const QString &dir)
{
    if (!QFileInfo(dir).isDir())
        return Error{QStringLiteral("project_missing"), QDir::toNativeSeparators(dir)};
    if (!isProject(dir))
        return Error{QStringLiteral("not_flocms"), QDir::toNativeSeparators(dir)};

    const QDir project(dir);
    ProjectInfo info;
    info.summary = projectSummary(dir);
    const QString envText = readEnvFile(dir);
    // What the website and the commands that boot the project use; a key
    // written twice is pointed at (duplicateEnvKeys, see env.h).
    const QMap<QString, QString> env = parseEnv(envText);
    info.appName = env.value(QStringLiteral("APP_NAME"));
    info.appEnv = env.value(QStringLiteral("APP_ENV"));
    info.appUrl = env.value(QStringLiteral("APP_URL"));
    info.skeletonVersion = env.value(QStringLiteral("APP_VERSION"));
    info.appDebug = envFlag(env, QStringLiteral("APP_DEBUG"), false);
    info.production = isProduction(env);
    // config.php: explode(',', Env::get('LANGUAGES'))
    for (const QString &language : env.value(QStringLiteral("LANGUAGES")).split(u',')) {
        if (!language.trimmed().isEmpty())
            info.languages << language.trimmed();
    }
    info.defaultLanguage = env.value(QStringLiteral("DEFAULT_LANG"));
    info.database = databaseConfig(env, dir);
    info.duplicateEnvKeys = duplicateKeys(envText);
    info.envByteOrderMark = hasByteOrderMark(envText);
    info.hasEnv = QFileInfo(project.filePath(QStringLiteral(".env"))).isFile();
    info.hasVendor = QFileInfo(project.filePath(QStringLiteral("vendor/autoload.php"))).isFile();
    QFile lock(project.filePath(QStringLiteral("composer.lock")));
    info.hasLock = lock.open(QIODevice::ReadOnly);
    if (info.hasLock) {
        const auto [ours, others] = lockedPackages(lock.readAll());
        info.packages = ours;
        info.otherPackages = others;
    }
    info.launcher = floLauncher(dir);
    return info;
}

QList<ProjectSummary> listProjects(const QStringList &projectFolders, const QStringList &projects)
{
    QStringList seen;
    QList<ProjectSummary> list;
    const auto add = [&](const QString &dir) {
        const QString key = canonical(dir);
        if (seen.contains(key))
            return;
        seen << key;
        list << projectSummary(dir);
    };
    for (const QString &folder : projectFolders) {
        const QFileInfoList entries =
            QDir(folder).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &entry : entries) {
            if (isProject(entry.filePath()))
                add(entry.filePath());
        }
    }
    for (const QString &dir : projects)
        add(dir);
    std::stable_sort(list.begin(), list.end(), [](const ProjectSummary &a, const ProjectSummary &b) {
        const int order = QString::localeAwareCompare(a.folderName, b.folderName);
        return order != 0 ? order < 0 : a.path < b.path;
    });
    return list;
}

} // namespace FloStudio::Core
