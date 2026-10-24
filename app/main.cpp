// FloCMS Studio. Stage 1: the build check window; see README.md.

#include <flostudio/core/buildinfo.h>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QStringList>
#include <QTimer>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

// --smoke-test: every warning counts as a failure; a missing QML module or
// plugin shows up as a warning first. Qt can warn from its render thread,
// so the list is guarded.
QMutex &warningsMutex()
{
    static QMutex mutex; // created on first use, not before main()
    return mutex;
}
QStringList g_warnings;
QtMessageHandler g_previousHandler = nullptr;

void countingHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (type != QtDebugMsg && type != QtInfoMsg) {
        const QString text = context.category && qstrcmp(context.category, "default") != 0
            ? QStringLiteral("%1: %2").arg(QString::fromLatin1(context.category), message)
            : message;
        const QMutexLocker lock(&warningsMutex());
        g_warnings << text;
    }
    if (g_previousHandler)
        g_previousHandler(type, context, message);
}

QStringList takeWarnings()
{
    const QMutexLocker lock(&warningsMutex());
    return g_warnings;
}

// Installed before QGuiApplication exists (so its own warnings count too)
// and removed before main() returns (the list dies after main).
class SmokeHandler
{
public:
    explicit SmokeHandler(bool on)
        : m_on(on)
    {
        if (m_on)
            g_previousHandler = qInstallMessageHandler(countingHandler);
    }
    ~SmokeHandler()
    {
        if (m_on)
            qInstallMessageHandler(g_previousHandler);
    }
    SmokeHandler(const SmokeHandler &) = delete;
    SmokeHandler &operator=(const SmokeHandler &) = delete;

private:
    bool m_on;
};

bool hasArgument(int argc, char *argv[], const char *name)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0)
            return true;
    }
    return false;
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

// The window's background colour in qml/Main.qml.
constexpr QRgb kBackground = 0xFFF4F7F9;

} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (argc > 1)
        attachParentConsole();
#endif

    const SmokeHandler smokeHandler(hasArgument(argc, argv, "--smoke-test"));

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
                                      QStringLiteral("Open the window, wait until it is on screen, "
                                                     "check one frame and quit; fails on any warning."));
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

    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/FloStudio/App/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical("Could not load the main window.");
        return 1;
    }

    if (smoke || parser.isSet(shotOpt)) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        if (!window) {
            qCritical("The main window is not a Qt Quick window.");
            return 1;
        }
        // Frames that really reached the screen (signalled from the render
        // thread). Waiting for them shows the window was shown and drawn.
        auto swapped = std::make_shared<std::atomic_int>(0);
        QObject::connect(window, &QQuickWindow::frameSwapped, window,
                         [swapped] { ++*swapped; }, Qt::DirectConnection);

        const QString file = parser.value(shotOpt);
        auto *poll = new QTimer(&app); // owned by app
        poll->setInterval(100);
        auto waited = std::make_shared<int>(0);
        QObject::connect(poll, &QTimer::timeout, &app, [=] {
            *waited += poll->interval();
            if (swapped->load() < 1 && *waited < 15000)
                return; // keep waiting, up to 15 seconds
            poll->stop();

            const QImage frame = window->grabWindow();
            const bool onScreen = swapped->load() >= 1;
            const bool background = !frame.isNull() && frame.pixel(4, 4) == kBackground;
            bool ok = onScreen && background;
            if (ok && !file.isEmpty())
                ok = frame.save(file);
            if (smoke) {
                const QStringList warnings = takeWarnings();
                for (const QString &warning : warnings)
                    std::printf("warning: %s\n", qUtf8Printable(warning));
                const int written = std::printf(
                    "smoke test: %d frame(s) on screen, background %s, %d warning(s)\n", swapped->load(),
                    background ? "as expected" : "wrong or missing", int(warnings.size()));
                ok = ok && warnings.isEmpty() && written > 0;
            }
            QCoreApplication::exit(ok ? 0 : 1);
        });
        poll->start();
    }

    return QGuiApplication::exec();
}
