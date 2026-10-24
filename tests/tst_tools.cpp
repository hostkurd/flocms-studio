#include "testutil.h"

#include <flostudio/core/tools.h>

#include <QFile>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>

using namespace FloStudio::Core;

namespace {

QString writeFile(const QString &path, const QByteArray &text, bool executable = false)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        qFatal("cannot write %s", qUtf8Printable(path));
    file.write(text);
    file.close();
    if (executable)
        file.setPermissions(file.permissions() | QFile::ExeOwner | QFile::ExeUser);
    return QDir::toNativeSeparators(path);
}

} // namespace

class TestTools : public QObject
{
    Q_OBJECT

private slots:
    void composerVersions()
    {
        // Real output, with Composer's notes for root before the version line
        QCOMPARE(composerVersion(TestUtil::fixture(QStringLiteral("tools/composer-version.out"))), QStringLiteral("2.8.12"));
        QCOMPARE(composerVersion(QStringLiteral("Composer version 2.9.1 2026-01-02 10:00:00\r\n")), QStringLiteral("2.9.1"));
        QCOMPARE(composerVersion(QStringLiteral("nothing")), QString());
    }

    void databaseVersions()
    {
        // Real output (MariaDB 10.11 on Ubuntu 24.04)
        QCOMPARE(databaseVersion(TestUtil::fixture(QStringLiteral("tools/mariadb-version.out"))),
                 (DatabaseProduct{QStringLiteral("MariaDB"), QStringLiteral("10.11.14")}));
        QCOMPARE(databaseVersion(TestUtil::fixture(QStringLiteral("tools/mysql-version.out"))),
                 (DatabaseProduct{QStringLiteral("MariaDB"), QStringLiteral("10.11.14")}));
        // The formats of MariaDB 11 and MySQL 5.7 / 8 (from their
        // documentation; not captured here)
        QCOMPARE(databaseVersion(QStringLiteral("mariadb from 11.4.3-MariaDB, client 15.2 for Win64 (AMD64)")),
                 (DatabaseProduct{QStringLiteral("MariaDB"), QStringLiteral("11.4.3")}));
        QCOMPARE(databaseVersion(QStringLiteral("mysql  Ver 8.0.39 for Linux on x86_64 (MySQL Community Server - GPL)")),
                 (DatabaseProduct{QStringLiteral("MySQL"), QStringLiteral("8.0.39")}));
        QCOMPARE(databaseVersion(QStringLiteral("mysql  Ver 14.14 Distrib 5.7.44, for Linux (x86_64) using  EditLine wrapper")),
                 (DatabaseProduct{QStringLiteral("MySQL"), QStringLiteral("5.7.44")}));
        QCOMPARE(databaseVersion(QStringLiteral("command not found")), DatabaseProduct{});
    }

    void phpProbe()
    {
        const PhpInfo info = parsePhpProbe(QStringLiteral("8.4.2\r\nC:\\php\\php.exe\r\n\r\ncore,date,pdo,mbstring\r\n"));
        QCOMPARE(info.version, QStringLiteral("8.4.2"));
        QCOMPARE(info.binary, QStringLiteral("C:\\php\\php.exe"));
        QCOMPARE(info.iniFile, QString());
        QCOMPARE(info.extensions, (QStringList{QStringLiteral("core"), QStringLiteral("date"), QStringLiteral("pdo"),
                                               QStringLiteral("mbstring")}));
        QCOMPARE(parsePhpProbe(QStringLiteral("Could not open input file")).version, QString());
        QVERIFY(!phpProbeCode().contains(u'"'));
    }

    void phpVersions()
    {
        QVERIFY(phpVersionSupported(QStringLiteral("8.1.0")));
        QVERIFY(phpVersionSupported(QStringLiteral("8.5.1")));
        QVERIFY(phpVersionSupported(QStringLiteral("10.0.0")));
        QVERIFY(!phpVersionSupported(QStringLiteral("8.0.30")));
        QVERIFY(!phpVersionSupported(QStringLiteral("7.4.33")));
        QVERIFY(!phpVersionSupported(QString()));
    }

