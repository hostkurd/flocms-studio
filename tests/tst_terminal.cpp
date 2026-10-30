// Open terminal (libs/core terminal.h): the launchers, the line the
// starter prints, and the terminal chosen on each system.

#include <flostudio/core/process.h>
#include <flostudio/core/terminal.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace FloStudio::Core;

namespace {

QString text(const QList<QPair<QString, QByteArray>> &files, const QString &name)
{
    for (const auto &[file, contents] : files) {
        if (file == name)
            return QString::fromUtf8(contents);
    }
    return {};
}

// A lookup that knows only these programs
std::function<QString(const QString &)> only(const QStringList &names)
{
    return [names](const QString &name) { return names.contains(name) ? QStringLiteral("/bin/") + name : QString(); };
}

} // namespace

class TestTerminal : public QObject
{
    Q_OBJECT

private slots:
    void windowsLaunchers()
    {
        TerminalTools tools;
        tools.php = QStringLiteral("D:/xampp/php/php.exe");
        tools.composer = QStringLiteral("D:/xampp/php/php.exe");
        tools.composerArguments = {QStringLiteral("C:/ProgramData/ComposerSetup/bin/composer.phar")};
        const auto files = terminalLaunchers(tools, TerminalPlatform::Windows);
        QCOMPARE(files.size(), 2);
        QCOMPARE(text(files, QStringLiteral("php.cmd")), QStringLiteral("@\"D:\\xampp\\php\\php.exe\" %*\r\n"));
        QCOMPARE(text(files, QStringLiteral("composer.cmd")),
                 QStringLiteral("@\"D:\\xampp\\php\\php.exe\" \"C:\\ProgramData\\ComposerSetup\\bin\\composer.phar\" %*\r\n"));
        // A .bat Composer is called, so the launcher goes on after it
        tools.composer = QStringLiteral("C:/tools/composer.bat");
        tools.composerArguments.clear();
        tools.composerBatch = true;
        QCOMPARE(text(terminalLaunchers(tools, TerminalPlatform::Windows), QStringLiteral("composer.cmd")),
                 QStringLiteral("@call \"C:\\tools\\composer.bat\" %*\r\n"));
        // No Composer: no launcher for it
        tools.composer.clear();
        QCOMPARE(terminalLaunchers(tools, TerminalPlatform::Windows).size(), 1);
    }

    void shellLaunchers()
    {
        TerminalTools tools;
        tools.php = QStringLiteral("/Users/me/Library/App Support/php's/bin/php");
        tools.composer = QStringLiteral("/opt/homebrew/bin/composer");
        const auto files = terminalLaunchers(tools, TerminalPlatform::MacOS);
        QCOMPARE(text(files, QStringLiteral("php")),
                 QStringLiteral("#!/bin/sh\nexec '/Users/me/Library/App Support/php'\\''s/bin/php' \"$@\"\n"));
        QCOMPARE(text(files, QStringLiteral("composer")), QStringLiteral("#!/bin/sh\nexec '/opt/homebrew/bin/composer' \"$@\"\n"));
    }

    void launchersRun()
    {
#ifdef Q_OS_WIN
        QSKIP("the shell launchers are for macOS and Linux");
#else
        const QTemporaryDir dir;
        TerminalTools tools;
        tools.php = QStringLiteral("/bin/echo");
        QVERIFY(writeTerminalLaunchers(dir.path(), tools).ok());
        const QString php = QDir(dir.path()).filePath(QStringLiteral("php"));
        QVERIFY(QFileInfo(php).isExecutable());
        Command command;
        command.program = php;
        command.arguments = {QStringLiteral("-v"), QStringLiteral("a b")};
        const Captured out = runCaptured(command, 20000);
        QVERIFY(out.succeeded());
        QCOMPARE(out.out.trimmed(), QStringLiteral("-v a b"));
        // Composer gone since: its old launcher goes
        QFile stale(QDir(dir.path()).filePath(QStringLiteral("composer")));
        QVERIFY(stale.open(QIODevice::WriteOnly));
        stale.close();
        QVERIFY(writeTerminalLaunchers(dir.path(), tools).ok());
        QVERIFY(!stale.exists());
#endif
    }

