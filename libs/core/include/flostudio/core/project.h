#pragma once

#include <flostudio/core/export.h>
#include <flostudio/core/result.h>

#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>

namespace FloStudio::Core {

// Reading a FloCMS project from its files: no PHP needed, so the project
// list and the Overview appear at once. Anything that needs PHP runs
// `php flo` (commands.h).

inline constexpr auto kSkeletonPackage = "hostkurd/flocms";
inline constexpr auto kCorePackage = "hostkurd/flocms-core";
inline constexpr auto kAdminPackage = "hostkurd/flocms-admin";
inline constexpr auto kApiPackage = "hostkurd/flocms-api";

// What the home page (/) shows: Config::set('home', ...) in
// config/config.php, which `php flo setup` writes.
enum class Home { Site, Admin, Api };

// The project as `php flo setup` names it, plus a website that added the
// API (composer require hostkurd/flocms-api, php flo api:install).
enum class ProjectType {
    Site,         // setup: site
    SiteAdmin,    // setup: site-admin
    SiteApi,      // a website with the API
    SiteAdminApi, // a website with the admin panel and the API
    Admin,        // setup: admin
    ApiAdmin,     // setup: api-admin
    Api,          // setup: api
};

struct Package
{
    QString name;
    QString version; // without a leading "v": 2.9.0, dev-main

    bool operator==(const Package &) const = default;
};

// The database settings the `php flo` commands use (flocms-cli's
// ConnectionFactory), never the password.
struct DatabaseConfig
{
    QString driver;      // mysql, sqlite, or a DB_DSN's driver
    QString host;        // mysql: DB_HOST, default localhost
    int port = 3306;     // mysql: DB_PORT
    QString name;        // DB_NAME; for sqlite the file, made absolute
    QString username;    // DB_USERNAME
    bool usesDsn = false;
    // What `php flo` needs before it connects: a DSN, or DB_NAME (and for
    // mysql DB_USERNAME). Not configured: Studio offers the database form.
    bool configured = false;

    bool operator==(const DatabaseConfig &) const = default;
};

struct ProjectSummary
{
    QString path;       // the folder, as Studio knows it
    QString folderName; // the folder's own name
    Home home = Home::Site;
    bool hasAdmin = false; // composer.json requires flocms-admin
    bool hasApi = false;   // composer.json requires flocms-api
    bool missing = false;  // the folder is gone or no longer a FloCMS project

    bool operator==(const ProjectSummary &) const = default;
};

struct ProjectInfo
{
    ProjectSummary summary;
    QString appName;         // APP_NAME
    QString appEnv;          // APP_ENV ("" when not set: FloCMS then uses production)
    QString appUrl;          // APP_URL
    QString skeletonVersion; // APP_VERSION: the skeleton the project started from
    bool appDebug = false;   // APP_DEBUG
    bool production = true;  // isProduction(): migrate and friends need --force
    QStringList languages;   // LANGUAGES
    QString defaultLanguage; // DEFAULT_LANG
    DatabaseConfig database;
    QList<Package> packages;      // hostkurd/* from composer.lock
    QList<Package> otherPackages; // everything else in composer.lock
    QStringList duplicateEnvKeys; // see env.h
    bool hasEnv = false;
    bool hasVendor = false;  // vendor/autoload.php: composer install has run
    bool hasLock = false;
    QString launcher;        // the flo launcher to run, empty when there is none yet

    bool operator==(const ProjectInfo &) const = default;
};

// flocms-cli's Project::isProjectRoot: composer.json is the skeleton or
// requires flocms-core (require or require-dev).
FLOSTUDIOCORE_EXPORT bool isProject(const QString &dir);

// The launcher `php flo` runs: the project's own `flo` file, or
// vendor/bin/flo from flocms-cli. Empty when neither exists.
FLOSTUDIOCORE_EXPORT QString floLauncher(const QString &dir);

// The same match `php flo setup` uses: the first
// Config::set('home', 'site'|'admin'|'api'); anything else is the site.
FLOSTUDIOCORE_EXPORT Home parseHome(const QString &configPhp);

FLOSTUDIOCORE_EXPORT ProjectType projectType(Home home, bool hasAdmin, bool hasApi);
FLOSTUDIOCORE_EXPORT ProjectType projectType(const ProjectSummary &summary);

// The --as value of `php flo setup` for a type; empty for SiteApi and
// SiteAdminApi, which setup has no choice for.
FLOSTUDIOCORE_EXPORT QString setupChoice(ProjectType type);

// (hostkurd/* packages, the others) from a composer.lock's text, each
// sorted by name. Invalid JSON gives two empty lists.
FLOSTUDIOCORE_EXPORT QPair<QList<Package>, QList<Package>> lockedPackages(const QByteArray &composerLock);

// The database settings from .env values (as parseEnvForCommands gives
// them) for the project in dir (a relative sqlite file is made absolute).
FLOSTUDIOCORE_EXPORT DatabaseConfig databaseConfig(const QMap<QString, QString> &env, const QString &dir);

FLOSTUDIOCORE_EXPORT ProjectSummary projectSummary(const QString &dir);

// Everything the Overview shows. Error codes: project_missing (no such
// folder), not_flocms (not a FloCMS project).
FLOSTUDIOCORE_EXPORT Result<ProjectInfo> projectInfo(const QString &dir);

// Every project Studio knows: the sub-folders of projectFolders that are
// FloCMS projects, and each of projects (shown as missing when gone). One
// entry per folder, sorted by folder name for the current language.
FLOSTUDIOCORE_EXPORT QList<ProjectSummary> listProjects(const QStringList &projectFolders,
                                                       const QStringList &projects);

} // namespace FloStudio::Core
