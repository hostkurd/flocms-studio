// FloServer and Studio working together (design D14, core floserver.h).

#include "testutil.h"

#include <flostudio/core/commands.h>
#include <flostudio/core/floserver.h>
#include <flostudio/core/tools.h>

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

using namespace FloStudio::Core;

namespace {

// A fixture with @ROOT@ as `root` (an absolute folder on every system)
QByteArray fixtureIn(const QString &name, const QString &root)
{
    QString text = TestUtil::fixture(QStringLiteral("floserver/") + name);
    return text.replace(QStringLiteral("@ROOT@"), QDir::fromNativeSeparators(root) + u'/').toUtf8();
}

void writeFile(const QString &path, const QByteArray &text, bool executable = false)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        qFatal("cannot write %s", qUtf8Printable(path));
    file.write(text);
    file.close();
    if (executable)
        file.setPermissions(file.permissions() | QFile::ExeOwner | QFile::ExeUser);
}

#ifdef Q_OS_WIN
const QString kExe = QStringLiteral(".exe");
#else
const QString kExe;
#endif

// FloServer's data folder with integration.json, its shims and its flo
struct FakeFloServer
{
    QTemporaryDir dir;
    QString data() const { return dir.path() + QStringLiteral("/data"); }
    QString shims() const { return data() + QStringLiteral("/shims"); }
    QString flo() const { return dir.path() + QStringLiteral("/bin/flo") + kExe; }

    FakeFloServer()
    {
        const QString json = QStringLiteral(R"({"schema": 1, "floserver_version": "0.1.0", "cli": "%1", "shims": "%2",
            "runtimes": [{"name": "php", "version": "8.5.12"}, {"name": "composer", "version": "2.9.1"}],
            "databases": [{"series": "11.4", "version": "11.4.13", "port": 3307}]})")
                                 .arg(QDir::fromNativeSeparators(flo()), QDir::fromNativeSeparators(shims()));
        writeFile(data() + QStringLiteral("/integration.json"), json.toUtf8());
        writeFile(shims() + QStringLiteral("/php") + kExe, "#!/bin/sh\n", true);
        writeFile(shims() + QStringLiteral("/composer") + kExe, "#!/bin/sh\n", true);
        writeFile(flo(), "#!/bin/sh\n", true);
        qputenv("FLOSERVER_DATA_DIR", QDir::toNativeSeparators(data()).toUtf8());
    }
    ~FakeFloServer() { qunsetenv("FLOSERVER_DATA_DIR"); }
    FakeFloServer(const FakeFloServer &) = delete;
    FakeFloServer &operator=(const FakeFloServer &) = delete;
};

} // namespace

class TestFloServer : public QObject
{
    Q_OBJECT

private slots:
    void theDataFolder()
    {
        const QTemporaryDir dir;
        qputenv("FLOSERVER_DATA_DIR", dir.path().toUtf8());
        QCOMPARE(floServerDataFolder(), QDir::toNativeSeparators(dir.path()));
        // Relative: not used (it would mean wherever Studio runs)
        qputenv("FLOSERVER_DATA_DIR", "floserver");
        QVERIFY(floServerDataFolder() != QStringLiteral("floserver"));
        qunsetenv("FLOSERVER_DATA_DIR");
#if defined(Q_OS_WIN)
        QVERIFY(floServerDataFolder().endsWith(QStringLiteral("\\FloServer")));
#elif defined(Q_OS_MACOS)
        QVERIFY(floServerDataFolder().endsWith(QStringLiteral("/Library/Application Support/FloServer")));
#else
        const QByteArray xdg = qgetenv("XDG_DATA_HOME");
        qputenv("XDG_DATA_HOME", dir.path().toUtf8());
        QCOMPARE(floServerDataFolder(), dir.path() + QStringLiteral("/floserver"));
        qunsetenv("XDG_DATA_HOME");
        QCOMPARE(floServerDataFolder(), QDir::homePath() + QStringLiteral("/.local/share/floserver"));
        if (!xdg.isEmpty())
            qputenv("XDG_DATA_HOME", xdg);
#endif
    }

    void readsIntegrationJson()
    {
        const QTemporaryDir root;
        const auto info = parseFloServerIntegration(fixtureIn(QStringLiteral("integration.json"), root.path()));
        QVERIFY(info);
        QCOMPARE(info->version, QStringLiteral("0.1.0"));
        QCOMPARE(info->cli, QDir::toNativeSeparators(root.path() + QStringLiteral("/opt/floserver/bin/flo")));
        QCOMPARE(info->shims, QDir::toNativeSeparators(root.path() + QStringLiteral("/home/ali/.local/share/floserver/shims")));
        // Entries that aren't a runtime or a database with a port are left out
        QCOMPARE(info->runtimes.size(), 5);
        QCOMPARE(info->databases, (QList<FloServerDatabase>{{QStringLiteral("11.4"), QStringLiteral("11.4.13"), 3306}}));
        QCOMPARE(floServerVersions(*info, QStringLiteral("php")), (QStringList{QStringLiteral("8.5.12"), QStringLiteral("8.3.27")}));
        QCOMPARE(floServerVersions(*info, QStringLiteral("composer")), QStringList{QStringLiteral("2.9.1")});
        QVERIFY(floServerVersions(*info, QStringLiteral("node")).isEmpty());
    }

