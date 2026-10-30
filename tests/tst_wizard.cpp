// The new project wizard's choices and steps (libs/core wizard.h).

#include <flostudio/core/commands.h>
#include <flostudio/core/env.h>
#include <flostudio/core/process.h>
#include <flostudio/core/wizard.h>

#include <QDir>
#include <QMap>
#include <QTemporaryDir>
#include <QTest>

using namespace FloStudio::Core;

namespace {

NewProject valid(const QString &parent)
{
    NewProject project;
    project.name = QStringLiteral("my-shop");
    project.parentDir = parent;
    project.setup = Setup::SiteAdmin;
    project.languages = {QStringLiteral("en"), QStringLiteral("ar"), QStringLiteral("ku")};
    project.defaultLanguage = QStringLiteral("ar");
    project.adminName = QStringLiteral("Studio Admin");
    project.adminEmail = QStringLiteral("admin@example.com");
    project.adminPassword = QStringLiteral("correct horse");
    return project;
}

QStringList ids(const QList<WizardStep> &steps)
{
    QStringList list;
    for (const WizardStep &step : steps)
        list << step.id;
    return list;
}

QMap<QString, QString> asMap(const QList<QPair<QString, QString>> &values)
{
    QMap<QString, QString> map;
    for (const auto &[key, value] : values)
        map.insert(key, value);
    return map;
}

QStringList fields(const QList<FieldProblem> &problems)
{
    QStringList list;
    for (const FieldProblem &problem : problems)
        list << problem.field + u'=' + problem.code;
    return list;
}

} // namespace

class TestWizard : public QObject
{
    Q_OBJECT

private slots:
    void setups()
    {
        for (const QString &name : {QStringLiteral("site-admin"), QStringLiteral("site"), QStringLiteral("admin"),
                                    QStringLiteral("api-admin"), QStringLiteral("api")}) {
            Setup setup{};
            QVERIFY(setupFromName(name, &setup));
            QCOMPARE(setupName(setup), name);
        }
        Setup none{};
        QVERIFY(!setupFromName(QStringLiteral("blog"), &none));
        QVERIFY(setupHasAdmin(Setup::ApiAdmin));
        QVERIFY(!setupHasAdmin(Setup::Api));
        QVERIFY(setupHasSite(Setup::Site));
        QVERIFY(!setupHasSite(Setup::Admin));
    }

    void folderNames()
    {
        QCOMPARE(projectFolderName(QStringLiteral("My Shop!")), QStringLiteral("my-shop"));
        QCOMPARE(projectFolderName(QStringLiteral("  AR  lighting -- backend ")), QStringLiteral("ar-lighting-backend"));
        QCOMPARE(projectFolderName(QStringLiteral("fectum_hellas")), QStringLiteral("fectum_hellas"));
        QCOMPARE(projectFolderName(QStringLiteral("مشروع")), QString()); // nothing a folder name keeps
        QCOMPARE(projectFolderName(QStringLiteral("-x-")), QStringLiteral("x"));
        QCOMPARE(databaseNameFor(QStringLiteral("ar-lighting-backend")), QStringLiteral("ar_lighting_backend"));
    }

