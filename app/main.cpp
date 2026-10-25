// FloCMS Studio: starts the app shell (stage 3). See README.md.

#include "language.h"
#include "studio.h"

#include <flostudio/core/buildinfo.h>

#include <QCommandLineParser>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QNetworkProxyFactory>
#include <QQuickStyle>
#include <QGuiApplication>
#include <QIcon>
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

// The window's background colour in qml/Main.qml (Theme.bg, light).
constexpr QRgb kBackground = 0xFFF4F7F9;

// The bundled fonts (D8), and the system's UI font for English labels
// (a native look). Each falls back to the other for the script it lacks:
// a Latin project name in a Kurdish window uses the system font, Arabic
// letters in an English window Noto Sans Arabic.
bool loadFonts()
{
    bool all = true;
    const QDir fonts(QStringLiteral(":/fonts"));
    for (const QString &file : fonts.entryList({QStringLiteral("*.ttf")})) {
        if (QFontDatabase::addApplicationFont(fonts.filePath(file)) < 0) {
            qWarning("Could not load the font %s", qUtf8Printable(file));
            all = false;
        }
    }
    // The labels use the system's font (Language::systemFont)
    const QString system = Language::systemFont();
    QFont::insertSubstitution(QStringLiteral("Noto Sans Arabic"), system);
    QFont::insertSubstitution(system, QStringLiteral("Noto Sans Arabic"));
    QFont::insertSubstitution(QStringLiteral("Instrument Sans"), QStringLiteral("Noto Sans Arabic"));
    QFont::insertSubstitution(QStringLiteral("Bricolage Grotesque"), QStringLiteral("Noto Sans Arabic"));
    QFont::insertSubstitution(QStringLiteral("JetBrains Mono"), QStringLiteral("Noto Sans Arabic"));
    return all;
}

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
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    // The logo in the title bar and the taskbar; on Linux the desktop file
    // (flocms-studio.desktop) names it for Wayland
    QIcon icon;
    for (const int size : {64, 128, 256, 512})
        icon.addFile(QStringLiteral(":/icons/app-icon-%1.png").arg(size), QSize(size, size));
    QGuiApplication::setWindowIcon(icon);
    QGuiApplication::setDesktopFileName(QStringLiteral("flocms-studio"));
    QNetworkProxyFactory::setUseSystemConfiguration(true); // tool downloads behind a company proxy

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
    const QCommandLineOption demoOpt(QStringLiteral("demo"),
                                     QStringLiteral("Show the state in a demo file (no tools are looked for, "
                                                    "nothing is saved)."),
                                     QStringLiteral("file"));
    const QCommandLineOption languageOpt(QStringLiteral("language"),
                                         QStringLiteral("en, ar or ckb, for this run (with --demo)."),
                                         QStringLiteral("code"));
    const QCommandLineOption themeOpt(QStringLiteral("theme"),
                                      QStringLiteral("light or dark, for this run (with --demo)."),
                                      QStringLiteral("name"));
    parser.addOptions({versionOpt, smokeOpt, shotOpt, demoOpt, languageOpt, themeOpt});
    parser.process(app);

    if (parser.isSet(versionOpt)) {
        const QByteArray text = FloStudio::Core::toText(FloStudio::Core::currentBuildInfo()).toUtf8() + '\n';
        return std::fwrite(text.constData(), 1, size_t(text.size()), stdout) == size_t(text.size()) ? 0 : 1;
    }

    const bool smoke = parser.isSet(smokeOpt);
    if (!loadFonts() && smoke)
        return 1;

    // Before the engine, so they outlive it: it holds them as singletons
    Language language;
    Studio studio(&language);
    QQmlApplicationEngine engine;
    language.setEngine(&engine);
    // Singletons that stay C++'s (qmlRegisterSingletonInstance does the
    // same through a QPointer that clang-analyzer misreads)
    qmlRegisterSingletonType<Language>("FloStudio.Backend", 1, 0, "Lang", [&language](QQmlEngine *, QJSEngine *) -> QObject * {
        QJSEngine::setObjectOwnership(&language, QJSEngine::CppOwnership);
        return &language;
    });
    qmlRegisterSingletonType<Studio>("FloStudio.Backend", 1, 0, "Studio", [&studio](QQmlEngine *, QJSEngine *) -> QObject * {
        QJSEngine::setObjectOwnership(&studio, QJSEngine::CppOwnership);
        return &studio;
    });

    if (parser.isSet(demoOpt)) {
        if (const auto demo = studio.startDemo(parser.value(demoOpt)); !demo) {
            qCritical("Could not read the demo file: %s", qUtf8Printable(demo.error().detail));
            return 2;
        }
        if (parser.isSet(languageOpt))
            studio.setLanguage(parser.value(languageOpt));
        if (parser.isSet(themeOpt))
            studio.setTheme(parser.value(themeOpt));
    } else {
        studio.start();
    }

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
            // A screenshot waits a little longer, so layouts that settle
            // after the first frame (a page opened on a section) are in it
            if (!smoke && *waited < 800)
                return;
            poll->stop();

            const QImage frame = window->grabWindow();
            const bool onScreen = swapped->load() >= 1;
            // The smoke test runs light: the background is Theme.bg; a
            // screenshot is any theme
            const bool background = !frame.isNull() && (!smoke || frame.pixel(frame.width() - 4, frame.height() - 4) == kBackground);
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
