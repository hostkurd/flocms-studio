#pragma once

#include <flostudio/core/export.h>
#include <flostudio/core/process.h>
#include <flostudio/core/result.h>
#include <flostudio/core/settings.h>

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>

namespace FloStudio::Core {

// The `php flo` and `composer` commands Studio runs for its screens (D11).
// Only the commands below are allowed; a screen that needs another one
// adds it here, in a reviewed change. Every command gets --no-interaction
// (nobody can answer a question) and --no-ansi.

enum class Program { Flo, Composer };

// `php flo` commands, and the make: generators by prefix.
FLOSTUDIOCORE_EXPORT const QStringList &allowedFloCommands();
FLOSTUDIOCORE_EXPORT const QStringList &allowedComposerCommands();

// The command (the first argument) is allowed, and no argument would make
// it work on another folder (composer --working-dir / -d) or hold a line
// break or a NUL.
FLOSTUDIOCORE_EXPORT bool isAllowed(Program program, const QStringList &arguments);

// php flo migrate --force / composer require hostkurd/flocms-api:^1.3
FLOSTUDIOCORE_EXPORT QString shownCommand(Program program, const QStringList &arguments);

struct CommandRequest
{
    Program program = Program::Flo;
    QString projectDir;    // where it runs: the project (create-project: the parent folder)
    QStringList arguments; // the command and its arguments, without the global options
    QByteArray input;      // see Command::input
    QMap<QString, QString> environment;
};

// The Command to run (process.h). Error codes: not_allowed,
// project_missing (no such folder), not_flocms and no_launcher (php flo
// in a folder that isn't a FloCMS project, or has no flo launcher yet),
// php_not_found, composer_not_found, unsafe_argument (Composer is a .bat
// file and an argument has characters cmd.exe would read itself).
FLOSTUDIOCORE_EXPORT Result<Command> prepareCommand(const Settings &settings, const CommandRequest &request);

} // namespace FloStudio::Core
