#include "testutil.h"

#include <flostudio/core/process.h>
#include <flostudio/core/tools.h>

#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

using namespace FloStudio::Core;

namespace {

Command helper(const QStringList &arguments)
{
    Command command;
    command.program = TestUtil::helperProgram();
    command.arguments = arguments;
    return command;
}

// Collects a streaming run.
struct Run
{
    QStringList out;
    QStringList err;
    int exits = 0;
    int code = -2;
    bool crashed = false;
};

QProcess *stream(const Command &command, QObject *owner, Run &run)
{
    return startStreaming(
        command, owner,
        [&run](const OutputLine &line) { (line.error ? run.err : run.out) << line.text; },
        [&run](int code, bool crashed) {
            ++run.exits;
            run.code = code;
            run.crashed = crashed;
        });
}

} // namespace

class TestProcess : public QObject
{
    Q_OBJECT

private slots:
    void cleansLines()
    {
        QCOMPARE(cleanLine(QStringLiteral("\x1b[32m[OK]\x1b[39m done")), QStringLiteral("[OK] done"));
        QCOMPARE(cleanLine(QStringLiteral("\x1b]8;;http://x\x07link\x1b]8;;\x07")), QStringLiteral("link"));
        QCOMPARE(cleanLine(QStringLiteral("abc\b\bXY")), QStringLiteral("aXY"));
        QCOMPARE(cleanLine(QStringLiteral("\b\bstart")), QStringLiteral("start"));
        QCOMPARE(cleanLine(QStringLiteral("a\tb\x01\x7f")), QStringLiteral("a\tb"));
        QCOMPARE(cleanLine(QStringLiteral("کوردی ✓ ‏مرحبا")), QStringLiteral("کوردی ✓ ‏مرحبا"));
        QCOMPARE(cleanLine(QStringLiteral("x😀\b")), QStringLiteral("x")); // a whole emoji (two UTF-16 units)
        QCOMPARE(cleanLine(QStringLiteral("cut \x1b[")), QStringLiteral("cut "));
    }

    void takesFinishedLines()
    {
        QString pending;
        QCOMPARE(takeLines(pending, QStringLiteral("one\ntw"), false), QStringList{QStringLiteral("one")});
        QCOMPARE(pending, QStringLiteral("tw"));
        QCOMPARE(takeLines(pending, QStringLiteral("o\r\nthree"), false), QStringList{QStringLiteral("two")});
        QCOMPARE(takeLines(pending, QString(), true), QStringList{QStringLiteral("three")});
        QVERIFY(pending.isEmpty());
        QCOMPARE(takeLines(pending, QStringLiteral("\n\n"), false), (QStringList{QString(), QString()}));
        // A redrawn line keeps what a terminal shows last
        QCOMPARE(takeLines(pending, QStringLiteral("  0% [>--]\r 50% [=>-]\r100% [==]\n"), false),
                 QStringList{QStringLiteral("100% [==]")});
        QCOMPARE(takeLines(pending, QStringLiteral("working\r   \r\n"), false), QStringList());
    }

    void showsCommandLines()
    {
        QCOMPARE(shownCommandLine(QStringLiteral("php flo"), {QStringLiteral("a"), QStringLiteral("b c"), QStringLiteral("say \"hi\"")}),
                 QStringLiteral("php flo a \"b c\" \"say \\\"hi\\\"\""));
    }

    void capturesOutputAndExitCode()
    {
        const Captured out = runCaptured(helper({QStringLiteral("lines"), QStringLiteral("3")}));
        QVERIFY(out.started);
        QVERIFY(out.succeeded());
        QCOMPARE(QString(out.out).remove(u'\r'), QStringLiteral("out 1\nout 2\nout 3\n"));
        QCOMPARE(QString(out.err).remove(u'\r'), QStringLiteral("err 1\nerr 2\nerr 3\n"));

        const Captured failed = runCaptured(helper({QStringLiteral("exit"), QStringLiteral("7")}));
        QCOMPARE(failed.exitCode, 7);
        QVERIFY(!failed.succeeded());
    }

    void passesArgumentsExactly()
    {
        const QStringList args{QStringLiteral("Blog Post"), QStringLiteral("--name=\"x\""), QStringLiteral("مرحبا بك"),
                               QStringLiteral("a\\b\\"), QStringLiteral("100%"), QStringLiteral("x&y"), QStringLiteral("^1.3")};
        const Captured out = runCaptured(helper(QStringList{QStringLiteral("args")} + args));
        QCOMPARE(QString(out.out).remove(u'\r').split(u'\n', Qt::SkipEmptyParts), args);
    }

