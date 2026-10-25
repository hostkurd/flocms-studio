// Studio's own MariaDB server (MariaDB design note D1–D12).
//
// The real-server test runs when FLOSTUDIO_MARIADBD names a mariadbd
// (CI on macOS: Homebrew's), or with FLOSTUDIO_ONLINE=1 on the MariaDB
// Studio downloads for this computer (CI on Windows and Linux: 12.3):
// set up a data folder, start, set root's password, create a database,
// stop cleanly, start again with the password.

#include <flostudio/core/dbserver.h>
#include <flostudio/core/downloads.h>
#include <flostudio/core/tools.h>

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QSet>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

using namespace FloStudio::Core;

namespace {

void writeFile(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        qFatal("cannot write %s", qUtf8Printable(path));
    file.write(text);
}

// A fake install: <root>/bin/mariadbd etc.
QString fakeInstall(const QString &root, bool windows, bool scripts)
{
    const QString exe = windows ? QStringLiteral(".exe") : QString();
    for (const QString &name : {QStringLiteral("mariadbd"), QStringLiteral("mariadb"), QStringLiteral("mariadb-admin")})
        writeFile(root + QStringLiteral("/bin/") + name + exe, "x");
    writeFile(root + (scripts ? QStringLiteral("/scripts/") : QStringLiteral("/bin/")) + QStringLiteral("mariadb-install-db") + exe, "x");
    return root + QStringLiteral("/bin/mariadbd") + exe;
}

// Deleted now, from the event loop's deferred deletes (no raw delete, D11).
void destroyNow(QObject *object)
{
    object->deleteLater();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

} // namespace

class TestDatabaseServer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void findsTheFilesOfAnInstall()
    {
        const QTemporaryDir dir;
        const QString server = fakeInstall(dir.filePath(QStringLiteral("mariadb-12.3")), false, true);
        const DatabaseServerFiles files =
            databaseServerFiles(server, QStringLiteral("12.3"), QStringLiteral("linux-x86_64"), dir.filePath(QStringLiteral("servers")));
        QCOMPARE(files.baseDir, dir.filePath(QStringLiteral("mariadb-12.3")));
        QCOMPARE(files.installDb, dir.filePath(QStringLiteral("mariadb-12.3/scripts/mariadb-install-db")));
        QCOMPARE(files.admin, dir.filePath(QStringLiteral("mariadb-12.3/bin/mariadb-admin")));
        QCOMPARE(files.client, dir.filePath(QStringLiteral("mariadb-12.3/bin/mariadb")));
        QCOMPARE(files.folder, dir.filePath(QStringLiteral("servers/12.3")));
        QCOMPARE(files.dataDir, dir.filePath(QStringLiteral("servers/12.3/data")));
        QCOMPARE(files.config, dir.filePath(QStringLiteral("servers/12.3/my.cnf")));
        QCOMPARE(files.socket, dir.filePath(QStringLiteral("servers/12.3/mysqld.sock")));

        // Windows: everything in bin/, my.ini, no socket
        const QString winServer = fakeInstall(dir.filePath(QStringLiteral("win")), true, false);
        const DatabaseServerFiles win =
            databaseServerFiles(winServer, QStringLiteral("12.3"), QStringLiteral("windows-x86_64"), dir.filePath(QStringLiteral("servers")));
        QCOMPARE(win.installDb, dir.filePath(QStringLiteral("win/bin/mariadb-install-db.exe")));
        QCOMPARE(win.config, dir.filePath(QStringLiteral("servers/12.3/my.ini")));
        QVERIFY(win.socket.isEmpty());

        // A system's sbin/mariadbd: the tools are in the install's bin/
        writeFile(dir.filePath(QStringLiteral("usr/sbin/mariadbd")), "x");
        writeFile(dir.filePath(QStringLiteral("usr/bin/mariadb-install-db")), "x");
        writeFile(dir.filePath(QStringLiteral("usr/bin/mariadb")), "x");
        const DatabaseServerFiles system = databaseServerFiles(dir.filePath(QStringLiteral("usr/sbin/mariadbd")), QStringLiteral("10.11"),
                                                               QStringLiteral("linux-x86_64"), dir.path());
        QCOMPARE(system.installDb, dir.filePath(QStringLiteral("usr/bin/mariadb-install-db")));
        QCOMPARE(system.client, dir.filePath(QStringLiteral("usr/bin/mariadb")));
    }

