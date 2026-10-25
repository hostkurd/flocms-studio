// Tool downloads (D15). Offline: the tool list, unpacking and installing
// with archives made here. Online, when FLOSTUDIO_ONLINE=1 (CI's tools
// workflow): this computer's PHP and Composer downloaded from the real
// sources with the built-in list, installed, run, and checked against
// what `php flo doctor` needs.

#include <flostudio/core/downloads.h>
#include <flostudio/core/process.h>
#include <flostudio/core/tools.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxyFactory>
#include <QProcess>
#include <QTemporaryDir>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include <algorithm>

using namespace FloStudio::Core;

namespace {

QJsonObject toolEntry(const QString &id, const QString &tool, const QString &version, const QString &platform,
                      const QString &url = QStringLiteral("https://example.org/f"), const QString &archive = QStringLiteral("zip"))
{
    return QJsonObject{{QStringLiteral("id"), id},           {QStringLiteral("tool"), tool},
                       {QStringLiteral("version"), version}, {QStringLiteral("platform"), platform},
                       {QStringLiteral("urls"), QJsonArray{url}},
                       {QStringLiteral("sha256"), QString(64, u'a')},
                       {QStringLiteral("size"), 10},         {QStringLiteral("archive"), archive},
                       {QStringLiteral("source"), QStringLiteral("test")}};
}

QByteArray manifest(const QJsonArray &tools)
{
    return QJsonDocument(QJsonObject{{QStringLiteral("v"), 1}, {QStringLiteral("tools"), tools}}).toJson();
}

void writeFile(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        qFatal("cannot write %s", qUtf8Printable(path));
    file.write(text);
}

// tar.gz with `tar -czf` (every system has it)
bool makeTarGz(const QString &archive, const QString &folder)
{
    QProcess tar;
    tar.setWorkingDirectory(folder);
    tar.start(QStringLiteral("tar"), {QStringLiteral("-czf"), archive, QStringLiteral(".")});
    return tar.waitForFinished(30000) && tar.exitCode() == 0;
}

ToolPackage package(const QString &tool, const QString &platform, const QString &archive, const QString &sha256)
{
    ToolPackage p;
    p.id = tool == u"php" ? QStringLiteral("php-8.5") : tool;
    p.tool = tool;
    p.version = QStringLiteral("8.5.0");
    p.platform = platform;
    p.archive = archive;
    p.sha256 = sha256;
    p.urls = {QStringLiteral("https://example.org/x")};
    return p;
}

} // namespace

