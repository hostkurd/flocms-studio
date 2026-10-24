// Runs Studio's Core against a real, installed FloCMS project: set
// FLOSTUDIO_REAL_PROJECT to its folder (with a configured, reachable
// database; port 8190 free). The admin panel's commands are checked when
// the project has flocms-admin. Skipped when the variable isn't set. CI
// sets it to a site made with `composer create-project hostkurd/flocms`.
//
//   FLOSTUDIO_REAL_PROJECT=/path/to/site ctest -R tst_realproject --output-on-failure

#include <flostudio/core/commands.h>
#include <flostudio/core/output.h>
#include <flostudio/core/project.h>
#include <flostudio/core/tools.h>

#include <QProcess>
#include <QTest>

#include <algorithm>

using namespace FloStudio::Core;

class TestRealProject : public QObject
{
    Q_OBJECT

    QString m_dir;
    Settings m_settings;
    bool m_admin = false;

    Captured flo(const QStringList &arguments)
    {
        const auto command = prepareCommand(m_settings, CommandRequest{Program::Flo, m_dir, arguments, {}, {}});
        if (!command)
            qFatal("%s", qUtf8Printable(command.error().code));
        return runCaptured(command.value());
    }

private slots:
    void initTestCase()
    {
        m_dir = qEnvironmentVariable("FLOSTUDIO_REAL_PROJECT");
        if (m_dir.isEmpty())
            QSKIP("FLOSTUDIO_REAL_PROJECT is not set");
    }

    void readsTheProject()
    {
        const auto info = projectInfo(m_dir);
        QVERIFY(info.ok());
        QVERIFY(info->hasVendor);
        QVERIFY(info->database.configured);
        QVERIFY(info->hasLock);
        QVERIFY(std::any_of(info->packages.begin(), info->packages.end(),
                            [](const Package &p) { return p.name == QLatin1String(kCorePackage); }));
        m_admin = info->summary.hasAdmin;
        qInfo("%s: %lld hostkurd packages, admin panel: %s, database: %s", qUtf8Printable(info->summary.folderName),
              static_cast<long long>(info->packages.size()), m_admin ? "yes" : "no", qUtf8Printable(info->database.driver));
        QVERIFY(!info->launcher.isEmpty());
    }

    void doctorAndMigrations()
    {
        const Captured doctor = flo({QStringLiteral("doctor"), QStringLiteral("--format=json")});
        const auto report = parseDoctor(doctor.out);
        QVERIFY2(report, qUtf8Printable(doctor.err));
        QCOMPARE(report->ok, doctor.exitCode == 0);

        const Captured status = flo({QStringLiteral("migrate:status")});
        QVERIFY2(status.succeeded(), qUtf8Printable(errorMessage(status.err)));
        const auto migrations = parseMigrateStatus(status.out);
        QVERIFY(migrations && !migrations->isEmpty());
        QCOMPARE(pendingMigrations(*migrations), 0);

        const Captured shown = flo({QStringLiteral("db:show"), QStringLiteral("--format=json")});
        const auto show = parseDatabaseShow(shown.out);
        QVERIFY2(show, qUtf8Printable(errorMessage(shown.err)));
        QVERIFY(std::any_of(show->tables.begin(), show->tables.end(),
                            [](const DatabaseTable &t) { return t.name == u"users"; }));

        if (!m_admin)
            return;
        const Captured admin = flo({QStringLiteral("admin:doctor")});
        QVERIFY(parseAdminDoctor(admin.out, admin.err));
        QVERIFY(parseAdminList(flo({QStringLiteral("admin:list")}).out));
    }

    void servesAndStops()
    {
        const auto command = prepareCommand(
            m_settings, CommandRequest{Program::Flo, m_dir, {QStringLiteral("serve"), QStringLiteral("--port=8190")}, {}, {}});
        QVERIFY(command.ok());
        QObject owner;
        QString url;
        QStringList serverLines;
        bool ended = false;
        QProcess *server = startStreaming(
            command.value(), &owner,
            [&](const OutputLine &line) {
                if (url.isEmpty())
                    url = serveUrl(line.text);
                if (line.error)
                    serverLines << line.text;
            },
            [&](int, bool) { ended = true; });
        QTRY_VERIFY_WITH_TIMEOUT(!url.isEmpty(), 20000);
        QCOMPARE(url, QStringLiteral("http://127.0.0.1:8190"));
        // "[OK] Serving" comes before php -S listens: wait for the port
        QTRY_VERIFY_WITH_TIMEOUT(portAnswers(QStringLiteral("127.0.0.1"), 8190, 200), 20000);
        QTRY_VERIFY_WITH_TIMEOUT(serverLines.join(u'\n').contains(QStringLiteral("Development Server")), 20000);

        stopProcess(server);
        QTRY_VERIFY_WITH_TIMEOUT(ended, 10000);
        // php -S, the child of php flo serve, is gone too
        QTRY_VERIFY_WITH_TIMEOUT(!portAnswers(QStringLiteral("127.0.0.1"), 8190, 200), 10000);
    }
};

QTEST_GUILESS_MAIN(TestRealProject)
#include "tst_realproject.moc"
