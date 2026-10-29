#include <flostudio/core/commands.h>
#include <flostudio/core/env.h>
#include <flostudio/core/floserver.h>
#include <flostudio/core/project.h>
#include <flostudio/core/tools.h>

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace FloStudio::Core {

namespace {

// Composer commands whose progress bars are turned off (--no-progress).
bool showsProgress(const QString &command)
{
    static const QStringList commands{QStringLiteral("create-project"), QStringLiteral("require"),
                                      QStringLiteral("install"), QStringLiteral("update")};
    return commands.contains(command);
}

// A .bat/.cmd program (php.bat, composer.bat) runs through cmd.exe, which
// reads the whole command line itself: every argument must be one it
// leaves alone. Programs that aren't batch files get their arguments as
// they are.
Result<Command> checkedForBatchFile(const Command &command)
{
    const QString suffix = QFileInfo(command.program).suffix().toLower();
    if (suffix != u"bat" && suffix != u"cmd")
        return command;
    for (const QString &argument : command.arguments) {
        if (!safeForBatchFile(argument))
            return Error{QStringLiteral("unsafe_argument"), argument};
    }
    return command;
}

bool changesFolder(const QString &argument)
{
    // -d DIR, -dDIR, --working-dir DIR, --working-dir=DIR
    return argument.startsWith(QStringLiteral("-d")) || argument == u"--working-dir"
           || argument.startsWith(QStringLiteral("--working-dir="));
}

} // namespace

const QStringList &allowedFloCommands()
{
    static const QStringList commands{
        // Overview and Doctor
        QStringLiteral("about"), QStringLiteral("doctor"), QStringLiteral("admin:doctor"), QStringLiteral("list"),
        QStringLiteral("help"), QStringLiteral("route:list"),
        // Database
        QStringLiteral("db:check"), QStringLiteral("db:show"), QStringLiteral("db:seed"), QStringLiteral("migrate"),
        QStringLiteral("migrate:status"), QStringLiteral("migrate:rollback"), QStringLiteral("migrate:fresh"),
        // Server and logs
        QStringLiteral("serve"), QStringLiteral("log:tail"),
        // Caches
        QStringLiteral("cache:clear"), QStringLiteral("view:clear"), QStringLiteral("optimize"),
        QStringLiteral("optimize:clear"),
        // New project, packages, first admin
        QStringLiteral("key:generate"), QStringLiteral("setup"), QStringLiteral("admin:install"),
        QStringLiteral("admin:list"), QStringLiteral("api:install"), QStringLiteral("user:create"),
    };
    return commands;
}

const QStringList &allowedComposerCommands()
{
    static const QStringList commands{QStringLiteral("create-project"), QStringLiteral("require"),
                                      QStringLiteral("install"),        QStringLiteral("update"),
                                      QStringLiteral("outdated"),       QStringLiteral("show")};
    return commands;
}

bool isAllowed(Program program, const QStringList &arguments)
{
    if (arguments.isEmpty())
        return false;
    const bool unsafe = std::any_of(arguments.begin(), arguments.end(), [program](const QString &argument) {
        return argument.contains(u'\n') || argument.contains(u'\r') || argument.contains(QChar(0))
               || (program == Program::Composer && changesFolder(argument));
    });
    if (unsafe)
        return false;
    const QString &command = arguments.constFirst();
    if (program == Program::Composer)
        return allowedComposerCommands().contains(command);
    return allowedFloCommands().contains(command)
           || (command.startsWith(QStringLiteral("make:")) && command.size() > 5);
}

QString shownCommand(Program program, const QStringList &arguments)
{
    return shownCommandLine(program == Program::Flo ? QStringLiteral("php flo") : QStringLiteral("composer"),
                            arguments);
}

Result<Command> prepareCommand(const Settings &settings, const CommandRequest &request)
{
    if (!isAllowed(request.program, request.arguments))
        return Error{QStringLiteral("not_allowed"), shownCommand(request.program, request.arguments)};
    if (!QFileInfo(request.projectDir).isDir())
        return Error{QStringLiteral("project_missing"), QDir::toNativeSeparators(request.projectDir)};

    Command command;
    command.workingDir = request.projectDir;
    command.input = request.input;
    command.environment = request.environment;
    command.shown = shownCommand(request.program, request.arguments);
    // No colours, no questions, whatever the user's own settings say
    command.environment.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
    command.environment.insert(QStringLiteral("COMPOSER_NO_INTERACTION"), QStringLiteral("1"));
    // FloServer's shims first (D14): what the command starts itself (php,
    // composer) is the project's version too
    command.environment.insert(QStringLiteral("PATH"), commandSearchPath(settings));

    QStringList arguments = request.arguments;
    arguments << QStringLiteral("--no-interaction") << QStringLiteral("--no-ansi");

    if (request.program == Program::Flo) {
        if (!isProject(request.projectDir))
            return Error{QStringLiteral("not_flocms"), QDir::toNativeSeparators(request.projectDir)};
        const QString launcher = floLauncher(request.projectDir);
        if (launcher.isEmpty())
            return Error{QStringLiteral("no_launcher"), QDir::toNativeSeparators(request.projectDir)};
        const auto php = phpProgram(settings);
        if (!php)
            return php.error();
        command.program = php.value();
        command.arguments = QStringList{launcher} + arguments;
        // `php flo setup` runs `composer require`: the same Composer as Studio
        if (const auto composer = composerLaunch(settings); composer && !composer->setupBinary.isEmpty())
            command.environment.insert(QStringLiteral("COMPOSER_BINARY"), composer->setupBinary);
        // The project's own .env decides, not a variable that happens to be
        // set where Studio was started (Env::load never overrides one)
        command.removeEnvironment = parseEnv(readEnvFile(request.projectDir)).keys();
        return checkedForBatchFile(command);
    }

    const auto composer = composerLaunch(settings);
    if (!composer)
        return composer.error();
    if (showsProgress(request.arguments.constFirst()))
        arguments << QStringLiteral("--no-progress");
    // create-project runs the skeleton's scripts (key:generate, setup):
    // no time limit for them
    command.environment.insert(QStringLiteral("COMPOSER_PROCESS_TIMEOUT"), QStringLiteral("0"));
    command.program = composer->program;
    command.arguments = composer->arguments + arguments;
    return checkedForBatchFile(command);
}

} // namespace FloStudio::Core