    void banner()
    {
        TerminalTools tools;
        tools.php = QStringLiteral("/usr/bin/php");
        QCOMPARE(terminalBanner(tools), QStringLiteral("FloCMS Studio: php /usr/bin/php"));
        tools.composer = QStringLiteral("/usr/bin/php");
        tools.composerArguments = {QStringLiteral("/home/me/composer.phar")};
        QCOMPARE(terminalBanner(tools), QStringLiteral("FloCMS Studio: php /usr/bin/php, composer /home/me/composer.phar"));
    }

    void windowsTerminal()
    {
        const auto wt = terminalCommand(TerminalPlatform::Windows, {}, QStringLiteral("C:/Sites/shop"),
                                        QStringLiteral("C:/Users/me/AppData/Local/Studio/terminal-bin"),
                                        QStringLiteral("FloCMS Studio: php C:\\php\\php.exe"),
                                        only({QStringLiteral("wt"), QStringLiteral("powershell")}));
        QVERIFY(wt.ok());
        QCOMPARE(wt.value().program, QStringLiteral("/bin/wt"));
        QCOMPARE(wt.value().arguments.mid(0, 7),
                 (QStringList{QStringLiteral("-d"), QStringLiteral("C:\\Sites\\shop"), QStringLiteral("/bin/powershell"),
                              QStringLiteral("-NoExit"), QStringLiteral("-NoLogo"), QStringLiteral("-EncodedCommand"),
                              wt.value().arguments.value(6)}));
        QVERIFY(!wt.value().newConsole);
        // The encoded script sets PATH and prints the line
        const QByteArray utf16 = QByteArray::fromBase64(wt.value().arguments.value(6).toLatin1());
        const QString script = QString::fromUtf16(reinterpret_cast<const char16_t *>(utf16.constData()), utf16.size() / 2);
        QVERIFY2(script.contains(QStringLiteral("$env:PATH = 'C:\\Users\\me\\AppData\\Local\\Studio\\terminal-bin' + ';' + $env:PATH")),
                 qPrintable(script));
        QVERIFY(script.contains(QStringLiteral("Write-Host 'FloCMS Studio: php C:\\php\\php.exe'")));

        // No Windows Terminal: PowerShell in a console of its own
        const auto ps = terminalCommand(TerminalPlatform::Windows, {}, QStringLiteral("C:/Sites/shop"), QStringLiteral("C:/bin"),
                                        QStringLiteral("x"), only({QStringLiteral("powershell")}));
        QCOMPARE(ps.value().program, QStringLiteral("/bin/powershell"));
        QCOMPARE(ps.value().workingDir, QStringLiteral("C:/Sites/shop"));
        QVERIFY(ps.value().newConsole);
        QCOMPARE(terminalCommand(TerminalPlatform::Windows, {}, QStringLiteral("C:/a"), QStringLiteral("C:/b"), {}, only({}))
                     .error()
                     .code,
                 QStringLiteral("terminal_not_found"));
    }

    void macTerminal()
    {
        const auto mac = terminalCommand(TerminalPlatform::MacOS, {}, QStringLiteral("/Users/me/Sites/it's"),
                                         QStringLiteral("/Users/me/Library/Application Support/Studio/terminal-bin"),
                                         QStringLiteral("FloCMS Studio: php /opt/php"), only({QStringLiteral("open")}));
        QVERIFY(mac.ok());
        QCOMPARE(mac.value().program, QStringLiteral("/bin/open"));
        QCOMPARE(mac.value().arguments.mid(0, 2), (QStringList{QStringLiteral("-a"), QStringLiteral("Terminal")}));
        QCOMPARE(mac.value().arguments.value(2), mac.value().scriptPath);
        QVERIFY(mac.value().scriptPath.startsWith(QStringLiteral("/Users/me/Library/Application Support/Studio/terminal-")));
        QVERIFY(mac.value().scriptPath.endsWith(QStringLiteral(".command")));
        const QString script = QString::fromUtf8(mac.value().script);
        QVERIFY2(script.contains(QStringLiteral("cd '/Users/me/Sites/it'\\''s' || exit 1")), qPrintable(script));
        QVERIFY(script.contains(QStringLiteral("PATH='/Users/me/Library/Application Support/Studio/terminal-bin':\"$PATH\"")));
        QVERIFY(script.contains(QStringLiteral("exec \"${SHELL:-/bin/zsh}\" -l")));
    }

