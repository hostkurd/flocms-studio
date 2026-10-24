// The app's view model (app/studio.h): settings saved as they change, the
// project list and the open project, tools found on a worker thread,
// demos. Runs with QStandardPaths' test mode, so the real settings of the
// person running the tests are never touched.

#include "testutil.h"

#include "language.h"
#include "logmodel.h"
#include "studio.h"

#include <flostudio/core/server.h>
#include <flostudio/core/settings.h>
#include <flostudio/core/tools.h>

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

using namespace FloStudio::Core;

namespace {

void writeFile(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        qFatal("cannot write %s", qUtf8Printable(path));
    file.write(text);
}

void makeSite(const QString &dir)
{
    writeFile(dir + QStringLiteral("/flo"), "");
    writeFile(dir + QStringLiteral("/composer.json"), TestUtil::fixture(QStringLiteral("project/composer.json")).toUtf8());
    writeFile(dir + QStringLiteral("/composer.lock"), TestUtil::fixture(QStringLiteral("project/composer.lock")).toUtf8());
    writeFile(dir + QStringLiteral("/.env"), "APP_NAME=\"Test Site\"\nAPP_ENV=local\nDB_TYPE=sqlite\nDB_NAME=storage/database.sqlite\n");
}

} // namespace

class TestStudio : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init()
    {
        // A fresh settings folder for each test
        QDir(settingsFolder()).removeRecursively();
    }

    void startsOnTheWelcomePage()
    {
        Language language;
        Studio studio(&language);
        studio.start();
        QCOMPARE(studio.page(), QStringLiteral("welcome"));
        QCOMPARE(studio.projects()->count(), 0);
        QVERIFY(studio.checkingTools());
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools(), 60000);
        QCOMPARE(studio.tools().size(), 3); // PHP, Composer, MariaDB, found or not
        QVERIFY(studio.problem().isEmpty());
    }

    void addsAFolderAndOpensItsProject()
    {
        const QTemporaryDir dir;
        makeSite(dir.filePath(QStringLiteral("Sites/shop")));
        QDir().mkpath(dir.filePath(QStringLiteral("Sites/not-a-project")));
        Language language;
        Studio studio(&language);
        studio.start();
        studio.addProjectFolder(QUrl::fromLocalFile(dir.filePath(QStringLiteral("Sites"))));
        QCOMPARE(studio.projects()->count(), 1);
        QCOMPARE(studio.page(), QStringLiteral("project"));
        QCOMPARE(studio.current().value(QStringLiteral("name")).toString(), QStringLiteral("shop"));
        QCOMPARE(studio.current().value(QStringLiteral("appName")).toString(), QStringLiteral("Test Site"));
        QCOMPARE(studio.current().value(QStringLiteral("database")).toMap().value(QStringLiteral("driver")).toString(),
                 QStringLiteral("sqlite"));
        QCOMPARE(studio.current().value(QStringLiteral("packages")).toList().size(), 7);

        // Saved: a new Studio finds it again
        QCOMPARE(loadSettings(settingsFolder()).value().projectFolders,
                 QStringList{QDir::toNativeSeparators(dir.filePath(QStringLiteral("Sites")))});
        Studio again(&language);
        again.start();
        QCOMPARE(again.projects()->count(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && !again.checkingTools(), 60000);
    }

    void refusesAFolderThatIsNotFloCms()
    {
        const QTemporaryDir dir;
        Language language;
        Studio studio(&language);
        studio.start();
        QSignalSpy problem(&studio, &Studio::problemChanged);
        studio.addProject(QUrl::fromLocalFile(dir.path()));
        QCOMPARE(problem.count(), 1);
        QCOMPARE(studio.problem().value(QStringLiteral("code")).toString(), QStringLiteral("not_flocms"));
        QCOMPARE(studio.projects()->count(), 0);
        studio.dismissProblem();
        QVERIFY(studio.problem().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools(), 60000);
    }

    void aProjectThatIsGoneStaysListed()
    {
        const QTemporaryDir dir;
        const QString site = dir.filePath(QStringLiteral("one"));
        makeSite(site);
        Language language;
        Studio studio(&language);
        studio.start();
        studio.addProject(QUrl::fromLocalFile(site));
        QCOMPARE(studio.projects()->count(), 1);
        QDir(site).removeRecursively();
        studio.refreshProjects();
        QCOMPARE(studio.projects()->data(studio.projects()->index(0), ProjectsModel::MissingRole).toBool(), true);
        QCOMPARE(studio.current().value(QStringLiteral("error")).toString(), QStringLiteral("project_missing"));
        studio.removeProject(studio.currentPath());
        QCOMPARE(studio.projects()->count(), 0);
        QCOMPARE(studio.page(), QStringLiteral("welcome"));
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools(), 60000);
    }

    void settingsAreSavedAsTheyChange()
    {
        Language language;
        Studio studio(&language);
        studio.start();
        QSignalSpy dark(&studio, &Studio::darkChanged);
        studio.setTheme(QStringLiteral("dark"));
        QVERIFY(studio.dark());
        QCOMPARE(dark.count(), 1);
        studio.setServerPort(8080);
        studio.setEditorCommand(QStringLiteral("phpstorm {folder}"));
        studio.setLanguage(QStringLiteral("ckb"));
        studio.setServerNetwork(true);
        const Settings saved = loadSettings(settingsFolder()).value();
        QCOMPARE(saved.theme, QStringLiteral("dark"));
        QCOMPARE(saved.serverPort, 8080);
        QCOMPARE(saved.editorCommand, QStringLiteral("phpstorm {folder}"));
        QCOMPARE(saved.language, QStringLiteral("ckb"));
        QVERIFY(saved.serverNetwork);
        studio.setServerPort(0); // not a port: normalized() puts back 8000
        QCOMPARE(loadSettings(settingsFolder()).value().serverPort, 8000);
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools(), 60000);
    }

    void neverSavesOverUnreadableSettings()
    {
#ifdef Q_OS_UNIX
        if (::geteuid() == 0)
            QSKIP("root can read any file");
#endif
        QDir().mkpath(settingsFolder());
        const QString file = QDir(settingsFolder()).filePath(QStringLiteral("settings.json"));
        writeFile(file, R"({"language":"ar"})");
        QFile(file).setPermissions(QFileDevice::Permissions());
        if (QFile f(file); f.open(QIODevice::ReadOnly))
            QSKIP("the file can't be made unreadable here");
        Language language;
        Studio studio(&language);
        studio.start();
        QCOMPARE(studio.problem().value(QStringLiteral("code")).toString(), QStringLiteral("settings_read"));
        studio.setTheme(QStringLiteral("dark"));
        QFile(file).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QCOMPARE(loadSettings(settingsFolder()).value().language, QStringLiteral("ar")); // untouched
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools(), 60000);
    }

    void demosFillEverythingAndSaveNothing()
    {
        Language language;
        Studio studio(&language);
        QVERIFY(studio.startDemo(QStringLiteral(FLOSTUDIO_SOURCE "/app/demo/download-progress.json")).ok());
        QVERIFY(studio.demo());
        QCOMPARE(studio.page(), QStringLiteral("project"));
        QCOMPARE(studio.demoDialog(), QStringLiteral("downloads"));
        QVERIFY(studio.mariadbFromHomebrew()); // the demo is a Mac
        QCOMPARE(studio.downloads().size(), 2);
        QVERIFY(studio.downloading());
        QCOMPARE(studio.current().value(QStringLiteral("name")).toString(), QStringLiteral("fectumhellas"));
        QVERIFY(!studio.downloadOffer().isEmpty());
        studio.setTheme(QStringLiteral("light"));
        QVERIFY(!QFile::exists(QDir(settingsFolder()).filePath(QStringLiteral("settings.json"))));
        QCOMPARE(studio.startDemo(QStringLiteral("/no/such/demo.json")).error().code, QStringLiteral("demo_file"));
    }

    void everyDemoLoads()
    {
        const QDir demos(QStringLiteral(FLOSTUDIO_SOURCE "/app/demo"));
        const QStringList files = demos.entryList({QStringLiteral("*.json")});
        QVERIFY(files.size() >= 5);
        for (const QString &file : files) {
            Language language;
            Studio studio(&language);
            QVERIFY2(studio.startDemo(demos.filePath(file)).ok(), qUtf8Printable(file));
        }
    }

    void theOfferIsThisComputersToolList()
    {
        Language language;
        Studio studio(&language);
        studio.start();
        const QVariantList offer = studio.downloadOffer();
        QVERIFY(!offer.isEmpty());
        int php = 0;
        for (const QVariant &row : offer)
            php += row.toMap().value(QStringLiteral("tool")).toString() == u"php" ? 1 : 0;
        QCOMPARE(php, 2); // the two newest PHP versions (Q9)
        studio.startDownloads({QStringLiteral("no-such-package")});
        QVERIFY(!studio.downloading());
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools(), 60000);
    }

    // --- Stage 4: the log and the server ----------------------------------

    void theLogReadsTheRealServerOutput()
    {
        LogModel logs;
        const QStringList server = TestUtil::fixture(QStringLiteral("flo/serve.err")).split(u'\n', Qt::SkipEmptyParts);
        for (const QString &line : server)
            logs.addServerLine(line);
        const QStringList app = TestUtil::fixture(QStringLiteral("flo/app.log")).split(u'\n', Qt::SkipEmptyParts);
        for (const QString &line : app)
            logs.addAppLogLine(line);
        QCOMPARE(logs.total(), 0); // batched
        logs.flush();
        const int connections = static_cast<int>(std::count_if(server.begin(), server.end(), [](const QString &line) {
            return line.endsWith(QStringLiteral(" Accepted")) || line.endsWith(QStringLiteral(" Closing"));
        }));
        QCOMPARE(logs.total(), static_cast<int>(server.size() + app.size()) - connections);
        QVERIFY(logs.problems() >= 2); // the fatal error and its 500
        // A page not found is no problem: neither the exception nor its app.log line
        for (int row = 0; row < logs.rowCount(); ++row) {
            const QModelIndex index = logs.index(row);
            if (logs.data(index, LogModel::TextRole).toString().contains(QStringLiteral("HTTP 404")))
                QVERIFY(logs.data(index, LogModel::LevelRole).toString() != u"problem");
        }

        // Stack trace lines belong to the line before and share its level
        bool trace = false;
        for (int row = 1; row < logs.rowCount(); ++row) {
            const QModelIndex index = logs.index(row);
            if (logs.data(index, LogModel::TextRole).toString() == u"Stack trace:") {
                trace = true;
                QVERIFY(logs.data(index, LogModel::ContinuationRole).toBool());
                QCOMPARE(logs.data(index, LogModel::LevelRole), logs.data(logs.index(row - 1), LogModel::LevelRole));
            }
        }
        QVERIFY(trace);

        // Filters
        logs.setSource(QStringLiteral("app"));
        QCOMPARE(logs.shown(), static_cast<int>(app.size()));
        logs.setSource(QStringLiteral("all"));
        logs.setProblemsOnly(true);
        QVERIFY(logs.shown() >= logs.problems());
        for (int row = 0; row < logs.rowCount(); ++row)
            QCOMPARE(logs.data(logs.index(row), LogModel::LevelRole).toString(), QStringLiteral("problem"));
        logs.setProblemsOnly(false);
        logs.setSearch(QStringLiteral("style.css"));
        QVERIFY(logs.shown() >= 1);
        QVERIFY(logs.text().contains(QStringLiteral("GET /themes/default/css/style.css")));
        logs.setSearch(QStringLiteral("no such words"));
        QCOMPARE(logs.shown(), 0);
        logs.setSearch({});
        // Studio's own lines show with every source
        logs.addStudioLine(QStringLiteral("http://127.0.0.1:8000"), QStringLiteral("started"));
        logs.setSource(QStringLiteral("server"));
        logs.flush();
        QCOMPARE(logs.data(logs.index(logs.rowCount() - 1), LogModel::SourceRole).toString(), QStringLiteral("studio"));
        logs.clear();
        QCOMPARE(logs.total(), 0);
        QCOMPARE(logs.problems(), 0);
    }

    void theLogKeepsTheNewestLines()
    {
        LogModel logs(nullptr, 1000);
        for (int i = 0; i < 2500; ++i)
            logs.addAppLogLine(QStringLiteral("[2026-10-24 10:00:00] %1: line %2").arg(i % 10 == 0 ? QStringLiteral("error") : QStringLiteral("info")).arg(i));
        logs.flush();
        QVERIFY(logs.total() <= 1000);
        QVERIFY(logs.total() >= 900);
        QVERIFY(logs.data(logs.index(logs.rowCount() - 1), LogModel::TextRole).toString().endsWith(QStringLiteral("line 2499")));
        QCOMPARE(logs.problems(), (logs.total() + 9) / 10);
    }

    void aProjectThatCannotBeServed()
    {
        const QTemporaryDir dir;
        const QString site = dir.filePath(QStringLiteral("nolauncher"));
        makeSite(site);
        QFile::remove(site + QStringLiteral("/flo")); // composer install never ran
        Language language;
        Studio studio(&language);
        studio.start();
        studio.addProject(QUrl::fromLocalFile(site));
        QCOMPARE(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("stopped"));
        QVERIFY(studio.logs());
        studio.startServer();
        QCOMPARE(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
        const QString error = studio.server().value(QStringLiteral("error")).toString();
        QVERIFY2(error == u"no_launcher" || error == u"php_not_found", qUtf8Printable(error));
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools(), 60000);
    }

    void theAppLogIsFollowed()
    {
        const QTemporaryDir dir;
        const QString site = dir.filePath(QStringLiteral("logs"));
        makeSite(site);
        writeFile(appLogPath(site), "[2026-10-24 09:00:00] info: before Studio\n");
        Language language;
        Studio studio(&language);
        studio.start();
        studio.addProject(QUrl::fromLocalFile(site));
        LogModel *logs = studio.logs();
        QVERIFY(logs);
        QTRY_COMPARE(logs->total(), 1);
        QFile log(appLogPath(site));
        QVERIFY(log.open(QIODevice::Append));
        log.write("[2026-10-24 09:00:01] error: written later {\"a\":1}\n");
        log.close();
        QTRY_COMPARE_WITH_TIMEOUT(logs->total(), 2, 5000);
        QCOMPARE(logs->problems(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools(), 60000);
    }

    // A real project (FLOSTUDIO_REAL_PROJECT, as tst_realproject): php flo
    // serve starts, answers, logs, and stops with its php -S child.
    void servesARealProject()
    {
        const QString project = qEnvironmentVariable("FLOSTUDIO_REAL_PROJECT");
        if (project.isEmpty())
            QSKIP("FLOSTUDIO_REAL_PROJECT is not set");
        Language language;
        Studio studio(&language);
        studio.start();
        studio.setServerPort(18400);
        studio.addProject(QUrl::fromLocalFile(project));
        QSignalSpy changed(&studio, &Studio::serverChanged);
        studio.startServer();
        QCOMPARE(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("starting"));
        QTRY_COMPARE_WITH_TIMEOUT(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("running"), 30000);
        const QString url = studio.server().value(QStringLiteral("url")).toString();
        QVERIFY2(url.startsWith(QStringLiteral("http://127.0.0.1:184")), qUtf8Printable(url));
        QVERIFY(!studio.server().value(QStringLiteral("php")).toString().isEmpty());
        QCOMPARE(studio.projects()->data(studio.projects()->index(studio.projects()->indexOf(studio.currentPath())),
                                         ProjectsModel::RunningRole).toBool(),
                 true);
        const int port = urlPort(url);
        QVERIFY(portAnswers(QStringLiteral("127.0.0.1"), port, 1000));
        // A request for a page that isn't there: the server and app.log say so
        QTcpSocket socket;
        socket.connectToHost(QStringLiteral("127.0.0.1"), static_cast<quint16>(port));
        QVERIFY(socket.waitForConnected(3000));
        socket.write("GET /studio-test-missing HTTP/1.0\r\nHost: localhost\r\n\r\n");
        QVERIFY(socket.waitForReadyRead(10000));
        QVERIFY(socket.readAll().startsWith("HTTP/1."));
        socket.close();
        LogModel *logs = studio.logs();
        QTRY_VERIFY_WITH_TIMEOUT(logs->text().contains(QStringLiteral("HTTP 404")), 10000);

        studio.stopServer();
        QTRY_COMPARE_WITH_TIMEOUT(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("stopped"), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!portAnswers(QStringLiteral("127.0.0.1"), port, 300), 10000); // php -S went too
        QVERIFY(changed.count() >= 3);
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools(), 60000);
    }

    void closingStudioStopsItsServers()
    {
        const QString project = qEnvironmentVariable("FLOSTUDIO_REAL_PROJECT");
        if (project.isEmpty())
            QSKIP("FLOSTUDIO_REAL_PROJECT is not set");
        int port = 0;
        {
            Language language;
            Studio studio(&language);
            studio.start();
            studio.setServerPort(18500);
            studio.addProject(QUrl::fromLocalFile(project));
            studio.startServer();
            QTRY_COMPARE_WITH_TIMEOUT(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("running"), 30000);
            port = urlPort(studio.server().value(QStringLiteral("url")).toString());
            QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools(), 60000);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!portAnswers(QStringLiteral("127.0.0.1"), port, 300), 10000);
        // and nothing is left noted for the next start
        QCOMPARE(stopLeftoverServers(settingsFolder()).size(), 0);
    }
};

QTEST_MAIN(TestStudio)
#include "tst_studio.moc"
