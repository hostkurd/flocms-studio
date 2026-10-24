#include <flostudio/core/packages.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace FloStudio::Core {

namespace {

const char *const kApi = "hostkurd/flocms-api";
const char *const kAdmin = "hostkurd/flocms-admin";

PackageStep step(const char *id, Program program, const QStringList &arguments)
{
    return PackageStep{QLatin1String(id), program, arguments};
}

// What the package's installer is, when it has one
QString installer(const QString &package)
{
    if (package == QLatin1String(kAdmin))
        return QStringLiteral("admin:install");
    if (package == QLatin1String(kApi))
        return QStringLiteral("api:install");
    return {};
}

} // namespace

QStringList outdatedArguments()
{
    return {QStringLiteral("outdated"), QStringLiteral("--direct"), QStringLiteral("--all"), QStringLiteral("--format=json")};
}

QString plainVersion(const QString &version)
{
    return version.startsWith(u'v') && version.size() > 1 && version.at(1).isDigit() ? version.mid(1) : version;
}

QString hostkurdRepository(const QString &source)
{
    static const QRegularExpression github(QStringLiteral(R"(^https://github\.com/(hostkurd/[A-Za-z0-9_.-]+?)(?:\.git)?(?:/|$))"));
    const auto match = github.match(source);
    return match.hasMatch() ? QStringLiteral("https://github.com/") + match.captured(1) : QString();
}

bool hasUpgradeNotes(const QString &package)
{
    return package == QLatin1String(kAdmin);
}

std::optional<QList<PackageStatus>> parseOutdated(const QString &json)
{
    // Composer may print a warning line before the JSON
    const qsizetype start = json.indexOf(u'{');
    if (start < 0)
        return std::nullopt;
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(json.mid(start).toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonValue installed = document.object().value(QStringLiteral("installed"));
    if (!installed.isArray())
        return std::nullopt;
    QList<PackageStatus> packages;
    const QJsonArray list = installed.toArray();
    for (const auto &value : list) {
        const QJsonObject object = value.toObject();
        PackageStatus package;
        package.name = object.value(QStringLiteral("name")).toString();
        if (package.name.isEmpty())
            continue;
        package.version = plainVersion(object.value(QStringLiteral("version")).toString());
        package.latest = plainVersion(object.value(QStringLiteral("latest")).toString());
        package.status = object.value(QStringLiteral("latest-status")).toString();
        package.description = object.value(QStringLiteral("description")).toString();
        package.repository = hostkurdRepository(object.value(QStringLiteral("source")).toString());
        if (package.repository.isEmpty())
            package.repository = hostkurdRepository(object.value(QStringLiteral("homepage")).toString());
        // false, or the package that replaces it
        package.abandoned = object.value(QStringLiteral("abandoned")).toBool() || object.value(QStringLiteral("abandoned")).isString();
        packages << package;
    }
    return packages;
}

QList<PackageStep> updateSteps(const QString &package, const QString &latest)
{
    QList<PackageStep> steps{step("require", Program::Composer,
                                  {QStringLiteral("require"), package + QStringLiteral(":^") + plainVersion(latest),
                                   QStringLiteral("-W")})};
    if (const QString install = installer(package); !install.isEmpty())
        steps << step("install", Program::Flo, {install});
    steps << step("migrate", Program::Flo, {QStringLiteral("migrate"), QStringLiteral("--force")});
    return steps;
}

QList<PackageStep> addSteps(const QString &package)
{
    const QString install = installer(package);
    if (install.isEmpty())
        return {};
    return {step("require", Program::Composer, {QStringLiteral("require"), package}),
            step("install", Program::Flo, {install}),
            step("migrate", Program::Flo, {QStringLiteral("migrate"), QStringLiteral("--force")})};
}

} // namespace FloStudio::Core
