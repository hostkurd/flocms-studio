// A small program the process tests start (the same on every system, no
// shell needed). The first argument picks what it does:
//
//   lines N          N lines on stdout ("out 1"...) and stderr ("err 1"...)
//   args ...         each further argument on its own line
//   stdin            what it reads on stdin, then "end"
//   env NAME         the variable's value
//   exit N           exits with N
//   sleep MS         sleeps, then prints "woke"
//   split            UTF-8 Kurdish text written one byte at a time
//   redraw           a progress bar redrawn with \r, then text with backspaces
//   child PIDFILE    starts `helper sleep 60000` as its child, writes the
//                    child's process id to PIDFILE, then sleeps

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QThread>

#include <cstdio>
#include <cstdlib>

namespace {

void put(FILE *stream, const QByteArray &text)
{
    if (std::fwrite(text.constData(), 1, static_cast<size_t>(text.size()), stream) != static_cast<size_t>(text.size()))
        std::exit(90);
    if (std::fflush(stream) != 0)
        std::exit(91);
}

} // namespace

int main(int argc, char *argv[])
{
    const QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments().mid(1);
    const QString mode = args.value(0);

    if (mode == u"lines") {
        const int count = args.value(1).toInt();
        for (int i = 1; i <= count; ++i) {
            put(stdout, QByteArray("out ") + QByteArray::number(i) + '\n');
            put(stderr, QByteArray("err ") + QByteArray::number(i) + '\n');
        }
        return 0;
    }
    if (mode == u"args") {
        for (const QString &arg : args.mid(1))
            put(stdout, arg.toUtf8() + '\n');
        return 0;
    }
    if (mode == u"stdin") {
        QFile in;
        if (!in.open(stdin, QIODevice::ReadOnly))
            return 92;
        put(stdout, in.readAll() + "end\n");
        return 0;
    }
    if (mode == u"env") {
        put(stdout, qgetenv(args.value(1).toLocal8Bit().constData()) + '\n');
        return 0;
    }
    if (mode == u"exit")
        return args.value(1).toInt();
    if (mode == u"sleep") {
        QThread::msleep(static_cast<unsigned long>(args.value(1).toLong()));
        put(stdout, "woke\n");
        return 0;
    }
    if (mode == u"split") {
        const QByteArray text = QStringLiteral("کوردی ✓\nسڵاو\n").toUtf8();
        for (const char byte : text) {
            put(stdout, QByteArray(1, byte));
            QThread::msleep(2);
        }
        return 0;
    }
    if (mode == u"redraw") {
        put(stdout, "  0% [>---]\r 50% [==>-]\r100% [====]\n");
        put(stdout, "abc\b\bXY\n");
        put(stdout, "\x1b[32m[OK]\x1b[39m done\n");
        return 0;
    }
    if (mode == u"child") {
        QProcess child;
        child.setProgram(QCoreApplication::applicationFilePath());
        child.setArguments({QStringLiteral("sleep"), QStringLiteral("60000")});
        child.start();
        if (!child.waitForStarted(10000))
            return 93;
        QFile pidFile(args.value(1));
        if (!pidFile.open(QIODevice::WriteOnly))
            return 94;
        pidFile.write(QByteArray::number(child.processId()));
        pidFile.close();
        put(stdout, "started\n");
        child.waitForFinished(-1);
        return 0;
    }
    put(stderr, "unknown mode\n");
    return 2;
}
