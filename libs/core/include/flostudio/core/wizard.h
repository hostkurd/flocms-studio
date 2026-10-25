#pragma once

#include <flostudio/core/commands.h>
#include <flostudio/core/export.h>
#include <flostudio/core/result.h>

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

// The new project wizard (stage 6, board 6): what the person chose, and the
// steps that make it, in the order Studio runs them:
//
//   composer create-project hostkurd/flocms <name>   (in the parent folder)
//   .env written by Studio: APP_NAME, APP_URL, LANGUAGES, DEFAULT_LANG, DB_*
//   CREATE DATABASE + a user for it, on Studio's own MariaDB (createDatabase)
//   php flo setup --as=<setup>                       (not for a plain website)
//   composer require hostkurd/flocms-api + php flo api:install   (website + API)
//   php flo db:check                                 (MySQL: the database answers)
//   php flo migrate --force                          (makes the SQLite file too)
//   php flo user:create --password-stdin             (with the admin panel)
//
// `composer create-project` without a terminal changes nothing in setup
// (the skeleton's post-create `php flo setup` keeps a plain website), so
// Studio runs setup itself. A step that fails stops the run; it can be run
// again from that step (setup continues where it stopped).
namespace FloStudio::Core {

enum class Setup { SiteAdmin, Site, Admin, ApiAdmin, Api };

struct NewProject
{
    QString name;      // the folder's name: lower case letters, digits, - and _
    QString parentDir; // where the folder is made
    Setup setup = Setup::SiteAdmin;
    bool addApi = false; // the JSON API too (website setups only)
    QStringList languages{QStringLiteral("en")}; // FloCMS codes: en, ar, ku, ...
    QString defaultLanguage = QStringLiteral("en");
    QString appName; // APP_NAME and the site names; the folder's name when empty
    int port = 8000; // APP_URL's port

    bool mysql = false; // else SQLite (storage/database.sqlite)
    QString dbHost = QStringLiteral("127.0.0.1");
    int dbPort = 3306;
    QString dbName;     // MySQL: the database, which must exist
    QString dbUser;
    QString dbPassword; // written to .env (FloCMS reads it there), never shown
    // Studio makes the database and a user just for it on its own MariaDB
    // (MariaDB design note D10): dbName, dbUser (not root) with dbPassword
    // (generated), allowed on that database only
    bool createDatabase = false;

    // The first admin (setups with the admin panel)
    QString adminName;
    QString adminEmail;
    QString adminPassword; // to user:create on its input, never in arguments
};

// site-admin, site, admin, api-admin, api (php flo setup --as=...) and back.
FLOSTUDIOCORE_EXPORT QString setupName(Setup setup);
FLOSTUDIOCORE_EXPORT bool setupFromName(const QString &name, Setup *setup);
FLOSTUDIOCORE_EXPORT bool setupHasAdmin(Setup setup);
FLOSTUDIOCORE_EXPORT bool setupHasSite(Setup setup);

// The folder's name from what the person typed: lower case, spaces and
// other characters become -, nothing at the ends. "My Shop!" → my-shop.
FLOSTUDIOCORE_EXPORT QString projectFolderName(const QString &typed);
// A MySQL database name for a folder name: my-shop → my_shop.
FLOSTUDIOCORE_EXPORT QString databaseNameFor(const QString &folderName);

// What is wrong with the choices, field by field: [{field, code}] with codes
// name_invalid, folder_exists, parent_invalid (not an absolute folder
// path; one that isn't there yet is made), languages_empty,
// default_language, db_name_invalid, db_user_empty, db_user_invalid (a
// user Studio makes: letters, digits and _, not root), db_port_invalid,
// db_host_invalid, db_password_invalid, admin_name_empty, email_invalid,
// password_short (FloCMS wants 8 bytes or more), password_invalid,
// app_name_invalid. Empty when everything can go.
struct FieldProblem
{
    QString field;
    QString code;
    bool operator==(const FieldProblem &) const = default;
};
FLOSTUDIOCORE_EXPORT QList<FieldProblem> checkNewProject(const NewProject &project);

// The folder that will be made.
FLOSTUDIOCORE_EXPORT QString newProjectFolder(const NewProject &project);

// The .env values Studio writes after create-project, in this order.
FLOSTUDIOCORE_EXPORT QList<QPair<QString, QString>> newProjectEnv(const NewProject &project);

enum class StepKind { Command, WriteEnv, CreateDatabase };

struct WizardStep
{
    QString id;   // create, env, db-create, setup, api-require, api-install, db-check, migrate, admin
    StepKind kind = StepKind::Command;
    CommandRequest request; // for a command (prepareCommand turns it into one)
    QString shown;          // the command as the screen shows it, no secrets
    bool operator==(const WizardStep &other) const
    {
        return id == other.id && kind == other.kind && request.program == other.request.program
               && request.projectDir == other.request.projectDir && request.arguments == other.request.arguments
               && request.input == other.request.input && shown == other.shown;
    }
};

// The SQL Studio runs as root on its own server for createDatabase: the
// database (utf8mb4), its user for localhost and 127.0.0.1 with the
// password, and every right on that database only. Holds the password:
// for the client's input, never for the screen (the step's shown text).
FLOSTUDIOCORE_EXPORT QString newProjectDatabaseSql(const NewProject &project);

// Every step for these choices. The admin's password is only in the last
// step's input.
FLOSTUDIOCORE_EXPORT QList<WizardStep> newProjectSteps(const NewProject &project);

} // namespace FloStudio::Core
