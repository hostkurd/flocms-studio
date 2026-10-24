// Running the site and following its log (libs/core server.h).

#include "testutil.h"

#include <flostudio/core/output.h>
#include <flostudio/core/server.h>
#include <flostudio/core/tools.h>

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#ifndef Q_OS_WIN
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>
#endif

using namespace FloStudio::Core;

namespace {

void writeFile(const QString &path, const QByteArray &text, QIODevice::OpenMode mode = QIODevice::WriteOnly)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    if (!file.open(mode))
        qFatal("cannot write %s", qUtf8Printable(path));
    file.write(text);
}

} // namespace

class TestServer : public QObject
{
    Q_OBJECT

private slots:
    void arguments()
    {
        QCOMPARE(serveArguments(8000, false),
                 (QStringList{QStringLiteral("serve"), QStringLiteral("--port=8000"), QStringLiteral("--host=127.0.0.1")}));
        QCOMPARE(serveArguments(8080, true).last(), QStringLiteral("--host=0.0.0.0"));
    }

    void urls()
    {
        QCOMPARE(localUrl(QStringLiteral("http://0.0.0.0:8001")), QStringLiteral("http://127.0.0.1:8001"));
        QCOMPARE(localUrl(QStringLiteral("http://127.0.0.1:8000")), QStringLiteral("http://127.0.0.1:8000"));
        QCOMPARE(localUrl(QString()), QString());
        QCOMPARE(urlPort(QStringLiteral("http://127.0.0.1:8123")), 8123);
        QCOMPARE(urlPort(QStringLiteral("nonsense")), 0);
        for (const QString &url : networkUrls(QStringLiteral("http://0.0.0.0:8001"))) {
            QVERIFY(url.startsWith(QStringLiteral("http://")));
            QVERIFY(url.endsWith(QStringLiteral(":8001")));
            QVERIFY(!url.contains(QStringLiteral("127.0.0.1")));
        }
        QVERIFY(networkUrls(QStringLiteral("http://0.0.0.0")).isEmpty()); // no port
    }

    void theFixtureServerUrl()
    {
        // The real first line of `php flo serve`
        const QString first = TestUtil::fixture(QStringLiteral("flo/serve.out")).split(u'\n').first();
        QCOMPARE(localUrl(serveUrl(first)), QStringLiteral("http://127.0.0.1:8123"));
    }

    void tailOfALog()
    {
        const QTemporaryDir dir;
        const QString log = appLogPath(dir.path());
        QVERIFY(log.endsWith(QStringLiteral("storage/logs/app.log")));
        const LogRead missing = readLogTail(log, 10);
        QVERIFY(missing.missing);
        QVERIFY(missing.lines.isEmpty());

        // The real log, CRLF line ends too, and a last line still being written
        const QByteArray real = TestUtil::fixture(QStringLiteral("flo/app.log")).toUtf8();
        writeFile(log, real + "[2026-10-24 09:54:00] info: first\r\n[2026-10-24 09:54:01] info: half");
        const LogRead tail = readLogTail(log, 2);
        QCOMPARE(tail.lines.size(), 2);
        QCOMPARE(tail.lines.last(), QStringLiteral("[2026-10-24 09:54:00] info: first"));
        QCOMPARE(tail.offset, QFileInfo(log).size() - QByteArray("[2026-10-24 09:54:01] info: half").size());
        QVERIFY(readLogTail(log, 1000).lines.size() >= 3);

        // A large log: only the end is read, the first line cut there is left out
        QByteArray big;
        for (int i = 0; i < 20000; ++i)
            big += "[2026-10-24 10:00:00] info: line " + QByteArray::number(i) + "\n";
        writeFile(log, big);
        const LogRead end = readLogTail(log, 3);
        QCOMPARE(end.lines, (QStringList{QStringLiteral("[2026-10-24 10:00:00] info: line 19997"),
                                         QStringLiteral("[2026-10-24 10:00:00] info: line 19998"),
                                         QStringLiteral("[2026-10-24 10:00:00] info: line 19999")}));
        QCOMPARE(end.offset, QFileInfo(log).size());
    }

