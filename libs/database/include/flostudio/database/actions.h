#pragma once

#include <flostudio/database/export.h>

#include <QString>
#include <QStringList>

// The Database tab's commands (stage 5, design note D5): reading the
// database (db:show, migrate:status) and changing it (migrate, roll back,
// fresh, seed), all through Core's allowlist.
//
// Studio runs every command with --no-interaction, so flocms-cli can't ask
// its own questions: a command that would ask on a production site
// (APP_ENV=production, or no APP_ENV) refuses without --force. Studio asks
// first, in its own window, then passes --force. planAction() says what to
// ask and what to run.
namespace FloStudio::Database {

enum class Action {
    Migrate,  // php flo migrate: the pending migrations
    Rollback, // php flo migrate:rollback: the last batch
    Fresh,    // php flo migrate:fresh: drops every table, migrates again
    Seed,     // php flo db:seed: database/seeders/DatabaseSeeder
};

enum class Confirm {
    None,         // runs at once
    Ask,          // a Yes / Cancel question
    TypeDatabase, // the person types the database's name first
};

struct ActionPlan
{
    QStringList arguments; // for Core's CommandRequest
    Confirm confirm = Confirm::None;
    bool force = false;       // --force is passed (the confirmation stands in for flocms-cli's)
    bool changesData = false; // tables or rows change: the screens refresh afterwards
    bool operator==(const ActionPlan &) const = default;
};

// What to ask and run for an action on a site. `production` is Core's
// isProduction() of the project's .env.
//   Migrate:  asks on production only.
//   Rollback: always asks (it undoes changes and may drop data).
//   Fresh:    always types the database name (it drops every table).
//   Seed:     always asks (it adds rows).
FLOSTUDIODATABASE_EXPORT ActionPlan planAction(Action action, bool production);

// The read-only commands: db:show --format=json, migrate:status.
FLOSTUDIODATABASE_EXPORT QStringList showArguments();
FLOSTUDIODATABASE_EXPORT QStringList statusArguments();

// The command as the screen names it ("php flo migrate").
FLOSTUDIODATABASE_EXPORT QString actionCommand(Action action);

// "migrate", "rollback", "fresh", "seed" and back (QML passes names).
FLOSTUDIODATABASE_EXPORT QString actionName(Action action);
FLOSTUDIODATABASE_EXPORT bool actionFromName(const QString &name, Action *action);

// The database name typed for Fresh matches the one configured: exactly,
// apart from spaces around it. An empty name never matches.
FLOSTUDIODATABASE_EXPORT bool typedNameMatches(const QString &typed, const QString &database);

} // namespace FloStudio::Database
