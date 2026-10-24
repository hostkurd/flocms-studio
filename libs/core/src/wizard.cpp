#include <flostudio/core/wizard.h>

#include <QDir>
#include <QFileInfo>
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

    if (project.mysql) {
        static const QRegularExpression database(QStringLiteral("^[A-Za-z0-9_$]{1,64}$"));
        if (!database.match(project.dbName).hasMatch())
            problems << FieldProblem{QStringLiteral("dbName"), QStringLiteral("db_name_invalid")};
        if (project.dbUser.trimmed().isEmpty())
            problems << FieldProblem{QStringLiteral("dbUser"), QStringLiteral("db_user_empty")};
        if (project.dbPort < 1 || project.dbPort > 65535)
            problems << FieldProblem{QStringLiteral("dbPort"), QStringLiteral("db_port_invalid")};
        if (project.dbHost.trimmed().isEmpty() || project.dbHost.contains(u'\n'))
            problems << FieldProblem{QStringLiteral("dbHost"), QStringLiteral("db_host_invalid")};
        if (project.dbPassword.contains(u'\n') || project.dbPassword.contains(u'\r'))
            problems << FieldProblem{QStringLiteral("dbPassword"), QStringLiteral("db_password_invalid")};
    }

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
        values << QPair<QString, QString>{QStringLiteral("DB_TYPE"), QStringLiteral("mysql")}
               << QPair<QString, QString>{QStringLiteral("DB_HOST"), project.dbHost.trimmed()}
               << QPair<QString, QString>{QStringLiteral("DB_PORT"), QString::number(project.dbPort)}
               << QPair<QString, QString>{QStringLiteral("DB_NAME"), project.dbName}
               << QPair<QString, QString>{QStringLiteral("DB_USERNAME"), project.dbUser.trimmed()}
               << QPair<QString, QString>{QStringLiteral("DB_PASSWORD"), project.dbPassword};
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