    void serversLiveBesideTheTools()
    {
        QCOMPARE(QFileInfo(databaseServersFolder()).fileName(), QStringLiteral("mariadb"));
        QCOMPARE(QFileInfo(databaseServersFolder()).path(), QFileInfo(toolsFolder()).path());
    }

    void readsTheSeries()
    {
        QCOMPARE(mariadbSeries(QStringLiteral("12.3.3")), QStringLiteral("12.3"));
        QCOMPARE(mariadbSeries(QStringLiteral("11.8")), QStringLiteral("11.8"));
        QCOMPARE(mariadbSeries(QStringLiteral(" 10.11.14-MariaDB")), QStringLiteral("10.11"));
        QCOMPARE(mariadbSeries(QStringLiteral("latest")), QString());
    }

    void writesTheConfig()
    {
        DatabaseServerFiles files;
        files.series = QStringLiteral("12.3");
        files.platform = QStringLiteral("windows-x86_64");
        files.baseDir = QStringLiteral("C:\\Users\\Jo Smith\\AppData\\Local\\FloCMS Studio\\tools\\mariadb-12.3");
        files.dataDir = QStringLiteral("C:\\Users\\Jo Smith\\AppData\\Local\\FloCMS Studio\\mariadb\\12.3\\data");
        files.pidFile = QStringLiteral("C:/x/mariadbd.pid");
        files.errorLog = QStringLiteral("C:/x/mariadbd.err");
        const QString config = databaseServerConfig(files, 3307);
        QVERIFY(config.contains(QStringLiteral("\r\n[mysqld]\r\n")));
        QVERIFY(config.contains(QStringLiteral("datadir=\"C:/Users/Jo Smith/AppData/Local/FloCMS Studio/mariadb/12.3/data\"")));
        QVERIFY(config.contains(QStringLiteral("port=3307")));
        QVERIFY(config.contains(QStringLiteral("bind-address=127.0.0.1")));
        QVERIFY(config.contains(QStringLiteral("character-set-server=utf8mb4")));
        QVERIFY(config.contains(QStringLiteral("collation-server=utf8mb4_unicode_ci")));
        QVERIFY(config.contains(QStringLiteral("innodb_buffer_pool_size=128M")));
        QVERIFY(!config.contains(QStringLiteral("socket=")));
        QVERIFY(!config.contains(u'\\'));
        QCOMPARE(config.count(QStringLiteral("port=3307")), 2); // [client] too

        files.platform = QStringLiteral("linux-x86_64");
        files.socket = QStringLiteral("/home/jo/.local/share/FloCMS Studio/mariadb/12.3/mysqld.sock");
        const QString unix = databaseServerConfig(files, 3306);
        QVERIFY(!unix.contains(u'\r'));
        QCOMPARE(unix.count(QStringLiteral("socket=\"/home/jo/.local/share/FloCMS Studio/mariadb/12.3/mysqld.sock\"")), 2);
    }

    void commandsNeverCarryThePassword()
    {
        const QTemporaryDir dir;
        const DatabaseServerFiles files = databaseServerFiles(fakeInstall(dir.filePath(QStringLiteral("m")), false, true),
                                                              QStringLiteral("12.3"), QStringLiteral("linux-x86_64"), dir.path());
        const QString secret = QStringLiteral("s3cretPassw0rd");
        for (const Command &command : {databaseShutdownCommand(files, 3306, secret),
                                       databaseSqlCommand(files, 3306, QStringLiteral("root"), secret, QStringLiteral("SELECT 1;"))}) {
            QVERIFY(!command.arguments.join(u' ').contains(secret));
            QVERIFY(!command.shown.contains(secret));
            QCOMPARE(command.environment.value(QStringLiteral("MYSQL_PWD")), secret);
            QVERIFY(command.removeEnvironment.contains(QStringLiteral("MYSQL_PWD")));
            QVERIFY(command.arguments.contains(QStringLiteral("--host=127.0.0.1")));
            QVERIFY(command.arguments.contains(QStringLiteral("--protocol=TCP")));
            QCOMPARE(command.arguments.first(), QStringLiteral("--no-defaults"));
            QVERIFY(command.environment.value(QStringLiteral("LD_LIBRARY_PATH")).startsWith(files.baseDir + QStringLiteral("/lib")));
        }
        const Command sql = databaseSqlCommand(files, 3310, QStringLiteral("app"), QString(), QStringLiteral("SELECT 1;"));
        QVERIFY(!sql.environment.contains(QStringLiteral("MYSQL_PWD")));
        QCOMPARE(sql.input, QByteArray("SELECT 1;"));
        QVERIFY(sql.arguments.contains(QStringLiteral("--port=3310")));
        QVERIFY(sql.arguments.contains(QStringLiteral("--user=app")));

        const Command install = databaseInstallCommand(files, 3306);
        QCOMPARE(install.program, files.installDb);
        QCOMPARE(install.arguments.first(), QStringLiteral("--no-defaults"));
        QVERIFY(install.arguments.contains(QStringLiteral("--auth-root-authentication-method=normal")));
        QVERIFY(install.arguments.contains(QStringLiteral("--skip-test-db")));
        const Command server = databaseServerCommand(files);
        QCOMPARE(server.arguments, QStringList{QStringLiteral("--defaults-file=") + QDir::toNativeSeparators(files.config)});
    }