    void writesInputAndEnvironment()
    {
        Command command = helper({QStringLiteral("stdin")});
        command.input = "Studio-test-123\n";
        QCOMPARE(QString(runCaptured(command).out).remove(u'\r'), QStringLiteral("Studio-test-123\nend\n"));

        // Without input the program reads an empty input (it never waits)
        QCOMPARE(QString(runCaptured(helper({QStringLiteral("stdin")})).out).remove(u'\r'), QStringLiteral("end\n"));

        command = helper({QStringLiteral("env"), QStringLiteral("FLOSTUDIO_TEST")});
        command.environment.insert(QStringLiteral("FLOSTUDIO_TEST"), QStringLiteral("kurdî"));
        QCOMPARE(runCaptured(command).out.trimmed(), QStringLiteral("kurdî"));

        // PATH is Studio's search path
        const Captured path = runCaptured(helper({QStringLiteral("env"), QStringLiteral("PATH")}));
        QCOMPARE(path.out.trimmed(), toolSearchPath().join(QDir::listSeparator()));
    }

    void timesOut()
    {
        QElapsedTimer timer;
        timer.start();
        const Captured out = runCaptured(helper({QStringLiteral("sleep"), QStringLiteral("20000")}), 300);
        QVERIFY(out.timedOut);
        QVERIFY(!out.succeeded());
        QVERIFY(timer.elapsed() < 10000);
    }

    void reportsAProgramThatCannotStart()
    {
        Command command;
        command.program = QDir::temp().filePath(QStringLiteral("no-such-program-flostudio"));
        const Captured out = runCaptured(command);
        QVERIFY(!out.started);
        QVERIFY(!out.err.isEmpty());

        QObject owner;
        Run run;
        stream(command, &owner, run);
        QTRY_COMPARE(run.exits, 1);
        QCOMPARE(run.code, -1);
        QCOMPARE(run.err.size(), 1);
    }

    void streamsLinesThenTheExit()
    {
        QObject owner;
        Run run;
        stream(helper({QStringLiteral("lines"), QStringLiteral("50")}), &owner, run);
        QTRY_COMPARE_WITH_TIMEOUT(run.exits, 1, 20000);
        QCOMPARE(run.code, 0);
        QCOMPARE(run.out.size(), 50);
        QCOMPARE(run.out.first(), QStringLiteral("out 1"));
        QCOMPARE(run.out.last(), QStringLiteral("out 50"));
        QCOMPARE(run.err.last(), QStringLiteral("err 50"));
    }

    void decodesCharactersSplitAcrossReads()
    {
        QObject owner;
        Run run;
        stream(helper({QStringLiteral("split")}), &owner, run);
        QTRY_COMPARE_WITH_TIMEOUT(run.exits, 1, 20000);
        QCOMPARE(run.out, (QStringList{QStringLiteral("کوردی ✓"), QStringLiteral("سڵاو")}));
    }

    void showsRedrawsAsATerminalWould()
    {
        QObject owner;
        Run run;
        stream(helper({QStringLiteral("redraw")}), &owner, run);
        QTRY_COMPARE_WITH_TIMEOUT(run.exits, 1, 20000);
        QCOMPARE(run.out, (QStringList{QStringLiteral("100% [====]"), QStringLiteral("aXY"), QStringLiteral("[OK] done")}));
    }

    void stopsTheProgramAndItsChildren()
    {
        const QTemporaryDir dir;
        const QString pidFile = dir.filePath(QStringLiteral("child.pid"));
        QObject owner;
        Run run;
        QProcess *process = stream(helper({QStringLiteral("child"), pidFile}), &owner, run);
        QTRY_COMPARE_WITH_TIMEOUT(run.out, QStringList{QStringLiteral("started")}, 20000);
        QFile file(pidFile);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const qint64 child = file.readAll().toLongLong();
        QVERIFY(TestUtil::processAlive(child));

        stopProcess(process, 2000);
        QTRY_COMPARE_WITH_TIMEOUT(run.exits, 1, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!TestUtil::processAlive(child), 10000);
        stopProcess(process); // already stopped: nothing happens
    }

    void stopsAtTimeoutWithChildren()
    {
        const QTemporaryDir dir;
        const QString pidFile = dir.filePath(QStringLiteral("child.pid"));
        const Captured out = runCaptured(helper({QStringLiteral("child"), pidFile}), 3000);
        QVERIFY(out.timedOut);
        QFile file(pidFile);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QTRY_VERIFY_WITH_TIMEOUT(!TestUtil::processAlive(file.readAll().toLongLong()), 10000);
    }
};

QTEST_GUILESS_MAIN(TestProcess)
#include "tst_process.moc"
