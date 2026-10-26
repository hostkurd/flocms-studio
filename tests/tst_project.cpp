#include "testutil.h"

#include <flostudio/core/project.h>

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

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

QByteArray fixtureBytes(const QString &relative)
{
    return TestUtil::fixture(relative).toUtf8();
}

// A project made of the real files of tests/fixtures/project (a
// site-admin setup with the API), with `home` and .env as given.
void makeProject(const QString &dir, const QString &home = QStringLiteral("site"), const QByteArray &env = {},
                 bool withLock = true)
{
    writeFile(dir + QStringLiteral("/flo"), "#!/usr/bin/env php\n<?php\n");
    writeFile(dir + QStringLiteral("/composer.json"), fixtureBytes(QStringLiteral("project/composer.json")));
    QString config = TestUtil::fixture(QStringLiteral("project/config.php"));
    config.replace(QStringLiteral("Config::set('home', 'site');"), QStringLiteral("Config::set('home', '%1');").arg(home));
    writeFile(dir + QStringLiteral("/config/config.php"), config.toUtf8());
    if (withLock)
        writeFile(dir + QStringLiteral("/composer.lock"), fixtureBytes(QStringLiteral("project/composer.lock")));
    if (!env.isNull())
        writeFile(dir + QStringLiteral("/.env"), env);
}

// A plain site: the skeleton's composer.json without the admin and API.
void makeSite(const QString &dir)
{
    writeFile(dir + QStringLiteral("/flo"), "");
    writeFile(dir + QStringLiteral("/composer.json"),
              R"({"name":"hostkurd/flocms","require":{"php":"^8.1","hostkurd/flocms-core":"^2.8.0"}})");
}

} // namespace

class TestProject : public QObject
{
    Q_OBJECT

private slots:
    void recognisesProjectsLikeTheCli()
    {
        const QTemporaryDir dir;
        makeProject(dir.filePath(QStringLiteral("full")));
        QVERIFY(isProject(dir.filePath(QStringLiteral("full"))));

        // A site that requires core (renamed in composer.json), even only in require-dev
        writeFile(dir.filePath(QStringLiteral("acme/composer.json")), R"({"name":"acme/site","require-dev":{"hostkurd/flocms-core":"^2.8"}})");
        QVERIFY(isProject(dir.filePath(QStringLiteral("acme"))));

        writeFile(dir.filePath(QStringLiteral("laravel/composer.json")), R"({"require":{"laravel/framework":"^11"}})");
        QVERIFY(!isProject(dir.filePath(QStringLiteral("laravel"))));
        writeFile(dir.filePath(QStringLiteral("broken/composer.json")), "{");
        QVERIFY(!isProject(dir.filePath(QStringLiteral("broken"))));
        QVERIFY(!isProject(dir.filePath(QStringLiteral("nothing"))));
    }

    void findsTheLauncher()
    {
        const QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a/composer.json")), R"({"name":"hostkurd/flocms"})");
        QCOMPARE(floLauncher(dir.filePath(QStringLiteral("a"))), QString());
        writeFile(dir.filePath(QStringLiteral("a/vendor/bin/flo")), "");
        QCOMPARE(floLauncher(dir.filePath(QStringLiteral("a"))),
                 QDir::toNativeSeparators(dir.filePath(QStringLiteral("a/vendor/bin/flo"))));
        writeFile(dir.filePath(QStringLiteral("a/flo")), "");
        QCOMPARE(floLauncher(dir.filePath(QStringLiteral("a"))), QDir::toNativeSeparators(dir.filePath(QStringLiteral("a/flo"))));
    }

    void readsHomeLikeSetup_data()
    {
        QTest::addColumn<QString>("config");
        QTest::addColumn<int>("home");
        QTest::newRow("skeleton") << TestUtil::fixture(QStringLiteral("project/config.php")) << int(Home::Site);
        QTest::newRow("admin") << "Config::set('home', 'admin');" << int(Home::Admin);
        QTest::newRow("api, no space") << "Config::set('home','api');" << int(Home::Api);
        QTest::newRow("several spaces") << "Config::set('home',    'admin');" << int(Home::Admin);
        QTest::newRow("unknown") << "Config::set('home', 'shop');" << int(Home::Site);
        QTest::newRow("double quotes") << "Config::set(\"home\", \"admin\");" << int(Home::Site);
        QTest::newRow("FloCMS 1.x") << "<?php Config::set('routes', $r);" << int(Home::Site);
        QTest::newRow("first wins") << "Config::set('home', 'api');\nConfig::set('home', 'admin');" << int(Home::Api);
    }