    void makesPasswords()
    {
        QSet<QString> seen;
        for (int i = 0; i < 50; ++i) {
            const QString password = generateDatabasePassword();
            QCOMPARE(password.size(), 24);
            for (const QChar c : password)
                QVERIFY(c.isLetterOrNumber() && c.unicode() < 128);
            seen.insert(password);
        }
        QCOMPARE(seen.size(), 50);
    }

    void quotesSql()
    {
        QCOMPARE(sqlIdentifier(QStringLiteral("my`db")), QStringLiteral("`my``db`"));
        QCOMPARE(sqlString(QStringLiteral("it's \\ ok")), QStringLiteral("'it\\'s \\\\ ok'"));
    }

    void readsMissingLibraries()
    {
        QCOMPARE(missingSharedLibrary(QStringLiteral("/x/bin/mariadbd: error while loading shared libraries: libaio.so.1: "
                                                     "cannot open shared object file: No such file or directory")),
                 QStringLiteral("libaio.so.1"));
        QCOMPARE(missingSharedLibrary(QStringLiteral("dyld[123]: Library not loaded: /opt/homebrew/opt/pcre2/lib/libpcre2-8.0.dylib")),
                 QStringLiteral("/opt/homebrew/opt/pcre2/lib/libpcre2-8.0.dylib"));
        QCOMPARE(missingSharedLibrary(QStringLiteral("mariadbd  Ver 12.3.3-MariaDB for Linux on x86_64")), QString());
    }

    void linksLibaioInsideTheInstall()
    {
#ifdef Q_OS_WIN
        QSKIP("Linux only");
#endif
        const QTemporaryDir dir;
        const DatabaseServerFiles files = databaseServerFiles(fakeInstall(dir.filePath(QStringLiteral("m")), false, true),
                                                              QStringLiteral("12.3"), QStringLiteral("linux-x86_64"), dir.path());
        const QString system = dir.filePath(QStringLiteral("usr-lib"));
        QCOMPARE(linkLibaioT64(files, {system}), QString()); // not on this system
        writeFile(system + QStringLiteral("/libaio.so.1t64"), "lib");
        const QString link = linkLibaioT64(files, {system});
        QCOMPARE(link, files.baseDir + QStringLiteral("/lib/libaio.so.1"));
        QVERIFY(QFileInfo(link).isSymLink());
        QCOMPARE(QFileInfo(link).symLinkTarget(), system + QStringLiteral("/libaio.so.1t64"));
        QCOMPARE(linkLibaioT64(files, {system}), link); // again: kept
        DatabaseServerFiles mac = files;
        mac.platform = QStringLiteral("macos-arm64");
        QCOMPARE(linkLibaioT64(mac, {system}), QString());
    }

    void findsAFreePort()
    {
        QTcpServer taken;
        QVERIFY(taken.listen(QHostAddress::LocalHost, 0));
        const int port = taken.serverPort();
        const int free = freeDatabasePort(port, 5);
        QVERIFY(free > port);
        QVERIFY(free <= port + 4);
    }