    void extensionsLikeDoctor()
    {
        const QStringList all{QStringLiteral("pdo"),  QStringLiteral("mbstring"), QStringLiteral("fileinfo"),
                              QStringLiteral("gd"),   QStringLiteral("openssl"),  QStringLiteral("curl"),
                              QStringLiteral("pdo_mysql")};
        QCOMPARE(missingExtensions(all, QStringLiteral("mysql")), MissingExtensions{});
        QCOMPARE(missingExtensions(all, QStringLiteral("sqlite")).required, QStringList{QStringLiteral("pdo_sqlite")});
        const MissingExtensions few = missingExtensions({QStringLiteral("PDO"), QStringLiteral("mbstring")}, QStringLiteral("mysql"));
        QCOMPARE(few.required, (QStringList{QStringLiteral("fileinfo"), QStringLiteral("gd"), QStringLiteral("pdo_mysql")}));
        QCOMPARE(few.recommended, (QStringList{QStringLiteral("openssl"), QStringLiteral("curl")}));
    }

    void batchFileArguments()
    {
        QVERIFY(safeForBatchFile(QStringLiteral("hostkurd/flocms-api:^1.3").remove(u'^'))); // a constraint without ^
        QVERIFY(safeForBatchFile(QStringLiteral("C:\\My Sites\\سایتی من")));
        QVERIFY(safeForBatchFile(QStringLiteral("--no-interaction")));
        QVERIFY(!safeForBatchFile(QStringLiteral("hostkurd/flocms-api:^1.3")));
        QVERIFY(!safeForBatchFile(QStringLiteral("a&b")));
        QVERIFY(!safeForBatchFile(QStringLiteral("100%")));
        QVERIFY(!safeForBatchFile(QStringLiteral("\"quoted\"")));
        QVERIFY(!safeForBatchFile(QStringLiteral("(x)")));
    }

    void editorCommands()
    {
        QCOMPARE(editorCommandLine(QStringLiteral("code \"{folder}\""), QStringLiteral("C:\\My Sites\\a")),
                 (QStringList{QStringLiteral("code"), QStringLiteral("C:\\My Sites\\a")}));
        QCOMPARE(editorCommandLine(QStringLiteral("\"C:\\Program Files\\App\\app.exe\" --new {folder}"), QStringLiteral("/x")),
                 (QStringList{QStringLiteral("C:\\Program Files\\App\\app.exe"), QStringLiteral("--new"), QStringLiteral("/x")}));
        QCOMPARE(editorCommandLine(QStringLiteral("phpstorm"), QStringLiteral("/x y")),
                 (QStringList{QStringLiteral("phpstorm"), QStringLiteral("/x y")}));
        QCOMPARE(editorCommandLine(QStringLiteral("subl --project={folder}/a.sublime"), QStringLiteral("/s")),
                 (QStringList{QStringLiteral("subl"), QStringLiteral("--project=/s/a.sublime")}));
        QCOMPARE(editorCommandLine(QStringLiteral("  "), QStringLiteral("/x")), QStringList());
        QCOMPARE(editorCommandLine(QStringLiteral("\"\" x"), QStringLiteral("/x")), QStringList());
    }

    void editorErrors()
    {
        Settings settings;
        settings.editorCommand = QStringLiteral("no-such-editor-studio-test {folder}");
        QCOMPARE(openInEditor(settings, QDir::tempPath()).error().code, QStringLiteral("editor_not_found"));
    }

