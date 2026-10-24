// The app's view model (app/studio.h): settings saved as they change, the
// project list and the open project, tools found on a worker thread,
// demos. Runs with QStandardPaths' test mode, so the real settings of the
// person running the tests are never touched.

#include "testutil.h"

#include "language.h"
#include "logmodel.h"
#include "newproject.h"
#include "projectchecks.h"
#include "projectgenerate.h"
#include "projectserver.h"
#include "studio.h"

#include <flostudio/core/env.h>
#include <flostudio/core/server.h>
#include <flostudio/core/settings.h>
#include <flostudio/core/preview.h>
#include <flostudio/core/tools.h>

#include <QAbstractItemModelTester>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

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

// Doctor and the database are read when a project opens (stage 5): wait
// for them, so nothing still runs in a folder a test deletes (Windows
// can't delete the folder a program runs in)
bool checksDone(const Studio &studio)
{
    const ProjectChecks *checks = const_cast<Studio &>(studio).checks();
    if (!checks)
        return true;
    return !checks->reading() && !checks->busy();
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
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
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
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && !again.checkingTools() && checksDone(studio) && checksDone(again), 60000);
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
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
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
        QTRY_VERIFY_WITH_TIMEOUT(checksDone(studio), 60000);
        QVERIFY(QDir(site).removeRecursively());
        studio.refreshProjects();
        QCOMPARE(studio.projects()->data(studio.projects()->index(0), ProjectsModel::MissingRole).toBool(), true);
        QCOMPARE(studio.current().value(QStringLiteral("error")).toString(), QStringLiteral("project_missing"));
        studio.removeProject(studio.currentPath());
        QCOMPARE(studio.projects()->count(), 0);
        QCOMPARE(studio.page(), QStringLiteral("welcome"));
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
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
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
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
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
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
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
    }

    // --- Stage 6: the new project wizard ----------------------------------

    void theWizardForm()
    {
        const QTemporaryDir dir;
        Language language;
        Studio studio(&language);
        studio.start();
        studio.addProjectFolder(QUrl::fromLocalFile(dir.path()));
        studio.showNewProject();
        QCOMPARE(studio.page(), QStringLiteral("new"));
        NewProjectWizard *wizard = studio.wizard();
        // The folder from Settings' project folders
        QCOMPARE(wizard->form().value(QStringLiteral("parentDir")).toString(), QDir::toNativeSeparators(dir.path()));
        wizard->setName(QStringLiteral("My Shop"));
        QCOMPARE(wizard->form().value(QStringLiteral("name")).toString(), QStringLiteral("my-shop"));
        QCOMPARE(wizard->form().value(QStringLiteral("dbName")).toString(), QStringLiteral("my_shop"));
        // Problems show once touched
        QVERIFY(!wizard->ready()); // the admin isn't filled in
        QVERIFY(wizard->problems().isEmpty());
        wizard->touch(QStringLiteral("adminEmail"));
        QCOMPARE(wizard->problems().value(QStringLiteral("adminEmail")).toString(), QStringLiteral("email_invalid"));
        // A website without the admin panel needs no admin
        wizard->set(QStringLiteral("setup"), QStringLiteral("site"));
        QVERIFY(wizard->ready());
        wizard->set(QStringLiteral("addApi"), true);
        QVERIFY(wizard->form().value(QStringLiteral("addApi")).toBool());
        wizard->set(QStringLiteral("setup"), QStringLiteral("api"));
        QVERIFY(!wizard->form().value(QStringLiteral("addApi")).toBool()); // only with a website
        // Languages and the default
        wizard->set(QStringLiteral("languages"), QStringList{QStringLiteral("ar"), QStringLiteral("ku")});
        QCOMPARE(wizard->form().value(QStringLiteral("defaultLanguage")).toString(), QStringLiteral("ar"));
        wizard->set(QStringLiteral("defaultLanguage"), QStringLiteral("ku"));
        QCOMPARE(wizard->form().value(QStringLiteral("defaultLanguage")).toString(), QStringLiteral("ku"));
        wizard->set(QStringLiteral("defaultLanguage"), QStringLiteral("en")); // not chosen
        QCOMPARE(wizard->form().value(QStringLiteral("defaultLanguage")).toString(), QStringLiteral("ku"));
        // The plan shows no password
        wizard->set(QStringLiteral("setup"), QStringLiteral("site-admin"));
        wizard->set(QStringLiteral("adminPassword"), QStringLiteral("secret-for-test"));
        for (const QVariant &step : wizard->plan())
            QVERIFY(!step.toMap().value(QStringLiteral("shown")).toString().contains(QStringLiteral("secret-for-test")));
        studio.showProjects();
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
    }

    void everyWizardDemoLoads()
    {
        Language language;
        Studio studio(&language);
        QVERIFY(studio.startDemo(QStringLiteral(FLOSTUDIO_SOURCE "/app/demo/new-create.json")).ok());
        QCOMPARE(studio.page(), QStringLiteral("new"));
        QCOMPARE(studio.wizard()->state(), QStringLiteral("running"));
        QCOMPARE(studio.wizard()->steps().size(), 6);
        studio.wizard()->start(); // a demo runs nothing
        QCOMPARE(studio.wizard()->state(), QStringLiteral("running"));
    }

    // A real new project with Composer (FLOSTUDIO_WIZARD_TESTS=1; CI): the
    // website + admin panel on SQLite, with its first admin
    void makesARealProject()
    {
        if (qEnvironmentVariable("FLOSTUDIO_WIZARD_TESTS") != u"1")
            QSKIP("FLOSTUDIO_WIZARD_TESTS is not set");
        const QTemporaryDir dir;
        Language language;
        Studio studio(&language);
        studio.start();
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools(), 60000);
        studio.showNewProject();
        NewProjectWizard *wizard = studio.wizard();
        wizard->set(QStringLiteral("parentDir"), dir.path());
        wizard->setName(QStringLiteral("wizard-site"));
        wizard->set(QStringLiteral("setup"), QStringLiteral("site-admin"));
        wizard->set(QStringLiteral("languages"), QStringList{QStringLiteral("en"), QStringLiteral("ku")});
        wizard->set(QStringLiteral("adminName"), QStringLiteral("Studio Admin"));
        wizard->set(QStringLiteral("adminEmail"), QStringLiteral("admin@example.com"));
        wizard->set(QStringLiteral("adminPassword"), QStringLiteral("wizard-pass-123"));
        QVERIFY(wizard->ready());
        QSignalSpy created(wizard, &NewProjectWizard::created);
        wizard->start();
        QVERIFY(studio.commandRunning()); // quitting asks first
        QTRY_VERIFY_WITH_TIMEOUT(wizard->state() != u"running", 600000);
        if (wizard->state() != u"done") {
            for (const QVariant &step : wizard->steps())
                qWarning() << step.toMap();
            qWarning().noquote() << wizard->lines().join(u'\n');
        }
        QCOMPARE(wizard->state(), QStringLiteral("done"));
        QCOMPARE(created.count(), 1);
        // The password went in, and was never shown
        QVERIFY(!wizard->lines().join(u'\n').contains(QStringLiteral("wizard-pass-123")));
        QVERIFY(wizard->lines().join(u'\n').contains(QStringLiteral("admin@example.com")));
        // The page stays until the person opens the project, which is listed
        const QString folder = QDir(dir.path()).filePath(QStringLiteral("wizard-site"));
        QCOMPARE(studio.page(), QStringLiteral("new"));
        studio.openNewProject();
        QCOMPARE(studio.page(), QStringLiteral("project"));
        QCOMPARE(QDir(studio.currentPath()), QDir(folder));
        QCOMPARE(wizard->state(), QStringLiteral("editing"));
        QCOMPARE(studio.current().value(QStringLiteral("type")).toString(), QStringLiteral("siteAdmin"));
        const auto env = parseEnv(readEnvFile(folder));
        QCOMPARE(env.value(QStringLiteral("LANGUAGES")), QStringLiteral("en,ku"));
        QCOMPARE(env.value(QStringLiteral("DB_TYPE")), QStringLiteral("sqlite"));
        QVERIFY(QFileInfo::exists(folder + QStringLiteral("/storage/database.sqlite")));
        QTRY_VERIFY_WITH_TIMEOUT(checksDone(studio), 120000);
    }

    // A step that fails (a Composer that leaves half a folder): what can
    // still be fixed, trying again, starting over
    void aWizardRunThatFails()
    {
#ifdef Q_OS_WIN
        QSKIP("the fake Composer is a shell script");
#else
        const QTemporaryDir dir;
        const QString composer = dir.filePath(QStringLiteral("composer"));
        writeFile(composer, "#!/bin/sh\n"
                            "[ \"$1\" = create-project ] || exit 0\n"
                            "mkdir \"$3\" && echo half > \"$3/composer.json\"\n"
                            "echo 'Could not download hostkurd/flocms' >&2\n"
                            "exit 1\n");
        QFile(composer).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        const QString parent = dir.filePath(QStringLiteral("sites"));
        Language language;
        Studio studio(&language);
        studio.start();
        studio.setComposerPath(composer);
        NewProjectWizard *wizard = studio.wizard();
        wizard->set(QStringLiteral("parentDir"), parent);
        wizard->setName(QStringLiteral("half"));
        wizard->set(QStringLiteral("setup"), QStringLiteral("site"));
        QVERIFY(wizard->ready());
        wizard->start();
        QVERIFY(studio.commandRunning());
        QTRY_COMPARE_WITH_TIMEOUT(wizard->state(), QStringLiteral("failed"), 20000);
        QVERIFY(!studio.commandRunning());
        const QVariantMap failed = wizard->steps().constFirst().toMap();
        QCOMPARE(failed.value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
        // Composer's own words are in the output; the step says how it ended
        QCOMPARE(failed.value(QStringLiteral("code")).toString(), QStringLiteral("step_exit"));
        QCOMPARE(failed.value(QStringLiteral("detail")).toString(), QStringLiteral("1"));
        QVERIFY(wizard->lines().join(u'\n').contains(QStringLiteral("Could not download")));
        const QString folder = QDir(parent).filePath(QStringLiteral("half"));
        QVERIFY(QFileInfo::exists(folder));
        // The folder exists now, which is no problem for this run
        QVERIFY(wizard->problems().isEmpty());
        QVERIFY(wizard->ready());
        // The name and what is made stay; the database can still change
        wizard->setName(QStringLiteral("other"));
        wizard->set(QStringLiteral("setup"), QStringLiteral("site-admin"));
        wizard->set(QStringLiteral("dbHost"), QStringLiteral("db.local"));
        QCOMPARE(wizard->form().value(QStringLiteral("name")).toString(), QStringLiteral("half"));
        QCOMPARE(wizard->form().value(QStringLiteral("setup")).toString(), QStringLiteral("site"));
        QCOMPARE(wizard->form().value(QStringLiteral("dbHost")).toString(), QStringLiteral("db.local"));
        // Again: the half folder goes first (or create-project would refuse it)
        QFile::remove(folder + QStringLiteral("/composer.json"));
        writeFile(folder + QStringLiteral("/left-over"), "x");
        wizard->retry();
        QCOMPARE(wizard->state(), QStringLiteral("running"));
        QTRY_COMPARE_WITH_TIMEOUT(wizard->state(), QStringLiteral("failed"), 20000);
        QVERIFY(!QFileInfo::exists(folder + QStringLiteral("/left-over")));
        // Start over: the folder this run made goes, and the form is new
        wizard->startOver();
        QCOMPARE(wizard->state(), QStringLiteral("editing"));
        QVERIFY(!QFileInfo::exists(folder));
        QVERIFY(QFileInfo::exists(parent)); // only the project's folder
        QVERIFY(wizard->steps().isEmpty());
#endif
    }

    // --- Stage 7: the Generate tab --------------------------------------

    void theGenerateForm()
    {
        ProjectGenerate generate(QStringLiteral("/nowhere"), nullptr, true);
        QCOMPARE(generate.generator(), QStringLiteral("admin-resource"));
        QVERIFY(!generate.ready());
        QVERIFY(generate.problems().isEmpty()); // nothing touched yet
        generate.set(QStringLiteral("name"), QStringLiteral("Property"));
        generate.set(QStringLiteral("slug"), QStringLiteral("listings"));
        generate.set(QStringLiteral("migration"), true);
        QVERIFY(generate.ready());
        QCOMPARE(generate.command(), QStringLiteral("php flo make:admin-resource Property --slug=listings --migration"));
        generate.set(QStringLiteral("slug"), QStringLiteral("Listings"));
        generate.touch(QStringLiteral("slug"));
        QCOMPARE(generate.problems().value(QStringLiteral("slug")).toString(), QStringLiteral("slug_name"));
        // Another generator starts empty
        generate.choose(QStringLiteral("migration"));
        QCOMPARE(generate.form().value(QStringLiteral("name")).toString(), QString());
        generate.set(QStringLiteral("name"), QStringLiteral("add_slug_to_posts"));
        generate.set(QStringLiteral("migrationKind"), QStringLiteral("change"));
        generate.set(QStringLiteral("table"), QStringLiteral("posts"));
        QCOMPARE(generate.command(), QStringLiteral("php flo make:migration add_slug_to_posts --table=posts"));
        // Without the admin panel: its generators can't run
        generate.choose(QStringLiteral("block"));
        generate.set(QStringLiteral("name"), QStringLiteral("LatestProjects"));
        QVERIFY(generate.ready());
        generate.setHasAdmin(false);
        QCOMPARE(generate.generator(), QStringLiteral("controller"));
        generate.choose(QStringLiteral("block"));
        generate.set(QStringLiteral("name"), QStringLiteral("LatestProjects"));
        QVERIFY(!generate.ready());
        // A demo runs nothing
        generate.runPreview();
        generate.create();
        QVERIFY(!generate.busy());
    }

    void theGenerateDemoShowsTheFiles()
    {
        Language language;
        Studio studio(&language);
        QVERIFY(studio.startDemo(QStringLiteral(FLOSTUDIO_SOURCE "/app/demo/generate-widget.json")).ok());
        ProjectGenerate *generate = studio.generate();
        QVERIFY(generate);
        QCOMPARE(generate->generator(), QStringLiteral("widget"));
        const QVariantMap preview = generate->preview();
        QVERIFY(preview.value(QStringLiteral("current")).toBool());
        const QVariantList files = preview.value(QStringLiteral("files")).toList();
        QCOMPARE(files.size(), 2);
        // config/admin.php: the one line make:admin-widget added, with context
        const QVariantMap config = files.at(1).toMap();
        QCOMPARE(config.value(QStringLiteral("kind")).toString(), QStringLiteral("changed"));
        QStringList added;
        for (const QVariant &line : config.value(QStringLiteral("lines")).toList()) {
            if (line.toMap().value(QStringLiteral("kind")).toString() == u"+")
                added << line.toMap().value(QStringLiteral("text")).toString();
        }
        QCOMPARE(added, QStringList{QStringLiteral("        \\App\\Admin\\Widgets\\PostStats::class,")});
        // The resources from admin:list, for the widget's resource
        QCOMPARE(studio.checks()->database().value(QStringLiteral("resources")).toStringList(), QStringList{QStringLiteral("Post")});
        // Changing the form makes the preview old
        generate->set(QStringLiteral("name"), QStringLiteral("Other"));
        QVERIFY(!generate->preview().value(QStringLiteral("current")).toBool());
    }

    // A real project (FLOSTUDIO_REAL_PROJECT): the preview runs the real
    // generator in a copy and the project stays as it was; Create writes it
    void generatesInARealProject()
    {
        const QString project = qEnvironmentVariable("FLOSTUDIO_REAL_PROJECT");
        if (project.isEmpty())
            QSKIP("FLOSTUDIO_REAL_PROJECT is not set");
        Language language;
        Studio studio(&language);
        studio.start();
        studio.addProject(QUrl::fromLocalFile(project));
        QTRY_VERIFY_WITH_TIMEOUT(checksDone(studio) && !studio.checkingTools(), 60000);
        ProjectGenerate *generate = studio.generate();
        QVERIFY(generate);
        const QDir migrations(project + QStringLiteral("/database/migrations"));
        const QStringList before = migrations.entryList(QDir::Files);

        generate->choose(QStringLiteral("migration"));
        generate->set(QStringLiteral("name"), QStringLiteral("studio_generate_test"));
        generate->runPreview();
        QVERIFY(generate->busy());
        QTRY_VERIFY_WITH_TIMEOUT(!generate->working(), 60000);
        QVariantMap preview = generate->preview();
        QVERIFY2(preview.value(QStringLiteral("state")).toString() == u"done",
                 qUtf8Printable(preview.value(QStringLiteral("error")).toString() + u' ' + preview.value(QStringLiteral("detail")).toString()
                                + u' ' + preview.value(QStringLiteral("message")).toString()));
        QVariantList files = preview.value(QStringLiteral("files")).toList();
        QCOMPARE(files.size(), 1);
        const QString path = files.at(0).toMap().value(QStringLiteral("path")).toString();
        QVERIFY2(path.startsWith(QStringLiteral("database/migrations/")) && path.endsWith(QStringLiteral("_studio_generate_test.php")),
                 qUtf8Printable(path));
        QCOMPARE(files.at(0).toMap().value(QStringLiteral("kind")).toString(), QStringLiteral("added"));
        QCOMPARE(migrations.entryList(QDir::Files), before); // only the copy
        QVERIFY(preview.value(QStringLiteral("outside")).toStringList().isEmpty());
        QVERIFY(!preview.value(QStringLiteral("notRemoved")).toBool());
        // The copy is gone
        QVERIFY(QDir(previewsFolder()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());

        // With the admin panel: a resource and its migration, nothing in the project
        if (generate->hasAdmin()) {
            generate->choose(QStringLiteral("admin-resource"));
            generate->set(QStringLiteral("name"), QStringLiteral("StudioThing"));
            generate->set(QStringLiteral("migration"), true);
            generate->runPreview();
            QTRY_VERIFY_WITH_TIMEOUT(!generate->working(), 60000);
            files = generate->preview().value(QStringLiteral("files")).toList();
            QCOMPARE(files.size(), 2);
            QCOMPARE(files.at(0).toMap().value(QStringLiteral("path")).toString(), QStringLiteral("admin/Resources/StudioThingResource.php"));
            QVERIFY(!QFileInfo::exists(project + QStringLiteral("/admin/Resources/StudioThingResource.php")));
            QCOMPARE(migrations.entryList(QDir::Files), before);
            generate->choose(QStringLiteral("migration"));
            generate->set(QStringLiteral("name"), QStringLiteral("studio_generate_test"));
        }

        // Create: in the project, and the database is read again
        QSignalSpy changed(generate, &ProjectGenerate::changedProject);
        generate->create();
        QVERIFY(studio.commandRunning());
        QTRY_VERIFY_WITH_TIMEOUT(!generate->busy(), 60000);
        QCOMPARE(generate->created().value(QStringLiteral("state")).toString(), QStringLiteral("done"));
        QCOMPARE(changed.count(), 1);
        QStringList made = migrations.entryList({QStringLiteral("*_studio_generate_test.php")}, QDir::Files);
        QCOMPARE(made.size(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(checksDone(studio), 60000);
        QCOMPARE(studio.checks()->migrations().value(QStringLiteral("pending")).toInt(), 1);

        // Again: the generator says it is there, in the preview
        generate->runPreview();
        QTRY_VERIFY_WITH_TIMEOUT(!generate->working(), 60000);
        preview = generate->preview();
        QCOMPARE(preview.value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
        QVERIFY2(preview.value(QStringLiteral("message")).toString().contains(QStringLiteral("already exists")),
                 qUtf8Printable(preview.value(QStringLiteral("message")).toString()));
        QVERIFY(QFile::remove(migrations.filePath(made.constFirst())));
        studio.checks()->refreshDatabase();
        QTRY_VERIFY_WITH_TIMEOUT(checksDone(studio), 60000);
    }

    // --- Stage 5: Doctor and the database ---------------------------------

    void theDoctorDemoReadsTheRealOutput()
    {
        Language language;
        Studio studio(&language);
        QVERIFY(studio.startDemo(QStringLiteral(FLOSTUDIO_SOURCE "/app/demo/doctor.json")).ok());
        ProjectChecks *checks = studio.checks();
        QVERIFY(checks);
        const QVariantMap doctor = checks->doctor();
        QCOMPARE(doctor.value(QStringLiteral("state")).toString(), QStringLiteral("done"));
        QCOMPARE(doctor.value(QStringLiteral("warnings")).toInt(), 2); // storage/cache/ and the migrations
        QVERIFY(doctor.value(QStringLiteral("ok")).toInt() > 15);
        QVERIFY(!doctor.value(QStringLiteral("checkedAt")).toString().isEmpty());
        const QVariantMap admin = checks->adminDoctor();
        QCOMPARE(admin.value(QStringLiteral("problems")).toInt(), 1);
        QVERIFY(admin.value(QStringLiteral("heading")).toString().startsWith(QStringLiteral("flocms-admin")));
        const QVariantMap database = checks->database();
        QCOMPARE(database.value(QStringLiteral("database")).toString(), QStringLiteral("fectum"));
        // admin:list links the tables it manages
        bool linked = false;
        for (const QVariant &row : database.value(QStringLiteral("tables")).toList())
            linked |= row.toMap().value(QStringLiteral("adminUrl")).toString() == u"/admin/media";
        QVERIFY(linked);
        QCOMPARE(checks->migrations().value(QStringLiteral("pending")).toInt(), 2);
        // Newest first: what waits is on top
        QCOMPARE(checks->migrations().value(QStringLiteral("rows")).toList().first().toMap().value(QStringLiteral("state")).toString(),
                 QStringLiteral("pending"));
        // A demo runs nothing
        QVERIFY(!checks->runAction(QStringLiteral("migrate"), {}, false));
        checks->runQuick(QStringLiteral("cache:clear"));
        QVERIFY(!checks->busy());
        QCOMPARE(checks->plan(QStringLiteral("fresh")).value(QStringLiteral("confirm")).toString(), QStringLiteral("typeDatabase"));
        QVERIFY(checks->plan(QStringLiteral("drop")).isEmpty());
    }

    // What runs is what was agreed to: .env read again at run time
    void runsOnlyWhatWasConfirmed()
    {
        const QTemporaryDir dir;
        const QString site = dir.filePath(QStringLiteral("fresh"));
        makeSite(site); // DB_TYPE=sqlite, DB_NAME=storage/database.sqlite
        Language language;
        Studio studio(&language);
        studio.start();
        studio.addProject(QUrl::fromLocalFile(site));
        ProjectChecks *checks = studio.checks();
        QTRY_VERIFY_WITH_TIMEOUT(checksDone(studio), 60000);

        // Fresh asks for the name .env gives
        QVariantMap plan = checks->plan(QStringLiteral("fresh"));
        QCOMPARE(plan.value(QStringLiteral("confirm")).toString(), QStringLiteral("typeDatabase"));
        QCOMPARE(plan.value(QStringLiteral("database")).toString(), QStringLiteral("storage/database.sqlite"));
        QVERIFY(!plan.value(QStringLiteral("production")).toBool());
        // .env changes while the question is open: the old name no longer runs it
        QVERIFY(writeEnvValues(site, {{QStringLiteral("DB_NAME"), QStringLiteral("storage/other.sqlite")}}).ok());
        QVERIFY(!checks->runAction(QStringLiteral("fresh"), QStringLiteral("storage/database.sqlite"), false));
        QVERIFY(!checks->busy());
        // ... and the site turns production: nothing asked for that runs
        QVERIFY(writeEnvValues(site, {{QStringLiteral("APP_ENV"), QStringLiteral("production")}}).ok());
        QVERIFY(!checks->runAction(QStringLiteral("rollback"), {}, false));
        QVERIFY(!checks->runAction(QStringLiteral("migrate"), {}, false));
        QVERIFY(!checks->busy());
        plan = checks->plan(QStringLiteral("migrate"));
        QVERIFY(plan.value(QStringLiteral("production")).toBool());
        QCOMPARE(plan.value(QStringLiteral("confirm")).toString(), QStringLiteral("ask"));
        QVERIFY(plan.value(QStringLiteral("command")).toString().endsWith(QStringLiteral("--force")));
        // The name as .env says it now, for the production asked about: runs
        QVERIFY(checks->runAction(QStringLiteral("fresh"), QStringLiteral(" storage/other.sqlite "), true));
        QCOMPARE(checks->output().value(QStringLiteral("command")).toString(), QStringLiteral("fresh"));
        QTRY_VERIFY_WITH_TIMEOUT(!checks->busy(), 60000);
        QTRY_VERIFY_WITH_TIMEOUT(checksDone(studio) && !studio.checkingTools(), 60000);
    }

    void readsARealProjectsDoctorAndDatabase()
    {
        const QString project = qEnvironmentVariable("FLOSTUDIO_REAL_PROJECT");
        if (project.isEmpty())
            QSKIP("FLOSTUDIO_REAL_PROJECT is not set");
        Language language;
        Studio studio(&language);
        studio.start();
        studio.addProject(QUrl::fromLocalFile(project));
        ProjectChecks *checks = studio.checks();
        QVERIFY(checks);
        QTRY_VERIFY_WITH_TIMEOUT(checksDone(studio), 60000);
        const QVariantMap doctor = checks->doctor();
        QCOMPARE(doctor.value(QStringLiteral("state")).toString(), QStringLiteral("done"));
        QVERIFY(doctor.value(QStringLiteral("checks")).toList().size() > 10);
        // The database answers in CI (SQLite, or the runner's MySQL)
        const QVariantMap database = checks->database();
        QVERIFY2(database.value(QStringLiteral("state")).toString() == u"done",
                 qUtf8Printable(database.value(QStringLiteral("detail")).toString()));
        QVERIFY(!database.value(QStringLiteral("tables")).toList().isEmpty());
        QCOMPARE(checks->migrations().value(QStringLiteral("state")).toString(), QStringLiteral("done"));
        QCOMPARE(checks->migrations().value(QStringLiteral("pending")).toInt(), 0);

        // Fresh without the name typed runs nothing
        QVERIFY(!checks->runAction(QStringLiteral("fresh"), QStringLiteral("not the name"), false));
        QVERIFY(!checks->busy());
        // Migrate with nothing to run, then a quick command, with their output
        QSignalSpy changed(checks, &ProjectChecks::changedData);
        QVERIFY(checks->runAction(QStringLiteral("migrate"), {}, false));
        QVERIFY(checks->busy());
        QTRY_VERIFY_WITH_TIMEOUT(!checks->busy(), 60000);
        QCOMPARE(checks->output().value(QStringLiteral("state")).toString(), QStringLiteral("done"));
        QCOMPARE(changed.count(), 1);
        checks->runQuick(QStringLiteral("cache:clear"));
        QTRY_VERIFY_WITH_TIMEOUT(!checks->busy(), 60000);
        QCOMPARE(checks->output().value(QStringLiteral("state")).toString(), QStringLiteral("done"));
        QVERIFY(checks->output().value(QStringLiteral("message")).toString().contains(QStringLiteral("cache")));
        checks->runQuick(QStringLiteral("migrate:fresh")); // not a quick command
        QVERIFY(!checks->busy());
        QTRY_VERIFY_WITH_TIMEOUT(checksDone(studio) && !studio.checkingTools(), 60000);
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

    void theLogStaysConsistent()
    {
        // Qt's model checker over inserts, trims, filters and clears
        LogModel logs(nullptr, 300);
        QAbstractItemModelTester tester(&logs, QAbstractItemModelTester::FailureReportingMode::QtTest);
        logs.setSearch(QStringLiteral("7"));
        for (int batch = 0; batch < 12; ++batch) {
            for (int i = 0; i < 50; ++i)
                logs.addAppLogLine(QStringLiteral("[2026-10-24 10:00:00] %1: line %2").arg(i % 5 == 0 ? QStringLiteral("error") : QStringLiteral("info")).arg(batch * 50 + i));
            logs.flush();
            QVERIFY(logs.total() <= 300);
            for (int row = 0; row < logs.rowCount(); ++row)
                QVERIFY(logs.data(logs.index(row), LogModel::TextRole).toString().contains(u'7'));
            if (batch == 6)
                logs.setProblemsOnly(true);
        }
        logs.setSearch({});
        logs.setProblemsOnly(false);
        QCOMPARE(logs.shown(), logs.total());
        logs.clear();
        QCOMPARE(logs.rowCount(), 0);
    }

    void appLogTimesInThisComputersZone()
    {
        LogModel logs;
        logs.addAppLogLine(QStringLiteral("[2026-10-24 09:53:57] info: written in UTC"));
        logs.flush();
        QCOMPARE(logs.data(logs.index(0), LogModel::TimeRole).toString(), QStringLiteral("09:53:57"));
        QSignalSpy changed(&logs, &QAbstractItemModel::dataChanged);
        logs.setAppTimeOffset(0); // PHP said UTC
        QCOMPARE(changed.count(), 1);
        QDateTime utc = QDateTime::fromString(QStringLiteral("2026-10-24 09:53:57"), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        utc.setTimeZone(QTimeZone(0));
        QCOMPARE(logs.data(logs.index(0), LogModel::TimeRole).toString(), utc.toLocalTime().toString(QStringLiteral("HH:mm:ss")));
        int seconds = 0;
        QVERIFY(ProjectServer::readUtcOffset(QStringLiteral("+03:00\n"), &seconds));
        QCOMPARE(seconds, 3 * 3600);
        QVERIFY(ProjectServer::readUtcOffset(QStringLiteral("-05:30"), &seconds));
        QCOMPARE(seconds, -(5 * 3600 + 1800));
        QVERIFY(ProjectServer::readUtcOffset(QStringLiteral("Z"), &seconds));
        QCOMPARE(seconds, 0);
        QVERIFY(!ProjectServer::readUtcOffset(QStringLiteral("Warning: something"), &seconds));
    }

    // A server that says it serves but never answers, then takes a second to
    // stop (a shell script as PHP): macOS and Linux
    void aServerThatNeverAnswers()
    {
#ifdef Q_OS_WIN
        QSKIP("the fake PHP is a shell script");
#else
        const QTemporaryDir dir;
        const QString site = dir.filePath(QStringLiteral("slow"));
        makeSite(site);
        const QString php = dir.filePath(QStringLiteral("php"));
        writeFile(php, "#!/bin/sh\n"
                       "[ \"$2\" = serve ] || exit 1\n" // Studio asking its version: no PHP here
                       "echo \"[OK] Serving $PWD at http://127.0.0.1:9 (Ctrl+C to stop)\"\n"
                       "trap 'sleep 1; exit 0' TERM\n"
                       "while true; do sleep 0.1; done\n");
        QFile(php).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        ProjectServer::setStartTimeout(1);
        Language language;
        Studio studio(&language);
        studio.start();
        studio.setPhpPath(php);
        studio.addProject(QUrl::fromLocalFile(site));
        for (int round = 0; round < 3; ++round) {
            studio.startServer();
            QCOMPARE(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("starting"));
            QTRY_COMPARE_WITH_TIMEOUT(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("failed"), 10000);
            QCOMPARE(studio.server().value(QStringLiteral("error")).toString(), QStringLiteral("server_no_answer"));
            // Still stopping: a second start waits, and the end of the first
            // is not taken for it
            studio.startServer();
            QCOMPARE(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("failed"));
            QTRY_COMPARE_WITH_TIMEOUT(studio.findChildren<QProcess *>().size(), 0, 10000); // none pile up
            QCOMPARE(studio.server().value(QStringLiteral("error")).toString(), QStringLiteral("server_no_answer"));
        }
        // Stop while starting
        studio.startServer();
        studio.stopServer();
        QTRY_COMPARE_WITH_TIMEOUT(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("stopped"), 10000);
        ProjectServer::setStartTimeout(20);
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
#endif
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
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
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
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
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
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
    }

    void removingARunningProjectStopsIt()
    {
        const QString project = qEnvironmentVariable("FLOSTUDIO_REAL_PROJECT");
        if (project.isEmpty())
            QSKIP("FLOSTUDIO_REAL_PROJECT is not set");
        Language language;
        Studio studio(&language);
        studio.start();
        studio.setServerPort(18600);
        studio.addProject(QUrl::fromLocalFile(project));
        studio.startServer();
        QTRY_COMPARE_WITH_TIMEOUT(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("running"), 30000);
        const int port = urlPort(studio.server().value(QStringLiteral("url")).toString());
        studio.removeProject(studio.currentPath());
        QCOMPARE(studio.server().value(QStringLiteral("state")).toString(), QStringLiteral("stopped"));
        QTRY_VERIFY_WITH_TIMEOUT(!portAnswers(QStringLiteral("127.0.0.1"), port, 300), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
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
            QTRY_VERIFY_WITH_TIMEOUT(!studio.checkingTools() && checksDone(studio), 60000);
        }
        QTRY_VERIFY_WITH_TIMEOUT(!portAnswers(QStringLiteral("127.0.0.1"), port, 300), 10000);
        // and nothing is left noted for the next start
        QFile record(serverRecordFile(settingsFolder()));
        if (record.open(QIODevice::ReadOnly))
            QCOMPARE(QJsonDocument::fromJson(record.readAll()).object(), QJsonObject());
    }
};

QTEST_MAIN(TestStudio)
#include "tst_studio.moc"
