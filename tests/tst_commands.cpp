#include "testutil.h"

#include <flostudio/core/commands.h>
#include <flostudio/core/tools.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace FloStudio::Core;

namespace {

QString writeFile(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        qFatal("cannot write %s", qUtf8Printable(path));
    file.write(text);
    return QDir::toNativeSeparators(path);
}

// A FloCMS project whose `flo` launcher is a PHP script printing what it
// received: its arguments and COMPOSER_BINARY, as JSON.
QString makeEchoProject(const QString &dir)
{
    writeFile(dir + QStringLiteral("/composer.json"), R"({"name":"hostkurd/flocms"})");
    return writeFile(dir + QStringLiteral("/flo"),
                     "<?php echo json_encode(['args' => array_slice($argv, 1), 'composer' => getenv('COMPOSER_BINARY'),"
                     " 'cwd' => getcwd(), 'nocolor' => getenv('NO_COLOR')]);\n");
}

QJsonObject runJson(const Command &command)
{
    const Captured out = runCaptured(command, 30000);
    if (!out.succeeded())
        qWarning("failed: %s %s", qUtf8Printable(out.out), qUtf8Printable(out.err));
    return QJsonDocument::fromJson(out.out.toUtf8()).object();
}

QStringList strings(const QJsonValue &value)
{
    QStringList list;
    for (const auto &item : value.toArray())
        list << item.toString();
    return list;
}

} // namespace

class TestCommands : public QObject
{
    Q_OBJECT

private slots:
    void onlyTheScreensCommands()
    {
        const auto flo = [](const QStringList &args) { return isAllowed(Program::Flo, args); };
        const auto composer = [](const QStringList &args) { return isAllowed(Program::Composer, args); };
        QVERIFY(flo({QStringLiteral("doctor"), QStringLiteral("--format=json")}));
        QVERIFY(flo({QStringLiteral("migrate"), QStringLiteral("--force")}));
        QVERIFY(flo({QStringLiteral("make:admin-resource"), QStringLiteral("Post")}));
        QVERIFY(flo({QStringLiteral("serve"), QStringLiteral("--port=8000")}));
        QVERIFY(flo({QStringLiteral("user:create"), QStringLiteral("--password-stdin")}));
        QVERIFY(!flo({QStringLiteral("make:")}));
        QVERIFY(!flo({QStringLiteral("user:password"), QStringLiteral("admin")}));
        QVERIFY(!flo({QStringLiteral("down")}));
        QVERIFY(!flo({}));
        QVERIFY(!flo({QStringLiteral("doctor"), QStringLiteral("a\nb")}));
        QVERIFY(composer({QStringLiteral("create-project"), QStringLiteral("hostkurd/flocms"), QStringLiteral("shop")}));
        QVERIFY(composer({QStringLiteral("require"), QStringLiteral("hostkurd/flocms-api:^1.3")}));
        QVERIFY(!composer({QStringLiteral("remove"), QStringLiteral("hostkurd/flocms-api")}));
        QVERIFY(!composer({QStringLiteral("run-script"), QStringLiteral("x")}));
        QVERIFY(!composer({QStringLiteral("global"), QStringLiteral("require"), QStringLiteral("x")}));
        QVERIFY(!composer({QStringLiteral("install"), QStringLiteral("-d"), QStringLiteral("/elsewhere")}));
        QVERIFY(!composer({QStringLiteral("install"), QStringLiteral("-d/elsewhere")}));
        QVERIFY(!composer({QStringLiteral("install"), QStringLiteral("--working-dir=/elsewhere")}));
        QVERIFY(composer({QStringLiteral("show"), QStringLiteral("--direct")}));
    }

    void showsTheCommandAsTyped()
    {
        QCOMPARE(shownCommand(Program::Flo, {QStringLiteral("migrate"), QStringLiteral("--force")}),
                 QStringLiteral("php flo migrate --force"));
        QCOMPARE(shownCommand(Program::Composer, {QStringLiteral("create-project"), QStringLiteral("hostkurd/flocms"),
                                                  QStringLiteral("My Site")}),
                 QStringLiteral("composer create-project hostkurd/flocms \"My Site\""));
        QCOMPARE(shownCommand(Program::Flo, {QStringLiteral("user:create"), QStringLiteral("--name=Ali Kurd"), QString()}),
                 QStringLiteral("php flo user:create \"--name=Ali Kurd\" \"\""));
    }

    void refusals()
    {
        const QTemporaryDir dir;
        Settings settings;
        const auto prepare = [&settings](Program program, const QString &where, const QStringList &args) {
            return prepareCommand(settings, CommandRequest{program, where, args, {}, {}});
        };
        QCOMPARE(prepare(Program::Flo, dir.path(), {QStringLiteral("tinker")}).error().code, QStringLiteral("not_allowed"));
        QCOMPARE(prepare(Program::Flo, dir.filePath(QStringLiteral("gone")), {QStringLiteral("doctor")}).error().code,
                 QStringLiteral("project_missing"));
        QCOMPARE(prepare(Program::Flo, dir.path(), {QStringLiteral("doctor")}).error().code, QStringLiteral("not_flocms"));
        writeFile(dir.filePath(QStringLiteral("composer.json")), R"({"name":"hostkurd/flocms"})");
        QCOMPARE(prepare(Program::Flo, dir.path(), {QStringLiteral("doctor")}).error().code, QStringLiteral("no_launcher"));
        writeFile(dir.filePath(QStringLiteral("flo")), "<?php");
        settings.phpPath = dir.filePath(QStringLiteral("no/php"));
        QCOMPARE(prepare(Program::Flo, dir.path(), {QStringLiteral("doctor")}).error().code, QStringLiteral("php_not_found"));
        settings.composerPath = dir.filePath(QStringLiteral("no/composer"));
        QCOMPARE(prepare(Program::Composer, dir.path(), {QStringLiteral("show")}).error().code,
                 QStringLiteral("composer_not_found"));

        // Composer as a lone .bat: cmd.exe would read ^ & % itself
        settings.composerPath = writeFile(dir.filePath(QStringLiteral("bin/composer.bat")), "@echo off");
        QCOMPARE(prepare(Program::Composer, dir.path(), {QStringLiteral("require"), QStringLiteral("hostkurd/flocms-api:^1.3")})
                     .error().code,
                 QStringLiteral("unsafe_argument"));
        QVERIFY(prepare(Program::Composer, dir.path(), {QStringLiteral("require"), QStringLiteral("hostkurd/flocms-api")}).ok());
    }

