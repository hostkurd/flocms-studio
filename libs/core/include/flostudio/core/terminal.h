#pragma once

#include <flostudio/core/export.h>
#include <flostudio/core/result.h>
#include <flostudio/core/settings.h>

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>

// Open terminal (design note of 30 Oct, part B): the person's own terminal,
// in the project's folder, with the PHP and Composer Studio uses first on
// the PATH. Studio has no terminal of its own.
//
// The PATH is set by a short starter that runs inside the new window
// (B-D3): Windows Terminal, GNOME Terminal and macOS `open` hand the window
// to a process that is already running and never sees Studio's
// environment. The starter puts one folder Studio owns first, holding
// small `php` and `composer` launchers (B-D4), prints one line saying what
// they run (B-D5), then starts the person's shell.
namespace FloStudio::Core {

enum class TerminalPlatform { Windows, MacOS, Linux };

// This computer's.
FLOSTUDIOCORE_EXPORT TerminalPlatform currentTerminalPlatform();

// The launchers' folder: terminal-bin in Studio's data folder.
FLOSTUDIOCORE_EXPORT QString terminalBinFolder();

struct TerminalTools
{
    QString php;                   // the PHP program
    QString composer;              // the program that runs Composer
    QStringList composerArguments; // [composer.phar] when it runs through PHP
    bool composerBatch = false;    // composer is a .bat/.cmd (Windows)
};

// The launcher files for the tools, file name → contents: php.cmd and
// composer.cmd on Windows, php and composer (shell scripts) elsewhere.
// Composer is left out when it wasn't found.
FLOSTUDIOCORE_EXPORT QList<QPair<QString, QByteArray>> terminalLaunchers(const TerminalTools &tools,
                                                                         TerminalPlatform platform);

// Writes them into `folder` (made when missing; executable on macOS and
// Linux), replacing what was there. Error code: terminal_bin.
FLOSTUDIOCORE_EXPORT Result<Done> writeTerminalLaunchers(const QString &folder, const TerminalTools &tools,
                                                         TerminalPlatform platform = currentTerminalPlatform());

// The PHP and Composer Studio uses (phpProgram, composerLaunch). Error
// code: php_not_found (Composer missing is fine: no launcher for it).
FLOSTUDIOCORE_EXPORT Result<TerminalTools> terminalTools(const Settings &settings);

// The line the starter prints:
// "FloCMS Studio: php C:\xampp\php\php.exe, composer C:\...\composer.phar"
FLOSTUDIOCORE_EXPORT QString terminalBanner(const TerminalTools &tools);

struct TerminalCommand
{
    QString program;
    QStringList arguments;
    QString workingDir;
    QString scriptPath;  // macOS: a .command file to write first, beside the launchers' folder
    QByteArray script;   // its contents
    bool newConsole = false; // Windows: the program needs a console window of its own
};

// How to open the terminal. `terminalTemplate` is Settings::terminalCommand:
// empty means automatic (B-D2) — Windows Terminal, else PowerShell;
// Terminal.app; $TERMINAL, x-terminal-emulator, GNOME Terminal, Konsole,
// Xfce Terminal. Else it works like the editor command ({folder}, quotes)
// and gets no starter: Studio then only puts the launchers' folder first
// on the PATH it starts it with. `find` turns a program name into its path
// (empty when it isn't there); `terminalEnv` is $TERMINAL. Error code:
// terminal_not_found.
FLOSTUDIOCORE_EXPORT Result<TerminalCommand> terminalCommand(TerminalPlatform platform,
                                                             const QString &terminalTemplate,
                                                             const QString &folder, const QString &binFolder,
                                                             const QString &banner,
                                                             const std::function<QString(const QString &)> &find,
                                                             const QString &terminalEnv = {});

// Opens it: writes the launchers and the macOS script, then starts the
// terminal (not waiting for it). Error codes: php_not_found,
// terminal_bin, terminal_not_found, terminal_failed.
FLOSTUDIOCORE_EXPORT Result<Done> openTerminal(const Settings &settings, const QString &folder);

} // namespace FloStudio::Core