    void followingALog()
    {
        const QTemporaryDir dir;
        const QString log = appLogPath(dir.path());
        LogRead read = readLogFrom(log, 0);
        QVERIFY(read.missing);
        writeFile(log, "[t] info: one\n[t] info: tw");
        read = readLogFrom(log, read.offset);
        QCOMPARE(read.lines, QStringList{QStringLiteral("[t] info: one")});
        writeFile(log, "o\n", QIODevice::Append);
        read = readLogFrom(log, read.offset);
        QCOMPARE(read.lines, QStringList{QStringLiteral("[t] info: two")});
        QVERIFY(readLogFrom(log, read.offset).lines.isEmpty());

        // Cleared: from the start again
        writeFile(log, "[t] info: new\n");
        read = readLogFrom(log, read.offset);
        QVERIFY(read.restarted);
        QCOMPARE(read.lines, QStringList{QStringLiteral("[t] info: new")});

        // Deleted while followed
        QFile::remove(log);
        read = readLogFrom(log, read.offset);
        QVERIFY(read.missing);
        QVERIFY(read.restarted);
        QCOMPARE(read.offset, 0);

        // A burst larger than one read comes in pieces, no line lost
        QByteArray many;
        for (int i = 0; i < 1000; ++i)
            many += "[t] info: " + QByteArray::number(i) + "\n";
        writeFile(log, many);
        QStringList all;
        qint64 offset = 0;
        for (int guard = 0; guard < 1000; ++guard) {
            const LogRead part = readLogFrom(log, offset, 1024);
            all += part.lines;
            if (part.offset == offset)
                break;
            offset = part.offset;
        }
        QCOMPARE(all.size(), 1000);
        QCOMPARE(all.last(), QStringLiteral("[t] info: 999"));

        // Replaced by a new log that is already longer than the old offset
        writeFile(log, "[t] info: old one\n");
        read = readLogFrom(log, 0);
        const qint64 oldOffset = read.offset;
        const QByteArray oldHead = read.head;
        writeFile(log, "[u] info: new first\n[u] info: new second, longer than the old file\n");
        read = readLogFrom(log, oldOffset, qint64{4} * 1024 * 1024, oldHead);
        QVERIFY(read.restarted);
        QCOMPARE(read.lines.first(), QStringLiteral("[u] info: new first"));
        // The same file, grown: not restarted
        writeFile(log, "[u] info: third\n", QIODevice::Append);
        const LogRead grown = readLogFrom(log, read.offset, qint64{4} * 1024 * 1024, read.head);
        QVERIFY(!grown.restarted);
        QCOMPARE(grown.lines, QStringList{QStringLiteral("[u] info: third")});

        // One line longer than a read is still taken
        writeFile(log, QByteArray(3000, 'x'));
        QCOMPARE(readLogFrom(log, 0, 1024).lines.first().size(), 1024);
    }

    void psLines()
    {
        QCOMPARE(readPsLine(QStringLiteral(" 4321 /usr/bin/php -S 127.0.0.1:8000 -t public router.php")),
                 (PsLine{4321, QStringLiteral("/usr/bin/php -S 127.0.0.1:8000 -t public router.php")}));
        QCOMPARE(readPsLine(QStringLiteral("garbage")).group, 0);
        QVERIFY(isBuiltInServer(QStringLiteral("/opt/homebrew/bin/php -S 0.0.0.0:8001 -t /x/public /x/router.php")));
        QVERIFY(isBuiltInServer(QStringLiteral("php8.4 -S 127.0.0.1:8000 -t public")));
        QVERIFY(isBuiltInServer(QStringLiteral("/usr/bin/php -d x=1 -S [::1]:8000")));
        QVERIFY(!isBuiltInServer(QStringLiteral("/usr/bin/php flo serve --port=8000")));
        QVERIFY(!isBuiltInServer(QStringLiteral("vim -S session.vim")));
        QVERIFY(!isBuiltInServer(QStringLiteral("/usr/bin/phpstorm -S 1:2")));
        // ... and the one of this project
        const QString command = QStringLiteral("/usr/bin/php -S 127.0.0.1:8000 -t /sites/shop/public /x/router.php");
        QVERIFY(isBuiltInServerOf(command, QStringLiteral("/sites/shop")));
        QVERIFY(isBuiltInServerOf(command, QStringLiteral("/sites/shop/")));
        QVERIFY(!isBuiltInServerOf(command, QStringLiteral("/sites/sho")));
        QVERIFY(!isBuiltInServerOf(QStringLiteral("php -S 127.0.0.1:8000"), QStringLiteral("/sites/shop")));
    }

