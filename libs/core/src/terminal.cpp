#include <flostudio/core/terminal.h>
#include <flostudio/core/floserver.h>
#include <flostudio/core/tools.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace FloStudio::Core {

namespace {

// 'text' for sh: a ' inside becomes '\''
QString shQuote(const QString &text)
{
    QString quoted = text;
    quoted.replace(u'\'', QStringLiteral("'\\''"));
    return u'\'' + quoted + u'\'';
}

// 'text' for PowerShell: a ' inside is doubled
QString psQuote(const QString &text)
{
    QString quoted = text;
    quoted.replace(u'\'', QStringLiteral("''"));
    return u'\'' + quoted + u'\'';
}

// "text" for a .cmd file: paths can't hold ", and % is doubled so cmd.exe
// doesn't read it as a variable
QString cmdQuote(const QString &text)
{
    QString quoted = text;
    quoted.replace(u'%', QStringLiteral("%%"));
    return u'"' + quoted + u'"';
}

// The platform's own separators, whatever system builds the command
QString nativePath(const QString &path, TerminalPlatform platform)
{
    QString native = path;
    if (platform == TerminalPlatform::Windows)
        native.replace(u'/', u'\\');
    return native;
}

bool isBatch(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == u"bat" || suffix == u"cmd";
}

// PowerShell's -EncodedCommand: base64 of the script in UTF-16LE, which
// keeps ; and quotes away from Windows Terminal's own parsing
QString encodedCommand(const QString &script)
{
    QByteArray utf16;
    utf16.reserve(script.size() * 2);
    for (const QChar c : script) {
        const char16_t unit = c.unicode();
        utf16.append(static_cast<char>(unit & 0xFFU));
        utf16.append(static_cast<char>((unit >> 8U) & 0xFFU));
    }
    return QString::fromLatin1(utf16.toBase64());
}

} // namespace

TerminalPlatform currentTerminalPlatform()
{
#if defined(Q_OS_WIN)
    return TerminalPlatform::Windows;
#elif defined(Q_OS_MACOS)
    return TerminalPlatform::MacOS;
#else
    return TerminalPlatform::Linux;
#endif
}

QString terminalBinFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("terminal-bin"));
}

QList<QPair<QString, QByteArray>> terminalLaunchers(const TerminalTools &tools, TerminalPlatform platform)
{
    QList<QPair<QString, QByteArray>> files;
    if (platform == TerminalPlatform::Windows) {
        const QString php = nativePath(tools.php, platform);
        files << QPair<QString, QByteArray>{QStringLiteral("php.cmd"), (QStringLiteral("@") + cmdQuote(php) + QStringLiteral(" %*\r\n")).toUtf8()};
        if (!tools.composer.isEmpty()) {
            QString line = tools.composerBatch ? QStringLiteral("@call ") : QStringLiteral("@");
            line += cmdQuote(nativePath(tools.composer, platform));
            for (const QString &argument : tools.composerArguments)
                line += u' ' + cmdQuote(nativePath(argument, platform));
            files << QPair<QString, QByteArray>{QStringLiteral("composer.cmd"), (line + QStringLiteral(" %*\r\n")).toUtf8()};
        }
        return files;
    }
    files << QPair<QString, QByteArray>{QStringLiteral("php"), (QStringLiteral("#!/bin/sh\nexec ") + shQuote(tools.php) + QStringLiteral(" \"$@\"\n")).toUtf8()};
    if (!tools.composer.isEmpty()) {
        QString line = QStringLiteral("#!/bin/sh\nexec ") + shQuote(tools.composer);
        for (const QString &argument : tools.composerArguments)
            line += u' ' + shQuote(argument);
        files << QPair<QString, QByteArray>{QStringLiteral("composer"), (line + QStringLiteral(" \"$@\"\n")).toUtf8()};
    }
    return files;
}

Result<Done> writeTerminalLaunchers(const QString &folder, const TerminalTools &tools, TerminalPlatform platform)
{
    if (!QDir().mkpath(folder))
        return Error{QStringLiteral("terminal_bin"), QDir::toNativeSeparators(folder)};
    // A launcher from an earlier run whose tool is gone now goes too
    for (const char *name : {"php", "composer", "php.cmd", "composer.cmd"}) {
        const QString path = QDir(folder).filePath(QLatin1String(name));
        if (QFileInfo(path).isFile())
            QFile::remove(path);
    }
    for (const auto &[name, contents] : terminalLaunchers(tools, platform)) {
        QSaveFile file(QDir(folder).filePath(name));
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() || !file.commit())
            return Error{QStringLiteral("terminal_bin"), QDir::toNativeSeparators(file.fileName())};
        if (platform != TerminalPlatform::Windows)
            QFile::setPermissions(file.fileName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                                                       | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                                                       | QFileDevice::ReadOther | QFileDevice::ExeOther);
    }
    return Done{};
}