    void readsHomeLikeSetup()
    {
        QFETCH(QString, config);
        QFETCH(int, home);
        QCOMPARE(int(parseHome(config)), home);
    }

    void typesMatchSetupsChoices()
    {
        QCOMPARE(projectType(Home::Site, false, false), ProjectType::Site);
        QCOMPARE(projectType(Home::Site, true, false), ProjectType::SiteAdmin);
        QCOMPARE(projectType(Home::Site, false, true), ProjectType::SiteApi);
        QCOMPARE(projectType(Home::Site, true, true), ProjectType::SiteAdminApi);
        QCOMPARE(projectType(Home::Admin, true, false), ProjectType::Admin);
        QCOMPARE(projectType(Home::Admin, true, true), ProjectType::ApiAdmin); // setup api-admin: home admin
        QCOMPARE(projectType(Home::Api, false, true), ProjectType::Api);
        QCOMPARE(projectType(Home::Api, true, true), ProjectType::ApiAdmin);
        QCOMPARE(setupChoice(ProjectType::SiteAdmin), QStringLiteral("site-admin"));
        QCOMPARE(setupChoice(ProjectType::ApiAdmin), QStringLiteral("api-admin"));
        QCOMPARE(setupChoice(ProjectType::SiteApi), QString());
    }

    void readsTheRealLockFile()
    {
        const auto [ours, others] = lockedPackages(fixtureBytes(QStringLiteral("project/composer.lock")));
        // tests/fixtures/flo/versions.txt lists the same install
        const QStringList expected = TestUtil::fixture(QStringLiteral("flo/versions.txt")).split(u'\n', Qt::SkipEmptyParts).mid(1);
        QStringList got;
        for (const Package &package : ours)
            got << package.name + QStringLiteral(" v") + package.version;
        QCOMPARE(got, expected);
        QCOMPARE(others, (QList<Package>{{QStringLiteral("psr/log"), QStringLiteral("3.0.2")}}));
        QVERIFY(lockedPackages("not json").first.isEmpty());
    }

