#include "testutil.h"

#include <flostudio/core/output.h>

#include <QTest>

using namespace FloStudio::Core;

// Every test reads real output of `php flo` from tests/fixtures/flo
// (tools/capture_fixtures.py; versions in versions.txt).
class TestOutput : public QObject
{
    Q_OBJECT

    static QString out(const QString &name) { return TestUtil::fixture(QStringLiteral("flo/%1.out").arg(name)); }
    static QString err(const QString &name) { return TestUtil::fixture(QStringLiteral("flo/%1.err").arg(name)); }

private slots:
    void doctor()
    {
        QCOMPARE(TestUtil::fixtureCode(QStringLiteral("doctor.json")), 0);
        const auto report = parseDoctor(out(QStringLiteral("doctor.json")));
        QVERIFY(report);
        QVERIFY(report->ok);
        QCOMPARE(report->failed, 0);
        QCOMPARE(report->warnings, 1);
        QCOMPARE(report->checks.first(), (DoctorCheck{CheckStatus::Ok, QStringLiteral("PHP version"), QStringLiteral("8.3.6")}));
        int warnings = 0;
        for (const DoctorCheck &check : report->checks)
            warnings += check.status == CheckStatus::Warn ? 1 : 0;
        QCOMPARE(warnings, report->warnings);
    }

    void doctorWithPendingMigrations()
    {
        const auto report = parseDoctor(out(QStringLiteral("doctor-pending.json")));
        QVERIFY(report && report->ok);
        const auto migrations = std::find_if(report->checks.begin(), report->checks.end(),
                                             [](const DoctorCheck &c) { return c.check == u"Migrations"; });
        QVERIFY(migrations != report->checks.end());
        QCOMPARE(migrations->status, CheckStatus::Warn);
        QVERIFY(migrations->detail.contains(QStringLiteral("pending")));
    }

    void doctorThatFails()
    {
        QCOMPARE(TestUtil::fixtureCode(QStringLiteral("doctor-denied.json")), 1);
        const auto report = parseDoctor(out(QStringLiteral("doctor-denied.json")));
        QVERIFY(report);
        QVERIFY(!report->ok);
        QCOMPARE(report->failed, 1);
        const auto database = std::find_if(report->checks.begin(), report->checks.end(),
                                           [](const DoctorCheck &c) { return c.status == CheckStatus::Fail; });
        QCOMPARE(database->check, QStringLiteral("Database"));
        QVERIFY(database->detail.contains(QStringLiteral("Access denied")));
    }

    void doctorTextIsNotJson()
    {
        QVERIFY(!parseDoctor(out(QStringLiteral("doctor"))));
        QVERIFY(!parseDoctor(QString()));
        QVERIFY(!parseDoctor(QStringLiteral("{\"ok\":true}")));
    }

    void databaseShow()
    {
        const auto show = parseDatabaseShow(out(QStringLiteral("db-show.json")));
        QVERIFY(show);
        QCOMPARE(show->driver, QStringLiteral("mysql"));
        QCOMPARE(show->host, QStringLiteral("127.0.0.1"));
        QCOMPARE(show->port, 3306);
        QCOMPARE(show->database, QStringLiteral("studio_full"));
        QVERIFY(show->server.contains(QStringLiteral("MariaDB")));
        QCOMPARE(show->tables.size(), 20); // "Tables (20)" in the text version
        QVERIFY(out(QStringLiteral("db-show")).contains(QStringLiteral("Tables (20)")));
        const auto users = std::find_if(show->tables.begin(), show->tables.end(),
                                        [](const DatabaseTable &t) { return t.name == u"users"; });
        QCOMPARE(users->rows, 0); // captured before user:create
        QVERIFY(!parseDatabaseShow(out(QStringLiteral("db-show-denied.json"))));
    }

    void migrateStatus()
    {
        const auto migrations = parseMigrateStatus(out(QStringLiteral("migrate-status")));
        QVERIFY(migrations);
        QCOMPARE(migrations->size(), 13);
        QCOMPARE(migrations->first(), (Migration{QStringLiteral("2026_10_10_000000_create_api_tables"),
                                                 QStringLiteral("hostkurd/flocms-api"), 1, MigrationState::Ran}));
        QCOMPARE(pendingMigrations(*migrations), 0);
    }