namespace {

// cmd.exe reads a .cmd file in the console's code page, not UTF-8: a path
// with other letters (an Arabic or Kurdish user name) gets its 8.3 short
// name, which is plain ASCII, when Windows has one
QString asciiPath(const QString &path)
{
#ifdef Q_OS_WIN
    const bool ascii = std::all_of(path.cbegin(), path.cend(), [](QChar c) { return c.unicode() < 0x80; });
    if (ascii)
        return path;
    const std::wstring wide = QDir::toNativeSeparators(path).toStdWString();
    const DWORD size = GetShortPathNameW(wide.c_str(), nullptr, 0);
    if (size == 0)
        return path;
    std::wstring shortName(size, L'\0');
    const DWORD written = GetShortPathNameW(wide.c_str(), shortName.data(), size);
    if (written == 0 || written >= size)
        return path;
    shortName.resize(written);
    return QString::fromStdWString(shortName);
#else
    return path;
#endif
}

} // namespace

Result<TerminalTools> terminalTools(const Settings &settings)
{
    const auto php = phpProgram(settings);
    if (!php)
        return php.error();
    TerminalTools tools;
    tools.php = asciiPath(php.value());
    if (const auto composer = composerLaunch(settings)) {
        tools.composer = asciiPath(composer.value().program);
        for (const QString &argument : composer.value().arguments)
            tools.composerArguments << asciiPath(argument);
        tools.composerBatch = isBatch(tools.composer);
    }
    return tools;
}

QString terminalBanner(const TerminalTools &tools)
{
    QString line = QStringLiteral("FloCMS Studio: php ") + QDir::toNativeSeparators(tools.php);
    if (!tools.composer.isEmpty()) {
        const QString composer = tools.composerArguments.isEmpty() ? tools.composer : tools.composerArguments.constLast();
        line += QStringLiteral(", composer ") + QDir::toNativeSeparators(composer);
    }
    return line;
}

namespace {

// What the starter does once in the folder (B-D3), as sh: the launchers
// first on PATH, the line, then the person's shell. zsh and bash re-read
// their startup files, which may put other folders first (macOS's
// path_helper, Homebrew), so they get Studio's own: zsh through ZDOTDIR
// (its files read the person's, then put the launchers first and put
// ZDOTDIR back), bash through --rcfile. Other shells just start.
QString shellStarter(const QString &folder, const QString &bin, const QString &banner, const QString &shellDir,
                     const QString &mode)
{
    return QStringLiteral("cd %1 || exit 1\n"
                          "PATH=%2:\"$PATH\"; export PATH\n"
                          "FLOSTUDIO_BIN=%2; export FLOSTUDIO_BIN\n"
                          "printf '%s\\n' %3\n"
                          "s=\"${SHELL:-/bin/sh}\"\n"
                          "case \"$s\" in\n"
                          "  */zsh) FLOSTUDIO_HOME_ZDOTDIR=\"${ZDOTDIR:-$HOME}\"; export FLOSTUDIO_HOME_ZDOTDIR\n"
                          "         ZDOTDIR=%4/zsh; export ZDOTDIR; exec \"$s\" %5 ;;\n"
                          "  */bash) exec \"$s\" --rcfile %4/bashrc -i ;;\n"
                          "  *) exec \"$s\" %5 ;;\n"
                          "esac\n")
        .arg(folder, bin, banner, shellDir, mode);
}

// The arguments a known Linux terminal takes before the program to run
QStringList linuxTerminalArguments(const QString &program, const QString &folder)
{
    const QString name = QFileInfo(program).fileName();
    if (name == u"gnome-terminal")
        return {QStringLiteral("--working-directory=") + folder, QStringLiteral("--")};
    if (name == u"konsole")
        return {QStringLiteral("--workdir"), folder, QStringLiteral("-e")};
    if (name == u"xfce4-terminal")
        return {QStringLiteral("--working-directory=") + folder, QStringLiteral("-x")};
    // xterm's way, which x-terminal-emulator and most others follow
    return {QStringLiteral("-e")};
}

} // namespace