    void readsTheEndOfTheErrorLog()
    {
        const QTemporaryDir dir;
        DatabaseServerFiles files;
        files.errorLog = dir.filePath(QStringLiteral("mariadbd.err"));
        QVERIFY(databaseErrorLogTail(files).isEmpty());
        QByteArray text;
        for (int i = 1; i <= 30; ++i)
            text += "line " + QByteArray::number(i) + "\r\n";
        writeFile(files.errorLog, text);
        const QStringList tail = databaseErrorLogTail(files, 3);
        QCOMPARE(tail, (QStringList{QStringLiteral("line 28"), QStringLiteral("line 29"), QStringLiteral("line 30")}));
    }

    void refusesABusyPortAndAMissingProgram()
    {
        const QTemporaryDir dir;
        QObject owner;
        DatabaseServerStart start;
        start.files = databaseServerFiles(dir.filePath(QStringLiteral("none/bin/mariadbd")), QStringLiteral("12.3"),
                                          currentPlatform(), dir.path());
        Result<Done> result = Done{};
        bool done = false;
        startDatabaseServer(start, &owner, {}, [&](const Result<Done> &r) { result = r; done = true; }, {});
        QTRY_VERIFY(done);
        QCOMPARE(result.error().code, QStringLiteral("db_program"));

        QTcpServer taken;
        QVERIFY(taken.listen(QHostAddress::LocalHost, 0));
        start.files = databaseServerFiles(fakeInstall(dir.filePath(QStringLiteral("m")), false, true), QStringLiteral("12.3"),
                                          currentPlatform(), dir.path());
        start.port = taken.serverPort();
        done = false;
        QObject *server = startDatabaseServer(start, &owner, {}, [&](const Result<Done> &r) { result = r; done = true; }, {});
        QTRY_VERIFY(done);
        QCOMPARE(result.error().code, QStringLiteral("db_port_busy"));
        QCOMPARE(result.error().detail, QString::number(start.port));
        QVERIFY(!databaseServerRunning(server));
        bool stopped = false;
        stopDatabaseServer(server, QString(), [&] { stopped = true; });
        QTRY_VERIFY(stopped);
    }

    // --- a real server (FLOSTUDIO_MARIADBD) --------------------------------

