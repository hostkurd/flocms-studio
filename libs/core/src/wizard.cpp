#include <flostudio/core/wizard.h>
#include <flostudio/core/dbserver.h>
#include <flostudio/core/floserver.h>
#include <flostudio/core/tools.h>

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace FloStudio::Core {

QString setupName(Setup setup)
{
    switch (setup) {
    case Setup::SiteAdmin:
        return QStringLiteral("site-admin");
    case Setup::Site:
        return QStringLiteral("site");
    case Setup::Admin:
        return QStringLiteral("admin");
    case Setup::ApiAdmin:
        return QStringLiteral("api-admin");
    case Setup::Api:
        return QStringLiteral("api");
    }
    return {};
}

bool setupFromName(const QString &name, Setup *setup)
{
    for (const Setup candidate : {Setup::SiteAdmin, Setup::Site, Setup::Admin, Setup::ApiAdmin, Setup::Api}) {
        if (setupName(candidate) == name) {
            *setup = candidate;
            return true;
        }
    }
    return false;
}

bool setupHasAdmin(Setup setup)
{
    return setup == Setup::SiteAdmin || setup == Setup::Admin || setup == Setup::ApiAdmin;
}

bool setupHasSite(Setup setup)
{
    return setup == Setup::SiteAdmin || setup == Setup::Site;
}

QString projectFolderName(const QString &typed)
{
    QString name;
    for (const QChar c : typed.trimmed().toLower()) {
        const bool plain = (c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9') || c == u'_';
        if (plain)
            name += c;
        else if (!name.isEmpty() && !name.endsWith(u'-')) // one - for a run of others
            name += u'-';
    }
    while (name.endsWith(u'-') || name.endsWith(u'_'))
        name.chop(1);
    while (name.startsWith(u'-') || name.startsWith(u'_'))
        name.remove(0, 1);
    return name;
}

QString databaseNameFor(const QString &folderName)
{
    QString name = folderName;
    name.replace(u'-', u'_');
    return name.left(64);
}

QString newProjectFolder(const NewProject &project)
{
    return QDir(project.parentDir).filePath(project.name);
}

QList<FieldProblem> checkNewProject(const NewProject &project)
{
    QList<FieldProblem> problems;
    static const QRegularExpression folder(QStringLiteral("^[a-z0-9][a-z0-9_-]{0,63}$"));
    // Windows keeps these names for devices, with or without an extension
    static const QRegularExpression device(QStringLiteral("^(con|prn|aux|nul|com[0-9]|lpt[0-9])(\\..*)?$"));
    if (!folder.match(project.name).hasMatch() || device.match(project.name).hasMatch())
        problems << FieldProblem{QStringLiteral("name"), QStringLiteral("name_invalid")};
    else if (QFileInfo::exists(newProjectFolder(project)))
        problems << FieldProblem{QStringLiteral("name"), QStringLiteral("folder_exists")};
    // The parent folder is made when it isn't there yet (~/Sites)
    if (project.parentDir.isEmpty() || !QDir::isAbsolutePath(project.parentDir)
        || (QFileInfo::exists(project.parentDir) && !QFileInfo(project.parentDir).isDir()))
        problems << FieldProblem{QStringLiteral("parentDir"), QStringLiteral("parent_invalid")};
    // APP_NAME goes into .env on one line
    if (project.appName.contains(u'\n') || project.appName.contains(u'\r'))
        problems << FieldProblem{QStringLiteral("appName"), QStringLiteral("app_name_invalid")};

    static const QRegularExpression code(QStringLiteral("^[a-z]{2,3}$"));
    if (project.languages.isEmpty()
        || !std::all_of(project.languages.begin(), project.languages.end(), [](const QString &l) { return code.match(l).hasMatch(); }))
        problems << FieldProblem{QStringLiteral("languages"), QStringLiteral("languages_empty")};
    else if (!project.languages.contains(project.defaultLanguage))
        problems << FieldProblem{QStringLiteral("defaultLanguage"), QStringLiteral("default_language")};

    if (project.mysql)
        problems << checkDatabaseLogin(databaseLogin(project), project.createDatabase);

    if (setupHasAdmin(project.setup)) {
        if (project.adminName.trimmed().isEmpty())
            problems << FieldProblem{QStringLiteral("adminName"), QStringLiteral("admin_name_empty")};
        // A part of what FloCMS's Users::validateEmail accepts (filter_var's
        // FILTER_VALIDATE_EMAIL): dot-separated atoms, then a domain with a
        // dot whose labels don't start or end with -
        static const QRegularExpression email(QStringLiteral(
            "^(?=.{1,64}@)[A-Za-z0-9!#$%&'*+/=?^_`{|}~-]+(\\.[A-Za-z0-9!#$%&'*+/=?^_`{|}~-]+)*"
            "@([A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?\\.)+[A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?$"));
        if (!email.match(project.adminEmail.trimmed()).hasMatch())
            problems << FieldProblem{QStringLiteral("adminEmail"), QStringLiteral("email_invalid")};
        // FloCMS counts bytes (strlen)
        if (project.adminPassword.toUtf8().size() < 8)
            problems << FieldProblem{QStringLiteral("adminPassword"), QStringLiteral("password_short")};
        if (project.adminPassword.contains(u'\n') || project.adminPassword.contains(u'\r'))
            problems << FieldProblem{QStringLiteral("adminPassword"), QStringLiteral("password_invalid")};
    }
    return problems;
}

DatabaseLogin databaseLogin(const NewProject &project)
{
    return DatabaseLogin{project.dbHost, project.dbPort, project.dbName, project.dbUser, project.dbPassword};
}

QList<QPair<QString, QString>> databaseEnv(const DatabaseLogin &login)
{
    return {{QStringLiteral("DB_TYPE"), QStringLiteral("mysql")},
            {QStringLiteral("DB_HOST"), login.host.trimmed()},
            {QStringLiteral("DB_PORT"), QString::number(login.port)},
            {QStringLiteral("DB_NAME"), login.name},
            {QStringLiteral("DB_USERNAME"), login.user.trimmed()},
            {QStringLiteral("DB_PASSWORD"), login.password}};
}

QList<FieldProblem> checkDatabaseLogin(const DatabaseLogin &login, bool studioMakesUser)
{
    QList<FieldProblem> problems;
    static const QRegularExpression database(QStringLiteral("^[A-Za-z0-9_$]{1,64}$"));
    if (!database.match(login.name).hasMatch())
        problems << FieldProblem{QStringLiteral("dbName"), QStringLiteral("db_name_invalid")};
    static const QRegularExpression newUser(QStringLiteral("^[A-Za-z0-9_]{1,80}$"));
    if (login.user.trimmed().isEmpty())
        problems << FieldProblem{QStringLiteral("dbUser"), QStringLiteral("db_user_empty")};
    else if (studioMakesUser
             && (!newUser.match(login.user).hasMatch() || login.user.compare(QStringLiteral("root"), Qt::CaseInsensitive) == 0))
        problems << FieldProblem{QStringLiteral("dbUser"), QStringLiteral("db_user_invalid")};
    if (studioMakesUser && login.password.isEmpty())
        problems << FieldProblem{QStringLiteral("dbPassword"), QStringLiteral("db_password_invalid")};
    if (login.port < 1 || login.port > 65535)
        problems << FieldProblem{QStringLiteral("dbPort"), QStringLiteral("db_port_invalid")};
    if (login.host.trimmed().isEmpty() || login.host.contains(u'\n'))
        problems << FieldProblem{QStringLiteral("dbHost"), QStringLiteral("db_host_invalid")};
    if (login.password.contains(u'\n') || login.password.contains(u'\r'))
        problems << FieldProblem{QStringLiteral("dbPassword"), QStringLiteral("db_password_invalid")};
    return problems;
}

bool isUnknownDatabase(const QString &message)
{
    return message.contains(QStringLiteral("[1049]")) || message.contains(QStringLiteral("Unknown database"), Qt::CaseInsensitive);
}

QString createDatabasePhpCode()
{
    // The name is checked before (checkDatabaseLogin) and quoted here too
    return QStringLiteral(
        "$in = json_decode(stream_get_contents(STDIN), true);"
        "if (!is_array($in)) { fwrite(STDERR, \"No login was given.\\n\"); exit(1); }"
        "try {"
        " $dsn = 'mysql:host=' . $in['host'] . ';port=' . (int) $in['port'] . ';charset=utf8mb4';"
        " $pdo = new PDO($dsn, $in['user'], $in['password'], [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION, PDO::ATTR_TIMEOUT => 10]);"
        " $name = '`' . str_replace('`', '``', $in['name']) . '`';"
        " $pdo->exec('CREATE DATABASE IF NOT EXISTS ' . $name . ' CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci');"
        " echo 'Database ' . $in['name'] . \" is ready.\\n\";"
        "} catch (Throwable $e) { fwrite(STDERR, $e->getMessage() . \"\\n\"); exit(1); }");
}

Result<Command> createDatabaseCommand(const Settings &settings, const DatabaseLogin &login, const QString &workingDir)
{
    const auto php = phpProgram(settings);
    if (!php)
        return php.error();
    Command command;
    command.program = php.value();
    command.arguments = {QStringLiteral("-r"), createDatabasePhpCode()};
    command.workingDir = workingDir;
    command.environment.insert(QStringLiteral("PATH"), commandSearchPath(settings));
    QJsonObject json{{QStringLiteral("host"), login.host.trimmed()},
                     {QStringLiteral("port"), login.port},
                     {QStringLiteral("name"), login.name},
                     {QStringLiteral("user"), login.user.trimmed()},
                     {QStringLiteral("password"), login.password}};
    command.input = QJsonDocument(json).toJson(QJsonDocument::Compact);
    command.shown = QStringLiteral("CREATE DATABASE %1 (utf8mb4) on %2:%3")
                        .arg(login.name, login.host.trimmed(), QString::number(login.port));
    return command;
}

QList<QPair<QString, QString>> newProjectEnv(const NewProject &project)
{
    const QString appName = project.appName.trimmed().isEmpty() ? project.name : project.appName.trimmed();
    QList<QPair<QString, QString>> values{
        {QStringLiteral("APP_NAME"), appName},
        {QStringLiteral("APP_ENV"), QStringLiteral("local")},
        {QStringLiteral("APP_DEBUG"), QStringLiteral("true")},
        {QStringLiteral("APP_URL"), QStringLiteral("http://127.0.0.1:%1").arg(project.port)},
        {QStringLiteral("SITE_NAME"), appName},
        {QStringLiteral("SITE_NAME_ADMIN"), appName},
        {QStringLiteral("LANGUAGES"), project.languages.join(u',')},
        {QStringLiteral("DEFAULT_LANG"), project.defaultLanguage},
    };
    if (project.mysql) {
        values << databaseEnv(databaseLogin(project));
    } else {
        values << QPair<QString, QString>{QStringLiteral("DB_TYPE"), QStringLiteral("sqlite")}
               << QPair<QString, QString>{QStringLiteral("DB_NAME"), QStringLiteral("storage/database.sqlite")};
    }
    return values;
}

namespace {

WizardStep command(const QString &id, Program program, const QString &dir, const QStringList &arguments)
{
    WizardStep step;
    step.id = id;
    step.kind = StepKind::Command;
    step.request.program = program;
    step.request.projectDir = dir;
    step.request.arguments = arguments;
    step.shown = shownCommand(program, arguments);
    return step;
}

} // namespace

QString newProjectDatabaseSql(const NewProject &project)
{
    const QString database = sqlIdentifier(project.dbName);
    // In GRANT a database name is a pattern: _ and % match any character
    QString pattern = project.dbName;
    pattern.replace(u'_', QStringLiteral("\\_")).replace(u'%', QStringLiteral("\\%"));
    const QString user = sqlString(project.dbUser);
    const QString password = sqlString(project.dbPassword);
    QString sql = QStringLiteral("CREATE DATABASE IF NOT EXISTS %1 CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n").arg(database);
    for (const char *host : {"localhost", "127.0.0.1"}) {
        const QString account = user + QStringLiteral("@'%1'").arg(QLatin1String(host));
        sql += QStringLiteral("CREATE USER IF NOT EXISTS %1 IDENTIFIED BY %2;\n").arg(account, password)
               + QStringLiteral("ALTER USER %1 IDENTIFIED BY %2;\n").arg(account, password)
               + QStringLiteral("GRANT ALL PRIVILEGES ON %1.* TO %2;\n").arg(sqlIdentifier(pattern), account);
    }
    return sql + QStringLiteral("FLUSH PRIVILEGES;\n");
}

QList<WizardStep> newProjectSteps(const NewProject &project)
{
    const QString folder = newProjectFolder(project);
    QList<WizardStep> steps;
    steps << command(QStringLiteral("create"), Program::Composer, project.parentDir,
                     {QStringLiteral("create-project"), QStringLiteral("hostkurd/flocms"), project.name});
    WizardStep env;
    env.id = QStringLiteral("env");
    env.kind = StepKind::WriteEnv;
    env.shown = QStringLiteral(".env");
    steps << env;
    // On Studio's own server: the database and its user first
    if (project.mysql && project.createDatabase) {
        WizardStep database;
        database.id = QStringLiteral("db-create");
        database.kind = StepKind::CreateDatabase;
        database.shown = QStringLiteral("CREATE DATABASE %1; CREATE USER '%2'; GRANT ALL ON %1.*")
                             .arg(sqlIdentifier(project.dbName), project.dbUser);
        steps << database;
    }
    // A plain website is what create-project made
    if (project.setup != Setup::Site) {
        steps << command(QStringLiteral("setup"), Program::Flo, folder,
                         {QStringLiteral("setup"), QStringLiteral("--as=") + setupName(project.setup)});
    }
    // Website + API: the package, then its installer (never setup to add one)
    if (project.addApi && setupHasSite(project.setup)) {
        steps << command(QStringLiteral("api-require"), Program::Composer, folder,
                         {QStringLiteral("require"), QStringLiteral("hostkurd/flocms-api")})
              << command(QStringLiteral("api-install"), Program::Flo, folder, {QStringLiteral("api:install")});
    }
    if (project.mysql) {
        steps << command(QStringLiteral("db-check"), Program::Flo, folder, {QStringLiteral("db:check")});
    }
    // APP_ENV=local was just written, so --force changes nothing but is
    // explicit: the wizard never stops on a question
    steps << command(QStringLiteral("migrate"), Program::Flo, folder, {QStringLiteral("migrate"), QStringLiteral("--force")});
    if (setupHasAdmin(project.setup)) {
        WizardStep admin = command(QStringLiteral("admin"), Program::Flo, folder,
                                   {QStringLiteral("user:create"), QStringLiteral("--name=") + project.adminName.trimmed(),
                                    QStringLiteral("--email=") + project.adminEmail.trimmed(),
                                    QStringLiteral("--role=super-admin"), QStringLiteral("--password-stdin")});
        admin.request.input = project.adminPassword.toUtf8() + '\n';
        steps << admin;
    }
    return steps;
}

} // namespace FloStudio::Core
