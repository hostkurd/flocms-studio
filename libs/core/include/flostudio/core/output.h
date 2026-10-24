#pragma once

#include <flostudio/core/export.h>

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace FloStudio::Core {

// Reading what `php flo` prints (D4): JSON where the command has it
// (doctor, db:show), the fixed text formats of flocms-cli otherwise
// (migrate:status, admin:doctor, admin:list). When a format doesn't match,
// the readers give nullopt and the screens show the command's output as
// it is: nothing is ever hidden. tests/fixtures/flo holds real output of
// each command (tools/capture_fixtures.py makes it).

enum class CheckStatus { Ok, Warn, Fail, Note };

// php flo doctor --format=json (exit code 1 when a check fails)
struct DoctorCheck
{
    CheckStatus status = CheckStatus::Note;
    QString check;  // "PHP version", "Extension gd", "Migrations"
    QString detail; // "8.3.6", "13 pending: run php flo migrate"
    bool operator==(const DoctorCheck &) const = default;
};

struct DoctorReport
{
    bool ok = false;
    int failed = 0;
    int warnings = 0;
    QList<DoctorCheck> checks;
    bool operator==(const DoctorReport &) const = default;
};

FLOSTUDIOCORE_EXPORT std::optional<DoctorReport> parseDoctor(const QString &json);

// php flo db:show --format=json
struct DatabaseTable
{
    QString name;
    qint64 rows = -1; // -1 when the command could not count them
    bool operator==(const DatabaseTable &) const = default;
};

struct DatabaseShow
{
    QString driver;
    QString host;
    int port = 0;
    QString database;
    QString username;
    QString server; // 10.11.14-MariaDB-0ubuntu0.24.04.1
    QList<DatabaseTable> tables;
    bool operator==(const DatabaseShow &) const = default;
};

FLOSTUDIOCORE_EXPORT std::optional<DatabaseShow> parseDatabaseShow(const QString &json);

// php flo migrate:status
enum class MigrationState { Ran, Pending, Changed, FileMissing };

struct Migration
{
    QString name;
    QString package; // "app" for database/migrations, else the package
    int batch = 0;   // 0 when it has not run
    MigrationState state = MigrationState::Pending;
    bool operator==(const Migration &) const = default;
};

// The rows; an empty list for "No migrations found ..."; nullopt when the
// output is something else (an error).
FLOSTUDIOCORE_EXPORT std::optional<QList<Migration>> parseMigrateStatus(const QString &out);
FLOSTUDIOCORE_EXPORT int pendingMigrations(const QList<Migration> &migrations);

// php flo admin:doctor: a heading, sections of ✓ / ! / ✗ lines and notes,
// and a summary ("[OK] Everything looks good." on stdout, or
// " ERROR  1 problem(s), 0 warning(s)." on stderr).
struct ReportLine
{
    CheckStatus status = CheckStatus::Note;
    QString text;
    bool operator==(const ReportLine &) const = default;
};

struct ReportSection
{
    QString title; // empty for lines before the first section
    QList<ReportLine> lines;
    bool operator==(const ReportSection &) const = default;
};

struct AdminDoctorReport
{
    QString heading; // "flocms-admin 0.39.3"
    QList<ReportSection> sections;
    QString summary;
    int problems = 0;
    int warnings = 0;
    bool operator==(const AdminDoctorReport &) const = default;
};

// nullopt when there is neither a heading nor a section.
FLOSTUDIOCORE_EXPORT std::optional<AdminDoctorReport> parseAdminDoctor(const QString &out, const QString &err);

// Every ASCII table (+----+ borders) in a command's output, with the
// section title above it (admin:list prints several).
struct TextTable
{
    QString title;
    QStringList headers;
    QList<QStringList> rows;
    bool operator==(const TextTable &) const = default;
};

FLOSTUDIOCORE_EXPORT QList<TextTable> parseTables(const QString &out);

// php flo admin:list: the admin panel's resources.
struct AdminResource
{
    QString name;        // Posts
    QString url;         // /admin/posts
    QString table;       // posts
    QString permissions; // content.posts.view (+create, update, delete)
    QString className;   // PostResource, or "built in"
    bool operator==(const AdminResource &) const = default;
};

FLOSTUDIOCORE_EXPORT std::optional<QList<AdminResource>> parseAdminList(const QString &out);

// The message of a failed command, from stderr: flocms-cli prints
// " [ERROR] message" (indented lines continue it) for a failure, and
// " ERROR  message" for an error line. Empty when there is none.
FLOSTUDIOCORE_EXPORT QString errorMessage(const QString &err);

// The text of the last "[OK] message" line ("User #1 created: ...").
FLOSTUDIOCORE_EXPORT QString successMessage(const QString &out);

// php flo serve's "[OK] Serving <folder> at http://127.0.0.1:8000 (Ctrl+C
// to stop)": the address. Printed before the server listens, so wait for
// the port (portAnswers()). Empty for other lines.
FLOSTUDIOCORE_EXPORT QString serveUrl(const QString &line);

} // namespace FloStudio::Core