    void whatIsChecked()
    {
        const QTemporaryDir dir;
        NewProject project = valid(dir.path());
        QCOMPARE(fields(checkNewProject(project)), QStringList());

        NewProject bad = project;
        bad.name = QStringLiteral("My Shop");
        bad.languages = {};
        bad.adminEmail = QStringLiteral("not an email");
        bad.adminPassword = QStringLiteral("short");
        QCOMPARE(fields(checkNewProject(bad)), (QStringList{QStringLiteral("name=name_invalid"), QStringLiteral("languages=languages_empty"),
                                                            QStringLiteral("adminEmail=email_invalid"),
                                                            QStringLiteral("adminPassword=password_short")}));
        // Windows device names; emails FloCMS's filter_var refuses
        for (const QString &name : {QStringLiteral("con"), QStringLiteral("nul"), QStringLiteral("com1"), QStringLiteral("lpt9")}) {
            bad = project;
            bad.name = name;
            QCOMPARE(fields(checkNewProject(bad)), QStringList{QStringLiteral("name=name_invalid")});
        }
        bad = project;
        bad.name = QStringLiteral("console");
        QVERIFY(fields(checkNewProject(bad)).isEmpty());
        for (const QString &email : {QStringLiteral("a@localhost"), QStringLiteral("a..b@example.com"), QStringLiteral(".a@example.com"),
                                     QStringLiteral("a@-example.com"), QStringLiteral("a@example..com"), QStringLiteral("أ@example.com")}) {
            bad = project;
            bad.adminEmail = email;
            QCOMPARE(fields(checkNewProject(bad)), QStringList{QStringLiteral("adminEmail=email_invalid")});
        }
        bad.adminEmail = QStringLiteral("first.last+shop@mail.example.co");
        QVERIFY(fields(checkNewProject(bad)).isEmpty());
        // FloCMS counts bytes: 4 Arabic letters are 8
        bad = project;
        bad.adminPassword = QStringLiteral("كلمة");
        QVERIFY(fields(checkNewProject(bad)).isEmpty());
        // The default must be chosen; the folder must not be there
        bad = project;
        bad.defaultLanguage = QStringLiteral("el");
        QCOMPARE(fields(checkNewProject(bad)), QStringList{QStringLiteral("defaultLanguage=default_language")});
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("my-shop")));
        QCOMPARE(fields(checkNewProject(project)), QStringList{QStringLiteral("name=folder_exists")});
        QVERIFY(QDir(dir.path()).rmdir(QStringLiteral("my-shop")));
        // A parent that isn't there yet is fine (it is made); a relative one isn't
        bad = project;
        bad.parentDir = dir.filePath(QStringLiteral("not/yet"));
        QVERIFY(fields(checkNewProject(bad)).isEmpty());
        bad.parentDir = QStringLiteral("relative/path");
        QCOMPARE(fields(checkNewProject(bad)), QStringList{QStringLiteral("parentDir=parent_invalid")});
        // No admin: its fields aren't needed
        bad = project;
        bad.setup = Setup::Site;
        bad.adminEmail.clear();
        bad.adminPassword.clear();
        QVERIFY(fields(checkNewProject(bad)).isEmpty());
        // MySQL
        bad = project;
        bad.mysql = true;
        bad.dbName = QStringLiteral("my-shop");
        bad.dbUser = QStringLiteral(" ");
        bad.dbPort = 0;
        bad.dbPassword = QStringLiteral("two\nlines");
        QCOMPARE(fields(checkNewProject(bad)), (QStringList{QStringLiteral("dbName=db_name_invalid"), QStringLiteral("dbUser=db_user_empty"),
                                                            QStringLiteral("dbPort=db_port_invalid"),
                                                            QStringLiteral("dbPassword=db_password_invalid")}));
    }

    void theEnv()
    {
        const QTemporaryDir dir;
        NewProject project = valid(dir.path());
        project.port = 8010;
        QList<QPair<QString, QString>> env = newProjectEnv(project);
        const QMap<QString, QString> values = asMap(env);
        QCOMPARE(values.value(QStringLiteral("APP_NAME")), QStringLiteral("my-shop"));
        QCOMPARE(values.value(QStringLiteral("APP_URL")), QStringLiteral("http://127.0.0.1:8010"));
        QCOMPARE(values.value(QStringLiteral("LANGUAGES")), QStringLiteral("en,ar,ku"));
        QCOMPARE(values.value(QStringLiteral("DEFAULT_LANG")), QStringLiteral("ar"));
        QCOMPARE(values.value(QStringLiteral("DB_TYPE")), QStringLiteral("sqlite"));
        QCOMPARE(values.value(QStringLiteral("DB_NAME")), QStringLiteral("storage/database.sqlite"));
        // Every key is one Studio may write (Core's writableEnvKeys)
        for (const auto &[key, value] : env)
            QVERIFY2(writableEnvKeys().contains(key), qUtf8Printable(key));

        project.mysql = true;
        project.dbName = QStringLiteral("my_shop");
        project.dbUser = QStringLiteral("root");
        project.dbPassword = QStringLiteral("p@ss word");
        project.appName = QStringLiteral("My Shop");
        env = newProjectEnv(project);
        const QMap<QString, QString> mysql = asMap(env);
        QCOMPARE(mysql.value(QStringLiteral("DB_TYPE")), QStringLiteral("mysql"));
        QCOMPARE(mysql.value(QStringLiteral("DB_PASSWORD")), QStringLiteral("p@ss word"));
        QCOMPARE(mysql.value(QStringLiteral("SITE_NAME")), QStringLiteral("My Shop"));

        // Written into the skeleton's real .env.example, read back as FloCMS reads it
        const QString site = dir.filePath(QStringLiteral("site"));
        QDir().mkpath(site);
        QVERIFY(QFile::copy(QStringLiteral(FLOSTUDIO_FIXTURES "/project/env.example"), site + QStringLiteral("/.env")));
        QFile(site + QStringLiteral("/.env")).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QVERIFY(writeEnvValues(site, env).ok());
        const auto read = parseEnv(readEnvFile(site));
        for (const auto &[key, value] : env)
            QCOMPARE(read.value(key), value);
    }

    void theSteps()
    {
        const QTemporaryDir dir;
        NewProject project = valid(dir.path());
        const QString folder = newProjectFolder(project);
        QList<WizardStep> steps = newProjectSteps(project);
        QCOMPARE(ids(steps), (QStringList{QStringLiteral("create"), QStringLiteral("env"), QStringLiteral("setup"),
                                          QStringLiteral("migrate"), QStringLiteral("admin")}));
        QCOMPARE(steps.at(0).request.program, Program::Composer);
        QCOMPARE(steps.at(0).request.projectDir, dir.path());
        QCOMPARE(steps.at(0).request.arguments,
                 (QStringList{QStringLiteral("create-project"), QStringLiteral("hostkurd/flocms"), QStringLiteral("my-shop")}));
        QCOMPARE(steps.at(1).kind, StepKind::WriteEnv);
        QCOMPARE(steps.at(2).request.arguments, (QStringList{QStringLiteral("setup"), QStringLiteral("--as=site-admin")}));
        QCOMPARE(steps.at(2).request.projectDir, folder);
        QCOMPARE(steps.at(3).request.arguments, (QStringList{QStringLiteral("migrate"), QStringLiteral("--force")}));
        // What is shown is what runs, with nothing secret
        QCOMPARE(steps.at(3).shown, QStringLiteral("php flo migrate --force"));
        // The password: only on the input, never in an argument or what is shown
        const WizardStep &admin = steps.at(4);
        QCOMPARE(admin.request.input, QByteArray("correct horse\n"));
        QVERIFY(admin.request.arguments.contains(QStringLiteral("--password-stdin")));
        for (const WizardStep &step : steps) {
            QVERIFY(!step.shown.contains(QStringLiteral("correct horse")));
            for (const QString &argument : step.request.arguments)
                QVERIFY(!argument.contains(QStringLiteral("correct horse")));
            // Every command is in Core's allowlist
            if (step.kind == StepKind::Command)
                QVERIFY2(isAllowed(step.request.program, step.request.arguments), qUtf8Printable(step.shown));
        }

        // A plain website: nothing to set up, no admin; website + API: the
        // package and its installer; MySQL: the connection is checked first
        project.setup = Setup::Site;
        project.addApi = true;
        project.mysql = true;
        steps = newProjectSteps(project);
        QCOMPARE(ids(steps), (QStringList{QStringLiteral("create"), QStringLiteral("env"), QStringLiteral("api-require"),
                                          QStringLiteral("api-install"), QStringLiteral("db-check"), QStringLiteral("migrate")}));
        QCOMPARE(steps.at(2).request.program, Program::Composer);
        QCOMPARE(steps.at(2).request.arguments, (QStringList{QStringLiteral("require"), QStringLiteral("hostkurd/flocms-api")}));
        // The API only goes with a website setup
        project.setup = Setup::ApiAdmin;
        QVERIFY(!ids(newProjectSteps(project)).contains(QStringLiteral("api-require")));
    }

    void aDatabaseOnStudiosServer()
    {
        // MariaDB design note D10
        NewProject project;
        project.name = QStringLiteral("my-shop");
        project.parentDir = QDir::tempPath();
        project.mysql = true;
        project.createDatabase = true;
        project.dbName = QStringLiteral("my_shop");
        project.dbUser = QStringLiteral("my_shop");
        project.dbPassword = QStringLiteral("Secr3tPassw0rd");
        project.dbPort = 3307;
        project.adminName = QStringLiteral("Admin");
        project.adminEmail = QStringLiteral("admin@example.com");
        project.adminPassword = QStringLiteral("long enough");
        const QList<WizardStep> steps = newProjectSteps(project);
        QCOMPARE(ids(steps).mid(0, 4), (QStringList{QStringLiteral("create"), QStringLiteral("env"), QStringLiteral("db-create"),
                                                    QStringLiteral("setup")}));
        QCOMPARE(steps.at(2).kind, StepKind::CreateDatabase);
        QVERIFY(!steps.at(2).shown.contains(project.dbPassword));
        QVERIFY(ids(steps).contains(QStringLiteral("db-check")));

        const QString sql = newProjectDatabaseSql(project);
        QVERIFY(sql.contains(QStringLiteral("CREATE DATABASE IF NOT EXISTS `my_shop` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;")));
        QVERIFY(sql.contains(QStringLiteral("CREATE USER IF NOT EXISTS 'my_shop'@'localhost' IDENTIFIED BY 'Secr3tPassw0rd';")));
        QVERIFY(sql.contains(QStringLiteral("CREATE USER IF NOT EXISTS 'my_shop'@'127.0.0.1' IDENTIFIED BY 'Secr3tPassw0rd';")));
        // _ is a wildcard in GRANT's database name: escaped
        QVERIFY(sql.contains(QStringLiteral("GRANT ALL PRIVILEGES ON `my\\_shop`.* TO 'my_shop'@'localhost';")));
        QVERIFY(sql.endsWith(QStringLiteral("FLUSH PRIVILEGES;\n")));

        // Studio's user: letters, digits and _, never root; a password always
        QVERIFY2(checkNewProject(project).isEmpty(), qUtf8Printable(fields(checkNewProject(project)).join(u',')));
        NewProject bad = project;
        bad.dbUser = QStringLiteral("root");
        bad.dbPassword.clear();
        QCOMPARE(fields(checkNewProject(bad)), (QStringList{QStringLiteral("dbUser=db_user_invalid"), QStringLiteral("dbPassword=db_password_invalid")}));
        bad.dbUser = QStringLiteral("my-shop");
        bad.dbPassword = project.dbPassword;
        QCOMPARE(fields(checkNewProject(bad)), QStringList{QStringLiteral("dbUser=db_user_invalid")});
        // Without createDatabase nothing changes
        bad.createDatabase = false;
        QVERIFY(checkNewProject(bad).isEmpty());
        QVERIFY(!ids(newProjectSteps(bad)).contains(QStringLiteral("db-create")));
    }

    // --- A database for a project (parts A and C, 30 Oct) ---

    void unknownDatabase()
    {
        QVERIFY(isUnknownDatabase(QStringLiteral(
            "Cannot connect to the database (127.0.0.1:3306/testdb3): SQLSTATE[HY000] [1049] Unknown database 'testdb3'")));
        QVERIFY(isUnknownDatabase(QStringLiteral("SQLSTATE[HY000] [1049] unknown database 'x'")));
        QVERIFY(!isUnknownDatabase(QStringLiteral("SQLSTATE[HY000] [1045] Access denied for user 'root'@'localhost'")));
        QVERIFY(!isUnknownDatabase(QStringLiteral("SQLSTATE[HY000] [2002] Connection refused")));
        QVERIFY(!isUnknownDatabase(QStringLiteral("Unknown database driver: oracle")));
        QVERIFY(!isUnknownDatabase(QString()));
    }

    void databaseLines()
    {
        const DatabaseLogin login{QStringLiteral(" 127.0.0.1 "), 3307, QStringLiteral("shop"), QStringLiteral(" shop "),
                                  QStringLiteral("p w#")};
        const auto env = asMap(databaseEnv(login));
        QCOMPARE(env.value(QStringLiteral("DB_TYPE")), QStringLiteral("mysql"));
        QCOMPARE(env.value(QStringLiteral("DB_HOST")), QStringLiteral("127.0.0.1"));
        QCOMPARE(env.value(QStringLiteral("DB_PORT")), QStringLiteral("3307"));
        QCOMPARE(env.value(QStringLiteral("DB_NAME")), QStringLiteral("shop"));
        QCOMPARE(env.value(QStringLiteral("DB_USERNAME")), QStringLiteral("shop"));
        QCOMPARE(env.value(QStringLiteral("DB_PASSWORD")), QStringLiteral("p w#"));
        // Every key is one Studio may write into an existing .env
        for (auto it = env.cbegin(); it != env.cend(); ++it)
            QVERIFY2(writableEnvKeys().contains(it.key()), qPrintable(it.key()));
    }

    void databaseLoginChecks()
    {
        DatabaseLogin login{QStringLiteral("127.0.0.1"), 3306, QStringLiteral("shop"), QStringLiteral("root"), QString()};
        QVERIFY(checkDatabaseLogin(login, false).isEmpty());
        // Studio makes the user: not root, and a password
        const auto made = checkDatabaseLogin(login, true);
        QVERIFY(made.contains(FieldProblem{QStringLiteral("dbUser"), QStringLiteral("db_user_invalid")}));
        QVERIFY(made.contains(FieldProblem{QStringLiteral("dbPassword"), QStringLiteral("db_password_invalid")}));
        login.name = QStringLiteral("shop`; DROP");
        login.port = 0;
        login.host.clear();
        const auto bad = checkDatabaseLogin(login, false);
        QVERIFY(bad.contains(FieldProblem{QStringLiteral("dbName"), QStringLiteral("db_name_invalid")}));
        QVERIFY(bad.contains(FieldProblem{QStringLiteral("dbPort"), QStringLiteral("db_port_invalid")}));
        QVERIFY(bad.contains(FieldProblem{QStringLiteral("dbHost"), QStringLiteral("db_host_invalid")}));
    }

    void createDatabaseCommandKeepsThePasswordOut()
    {
        Settings settings;
        settings.phpPath = QStringLiteral("php");
        const DatabaseLogin login{QStringLiteral("127.0.0.1"), 1, QStringLiteral("shop_db"), QStringLiteral("shop"),
                                  QStringLiteral("s3cret-pass")};
        const QTemporaryDir dir;
        const auto command = createDatabaseCommand(settings, login, dir.path());
        if (!command)
            QSKIP("no PHP on this computer");
        QVERIFY(!command.value().arguments.join(u' ').contains(QStringLiteral("s3cret-pass")));
        QVERIFY(!command.value().shown.contains(QStringLiteral("s3cret-pass")));
        QVERIFY(command.value().shown.contains(QStringLiteral("shop_db")));
        QVERIFY(command.value().input.contains("s3cret-pass"));
        // The code runs: nothing listens on port 1, so it says why and fails
        const Captured out = runCaptured(command.value(), 60000);
        QVERIFY(out.started);
        QVERIFY(!out.succeeded());
        QVERIFY(!out.err.trimmed().isEmpty());
        QVERIFY(!out.err.contains(QStringLiteral("s3cret-pass")));
    }
};

QTEST_GUILESS_MAIN(TestWizard)
#include "tst_wizard.moc"
