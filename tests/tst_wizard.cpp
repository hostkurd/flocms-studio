// The new project wizard's choices and steps (libs/core wizard.h).

#include <flostudio/core/commands.h>
#include <flostudio/core/env.h>
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
};

QTEST_GUILESS_MAIN(TestWizard)
#include "tst_wizard.moc"