    void migrateStatusPending()
    {
        const auto migrations = parseMigrateStatus(out(QStringLiteral("migrate-status-pending")));
        QVERIFY(migrations);
        QCOMPARE(migrations->size(), 13);
        QCOMPARE(pendingMigrations(*migrations), 13);
        QCOMPARE(migrations->first().batch, 0);
    }

    void migrateStatusChangedAndMissing()
    {
        const auto migrations = parseMigrateStatus(out(QStringLiteral("migrate-status-changed")));
        QVERIFY(migrations);
        QCOMPARE(migrations->size(), 13);
        const auto state = [&](const QString &suffix) {
            return std::find_if(migrations->begin(), migrations->end(),
                                [&](const Migration &m) { return m.name.endsWith(suffix); })
                ->state;
        };
        QCOMPARE(state(QStringLiteral("create_posts_table")), MigrationState::Changed);
        QCOMPARE(state(QStringLiteral("create_users_bio_column")), MigrationState::FileMissing);
        QCOMPARE(state(QStringLiteral("create_users_table")), MigrationState::Ran);
    }

    void migrateStatusNone()
    {
        const auto migrations = parseMigrateStatus(out(QStringLiteral("migrate-status-none")));
        QVERIFY(migrations);
        QVERIFY(migrations->isEmpty());
    }

    void migrateStatusThatFails()
    {
        QCOMPARE(TestUtil::fixtureCode(QStringLiteral("migrate-status-denied")), 1);
        QVERIFY(!parseMigrateStatus(out(QStringLiteral("migrate-status-denied"))));
        QCOMPARE(errorMessage(err(QStringLiteral("migrate-status-denied"))),
                 QStringLiteral("Cannot connect to the database (127.0.0.1:3306/studio_full): SQLSTATE[HY000] [1045] "
                                "Access denied for user 'root'@'localhost' (using password: YES) "
                                "Check DB_USERNAME and DB_PASSWORD."));
    }

    void adminDoctor()
    {
        const auto report = parseAdminDoctor(out(QStringLiteral("admin-doctor")), err(QStringLiteral("admin-doctor")));
        QVERIFY(report);
        QCOMPARE(report->heading, QStringLiteral("flocms-admin 0.39.3"));
        QCOMPARE(report->summary, QStringLiteral("Everything looks good."));
        QCOMPARE(report->problems, 0);
        QCOMPARE(report->warnings, 0);
        QCOMPARE(report->sections.first().title, QString()); // the lines under the heading
        QCOMPARE(report->sections.first().lines.first(),
                 (ReportLine{CheckStatus::Ok, QStringLiteral("config/admin.php found")}));
        const auto posts = std::find_if(report->sections.begin(), report->sections.end(),
                                        [](const ReportSection &s) { return s.title.contains(QStringLiteral("PostResource")); });
        QVERIFY(posts != report->sections.end());
        QCOMPARE(posts->title, QStringLiteral("App\\Admin\\Resources\\PostResource  (/admin/posts)"));
        QCOMPARE(posts->lines.at(1).status, CheckStatus::Note); // "CSV: export ..."
        QCOMPARE(report->sections.last().title, QStringLiteral("Pages  (/admin/site-pages)"));
    }

    void adminDoctorWithAProblem()
    {
        QCOMPARE(TestUtil::fixtureCode(QStringLiteral("admin-doctor-problem")), 1);
        const auto report =
            parseAdminDoctor(out(QStringLiteral("admin-doctor-problem")), err(QStringLiteral("admin-doctor-problem")));
        QVERIFY(report);
        QCOMPARE(report->problems, 1);
        QCOMPARE(report->summary, QStringLiteral("1 problem(s), 0 warning(s)."));
        bool found = false;
        for (const ReportSection &section : report->sections) {
            for (const ReportLine &line : section.lines)
                found = found || (line.status == CheckStatus::Fail && line.text.contains(QStringLiteral("\"posts\" not found")));
        }
        QVERIFY(found);
    }