    void runsARealServer()
    {
        QString mariadbd = qEnvironmentVariable("FLOSTUDIO_MARIADBD");
        QString version = qEnvironmentVariable("FLOSTUDIO_MARIADB_SERIES");
        const QTemporaryDir tools;
        if (mariadbd.isEmpty() && qEnvironmentVariable("FLOSTUDIO_ONLINE") == u"1") {
            const QList<ToolPackage> packages = bundledToolPackages();
            const auto it = std::find_if(packages.begin(), packages.end(), [](const ToolPackage &p) { return p.tool == u"mariadb"; });
            if (it == packages.end())
                QSKIP("Studio downloads no MariaDB for this computer");
            QObject installer;
            bool installed = false;
            Result<QString> program = Error{};
            startToolInstall(*it, &installer, {}, [&](const Result<QString> &r) { program = r; installed = true; }, tools.path());
            QTRY_VERIFY_WITH_TIMEOUT(installed, 900000);
            QVERIFY2(program.ok(), qUtf8Printable(program ? QString() : program.error().code + u' ' + program.error().detail));
            mariadbd = program.value();
            version = mariadbSeries(it->version);
            qInfo("downloaded MariaDB %s: %s", qUtf8Printable(it->version), qUtf8Printable(mariadbd));
        }
        if (mariadbd.isEmpty())
            QSKIP("neither FLOSTUDIO_MARIADBD nor FLOSTUDIO_ONLINE is set");
        if (version.isEmpty())
            version = QStringLiteral("test");
        const QTemporaryDir dir;
        QObject owner;
        DatabaseServerStart start;
        start.files = databaseServerFiles(mariadbd, version, currentPlatform(), dir.path());
        start.port = freeDatabasePort(3406, 50);
        QVERIFY(start.port > 0);
        start.rootPassword = generateDatabasePassword();
        start.timeoutMs = 120000;
        QStringList log;
        const auto onLine = [&log](const OutputLine &line) { log << line.text; };
        int exits = 0;
        bool exitCrashed = true;
        const auto onExit = [&](int, bool crashed) { ++exits; exitCrashed = crashed; };

        Result<Done> result = Error{};
        bool done = false;
        QObject *server = startDatabaseServer(start, &owner, onLine, [&](const Result<Done> &r) { result = r; done = true; }, onExit);
        QTRY_VERIFY_WITH_TIMEOUT(done, 300000);
        QVERIFY2(result.ok(), qUtf8Printable(result ? QString() : result.error().code + u' ' + result.error().detail + u'\n' + log.join(u'\n')));
        qInfo("%s", qUtf8Printable(log.join(u'\n')));
        QVERIFY(databaseServerRunning(server));
        QVERIFY(databaseServerInitialized(start.files));
        QVERIFY(databaseRootPasswordSet(start.files));
        QVERIFY(portAnswers(QStringLiteral("127.0.0.1"), start.port, 1000));

        // root with the password works; without it, it doesn't
        const QString sql = QStringLiteral("CREATE DATABASE flo_test CHARACTER SET utf8mb4;\nSELECT @@character_set_server, @@bind_address;\n");
        Captured created = runCaptured(databaseSqlCommand(start.files, start.port, QStringLiteral("root"), start.rootPassword, sql));
        QVERIFY2(created.succeeded(), qUtf8Printable(created.err));
        QVERIFY2(created.out.contains(QStringLiteral("utf8mb4")), qUtf8Printable(created.out));
        QVERIFY2(created.out.contains(QStringLiteral("127.0.0.1")), qUtf8Printable(created.out));
        const Captured refused = runCaptured(databaseSqlCommand(start.files, start.port, QStringLiteral("root"), QString(), QStringLiteral("SELECT 1;")));
        QVERIFY(!refused.succeeded());

        // A clean stop
        bool stopped = false;
        stopDatabaseServer(server, start.rootPassword, [&] { stopped = true; });
        QTRY_VERIFY_WITH_TIMEOUT(stopped, 60000);
        QVERIFY(!databaseServerRunning(server));
        QCOMPARE(exits, 1);
        QVERIFY(!exitCrashed);
        QTRY_VERIFY_WITH_TIMEOUT(!portAnswers(QStringLiteral("127.0.0.1"), start.port, 300), 10000);
        const QString errors = databaseErrorLogTail(start.files, 40).join(u'\n');
        QVERIFY2(errors.contains(QStringLiteral(": Shutdown complete")), qUtf8Printable(errors));

        // Again: no set-up, the database is still there
        done = false;
        log.clear();
        destroyNow(server);
        server = startDatabaseServer(start, &owner, onLine, [&](const Result<Done> &r) { result = r; done = true; }, onExit);
        QTRY_VERIFY_WITH_TIMEOUT(done, 120000);
        QVERIFY2(result.ok(), qUtf8Printable(result ? QString() : result.error().detail));
        QVERIFY(!log.join(u'\n').contains(QStringLiteral("mariadb-install-db")));
        const Captured listed = runCaptured(databaseSqlCommand(start.files, start.port, QStringLiteral("root"), start.rootPassword,
                                                               QStringLiteral("SHOW DATABASES LIKE 'flo_test';")));
        QCOMPARE(listed.out.trimmed(), QStringLiteral("flo_test"));

        // The port is now taken: a second server refuses
        done = false;
        QObject *second = startDatabaseServer(start, &owner, {}, [&](const Result<Done> &r) { result = r; done = true; }, {});
        QTRY_VERIFY(done);
        QCOMPARE(result.error().code, QStringLiteral("db_port_busy"));
        destroyNow(second);

        // As when Studio closes: blocking, cleanly
        // The server's own line ("mariadbd: Shutdown complete"), not InnoDB's "Shutdown completed; ..."
        const auto shutdowns = [&start] {
            const QStringList lines = databaseErrorLogTail(start.files, 500);
            return std::count_if(lines.begin(), lines.end(),
                                 [](const QString &line) { return line.endsWith(QStringLiteral(": Shutdown complete")); });
        };
        const auto before = shutdowns();
        stopDatabaseServerNow(server, start.rootPassword);
        QVERIFY(!databaseServerRunning(server));
        QCOMPARE(exits, 1); // not told
        QVERIFY(!portAnswers(QStringLiteral("127.0.0.1"), start.port, 300));
        QCOMPARE(shutdowns(), before + 1);
    }
};

QTEST_GUILESS_MAIN(TestDatabaseServer)
#include "tst_dbserver.moc"
