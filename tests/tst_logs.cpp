#include "testutil.h"

#include <flostudio/core/logs.h>

#include <QTest>

using namespace FloStudio::Core;

class TestLogs : public QObject
{
    Q_OBJECT

    static QStringList serverLines()
    {
        return TestUtil::fixture(QStringLiteral("flo/serve.err")).split(u'\n', Qt::SkipEmptyParts);
    }

private slots:
    void everyRealLineIsRead()
    {
        // Each line of a real `php flo serve` run gets a kind; nothing is Other
        for (const QString &text : serverLines()) {
            const ServerLine line = readServerLine(text);
            QVERIFY2(line.kind != ServerLineKind::Other, qUtf8Printable(text));
        }
    }

    void serverLineKinds()
    {
        const ServerLine started = readServerLine(serverLines().first());
        QCOMPARE(started.kind, ServerLineKind::Started);
        QVERIFY(!started.time.isEmpty());
        QVERIFY(!isProblem(started));

        QCOMPARE(readServerLine(QStringLiteral("[Sat Oct 24 12:53:56 2026] 127.0.0.1:47124 Accepted")).kind,
                 ServerLineKind::Connection);
        QCOMPARE(readServerLine(QStringLiteral("[Mon Oct  5 09:01:02 2026] [::1]:50000 Closing")).kind,
                 ServerLineKind::Connection);

        const ServerLine css = readServerLine(QStringLiteral("[Sat Oct 24 12:53:56 2026] 127.0.0.1:47124 [200]: GET /themes/default/css/style.css"));
        QCOMPARE(css.kind, ServerLineKind::Request);
        QCOMPARE(css.status, 200);
        QCOMPARE(css.method, QStringLiteral("GET"));
        QCOMPARE(css.path, QStringLiteral("/themes/default/css/style.css"));
        QCOMPARE(css.time, QStringLiteral("Sat Oct 24 12:53:56 2026"));
        QVERIFY(!isProblem(css));
    }

    void errorsAndTheirTraces()
    {
        const QStringList lines = serverLines();
        const auto find = [&lines](const QString &part) {
            for (const QString &line : lines) {
                if (line.contains(part))
                    return readServerLine(line);
            }
            return ServerLine{};
        };
        const ServerLine notFound = find(QStringLiteral("HttpException: HTTP 404"));
        QCOMPARE(notFound.kind, ServerLineKind::Exception);
        QCOMPARE(notFound.status, 404);
        QVERIFY(notFound.file.endsWith(QStringLiteral("App.php")));
        QCOMPARE(notFound.line, 92);
        QVERIFY(!isProblem(notFound)); // a missing page is not a problem of the site

        const ServerLine fatal = find(QStringLiteral("PHP Fatal error"));
        QCOMPARE(fatal.kind, ServerLineKind::PhpError);
        QVERIFY(fatal.message.startsWith(QStringLiteral("Uncaught Error: Call to undefined function")));
        QVERIFY(isProblem(fatal));

        const ServerLine failed = find(QStringLiteral("[500]"));
        QCOMPARE(failed.kind, ServerLineKind::Request);
        QCOMPARE(failed.status, 500);
        QCOMPARE(failed.path, QStringLiteral("/studio-fatal.php"));
        QVERIFY(failed.message.startsWith(QStringLiteral("Uncaught Error")));
        QVERIFY(isProblem(failed));

        const ServerLine warning = find(QStringLiteral("PHP Warning"));
        QCOMPARE(warning.kind, ServerLineKind::PhpWarning);
        QVERIFY(warning.message.startsWith(QStringLiteral("Studio test warning")));

        QCOMPARE(readServerLine(QStringLiteral("Stack trace:")).kind, ServerLineKind::Trace);
        QCOMPARE(readServerLine(QStringLiteral("#0 {main}")).kind, ServerLineKind::Trace);
        QCOMPARE(readServerLine(QStringLiteral("  thrown in /x/a.php on line 2")).kind, ServerLineKind::Trace);
        QCOMPARE(readServerLine(QStringLiteral("something else")).kind, ServerLineKind::Other);

        const ServerLine other = readServerLine(QStringLiteral("[Sat Oct 24 12:53:57 2026] RuntimeException: boom in /x/a.php:7"));
        QCOMPARE(other.kind, ServerLineKind::Exception);
        QCOMPARE(other.status, 0);
        QVERIFY(isProblem(other));
    }

    void appLog()
    {
        const QStringList lines = TestUtil::fixture(QStringLiteral("flo/app.log")).split(u'\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 2);
        const AppLogLine line = readAppLogLine(lines.first());
        QVERIFY(!line.continuation);
        QCOMPARE(line.time, QStringLiteral("2026-10-24 09:53:57"));
        QCOMPARE(line.level, AppLogLevel::Error);
        QCOMPARE(line.message, QStringLiteral("HTTP 404"));
        QVERIFY(line.context.startsWith(QStringLiteral("{\"type\":\"FloCMS\\\\Core\\\\HttpException\"")));
        QCOMPARE(line.clientErrorStatus, 404);
        QVERIFY(!isProblem(line)); // logged as error by core, but a page not found
    }

    void appLogLevelsAndContext()
    {
        const AppLogLine error = readAppLogLine(QStringLiteral("[2026-10-24 10:00:00] error: Payment {id} failed {\"order\":7}"));
        QCOMPARE(error.message, QStringLiteral("Payment {id} failed"));
        QCOMPARE(error.context, QStringLiteral("{\"order\":7}"));
        QVERIFY(isProblem(error));

        const AppLogLine plain = readAppLogLine(QStringLiteral("[2026-10-24 10:00:00] info: Started {not json"));
        QCOMPARE(plain.level, AppLogLevel::Info);
        QCOMPARE(plain.message, QStringLiteral("Started {not json"));
        QVERIFY(plain.context.isEmpty());
        QVERIFY(!isProblem(plain));

        QCOMPARE(readAppLogLine(QStringLiteral("[2026-10-24 10:00:00] critical: x")).level, AppLogLevel::Critical);
        QCOMPARE(readAppLogLine(QStringLiteral("[2026-10-24 10:00:00] warning: x")).level, AppLogLevel::Warning);
        QVERIFY(!isProblem(readAppLogLine(QStringLiteral("[2026-10-24 10:00:00] warning: x"))));

        const AppLogLine server = readAppLogLine(QStringLiteral("[2026-10-24 10:00:00] error: HTTP 500 {\"type\":\"FloCMS\\\\Core\\\\HttpException\"}"));
        QCOMPARE(server.clientErrorStatus, 0);
        QVERIFY(isProblem(server));

        const AppLogLine more = readAppLogLine(QStringLiteral("  second line of a message"));
        QVERIFY(more.continuation);
        QVERIFY(!isProblem(more));
    }
};

QTEST_GUILESS_MAIN(TestLogs)
#include "tst_logs.moc"