class TestDownloads : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QNetworkProxyFactory::setUseSystemConfiguration(true); }

    void platforms()
    {
        const QString platform = currentPlatform();
        QVERIFY2(QStringList({QStringLiteral("windows-x86_64"), QStringLiteral("macos-arm64"), QStringLiteral("macos-x86_64"),
                              QStringLiteral("linux-x86_64"), QStringLiteral("linux-arm64")})
                     .contains(platform),
                 qUtf8Printable(platform));
    }

    void readsTheToolList()
    {
        const QJsonArray tools{
            toolEntry(QStringLiteral("php-8.4"), QStringLiteral("php"), QStringLiteral("8.4.14"), QStringLiteral("linux-x86_64")),
            toolEntry(QStringLiteral("composer"), QStringLiteral("composer"), QStringLiteral("2.8.12"), QStringLiteral("any"),
                      QStringLiteral("https://getcomposer.org/download/2.8.12/composer.phar"), QStringLiteral("phar")),
            toolEntry(QStringLiteral("php-8.5"), QStringLiteral("php"), QStringLiteral("8.5.1"), QStringLiteral("linux-x86_64")),
            toolEntry(QStringLiteral("php-8.5"), QStringLiteral("php"), QStringLiteral("8.5.1"), QStringLiteral("macos-arm64")),
        };
        const auto packages = toolPackages(manifest(tools), QStringLiteral("linux-x86_64"));
        QVERIFY(packages.ok());
        QStringList ids;
        for (const ToolPackage &p : packages.value())
            ids << p.id;
        QCOMPARE(ids, (QStringList{QStringLiteral("php-8.5"), QStringLiteral("php-8.4"), QStringLiteral("composer")}));
        QCOMPARE(packages->first().urls, QStringList{QStringLiteral("https://example.org/f")});
        QCOMPARE(toolPackages("nope", QStringLiteral("linux-x86_64")).error().code, QStringLiteral("tools_list"));
    }

    void leavesOutUnsafeEntries()
    {
        QJsonObject http = toolEntry(QStringLiteral("php-8.5"), QStringLiteral("php"), QStringLiteral("8.5.1"), QStringLiteral("any"),
                                     QStringLiteral("http://example.org/php.zip"));
        QJsonObject badHash = toolEntry(QStringLiteral("php-8.4"), QStringLiteral("php"), QStringLiteral("8.4.1"), QStringLiteral("any"));
        badHash.insert(QStringLiteral("sha256"), QStringLiteral("ABC"));
        QJsonObject escape = toolEntry(QStringLiteral("../evil"), QStringLiteral("php"), QStringLiteral("8.4.1"), QStringLiteral("any"));
        QJsonObject exe = toolEntry(QStringLiteral("php-8.3"), QStringLiteral("php"), QStringLiteral("8.3.1"), QStringLiteral("any"),
                                    QStringLiteral("https://example.org/x.exe"), QStringLiteral("exe"));
        QJsonObject noSize = toolEntry(QStringLiteral("php-8.2"), QStringLiteral("php"), QStringLiteral("8.2.1"), QStringLiteral("any"));
        noSize.insert(QStringLiteral("size"), 0);
        const QJsonObject good = toolEntry(QStringLiteral("composer"), QStringLiteral("composer"), QStringLiteral("2.10.3"),
                                           QStringLiteral("any"), QStringLiteral("https://example.org/composer.phar"),
                                           QStringLiteral("phar"));
        const auto packages = toolPackages(manifest({http, badHash, escape, exe, noSize, good}), QStringLiteral("linux-x86_64"));
        QVERIFY(packages.ok());
        QCOMPARE(packages->size(), 1); // only the good one
        QCOMPARE(packages->first().id, QStringLiteral("composer"));
    }

    void theBuiltInListIsValid()
    {
        QFile file(QStringLiteral(":/flostudio/core/tools.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray json = file.readAll();
        const QJsonArray all = QJsonDocument::fromJson(json).object().value(QStringLiteral("tools")).toArray();
        // Every entry, those for any platform included, is accepted somewhere
        QSet<QString> accepted;
        for (const QString &platform : {QStringLiteral("windows-x86_64"), QStringLiteral("macos-arm64"), QStringLiteral("macos-x86_64"),
                                        QStringLiteral("linux-x86_64"), QStringLiteral("linux-arm64")}) {
            const auto packages = toolPackages(json, platform);
            QVERIFY(packages.ok());
            for (const ToolPackage &p : packages.value())
                accepted << p.id + u'@' + p.platform;
        }
        QSet<QString> listed;
        for (const auto &item : all)
            listed << item.toObject().value(QStringLiteral("id")).toString() + u'@'
                          + item.toObject().value(QStringLiteral("platform")).toString();
        QCOMPARE(accepted, listed);
        QVERIFY(listed.size() >= 10);
    }

    void phpIniForWindows()
    {
        const QString ini = windowsPhpIni(QStringLiteral("C:\\Users\\a\\tools\\php-8.5\\ext"));
        QVERIFY(ini.contains(QStringLiteral("extension_dir = \"C:\\Users\\a\\tools\\php-8.5\\ext\"")));
        // Everything `php flo doctor` checks (see missingExtensions), and Composer's zip
        QStringList enabled;
        for (const QString &line : ini.split(QStringLiteral("\r\n")))
            if (line.startsWith(QStringLiteral("extension=")))
                enabled << line.mid(10);
        enabled << QStringLiteral("pdo"); // built in
        QCOMPARE(missingExtensions(enabled, QStringLiteral("mysql")), MissingExtensions{});
        QCOMPARE(missingExtensions(enabled, QStringLiteral("sqlite")), MissingExtensions{});
        QVERIFY(enabled.contains(QStringLiteral("zip")));
    }

    void hashesFiles()
    {
        const QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("a")), "abc");
        QCOMPARE(sha256OfFile(dir.filePath(QStringLiteral("a"))),
                 QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        QCOMPARE(sha256OfFile(dir.filePath(QStringLiteral("missing"))), QString());
    }

    void installsAStaticPhp()
    {
        // static-php-cli's tar.gz holds one file: php
        const QTemporaryDir dir;
        const QString content = dir.filePath(QStringLiteral("content"));
        writeFile(content + QStringLiteral("/php"), "#!/bin/sh\necho 8.5.0\n");
        const QString archive = dir.filePath(QStringLiteral("php.tar.gz"));
        QVERIFY(makeTarGz(archive, content));
        const QString tools = dir.filePath(QStringLiteral("tools"));
        const ToolPackage php = package(QStringLiteral("php"), QStringLiteral("linux-x86_64"), QStringLiteral("tar.gz"),
                                        sha256OfFile(archive));
        QCOMPARE(installedProgram(php, tools), QString());
        const auto installed = installDownloaded(php, archive, tools);
        QVERIFY2(installed.ok(), qUtf8Printable(installed ? QString() : installed.error().detail));
        QCOMPARE(installed.value(), QDir::toNativeSeparators(tools + QStringLiteral("/php-8.5/php")));
        QCOMPARE(installedProgram(php, tools), installed.value());
#ifndef Q_OS_WIN
        QVERIFY(QFileInfo(installed.value()).isExecutable());
#endif
        QCOMPARE(installedToolFolders(tools), QStringList{QDir::toNativeSeparators(tools + QStringLiteral("/php-8.5"))});

        // Another package (a newer file) replaces it; a different hash isn't "installed"
        ToolPackage other = php;
        other.sha256 = QString(64, u'b');
        QCOMPARE(installedProgram(other, tools), QString());
        QVERIFY(installDownloaded(other, archive, tools).ok());
        QCOMPARE(installedProgram(php, tools), QString());
        QVERIFY(!installedProgram(other, tools).isEmpty());
        QVERIFY(!QDir(tools + QStringLiteral("/php-8.5.new")).exists());
    }

    void movesUpTheTopFolder()
    {
        // MariaDB's archives: everything inside mariadb-11.8.5-winx64/
        const QTemporaryDir dir;
        const QString content = dir.filePath(QStringLiteral("content"));
        writeFile(content + QStringLiteral("/mariadb-11.8.5/bin/mariadbd"), "x");
        writeFile(content + QStringLiteral("/mariadb-11.8.5/bin/mariadb"), "x");
        const QString archive = dir.filePath(QStringLiteral("m.tar.gz"));
        QVERIFY(makeTarGz(archive, content));
        const QString tools = dir.filePath(QStringLiteral("tools"));
        ToolPackage db = package(QStringLiteral("mariadb"), QStringLiteral("linux-x86_64"), QStringLiteral("tar.gz"), sha256OfFile(archive));
        db.id = QStringLiteral("mariadb-11.8");
        const auto installed = installDownloaded(db, archive, tools);
        QVERIFY(installed.ok());
        QVERIFY(QFileInfo::exists(tools + QStringLiteral("/mariadb-11.8/bin/mariadbd")));
        QCOMPARE(installedToolFolders(tools), QStringList{QDir::toNativeSeparators(tools + QStringLiteral("/mariadb-11.8/bin"))});
    }

    void leavesOutMariadbsTestSuiteAndLibraries()
    {
        // MariaDB design note D3: only what Studio runs is unpacked
        const QTemporaryDir dir;
        const QString top = dir.filePath(QStringLiteral("content/mariadb-12.3.2-linux-systemd-x86_64"));
        for (const char *name : {"bin/mariadbd", "bin/mariadb-admin", "scripts/mariadb-install-db", "lib/libmariadb.so.3",
                                 "lib/plugin/ha_rocksdb.so", "share/english/errmsg.sys", "lib/libmariadbd.a",
                                 "mariadb-test/mariadb-test-run.pl", "mysql-test/suite/x.test", "sql-bench/run-all-tests",
                                 "include/mysql/mysql.h"})
            writeFile(top + u'/' + QLatin1String(name), "x");
        const QString archive = dir.filePath(QStringLiteral("m.tar.gz"));
        QVERIFY(makeTarGz(archive, dir.filePath(QStringLiteral("content"))));
        const QString tools = dir.filePath(QStringLiteral("tools"));
        ToolPackage db = package(QStringLiteral("mariadb"), QStringLiteral("linux-x86_64"), QStringLiteral("tar.gz"),
                                 sha256OfFile(archive));
        db.id = QStringLiteral("mariadb-12.3");
        QVERIFY(installDownloaded(db, archive, tools).ok());
        const QString root = tools + QStringLiteral("/mariadb-12.3/");
        for (const char *kept : {"bin/mariadbd", "bin/mariadb-admin", "scripts/mariadb-install-db", "lib/libmariadb.so.3",
                                 "lib/plugin/ha_rocksdb.so", "share/english/errmsg.sys"})
            QVERIFY2(QFileInfo::exists(root + QLatin1String(kept)), kept);
        for (const char *left : {"lib/libmariadbd.a", "mariadb-test", "mysql-test", "sql-bench", "include"})
            QVERIFY2(!QFileInfo::exists(root + QLatin1String(left)), left);
        // Other tools keep everything
        QCOMPARE(extractArchive(archive, QStringLiteral("tar.gz"), dir.filePath(QStringLiteral("all"))).ok(), true);
        QVERIFY(QFileInfo::exists(dir.filePath(QStringLiteral("all/mysql-test/suite/x.test"))));
    }

    void installsComposer()
    {
        const QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("dl")), "<?php // composer");
        const QString tools = dir.filePath(QStringLiteral("tools"));
        const ToolPackage phar = package(QStringLiteral("composer"), QStringLiteral("any"), QStringLiteral("phar"),
                                         sha256OfFile(dir.filePath(QStringLiteral("dl"))));
        const auto installed = installDownloaded(phar, dir.filePath(QStringLiteral("dl")), tools);
        QVERIFY(installed.ok());
        QVERIFY(installed->endsWith(QStringLiteral("composer.phar")));
    }

    void aBrokenArchiveIsAnError()
    {
        const QTemporaryDir dir;
        writeFile(dir.filePath(QStringLiteral("bad.tar.gz")), "not an archive");
        const QString tools = dir.filePath(QStringLiteral("tools"));
        const ToolPackage php = package(QStringLiteral("php"), QStringLiteral("linux-x86_64"), QStringLiteral("tar.gz"), QString(64, u'c'));
        QCOMPARE(installDownloaded(php, dir.filePath(QStringLiteral("bad.tar.gz")), tools).error().code, QStringLiteral("tool_extract"));
        QVERIFY(!QDir(installFolder(php, tools)).exists());

        // An archive without the program in it
        const QString content = dir.filePath(QStringLiteral("content"));
        writeFile(content + QStringLiteral("/README"), "x");
        QVERIFY(makeTarGz(dir.filePath(QStringLiteral("empty.tar.gz")), content));
        QCOMPARE(installDownloaded(php, dir.filePath(QStringLiteral("empty.tar.gz")), tools).error().code, QStringLiteral("tool_install"));
    }

    void unreachableAddressesFailInTurn()
    {
        const QTemporaryDir dir;
        ToolPackage php = package(QStringLiteral("php"), currentPlatform(), QStringLiteral("tar.gz"), QString(64, u'd'));
        php.urls = {QStringLiteral("https://127.0.0.1:1/a.tar.gz"), QStringLiteral("https://127.0.0.1:1/b.tar.gz")};
        QObject owner;
        bool done = false;
        Error error;
        startToolInstall(php, &owner, {}, [&](const Result<QString> &result) {
            done = true;
            if (!result)
                error = result.error();
        }, dir.path());
        QVERIFY(!done); // never before startToolInstall returns
        QTRY_VERIFY_WITH_TIMEOUT(done, 30000);
        QCOMPARE(error.code, QStringLiteral("tool_download"));
        QVERIFY(error.detail.contains(QStringLiteral("a.tar.gz")));
        QVERIFY(error.detail.contains(QStringLiteral("b.tar.gz")));
    }

    void aFailedInstallKeepsTheOldOne()
    {
        const QTemporaryDir dir;
        const QString content = dir.filePath(QStringLiteral("content"));
        writeFile(content + QStringLiteral("/php"), "#!/bin/sh\n");
        const QString archive = dir.filePath(QStringLiteral("php.tar.gz"));
        QVERIFY(makeTarGz(archive, content));
        const QString tools = dir.filePath(QStringLiteral("tools"));
        const ToolPackage php = package(QStringLiteral("php"), QStringLiteral("linux-x86_64"), QStringLiteral("tar.gz"),
                                        sha256OfFile(archive));
        QVERIFY(installDownloaded(php, archive, tools).ok());

        // A newer package without the program: refused, the old one stays
        const QString empty = dir.filePath(QStringLiteral("empty"));
        writeFile(empty + QStringLiteral("/README"), "x");
        QVERIFY(makeTarGz(dir.filePath(QStringLiteral("empty.tar.gz")), empty));
        ToolPackage newer = php;
        newer.sha256 = QString(64, u'e');
        QCOMPARE(installDownloaded(newer, dir.filePath(QStringLiteral("empty.tar.gz")), tools).error().code,
                 QStringLiteral("tool_install"));
        QVERIFY(!installedProgram(php, tools).isEmpty());

        // Cancelled before the swap: the same
        const std::atomic<bool> cancelled{true};
        QCOMPARE(installDownloaded(newer, archive, tools, &cancelled).error().code, QStringLiteral("tool_cancelled"));
        QVERIFY(!installedProgram(php, tools).isEmpty());
        QVERIFY(!QDir(tools + QStringLiteral("/php-8.5.new")).exists());
        QVERIFY(!QDir(tools + QStringLiteral("/php-8.5.old")).exists());
    }

    void refusesLinksThatPointOutside()
    {
#ifdef Q_OS_WIN
        QSKIP("tar makes no links on Windows");
#else
        const QTemporaryDir dir;
        const QString content = dir.filePath(QStringLiteral("content"));
        writeFile(content + QStringLiteral("/php"), "#!/bin/sh\n");
        writeFile(dir.filePath(QStringLiteral("outside")), "keep");
        QVERIFY(QFile::link(dir.filePath(QStringLiteral("outside")), content + QStringLiteral("/link")));
        const QString archive = dir.filePath(QStringLiteral("php.tar.gz"));
        QVERIFY(makeTarGz(archive, content));
        const QString tools = dir.filePath(QStringLiteral("tools"));
        const ToolPackage php = package(QStringLiteral("php"), QStringLiteral("linux-x86_64"), QStringLiteral("tar.gz"),
                                        sha256OfFile(archive));
        const auto installed = installDownloaded(php, archive, tools);
        QCOMPARE(installed.error().code, QStringLiteral("tool_extract"));
        QVERIFY(!QDir(installFolder(php, tools)).exists());
        QVERIFY(!QDir(installFolder(php, tools) + QStringLiteral(".new")).exists());

        // A link inside the package is fine
        QFile::remove(content + QStringLiteral("/link"));
        QVERIFY(QFile::link(QStringLiteral("php"), content + QStringLiteral("/php-cli")));
        QVERIFY(makeTarGz(archive, content));
        ToolPackage again = php;
        again.sha256 = sha256OfFile(archive);
        QVERIFY(installDownloaded(again, archive, tools).ok());
#endif
    }

    void listsInstalledFoldersNewestFirst()
    {
        const QTemporaryDir dir;
        for (const char *name : {"php-8.4", "php-8.10", "php-8.5", "php-8.5.new", "php-8.5.old", "composer", "empty"}) {
            const QString folder = dir.filePath(QString::fromLatin1(name));
            QDir().mkpath(folder);
            if (qstrcmp(name, "empty") != 0)
                writeFile(folder + QStringLiteral("/.flostudio-package"), QByteArray(64, 'a'));
        }
        const auto native = [&dir](const char *name) { return QDir::toNativeSeparators(dir.filePath(QString::fromLatin1(name))); };
        QCOMPARE(installedToolFolders(dir.path()),
                 (QStringList{native("composer"), native("php-8.10"), native("php-8.5"), native("php-8.4")}));
    }

    void cancelledBeforeItStarts()
    {
        const QTemporaryDir dir;
        ToolPackage php = package(QStringLiteral("php"), currentPlatform(), QStringLiteral("tar.gz"), QString(64, u'd'));
        php.size = 10;
        QObject owner;
        bool done = false;
        Error error;
        QObject *install = startToolInstall(php, &owner, {}, [&](const Result<QString> &result) {
            done = true;
            error = result ? Error{} : result.error();
        }, dir.path());
        cancelToolInstall(install);
        QVERIFY(!done); // never from inside cancelToolInstall
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        QCOMPARE(error.code, QStringLiteral("tool_cancelled"));
        QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral(".downloads/php-8.5.part"))));
    }

    // A local plain-HTTP server: refused as not HTTPS, and a file larger
    // than the list says is stopped before it is all read
    void refusesPlainHttpAndOversizedFiles()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        const QByteArray body(qsizetype{64} * 1024, 'x');
        connect(&server, &QTcpServer::newConnection, &server, [&server, &body] {
            QTcpSocket *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket, &body] {
                socket->readAll();
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size())
                              + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        const QString url = QStringLiteral("http://127.0.0.1:%1/php.tar.gz").arg(server.serverPort());
        const auto run = [&url](qint64 size) -> Error {
            const QTemporaryDir dir;
            ToolPackage php = package(QStringLiteral("php"), currentPlatform(), QStringLiteral("tar.gz"), QString(64, u'd'));
            php.urls = {url};
            php.size = size;
            QObject owner;
            bool done = false;
            Error error;
            startToolInstall(php, &owner, {}, [&](const Result<QString> &result) {
                done = true;
                error = result ? Error{} : result.error();
            }, dir.path());
            if (!QTest::qWaitFor([&done] { return done; }, 10000))
                return Error{QStringLiteral("timeout"), url};
            if (QFileInfo::exists(dir.filePath(QStringLiteral(".downloads/php-8.5.part"))))
                return Error{QStringLiteral("part_left"), url};
            return error;
        };
        const Error plain = run(body.size());
        QCOMPARE(plain.code, QStringLiteral("tool_download"));
        QVERIFY2(plain.detail.contains(QStringLiteral("not HTTPS")), qUtf8Printable(plain.detail));
        const Error big = run(1000);
        QCOMPARE(big.code, QStringLiteral("tool_download"));
        QVERIFY2(big.detail.contains(QStringLiteral("larger than 1000 bytes")), qUtf8Printable(big.detail));
    }

    // --- online (FLOSTUDIO_ONLINE=1) --------------------------------------

    void downloadsThisComputersPhpAndComposer()
    {
        if (qEnvironmentVariable("FLOSTUDIO_ONLINE") != u"1")
            QSKIP("FLOSTUDIO_ONLINE is not set");
        const QTemporaryDir dir;
        const QList<ToolPackage> packages = bundledToolPackages();
        QVERIFY2(!packages.isEmpty(), "the built-in tool list has nothing for this computer");
        Settings settings;
        bool mariadbTried = false; // the default series only: the Linux bintar is ~470 MB
        for (const ToolPackage &p : packages) {
            if (p.tool == u"mariadb") {
                if (mariadbTried)
                    continue;
                mariadbTried = true;
            }
            QObject owner;
            bool done = false;
            Result<QString> result = Error{};
            qint64 last = 0;
            startToolInstall(p, &owner, [&last](qint64 received, qint64) { last = received; },
                             [&](const Result<QString> &r) {
                                 done = true;
                                 result = r;
                             },
                             dir.path());
            QTRY_VERIFY_WITH_TIMEOUT(done, 600000);
            QVERIFY2(result.ok(), qUtf8Printable(result ? QString() : result.error().code + u' ' + result.error().detail));
            QCOMPARE(last, p.size);
            qInfo("%s %s installed: %s", qUtf8Printable(p.id), qUtf8Printable(p.version), qUtf8Printable(result.value()));
            if (p.tool == u"php" && settings.phpPath.isEmpty()) {
                settings.phpPath = result.value();
                const PhpStatus php = detectPhp(settings);
                QCOMPARE(php.info.version, p.version);
                QVERIFY(php.supported);
                const MissingExtensions missing = missingExtensions(php.info.extensions, QStringLiteral("mysql"));
                QVERIFY2(missing.required.isEmpty() && missing.recommended.isEmpty(),
                         qUtf8Printable((missing.required + missing.recommended).join(u',')));
                QVERIFY(missingExtensions(php.info.extensions, QStringLiteral("sqlite")).required.isEmpty());
                QVERIFY2(php.info.extensions.contains(QStringLiteral("zip")), "Composer wants zip");
            }
            if (p.tool == u"mariadb") {
                // Runs at all? On Linux this shows whether the bintar finds
                // the system's libraries (libaio on Ubuntu 24.04: design Q5)
                QProcess server;
                server.start(result.value(), {QStringLiteral("--version")});
                QVERIFY(server.waitForFinished(60000));
                const QString output = QString::fromLocal8Bit(server.readAllStandardOutput() + server.readAllStandardError()).trimmed();
                qInfo("%s --version (exit %d): %s", qUtf8Printable(p.id), server.exitCode(), qUtf8Printable(output));
                QVERIFY(!QFileInfo::exists(QFileInfo(result.value()).dir().filePath(QStringLiteral("../mysql-test"))));
            }
            if (p.tool == u"composer") {
                settings.composerPath = result.value();
                const ToolStatus composer = detectComposer(settings);
                QCOMPARE(composer.version, p.version);
            }
        }
    }

    void aChangedFileIsRefused()
    {
        if (qEnvironmentVariable("FLOSTUDIO_ONLINE") != u"1")
            QSKIP("FLOSTUDIO_ONLINE is not set");
        const QList<ToolPackage> packages = bundledToolPackages();
        const auto composer = std::find_if(packages.begin(), packages.end(), [](const ToolPackage &p) { return p.tool == u"composer"; });
        QVERIFY(composer != packages.end());
        ToolPackage wrong = *composer;
        wrong.sha256 = QString(64, u'0');
        const QTemporaryDir dir;
        QObject owner;
        bool done = false;
        Error error;
        startToolInstall(wrong, &owner, {}, [&](const Result<QString> &r) {
            done = true;
            if (!r)
                error = r.error();
        }, dir.path());
        QTRY_VERIFY_WITH_TIMEOUT(done, 300000);
        QCOMPARE(error.code, QStringLiteral("tool_hash"));
        QVERIFY(QDir(dir.filePath(QStringLiteral(".downloads"))).isEmpty()); // the file is deleted
        QCOMPARE(installedProgram(wrong, dir.path()), QString());
    }
};

QTEST_GUILESS_MAIN(TestDownloads)
#include "tst_downloads.moc"