    void anythingElseIsNoFloServer()
    {
        const QTemporaryDir root;
        QVERIFY(!parseFloServerIntegration(fixtureIn(QStringLiteral("garbage.json"), root.path())));
        QVERIFY(!parseFloServerIntegration(fixtureIn(QStringLiteral("schema2.json"), root.path())));
        QVERIFY(!parseFloServerIntegration({}));
        QVERIFY(!parseFloServerIntegration("[]"));
        QVERIFY(!parseFloServerIntegration(R"({"schema": "1", "shims": "/x"})"));
        QVERIFY(!parseFloServerIntegration(R"({"schema": 1})"));             // no shims
        QVERIFY(!parseFloServerIntegration(R"({"schema": 1, "shims": "shims"})")); // relative
        // No flo: the shims still work
        const QString shims = QDir::fromNativeSeparators(root.path());
        const auto noCli = parseFloServerIntegration(QStringLiteral(R"({"schema": 1, "cli": "", "shims": "%1"})").arg(shims).toUtf8());
        QVERIFY(noCli);
        QVERIFY(noCli->cli.isEmpty());

        // From the data folder: missing, unreadable or garbage
        QVERIFY(!readFloServerIntegration(root.path()));
        QVERIFY(!readFloServerIntegration({}));
        writeFile(root.path() + QStringLiteral("/integration.json"), fixtureIn(QStringLiteral("garbage.json"), root.path()));
        QVERIFY(!readFloServerIntegration(root.path()));
        writeFile(root.path() + QStringLiteral("/integration.json"), fixtureIn(QStringLiteral("integration.json"), root.path()));
        QVERIFY(readFloServerIntegration(root.path()));
#ifdef Q_OS_UNIX
        if (::geteuid() != 0) { // root reads anything
            QFile(root.path() + QStringLiteral("/integration.json")).setPermissions(QFile::WriteOwner);
            QVERIFY(!readFloServerIntegration(root.path()));
        }
#endif
    }

    void usedOnlyWhenTurnedOnAndFound()
    {
        Settings settings;
        settings.phpPath = QStringLiteral("/nowhere/php");
        settings.composerPath = QStringLiteral("/nowhere/composer");
        {
            const FakeFloServer floServer;
            QVERIFY(!floServerInUse(settings)); // off
            QCOMPARE(phpProgram(settings).error().code, QStringLiteral("php_not_found"));
            settings.useFloServer = true;
            QVERIFY(floServerInUse(settings));
            // Its shims, whatever phpPath and composerPath say (they are kept)
            QCOMPARE(phpProgram(settings).value(), QDir::toNativeSeparators(floServer.shims() + QStringLiteral("/php") + kExe));
            const auto composer = composerLaunch(settings);
            QVERIFY(composer);
            QCOMPARE(composer->program, QDir::toNativeSeparators(floServer.shims() + QStringLiteral("/composer") + kExe));
            QVERIFY(composer->arguments.isEmpty());
            QVERIFY(composer->setupBinary.isEmpty()); // setup finds it first on the PATH
            QVERIFY(!composer->batchFile);
            // The shims go first on the PATH of commands
            QVERIFY(commandSearchPath(settings).startsWith(QDir::toNativeSeparators(floServer.shims()) + QDir::listSeparator()));
            QCOMPARE(commandSearchPath(settings).count(QDir::toNativeSeparators(floServer.shims())), 1);

            // A shim that is gone: not found, named by where it should be
            QFile::remove(floServer.shims() + QStringLiteral("/php") + kExe);
            QCOMPARE(phpProgram(settings).error().code, QStringLiteral("php_not_found"));
            QVERIFY(phpProgram(settings).error().detail.contains(QStringLiteral("shims")));
        }
        // FloServer gone (FLOSERVER_DATA_DIR unset, nothing at the usual
        // place in a test): the settings of before
        qputenv("FLOSERVER_DATA_DIR", QTemporaryDir().path().toUtf8());
        QVERIFY(!floServerInUse(settings));
        QCOMPARE(phpProgram(settings).error().detail, QStringLiteral("/nowhere/php"));
        QCOMPARE(commandSearchPath(settings), toolSearchPath().join(QDir::listSeparator()));
        qunsetenv("FLOSERVER_DATA_DIR");
    }

    void commandsGetTheShims()
    {
        const FakeFloServer floServer;
        const QTemporaryDir project;
        writeFile(project.path() + QStringLiteral("/composer.json"), R"({"name": "hostkurd/flocms"})");
        writeFile(project.path() + QStringLiteral("/flo"), "<?php\n");
        Settings settings;
        settings.useFloServer = true;
        CommandRequest request;
        request.projectDir = project.path();
        request.arguments = {QStringLiteral("migrate")};
        const auto command = prepareCommand(settings, request);
        QVERIFY(command);
        QCOMPARE(command->program, QDir::toNativeSeparators(floServer.shims() + QStringLiteral("/php") + kExe));
        QVERIFY(command->environment.value(QStringLiteral("PATH")).startsWith(QDir::toNativeSeparators(floServer.shims())));
        QVERIFY(!command->environment.contains(QStringLiteral("COMPOSER_BINARY")));
        request.program = Program::Composer;
        request.arguments = {QStringLiteral("install")};
        const auto composer = prepareCommand(settings, request);
        QVERIFY(composer);
        QCOMPARE(composer->program, QDir::toNativeSeparators(floServer.shims() + QStringLiteral("/composer") + kExe));
        QCOMPARE(composer->arguments.constFirst(), QStringLiteral("install"));
    }
};

QTEST_GUILESS_MAIN(TestFloServer)
#include "tst_floserver.moc"