    void versionsKeepNonNumericNames()
    {
        const auto [ours, others] = lockedPackages(
            R"({"packages":[{"name":"hostkurd/flocms-core","version":"dev-main"},{"name":"hostkurd/x","version":"v2.0.0-beta1"}],
                "packages-dev":[{"name":"phpunit/phpunit","version":"11.5.0"},{"version":"1.0"}]})");
        QCOMPARE(ours, (QList<Package>{{QStringLiteral("hostkurd/flocms-core"), QStringLiteral("dev-main")},
                                      {QStringLiteral("hostkurd/x"), QStringLiteral("2.0.0-beta1")}}));
        QCOMPARE(others.size(), 1);
    }

    void databaseLikeConnectionFactory()
    {
        const QString dir = QDir::cleanPath(QDir::tempPath() + QStringLiteral("/site"));
        const auto db = [&dir](const QString &env) {
            QMap<QString, QString> values;
            for (const QString &line : env.split(u'\n', Qt::SkipEmptyParts))
                values.insert(line.section(u'=', 0, 0), line.section(u'=', 1));
            return databaseConfig(values, dir);
        };
        const DatabaseConfig example = db(QStringLiteral("DB_TYPE=mysql\nDB_HOST=127.0.0.1\nDB_PORT=3306\nDB_NAME=\nDB_USERNAME="));
        QCOMPARE(example.driver, QStringLiteral("mysql"));
        QCOMPARE(example.host, QStringLiteral("127.0.0.1"));
        QVERIFY(!example.configured);

        const DatabaseConfig mysql = db(QStringLiteral("DB_NAME=shop\nDB_USERNAME=root"));
        QCOMPARE(mysql.driver, QStringLiteral("mysql"));
        QCOMPARE(mysql.host, QStringLiteral("localhost")); // the CLI's default
        QCOMPARE(mysql.port, 3306);
        QVERIFY(mysql.configured);
        QVERIFY(!db(QStringLiteral("DB_NAME=shop")).configured); // mysql needs DB_USERNAME too

        const DatabaseConfig sqlite = db(QStringLiteral("DB_TYPE=SQLite\nDB_NAME=storage/database.sqlite"));
        QCOMPARE(sqlite.driver, QStringLiteral("sqlite"));
        QCOMPARE(sqlite.name, dir + QStringLiteral("/storage/database.sqlite"));
        QVERIFY(sqlite.configured);
        QCOMPARE(db(QStringLiteral("DB_TYPE=sqlite\nDB_NAME=/var/db.sqlite")).name, QStringLiteral("/var/db.sqlite"));
        QCOMPARE(db(QStringLiteral("DB_TYPE=sqlite\nDB_NAME=C:\\db.sqlite")).name, QStringLiteral("C:\\db.sqlite"));
        QCOMPARE(db(QStringLiteral("DB_TYPE=sqlite\nDB_NAME=:memory:")).name, QStringLiteral(":memory:"));

        const DatabaseConfig dsn = db(QStringLiteral("DB_DSN=pgsql:host=x;dbname=y\nDB_TYPE=sqlite"));
        QCOMPARE(dsn.driver, QStringLiteral("pgsql"));
        QVERIFY(dsn.usesDsn && dsn.configured);
        QCOMPARE(db(QStringLiteral("DB_CONNECTION=sqlite\nDB_NAME=a.db")).driver, QStringLiteral("sqlite"));
        QCOMPARE(db(QStringLiteral("DB_NAME=null\nDB_USERNAME=root")).configured, false); // "null" is null
        QCOMPARE(db(QStringLiteral("DB_NAME=x\nDB_USERNAME=y\nDB_PORT=abc")).port, 0);
    }

    void readsTheWholeProject()
    {
        const QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("studio-full"));
        QString env = TestUtil::fixture(QStringLiteral("project/env.example"));
        env.replace(QStringLiteral("APP_NAME=FloCMS"), QStringLiteral("APP_NAME=\"Studio Full\""));
        env.replace(QStringLiteral("DB_NAME="), QStringLiteral("DB_NAME=studio_full"));
        env.replace(QStringLiteral("DB_USERNAME="), QStringLiteral("DB_USERNAME=root"));
        env += QStringLiteral("DB_NAME=again\n");
        makeProject(path, QStringLiteral("site"), env.toUtf8());

        const auto info = projectInfo(path);
        QVERIFY(info.ok());
        QCOMPARE(info->summary.folderName, QStringLiteral("studio-full"));
        QCOMPARE(info->summary.home, Home::Site);
        QVERIFY(info->summary.hasAdmin);
        QVERIFY(info->summary.hasApi);
        QCOMPARE(projectType(info->summary), ProjectType::SiteAdminApi);
        QCOMPARE(info->appName, QStringLiteral("Studio Full"));
        QCOMPARE(info->appEnv, QStringLiteral("local"));
        QVERIFY(!info->production);
        QVERIFY(info->appDebug);
        QCOMPARE(info->skeletonVersion, QStringLiteral("2.0.0"));
        QCOMPARE(info->languages, (QStringList{QStringLiteral("en"), QStringLiteral("ar"), QStringLiteral("ku")}));
        QCOMPARE(info->defaultLanguage, QStringLiteral("en"));
        QCOMPARE(info->database.name, QStringLiteral("studio_full")); // the first line, as the website
        QVERIFY(!info->envByteOrderMark);
        QCOMPARE(info->duplicateEnvKeys, QStringList{QStringLiteral("DB_NAME")});
        QVERIFY(info->database.configured);
        QCOMPARE(info->packages.size(), 7);
        QVERIFY(info->hasEnv && info->hasLock && !info->hasVendor);
        QVERIFY(!info->launcher.isEmpty());
    }

    void aProjectWithoutEnvIsProduction()
    {
        const QTemporaryDir dir;
        makeProject(dir.path(), QStringLiteral("admin"), QByteArray(), false);
        const auto info = projectInfo(dir.path());
        QVERIFY(info.ok());
        QVERIFY(info->production); // FloCMS's default without APP_ENV
        QVERIFY(!info->hasEnv && !info->hasLock);
        QVERIFY(info->packages.isEmpty());
        QCOMPARE(projectType(info->summary), ProjectType::ApiAdmin);
    }

    void infoErrors()
    {
        const QTemporaryDir dir;
        QCOMPARE(projectInfo(dir.filePath(QStringLiteral("gone"))).error().code, QStringLiteral("project_missing"));
        QCOMPARE(projectInfo(dir.path()).error().code, QStringLiteral("not_flocms"));
    }

    void listsFoldersAndAddedProjects()
    {
        const QTemporaryDir dir;
        const QString sites = dir.filePath(QStringLiteral("Sites"));
        makeSite(sites + QStringLiteral("/beta"));
        makeProject(sites + QStringLiteral("/Alpha"), QStringLiteral("api"));
        QDir().mkpath(sites + QStringLiteral("/notes"));
        const QString elsewhere = dir.filePath(QStringLiteral("work/gamma"));
        makeSite(elsewhere);
        const QList<ProjectSummary> list = listProjects(
            {sites, dir.filePath(QStringLiteral("none"))},
            {elsewhere, sites + QStringLiteral("/beta"), sites + QStringLiteral("/./beta"), dir.filePath(QStringLiteral("deleted"))});
        QStringList names;
        for (const ProjectSummary &project : list)
            names << project.folderName;
        QCOMPARE(names, (QStringList{QStringLiteral("Alpha"), QStringLiteral("beta"), QStringLiteral("deleted"),
                                     QStringLiteral("gamma")}));
        QVERIFY(list.at(2).missing);
        QCOMPARE(list.at(0).home, Home::Api);
        QVERIFY(!list.at(1).hasAdmin);

        // Removed from Studio: the folder's scan leaves it out; added by
        // hand it still shows
        QStringList shown;
        for (const ProjectSummary &project : listProjects({sites}, {}, {sites + QStringLiteral("/./Alpha")}))
            shown << project.folderName;
        QCOMPARE(shown, QStringList{QStringLiteral("beta")});
        QCOMPARE(listProjects({sites}, {sites + QStringLiteral("/Alpha")}, {sites + QStringLiteral("/Alpha")}).size(), 2);
        QVERIFY(sameFolder(sites + QStringLiteral("/beta"), sites + QStringLiteral("/x/../beta")));
        QVERIFY(!sameFolder(sites + QStringLiteral("/beta"), sites + QStringLiteral("/Alpha")));
        QVERIFY(!sameFolder(QString(), QString()));
    }

    // "Delete project": to the Recycle Bin / Trash, only a FloCMS project,
    // never one holding or inside a folder Studio needs
    void movesAProjectToTheTrash()
    {
        const QTemporaryDir dir;
        const QString notes = dir.filePath(QStringLiteral("notes"));
        QDir().mkpath(notes);
        QCOMPARE(moveProjectToTrash(notes).error().code, QStringLiteral("not_flocms"));
        QVERIFY(QFileInfo::exists(notes));

        const QString site = dir.filePath(QStringLiteral("shop"));
        makeSite(site);
        QDir().mkpath(site + QStringLiteral("/storage/studio"));
        QCOMPARE(moveProjectToTrash(site, {site + QStringLiteral("/storage/studio")}).error().code,
                 QStringLiteral("delete_protected"));
        QCOMPARE(moveProjectToTrash(site, {dir.path()}).error().code, QStringLiteral("delete_protected"));
        QVERIFY(QFileInfo::exists(site));

        const auto moved = moveProjectToTrash(site, {dir.filePath(QStringLiteral("elsewhere"))});
        if (!moved && moved.error().code == u"trash_failed")
            QSKIP("this system has no trash for the temporary folder");
        QVERIFY2(moved.ok(), moved.ok() ? "" : qPrintable(moved.error().code));
        QVERIFY(!QFileInfo::exists(site));
    }
};

QTEST_GUILESS_MAIN(TestProject)
#include "tst_project.moc"
