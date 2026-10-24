#pragma once

#include <flostudio/core/commands.h>
#include <flostudio/core/export.h>

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

// The Packages tab (stage 8, board 8): what Composer says about the
// project's packages, and the commands that update one or add the API or
// the admin panel. Versions installed come from composer.lock (project.h,
// D4); what is newer from `composer outdated` (it asks Packagist).
//
// Update:  composer require "<package>:^<latest>" -W
//          php flo admin:install | api:install   (flocms-admin, flocms-api:
//                                                 they only add what is missing)
//          php flo migrate --force               (the update's migrations)
// Add:     composer require hostkurd/flocms-api | flocms-admin
//          php flo api:install | admin:install
//          php flo migrate --force
namespace FloStudio::Core {

// composer outdated --direct --all --format=json: every package
// composer.json requires, newer or not.
FLOSTUDIOCORE_EXPORT QStringList outdatedArguments();

struct PackageStatus
{
    QString name;        // hostkurd/flocms-core
    QString version;     // 2.8.0 (without Composer's "v")
    QString latest;      // 2.9.0
    QString status;      // up-to-date, semver-safe-update (within the
                         // constraint), update-possible (a new major)
    QString description;
    QString repository;  // https://github.com/hostkurd/flocms-core, else empty
    bool abandoned = false;
    bool operator==(const PackageStatus &) const = default;
};

// The JSON `composer outdated --format=json` prints ({"installed": [...]}).
FLOSTUDIOCORE_EXPORT std::optional<QList<PackageStatus>> parseOutdated(const QString &json);

// "v2.9.0" → "2.9.0".
FLOSTUDIOCORE_EXPORT QString plainVersion(const QString &version);

// The package's own GitHub repository from Composer's source URL
// (https://github.com/<owner>/<repo>/tree/<tag>), for hostkurd's packages
// only (they have a CHANGELOG.md); empty for others.
FLOSTUDIOCORE_EXPORT QString hostkurdRepository(const QString &source);
// The packages that have an UPGRADING.md too.
FLOSTUDIOCORE_EXPORT bool hasUpgradeNotes(const QString &package);

struct PackageStep
{
    QString id; // require, install, migrate
    Program program = Program::Composer;
    QStringList arguments;
    bool operator==(const PackageStep &) const = default;
};

// Updating a package to `latest`.
FLOSTUDIOCORE_EXPORT QList<PackageStep> updateSteps(const QString &package, const QString &latest);
// Adding hostkurd/flocms-api or hostkurd/flocms-admin; empty for others.
FLOSTUDIOCORE_EXPORT QList<PackageStep> addSteps(const QString &package);

} // namespace FloStudio::Core