QString terminalShellFolder(const QString &binFolder)
{
    return QFileInfo(binFolder).dir().filePath(QStringLiteral("terminal-shell"));
}

QList<QPair<QString, QByteArray>> terminalShellFiles()
{
    const auto source = [](const char *file) {
        return QStringLiteral("[ -r \"$FLOSTUDIO_HOME_ZDOTDIR/%1\" ] && . \"$FLOSTUDIO_HOME_ZDOTDIR/%1\"\n")
            .arg(QLatin1String(file));
    };
    const QString first = QStringLiteral("PATH=\"$FLOSTUDIO_BIN:$PATH\"; export PATH\n");
    return {
        {QStringLiteral("zsh/.zshenv"), source(".zshenv").toUtf8()},
        {QStringLiteral("zsh/.zprofile"), source(".zprofile").toUtf8()},
        // The last file Studio's ZDOTDIR serves: the person's own .zlogin
        // and later shells read theirs again
        {QStringLiteral("zsh/.zshrc"),
         (source(".zshrc") + first
          + QStringLiteral("if [ \"$FLOSTUDIO_HOME_ZDOTDIR\" = \"$HOME\" ]; then unset ZDOTDIR; "
                           "else ZDOTDIR=\"$FLOSTUDIO_HOME_ZDOTDIR\"; fi\n"))
             .toUtf8()},
        {QStringLiteral("bashrc"), (QStringLiteral("[ -r \"$HOME/.bashrc\" ] && . \"$HOME/.bashrc\"\n") + first).toUtf8()},
    };
}

