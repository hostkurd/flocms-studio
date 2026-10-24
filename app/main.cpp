// FloCMS Studio. Stage 1: the build check window; see README.md.

#include <flostudio/core/buildinfo.h>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStringList>
#include <QTimer>

#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QStringList g_warnings;
QtMessageHandler g_previousHandler = nullptr;

// In --smoke-test every warning counts as a failure: a missing QML module
// or plugin in a static build shows up as a warning first.
void countingHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (type != QtDebugMsg && type != QtInfoMsg)
        g_warnings << (context.category && qstrcmp(context.category, "default") != 0
                           ? QStringLiteral("%1: %2").arg(QString::fromLatin1(context.category), message)
                           : message);
    if (g_previousHandler)
        g_previousHandler(type, context, message);
}

#ifdef Q_OS_WIN
// Studio.exe is a GUI program, so Windows gives it no console. When it is
// started from Command Prompt with options, print to that console; when the
// output is already redirected to a file or pipe, leave it alone.
void attachParentConsole()
{
    const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    const bool redirected = out != nullptr && out != INVALID_HANDLE_VALUE && GetFileType(out) != FILE_TYPE_UNKNOWN;
    if (!redirected && AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *ignored = nullptr;
        freopen_s(&ignored, "CONOUT$", "w", stdout);
        freopen_s(&ignored, "CONOUT$", "w", stderr);
    }
}
#endif

} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (argc > 1)
        attachParentConsole();
#endif

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("FloCMS Studio"));
    QGuiApplication::setOrganizationName(QStringLiteral("HostKurd"));
    QGuiApplication::setApplicationVersion(FloStudio::Core::currentBuildInfo().version);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("FloCMS Studio"));
    parser.addHelpOption();
    const QCommandLineOption versionOpt(QStringList{QStringLiteral("v"), QStringLiteral("version")},
                                        QStringLiteral("Print the version and build details."));
    const QCommandLineOption smokeOpt(QStringLiteral("smoke-test"),
                                      QStringLiteral("Open the window, draw it once and quit; "
                                                     "fails on any warning."));
    const QCommandLineOption shotOpt(QStringLiteral("screenshot"),
                                     QStringLiteral("Save a PNG of the window and quit."),
                                     QStringLiteral("file"));
    parser.addOptions({versionOpt, smokeOpt, shotOpt});
    parser.process(app);

    if (parser.isSet(versionOpt)) {
        const QByteArray text = FloStudio::Core::toText(FloStudio::Core::currentBuildInfo()).toUtf8() + '\n';
        return std::fwrite(text.constData(), 1, size_t(text.size()), stdout) == size_t(text.size()) ? 0 : 1;
    }

    const bool smoke = parser.isSet(smokeOpt);
    if (smoke)
        g_previousHandler = qInstallMessageHandler(countingHandler);

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/FloStudio/App/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical("Could not load the main window.");
        return 1;
    }

    if (smoke || parser.isSet(shotOpt)) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const QString file = parser.value(shotOpt);
        // Give the scene graph time to lay out and draw, then grab a frame:
        // grabbing renders the scene, so a broken renderer fails here too.
        QTimer::singleShot(1500, &app, [window, file, smoke] {
            const QImage frame = window ? window->grabWindow() : QImage();
            bool ok = !frame.isNull();
            if (ok && !file.isEmpty())
                ok = frame.save(file);
            if (smoke) {
                for (const QString &warning : std::as_const(g_warnings))
                    std::printf("warning: %s\n", qUtf8Printable(warning));
                const int written = std::printf("smoke test: %s, %d warning(s)\n",
                                                ok ? "window drawn" : "no frame", int(g_warnings.size()));
                ok = ok && g_warnings.isEmpty() && written > 0;
            }
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }

    return QGuiApplication::exec();
}