    void adminDoctorOfSomethingElse()
    {
        QVERIFY(!parseAdminDoctor(QStringLiteral("random text\n"), {}));
        QVERIFY(!parseAdminDoctor({}, err(QStringLiteral("unknown-command"))));
    }

    void tablesWithTheirTitles()
    {
        const QList<TextTable> tables = parseTables(out(QStringLiteral("admin-list")));
        QCOMPARE(tables.size(), 2);
        QCOMPARE(tables.at(0).title, QStringLiteral("Admin resources"));
        QCOMPARE(tables.at(0).headers,
                 (QStringList{QStringLiteral("Resource"), QStringLiteral("URL"), QStringLiteral("Table"),
                              QStringLiteral("Permissions"), QStringLiteral("Class")}));
        QCOMPARE(tables.at(1).title, QStringLiteral("Settings pages"));
        QCOMPARE(tables.at(1).rows.size(), 1);
        QCOMPARE(tables.at(1).rows.first().first(), QStringLiteral("Admin panel"));
        QCOMPARE(parseTables(out(QStringLiteral("db-show"))).first().title, QStringLiteral("Tables (20)"));
    }

    void adminList()
    {
        const auto resources = parseAdminList(out(QStringLiteral("admin-list")));
        QVERIFY(resources);
        QCOMPARE(resources->size(), 3);
        QCOMPARE(resources->first(),
                 (AdminResource{QStringLiteral("Posts"), QStringLiteral("/admin/posts"), QStringLiteral("posts"),
                                QStringLiteral("content.posts.view (+create, update, delete)"), QStringLiteral("PostResource")}));
        QCOMPARE(resources->at(1).className, QStringLiteral("built in"));
        QVERIFY(!parseAdminList(out(QStringLiteral("migrate-status"))));
    }

    void errorMessages()
    {
        QCOMPARE(errorMessage(err(QStringLiteral("unknown-command"))),
                 QStringLiteral("Command \"no-such-command\" is not defined. Did you mean \"make:command\"?"));
        QCOMPARE(errorMessage(err(QStringLiteral("user-create-again"))),
                 QStringLiteral("A user with the email \"admin@example.com\" already exists. Use user:password or user:role to change it."));
        QCOMPARE(errorMessage(err(QStringLiteral("admin-doctor-problem"))), QStringLiteral("1 problem(s), 0 warning(s)."));
        QCOMPARE(errorMessage(err(QStringLiteral("doctor.json"))), QString());
        QCOMPARE(errorMessage(QStringLiteral("PHP Warning: x\n")), QString());
    }

    void successMessages()
    {
        QCOMPARE(successMessage(out(QStringLiteral("user-create"))),
                 QStringLiteral("User #1 created: admin@example.com (Super Admin (3), active)."));
        QCOMPARE(successMessage(out(QStringLiteral("db-check"))),
                 QStringLiteral("Connected to studio_full (mysql, 10.11.14-MariaDB-0ubuntu0.24.04.1)."));
        QCOMPARE(successMessage(out(QStringLiteral("migrate"))).isEmpty(), false);
        QCOMPARE(successMessage(out(QStringLiteral("migrate-status"))), QString());
    }

    void serveAddress()
    {
        const QString first = TestUtil::fixture(QStringLiteral("flo/serve.out")).section(u'\n', 0, 0);
        QCOMPARE(serveUrl(first), QStringLiteral("http://127.0.0.1:8123"));
        QCOMPARE(serveUrl(QStringLiteral("[OK] Serving C:\\My Sites\\a at http://0.0.0.0:8001 (Ctrl+C to stop)")),
                 QStringLiteral("http://0.0.0.0:8001"));
        QCOMPARE(serveUrl(QStringLiteral("Port 8000 is in use.")), QString());
    }
};

QTEST_GUILESS_MAIN(TestOutput)
#include "tst_output.moc"