    void floCommandsShape()
    {
        const QTemporaryDir dir;
        const QString launcher = makeEchoProject(dir.path());
        Settings settings;
        settings.phpPath = writeFile(dir.filePath(QStringLiteral("bin/php")), "");
        settings.composerPath = writeFile(dir.filePath(QStringLiteral("bin/composer.phar")), "<?php");
        const auto command = prepareCommand(settings, CommandRequest{Program::Flo, dir.path(),
                                                                     {QStringLiteral("migrate"), QStringLiteral("--force")},
                                                                     QByteArray("secret\n"), {}});
        QVERIFY(command.ok());
        QCOMPARE(command->program, settings.phpPath);
        QCOMPARE(command->arguments, (QStringList{launcher, QStringLiteral("migrate"), QStringLiteral("--force"),
                                                  QStringLiteral("--no-interaction"), QStringLiteral("--no-ansi")}));
        QCOMPARE(command->workingDir, dir.path());
        QCOMPARE(command->shown, QStringLiteral("php flo migrate --force"));
        QCOMPARE(command->environment.value(QStringLiteral("COMPOSER_BINARY")), settings.composerPath);
        QCOMPARE(command->input, QByteArray("secret\n"));
        QVERIFY(!command->shown.contains(QStringLiteral("secret")));
    }

    void composerCommandsShape()
    {
        const QTemporaryDir dir;
        Settings settings;
        settings.phpPath = writeFile(dir.filePath(QStringLiteral("bin/php")), "");
        settings.composerPath = writeFile(dir.filePath(QStringLiteral("bin/composer.phar")), "<?php");
        const auto require = prepareCommand(
            settings, CommandRequest{Program::Composer, dir.path(), {QStringLiteral("require"), QStringLiteral("hostkurd/flocms-api:^1.3")}, {}, {}});
        QVERIFY(require.ok());
        QCOMPARE(require->program, settings.phpPath);
        QCOMPARE(require->arguments,
                 (QStringList{settings.composerPath, QStringLiteral("require"), QStringLiteral("hostkurd/flocms-api:^1.3"),
                              QStringLiteral("--no-interaction"), QStringLiteral("--no-ansi"), QStringLiteral("--no-progress")}));
        QCOMPARE(require->environment.value(QStringLiteral("COMPOSER_PROCESS_TIMEOUT")), QStringLiteral("0"));
        const auto show = prepareCommand(settings, CommandRequest{Program::Composer, dir.path(), {QStringLiteral("show")}, {}, {}});
        QVERIFY(!show->arguments.contains(QStringLiteral("--no-progress")));
    }

    void runsThroughRealPhp()
    {
        if (findTool(QStringLiteral("php")).isEmpty())
            QSKIP("no PHP on this computer");
        const QTemporaryDir dir;
        const QString project = dir.filePath(QStringLiteral("سایتی من")); // a Kurdish folder name with a space
        makeEchoProject(project);
        Settings settings;
        settings.composerPath = writeFile(dir.filePath(QStringLiteral("tools/composer.phar")), "<?php");
        const QStringList args{QStringLiteral("make:controller"), QStringLiteral("Blog Post"), QStringLiteral("--name=\"x\""),
                               QStringLiteral("مرحبا"), QStringLiteral("a\\b\\"), QStringLiteral("100%"), QString()};
        const auto command = prepareCommand(settings, CommandRequest{Program::Flo, project, args, {}, {}});
        QVERIFY(command.ok());
        const QJsonObject got = runJson(command.value());
        QCOMPARE(strings(got.value(QStringLiteral("args"))),
                 (args + QStringList{QStringLiteral("--no-interaction"), QStringLiteral("--no-ansi")}));
        QCOMPARE(got.value(QStringLiteral("composer")).toString(), settings.composerPath);
        QCOMPARE(got.value(QStringLiteral("nocolor")).toString(), QStringLiteral("1"));
        QCOMPARE(QFileInfo(got.value(QStringLiteral("cwd")).toString()).canonicalFilePath(), QFileInfo(project).canonicalFilePath());
    }

    void composerThroughRealPhp()
    {
        if (findTool(QStringLiteral("php")).isEmpty())
            QSKIP("no PHP on this computer");
        const QTemporaryDir dir;
        Settings settings;
        settings.composerPath = writeFile(dir.filePath(QStringLiteral("composer.phar")),
                                          "<?php echo json_encode(['args' => array_slice($argv, 1)]);\n");
        const auto command = prepareCommand(
            settings, CommandRequest{Program::Composer, dir.path(), {QStringLiteral("show"), QStringLiteral("hostkurd/*")}, {}, {}});
        QVERIFY(command.ok());
        QCOMPARE(strings(runJson(command.value()).value(QStringLiteral("args"))),
                 (QStringList{QStringLiteral("show"), QStringLiteral("hostkurd/*"), QStringLiteral("--no-interaction"),
                              QStringLiteral("--no-ansi")}));
    }
};

QTEST_GUILESS_MAIN(TestCommands)
#include "tst_commands.moc"
