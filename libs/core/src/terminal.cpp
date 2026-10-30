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

// "text" for a .cmd file: paths can't hold ", so quoting is enough
QString cmdQuote(const QString &text)
{
    return u'"' + text + u'"';
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

Result<TerminalTools> terminalTools(const Settings &settings)
{
    const auto php = phpProgram(settings);
    if (!php)
        return php.error();
    TerminalTools tools;
    tools.php = php.value();
    if (const auto composer = composerLaunch(settings)) {
        tools.composer = composer.value().program;
        tools.composerArguments = composer.value().arguments;
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

Result<TerminalCommand> terminalCommand(TerminalPlatform platform, const QString &terminalTemplate, const QString &folder,
                                        const QString &binFolder, const QString &banner,
                                        const std::function<QString(const QString &)> &find, const QString &terminalEnv)
{
    TerminalCommand command;
    command.workingDir = folder;
    const QString shownFolder = nativePath(folder, platform);
    const QString bin = nativePath(binFolder, platform);

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
        const QString script = QStringLiteral("$env:PATH = %1 + ';' + $env:PATH\nWrite-Host %2\n")
                                   .arg(psQuote(bin), psQuote(banner));
        const QStringList shell{QStringLiteral("-NoExit"), QStringLiteral("-NoLogo"), QStringLiteral("-EncodedCommand"),
                                encodedCommand(script)};
        const QString powershell = find(QStringLiteral("powershell"));
        if (const QString wt = find(QStringLiteral("wt")); !wt.isEmpty()) {
            command.program = wt;
            command.arguments = QStringList{QStringLiteral("-d"), shownFolder,
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
        command.script = (QStringLiteral("#!/bin/sh\ncd %1 || exit 1\nPATH=%2:\"$PATH\"; export PATH\n"
                                         "printf '%s\\n' %3\nexec \"${SHELL:-/bin/zsh}\" -l\n")
                              .arg(shQuote(folder), shQuote(binFolder), shQuote(banner)))
                             .toUtf8();
        command.program = open;
        command.arguments = {QStringLiteral("-a"), QStringLiteral("Terminal"), command.scriptPath};
        return command;
    }
    case TerminalPlatform::Linux: {
        const QStringList starter{
            QStringLiteral("sh"), QStringLiteral("-c"),
            QStringLiteral("cd \"$1\" || exit 1; PATH=\"$2:$PATH\"; export PATH; printf '%s\\n' \"$3\"; "
                           "exec \"${SHELL:-/bin/sh}\" -i"),
            QStringLiteral("sh"), folder, binFolder, banner};
        // $TERMINAL and x-terminal-emulator take the program after -e
        for (const QString &name : {terminalEnv.trimmed(), QStringLiteral("x-terminal-emulator")}) {
            if (name.isEmpty())
                continue;
            if (const QString program = find(name); !program.isEmpty()) {
                command.program = program;
                command.arguments = QStringList{QStringLiteral("-e")} + starter;
                return command;
            }
        }
        if (const QString program = find(QStringLiteral("gnome-terminal")); !program.isEmpty()) {
            command.program = program;
            command.arguments = QStringList{QStringLiteral("--working-directory=") + folder, QStringLiteral("--")} + starter;
            return command;
        }
        if (const QString program = find(QStringLiteral("konsole")); !program.isEmpty()) {
            command.program = program;
            command.arguments = QStringList{QStringLiteral("--workdir"), folder, QStringLiteral("-e")} + starter;
            return command;
        }
        if (const QString program = find(QStringLiteral("xfce4-terminal")); !program.isEmpty()) {
            command.program = program;
            command.arguments = QStringList{QStringLiteral("--working-directory=") + folder, QStringLiteral("-x")} + starter;
            return command;
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
    const auto command = terminalCommand(currentTerminalPlatform(), settings.terminalCommand, folder, bin,
                                         terminalBanner(tools.value()), [](const QString &name) { return findTool(name); },
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