    void linuxTerminals()
    {
        const QString folder = QStringLiteral("/home/me/sites/shop");
        const QString bin = QStringLiteral("/home/me/.local/share/Studio/terminal-bin");
        const auto first = [&](const QStringList &known, const QString &env = {}) {
            return terminalCommand(TerminalPlatform::Linux, {}, folder, bin, QStringLiteral("line"), only(known), env).value();
        };
        const TerminalCommand env = first({QStringLiteral("alacritty"), QStringLiteral("gnome-terminal")}, QStringLiteral("alacritty"));
        QCOMPARE(env.program, QStringLiteral("/bin/alacritty"));
        QCOMPARE(env.arguments.value(0), QStringLiteral("-e"));
        QCOMPARE(env.arguments.mid(1, 2), (QStringList{QStringLiteral("sh"), QStringLiteral("-c")}));
        QCOMPARE(env.arguments.mid(env.arguments.size() - 3), (QStringList{folder, bin, QStringLiteral("line")}));
        QCOMPARE(first({QStringLiteral("x-terminal-emulator"), QStringLiteral("konsole")}).program,
                 QStringLiteral("/bin/x-terminal-emulator"));
        const TerminalCommand gnome = first({QStringLiteral("gnome-terminal"), QStringLiteral("konsole")});
        QCOMPARE(gnome.arguments.mid(0, 2), (QStringList{QStringLiteral("--working-directory=") + folder, QStringLiteral("--")}));
        const TerminalCommand konsole = first({QStringLiteral("konsole")});
        QCOMPARE(konsole.arguments.mid(0, 3), (QStringList{QStringLiteral("--workdir"), folder, QStringLiteral("-e")}));
        const TerminalCommand xfce = first({QStringLiteral("xfce4-terminal")});
        QCOMPARE(xfce.arguments.value(1), QStringLiteral("-x"));
        QCOMPARE(terminalCommand(TerminalPlatform::Linux, {}, folder, bin, {}, only({})).error().code,
                 QStringLiteral("terminal_not_found"));
    }

    void linuxStarterRuns()
    {
#ifdef Q_OS_WIN
        QSKIP("the sh starter is for Linux");
#else
        // The starter, with `env` in place of the person's shell, shows
        // where it ends up: the folder, and the launchers first on PATH
        const QTemporaryDir folder;
        const QTemporaryDir bin;
        const auto command = terminalCommand(TerminalPlatform::Linux, {}, folder.path(), bin.path(),
                                             QStringLiteral("FloCMS Studio: php x"), only({QStringLiteral("konsole")}));
        const QStringList starter = command.value().arguments.mid(3);
        // The "shell": says where it is and what PATH it got
        const QString shell = QDir(bin.path()).filePath(QStringLiteral("fake-shell"));
        QFile file(shell);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#!/bin/sh\npwd\necho \"$PATH\"\n");
        file.close();
        QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        Command run;
        run.program = QStringLiteral("/bin/sh");
        run.arguments = starter.mid(1);
        run.environment.insert(QStringLiteral("SHELL"), shell);
        const Captured out = runCaptured(run, 20000);
        QVERIFY2(out.succeeded(), qPrintable(out.err));
        const QStringList lines = out.out.trimmed().split(u'\n');
        QCOMPARE(lines.value(0), QStringLiteral("FloCMS Studio: php x"));
        QCOMPARE(QFileInfo(lines.value(1)).canonicalFilePath(), QFileInfo(folder.path()).canonicalFilePath());
        QVERIFY2(lines.value(2).startsWith(bin.path() + u':'), qPrintable(lines.value(2)));
#endif
    }

    void ownCommand()
    {
        const auto own = terminalCommand(TerminalPlatform::Linux, QStringLiteral("wezterm start --cwd \"{folder}\""),
                                         QStringLiteral("/home/me/my site"), QStringLiteral("/bin-dir"), QStringLiteral("x"),
                                         only({QStringLiteral("wezterm")}));
        QCOMPARE(own.value().program, QStringLiteral("/bin/wezterm"));
        QCOMPARE(own.value().arguments,
                 (QStringList{QStringLiteral("start"), QStringLiteral("--cwd"), QStringLiteral("/home/me/my site")}));
        QVERIFY(own.value().script.isEmpty());
        QCOMPARE(terminalCommand(TerminalPlatform::Linux, QStringLiteral("nothing-here {folder}"), QStringLiteral("/a"),
                                 QStringLiteral("/b"), {}, only({}))
                     .error()
                     .code,
                 QStringLiteral("terminal_not_found"));
    }
};

QTEST_GUILESS_MAIN(TestTerminal)
#include "tst_terminal.moc"