    void findsTools()
    {
        const QTemporaryDir dir;
        const QString file = writeFile(dir.filePath(QStringLiteral("bin/tool")), "x");
        QCOMPARE(findTool(file), QDir::toNativeSeparators(QFileInfo(file).absoluteFilePath()));
        QCOMPARE(findTool(dir.filePath(QStringLiteral("bin/missing"))), QString());
        QCOMPARE(findTool(QString()), QString());
#ifdef Q_OS_WIN
        QVERIFY(!findTool(QStringLiteral("cmd")).isEmpty());
#else
        QVERIFY(!findTool(QStringLiteral("sh")).isEmpty());
#endif
        const QStringList path = toolSearchPath();
        QVERIFY(!path.isEmpty());
        const QString first = qEnvironmentVariable("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts).value(0);
        if (!first.isEmpty())
            QCOMPARE(path.first(), QDir::toNativeSeparators(QDir::cleanPath(first)));
    }

    void composerThroughPhp()
    {
        const QTemporaryDir dir;
        Settings settings;
        settings.phpPath = writeFile(dir.filePath(QStringLiteral("php/php")), "", true);

        // A composer.phar runs through PHP and is setup's COMPOSER_BINARY
        settings.composerPath = writeFile(dir.filePath(QStringLiteral("a/composer.phar")), "<?php");
        auto launch = composerLaunch(settings);
        QVERIFY(launch.ok());
        QCOMPARE(launch->program, settings.phpPath);
        QCOMPARE(launch->arguments, QStringList{settings.composerPath});
        QCOMPARE(launch->setupBinary, settings.composerPath);
        QVERIFY(!launch->batchFile);

        // Homebrew's and Linux's `composer`: the phar with a php line
        settings.composerPath = writeFile(dir.filePath(QStringLiteral("b/composer")), "#!/usr/bin/env php\n<?php\n", true);
        launch = composerLaunch(settings);
        QCOMPARE(launch->arguments, QStringList{settings.composerPath});

        // Composer-Setup: composer.bat with composer.phar beside it
        const QString phar = writeFile(dir.filePath(QStringLiteral("c/composer.phar")), "<?php");
        settings.composerPath = writeFile(dir.filePath(QStringLiteral("c/composer.bat")), "@php \"%~dp0composer.phar\" %*\r\n");
        launch = composerLaunch(settings);
        QCOMPARE(launch->program, settings.phpPath);
        QCOMPARE(launch->arguments, QStringList{phar});
        QCOMPARE(launch->setupBinary, phar);

        // A .bat alone runs as it is (arguments are checked, see commands)
        settings.composerPath = writeFile(dir.filePath(QStringLiteral("d/composer.bat")), "@echo off\r\n");
        launch = composerLaunch(settings);
        QCOMPARE(launch->program, settings.composerPath);
        QVERIFY(launch->batchFile);
        QCOMPARE(launch->setupBinary, settings.composerPath);

        // Something else (a native program): run directly, no COMPOSER_BINARY
        settings.composerPath = writeFile(dir.filePath(QStringLiteral("e/composer")), "\x7f" "ELF", true);
        launch = composerLaunch(settings);
        QCOMPARE(launch->program, settings.composerPath);
        QVERIFY(launch->arguments.isEmpty());
        QVERIFY(launch->setupBinary.isEmpty());

        settings.composerPath = dir.filePath(QStringLiteral("missing/composer"));
        QCOMPARE(composerLaunch(settings).error().code, QStringLiteral("composer_not_found"));
        settings.composerPath = phar;
        settings.phpPath = dir.filePath(QStringLiteral("missing/php"));
        QCOMPARE(composerLaunch(settings).error().code, QStringLiteral("php_not_found"));
        QCOMPARE(phpProgram(settings).error().code, QStringLiteral("php_not_found"));
    }

    void thisComputersPhp()
    {
        const PhpStatus php = detectPhp(Settings{});
        if (!php.tool.found)
            QSKIP("no PHP on this computer");
        QVERIFY2(php.tool.error.isEmpty(), qUtf8Printable(php.tool.error));
        QVERIFY(!php.info.version.isEmpty());
        QVERIFY(QFileInfo::exists(php.info.binary));
        QVERIFY(php.info.extensions.contains(QStringLiteral("core")));
        qInfo("PHP %s at %s", qUtf8Printable(php.info.version), qUtf8Printable(php.tool.path));
    }

    void thisComputersComposer()
    {
        const ToolStatus composer = detectComposer(Settings{});
        if (!composer.found)
            QSKIP("no Composer on this computer");
        QVERIFY2(composer.error.isEmpty(), qUtf8Printable(composer.path));
        QVERIFY(composer.version.startsWith(u'2'));
        qInfo("Composer %s at %s", qUtf8Printable(composer.version), qUtf8Printable(composer.path));
    }

    void thisComputersDatabase()
    {
        const DatabaseServerStatus db = detectDatabaseServer();
        qInfo("database: %s %s, running: %s", qUtf8Printable(db.product.name), qUtf8Printable(db.product.version),
              db.running ? "yes" : "no");
        QCOMPARE(db.product.name.isEmpty(), db.clientPath.isEmpty());
    }

    void ports()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QVERIFY(portAnswers(QStringLiteral("127.0.0.1"), server.serverPort(), 2000));
        const quint16 port = server.serverPort();
        server.close();
        QVERIFY(!portAnswers(QStringLiteral("127.0.0.1"), port, 500));
    }
};

QTEST_GUILESS_MAIN(TestTools)
#include "tst_tools.moc"