    void leftoverServersAreStopped()
    {
#ifdef Q_OS_WIN
        QSKIP("on Windows the job object ends a server with Studio");
#else
        const QString php = findTool(QStringLiteral("php"));
        if (php.isEmpty())
            QSKIP("no PHP on this computer");
        const QTemporaryDir settings;
        const QTemporaryDir sites;
        QCOMPARE(stopLeftoverServers(settings.path()).size(), 0); // nothing noted
        const QString shop = sites.filePath(QStringLiteral("shop"));
        QDir().mkpath(shop + QStringLiteral("/public"));

        // Starts a program in its own group, as Studio starts a server
        const auto spawn = [](const QList<QByteArray> &argv) {
            const pid_t pid = ::fork();
            if (pid == 0) {
                ::setpgid(0, 0);
                std::vector<char *> args;
                for (const QByteArray &arg : argv)
                    args.push_back(const_cast<char *>(arg.constData()));
                args.push_back(nullptr);
                ::execvp(args.front(), args.data());
                ::_exit(127);
            }
            return pid;
        };
        const QByteArray address = QByteArray("127.0.0.1:") + QByteArray::number(18000 + (::getpid() % 1000));
        // The shop's server, left by a Studio that is gone
        const pid_t server = spawn({php.toLocal8Bit(), "-S", address, "-t", (shop + QStringLiteral("/public")).toLocal8Bit()});
        // A group that is something else now
        const pid_t other = spawn({"sleep", "30"});
        // Some other PHP server, started by the person in a terminal
        const QByteArray address2 = QByteArray("127.0.0.1:") + QByteArray::number(19000 + (::getpid() % 1000));
        const pid_t theirs = spawn({php.toLocal8Bit(), "-S", address2});
        QVERIFY(server > 0 && other > 0 && theirs > 0);
        const int port = QString::fromLatin1(address).section(u':', 1).toInt();
        QTRY_VERIFY_WITH_TIMEOUT(portAnswers(QStringLiteral("127.0.0.1"), port, 200), 10000);

        recordServer(settings.path(), shop, server);
        recordServer(settings.path(), QStringLiteral("/sites/b"), other);
        recordServer(settings.path(), sites.filePath(QStringLiteral("theirs")), theirs);
        recordServer(settings.path(), QStringLiteral("/sites/c"), 999999); // long gone
        recordServer(settings.path(), QStringLiteral("/sites/c"), 0);       // forgotten again
        // Every note names its Studio (this one, which is "gone" for the test: own pid)
        QFile file(serverRecordFile(settings.path()));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonObject record = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QCOMPARE(record.size(), 3);
        QCOMPARE(record.value(shop).toObject().value(QStringLiteral("owner")).toInteger(), QCoreApplication::applicationPid());
        // A note of another Studio that still runs: left alone and kept
        const pid_t otherStudio = spawn({"sleep", "30"});
        record.insert(QStringLiteral("/sites/d"), QJsonObject{{QStringLiteral("group"), static_cast<double>(theirs)},
                                                             {QStringLiteral("owner"), static_cast<double>(otherStudio)}});
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(record).toJson());
        file.close();

        const QList<LeftoverServer> stopped = stopLeftoverServers(settings.path());
        QCOMPARE(stopped.size(), 1);
        QCOMPARE(stopped.first().projectDir, shop);
        int status = 0;
        QCOMPARE(::waitpid(server, &status, 0), server);
        QCOMPARE(::waitpid(other, &status, WNOHANG), 0);  // still running: not a server
        QCOMPARE(::waitpid(theirs, &status, WNOHANG), 0); // still running: not this project's
        // Only the other Studio's note stays
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject left = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(left.keys(), QStringList{QStringLiteral("/sites/d")});
        for (const pid_t pid : {other, theirs, otherStudio}) {
            ::kill(pid, SIGKILL);
            ::waitpid(pid, &status, 0);
        }
#endif
    }
};

QTEST_GUILESS_MAIN(TestServer)
#include "tst_server.moc"