Result<TerminalCommand> terminalCommand(TerminalPlatform platform, const QString &terminalTemplate, const QString &folder,
                                        const QString &binFolder, const TerminalTools &tools,
                                        const std::function<QString(const QString &)> &find, const QString &terminalEnv)
{
    TerminalCommand command;
    command.workingDir = folder;
    const QString shownFolder = nativePath(folder, platform);
    const QString bin = nativePath(binFolder, platform);
    const QString banner = nativePath(terminalBanner(tools), platform);

    // The person's own command: no starter
    if (!terminalTemplate.trimmed().isEmpty()) {
        QStringList parts = editorCommandLine(terminalTemplate, shownFolder);
        const QString program = parts.isEmpty() ? QString() : find(parts.takeFirst());
        if (program.isEmpty())
            return Error{QStringLiteral("terminal_not_found"), terminalTemplate};
        command.program = program;
        command.arguments = parts;
        command.newConsole = platform == TerminalPlatform::Windows;
        return command;
    }

    switch (platform) {
    case TerminalPlatform::Windows: {
        // PowerShell functions call the programs themselves (the .cmd
        // launchers would hand the arguments to cmd.exe, which reads & ^ %
        // itself); PATH has the launchers for programs PHP starts
        QString script = QStringLiteral("$env:PATH = %1 + ';' + $env:PATH\n").arg(psQuote(bin));
        script += QStringLiteral("function global:php { & %1 @args }\n").arg(psQuote(nativePath(tools.php, platform)));
        if (!tools.composer.isEmpty()) {
            QString call = psQuote(nativePath(tools.composer, platform));
            for (const QString &argument : tools.composerArguments)
                call += u' ' + psQuote(nativePath(argument, platform));
            script += QStringLiteral("function global:composer { & %1 @args }\n").arg(call);
        }
        script += QStringLiteral("Write-Host %1\n").arg(psQuote(banner));
        const QStringList shell{QStringLiteral("-NoExit"), QStringLiteral("-NoLogo"), QStringLiteral("-EncodedCommand"),
                                encodedCommand(script)};
        const QString powershell = find(QStringLiteral("powershell"));
        if (const QString wt = find(QStringLiteral("wt")); !wt.isEmpty()) {
            // Windows Terminal splits its command line at ; even in quotes
            QString wtFolder = shownFolder;
            wtFolder.replace(u';', QStringLiteral("\\;"));
            command.program = wt;
            command.arguments = QStringList{QStringLiteral("-d"), wtFolder,
                                            powershell.isEmpty() ? QStringLiteral("powershell.exe") : powershell}
                                + shell;
            return command;
        }
        if (powershell.isEmpty())
            return Error{QStringLiteral("terminal_not_found"), QStringLiteral("powershell")};
        command.program = powershell;
        command.arguments = shell;
        command.newConsole = true;
        return command;
    }
    case TerminalPlatform::MacOS: {
        const QString open = find(QStringLiteral("open"));
        if (open.isEmpty())
            return Error{QStringLiteral("terminal_not_found"), QStringLiteral("open")};
        // One script per folder, beside the launchers' folder
        const QString hash = QString::fromLatin1(
            QCryptographicHash::hash(folder.toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
        command.scriptPath = QFileInfo(binFolder).dir().filePath(QStringLiteral("terminal-%1.command").arg(hash));
        command.script = (QStringLiteral("#!/bin/sh\n")
                          + shellStarter(shQuote(folder), shQuote(binFolder), shQuote(banner),
                                         shQuote(terminalShellFolder(binFolder)), QStringLiteral("-l")))
                             .toUtf8();
        command.program = open;
        command.arguments = {QStringLiteral("-a"), QStringLiteral("Terminal"), command.scriptPath};
        return command;
    }
    case TerminalPlatform::Linux: {
        const QStringList starter{
            QStringLiteral("sh"), QStringLiteral("-c"),
            shellStarter(QStringLiteral("\"$1\""), QStringLiteral("\"$2\""), QStringLiteral("\"$3\""),
                         QStringLiteral("\"$4\""), QStringLiteral("-i")),
            QStringLiteral("sh"), folder, binFolder, banner, terminalShellFolder(binFolder)};
        // $TERMINAL first, then the ones whose way is known, then the
        // system's choice
        for (const QString &name : {terminalEnv.trimmed(), QStringLiteral("gnome-terminal"), QStringLiteral("konsole"),
                                    QStringLiteral("xfce4-terminal"), QStringLiteral("x-terminal-emulator")}) {
            if (name.isEmpty())
                continue;
            if (const QString program = find(name); !program.isEmpty()) {
                command.program = program;
                command.arguments = linuxTerminalArguments(program, folder) + starter;
                return command;
            }
        }
        return Error{QStringLiteral("terminal_not_found"), QStringLiteral("x-terminal-emulator")};
    }
    }
    return Error{QStringLiteral("terminal_not_found"), {}};
}

Result<Done> openTerminal(const Settings &settings, const QString &folder)
{
    const auto tools = terminalTools(settings);
    if (!tools)
        return tools.error();
    const QString bin = terminalBinFolder();
    if (const auto written = writeTerminalLaunchers(bin, tools.value()); !written)
        return written.error();
    if (currentTerminalPlatform() != TerminalPlatform::Windows) {
        const QDir shell(terminalShellFolder(bin));
        for (const auto &[name, contents] : terminalShellFiles()) {
            const QString path = shell.filePath(name);
            QDir().mkpath(QFileInfo(path).absolutePath());
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size() || !file.commit())
                return Error{QStringLiteral("terminal_bin"), path};
        }
    }
    const auto command = terminalCommand(currentTerminalPlatform(), settings.terminalCommand, folder, bin, tools.value(),
                                         [](const QString &name) { return findTool(name); },
                                         qEnvironmentVariable("TERMINAL"));
    if (!command)
        return command.error();
    if (!command.value().scriptPath.isEmpty()) {
        QSaveFile script(command.value().scriptPath);
        if (!script.open(QIODevice::WriteOnly) || script.write(command.value().script) != command.value().script.size()
            || !script.commit())
            return Error{QStringLiteral("terminal_bin"), command.value().scriptPath};
        QFile::setPermissions(command.value().scriptPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
    QProcess process;
    process.setProgram(command.value().program);
    process.setArguments(command.value().arguments);
    process.setWorkingDirectory(command.value().workingDir);
    // A terminal of the person's own choosing gets the launchers this way
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"), QDir::toNativeSeparators(bin) + QDir::listSeparator()
                                                   + environment.value(QStringLiteral("PATH")));
    process.setProcessEnvironment(environment);
#ifdef Q_OS_WIN
    if (command.value().newConsole) {
        process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
            arguments->flags &= ~static_cast<DWORD>(CREATE_NO_WINDOW | DETACHED_PROCESS);
            arguments->flags |= CREATE_NEW_CONSOLE;
        });
    }
#endif
    if (!process.startDetached())
        return Error{QStringLiteral("terminal_failed"), command.value().program};
    return Done{};
}

} // namespace FloStudio::Core
