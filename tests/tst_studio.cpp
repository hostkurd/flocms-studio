// The app's view model (app/studio.h): settings saved as they change, the
// project list and the open project, tools found on a worker thread,
// demos. Runs with QStandardPaths' test mode, so the real settings of the
// person running the tests are never touched.

#include "testutil.h"

#include "language.h"
#include "studio.h"

#include <flostudio/core/settings.h>

#include <QSignalSpy>
#include <QStandardPaths>
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
};

QTEST_MAIN(TestStudio)
#include "tst_studio.moc"
