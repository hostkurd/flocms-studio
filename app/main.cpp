// FloCMS Studio: starts the app shell (stage 3). See README.md.

#include "appinfo.h"
#include "language.h"
#include "singleinstance.h"
#include "studio.h"

#include <flostudio/core/buildinfo.h>
#include <flostudio/core/memory.h>
#include <flostudio/core/settings.h>

#include <QCommandLineParser>
#include <QDir>
#include <QEventLoop>
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
#include <utility>
#include <vector>
#include <functional>
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

// --memory-test: the memory in use at the start, after every page and
// project tab has been opened, and after Studio freed what is idle (as its
// sweep does after a few minutes), each on a line of its own; then quits.
void printMemory(const char *when)
{
    const FloStudio::Core::ProcessMemory memory = FloStudio::Core::currentProcessMemory();
    std::printf("memory %-12s in use %6.1f MB, own %6.1f MB\n", when, double(memory.inUse) / 1048576.0,
                double(memory.own) / 1048576.0);
    (void)std::fflush(stdout);
}

void runMemoryTest(Studio &studio)
{
    // Each step after the one before, with the time to wait for it
    auto steps = std::make_shared<std::vector<std::pair<int, std::function<void()>>>>();
    steps->emplace_back(2500, [] { printMemory("start"); });
    for (const char *tab : {"generate", "database", "logs", "packages", "doctor", "overview"})
        steps->emplace_back(600, [&studio, tab] { studio.requestTab(QString::fromLatin1(tab)); });
    const QString project = studio.currentPath();
    steps->emplace_back(600, [&studio] { studio.showSettings(); });
    steps->emplace_back(800, [&studio] { studio.openNewProject(); });
    steps->emplace_back(800, [&studio, project] { studio.selectProject(project); });
    steps->emplace_back(1000, [] { printMemory("all opened"); });
    steps->emplace_back(300, [&studio] { studio.releaseUnused(true); });
    // Studio gives the memory back a second after freeing it
    steps->emplace_back(2000, [] { printMemory("released"); });
    // Everything again: what was freed is used again, so this stays near
    // "all opened" instead of adding to it
    for (const char *tab : {"generate", "database", "logs", "packages", "doctor", "overview"})
        steps->emplace_back(600, [&studio, tab] { studio.requestTab(QString::fromLatin1(tab)); });
    steps->emplace_back(600, [&studio] { studio.showSettings(); });
    steps->emplace_back(800, [&studio] { studio.openNewProject(); });
    steps->emplace_back(800, [&studio, project] { studio.selectProject(project); });
    steps->emplace_back(1000, [] {
        printMemory("reopened");
        const QStringList warnings = takeWarnings();
        for (const QString &warning : warnings)
            std::printf("warning: %s\n", qUtf8Printable(warning));
        QCoreApplication::exit(warnings.isEmpty() ? 0 : 1);
    });
    auto *timer = new QTimer(QCoreApplication::instance()); // owned by the application
    timer->setSingleShot(true);
    auto at = std::make_shared<std::size_t>(0);
    QObject::connect(timer, &QTimer::timeout, timer, [timer, steps, at] {
        (*steps)[*at].second();
        if (++*at < steps->size())
            timer->start((*steps)[*at].first);
    });
    timer->start(steps->front().first);
}

} // namespace

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (argc > 1)
        attachParentConsole();
#endif

    // --memory-test counts warnings the same way
    const SmokeHandler smokeHandler(hasArgument(argc, argv, "--smoke-test") || hasArgument(argc, argv, "--memory-test"));

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
    const QCommandLineOption memoryOpt(QStringLiteral("memory-test"),
                                       QStringLiteral("With --demo: print the memory in use at the start, after opening "
                                                      "every page and tab, and after freeing what is idle; then quit. "
                                                      "Fails on any warning."));
    parser.addOptions({versionOpt, smokeOpt, shotOpt, demoOpt, languageOpt, themeOpt, memoryOpt});
    parser.process(app);

    if (parser.isSet(versionOpt)) {
        const QByteArray text = AppInfo::report().toUtf8() + '\n';
        return std::fwrite(text.constData(), 1, size_t(text.size()), stdout) == size_t(text.size()) ? 0 : 1;
    }

    const bool smoke = parser.isSet(smokeOpt);
    // One Studio per user: started again, the running one comes to the
    // front and this one quits. Tests, screenshots and demos run beside it.
    const bool alone = !smoke && !parser.isSet(shotOpt) && !parser.isSet(demoOpt) && !parser.isSet(memoryOpt);
    SingleInstance instance(FloStudio::Core::settingsFolder());
    if (alone && !instance.claim())
        return 0;
    // Closing: no longer answers, so a Studio started now runs instead of
    // asking this one
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &instance, &SingleInstance::release);
    // Started again before the window exists (during the splash): shown
    // once it does
    QQuickWindow *shownWindow = nullptr;
    bool showWhenReady = false;
    const auto bringForward = [](QQuickWindow *window) {
        // Restored if minimized, keeping maximized
        window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
        window->show();
        window->raise();
        window->requestActivate();
#ifdef Q_OS_WIN
        // Qt only flashes the taskbar button when another program has the
        // focus; the copy just started allowed this one to take it
        // NOLINTNEXTLINE(performance-no-int-to-ptr): a window handle
        SetForegroundWindow(reinterpret_cast<HWND>(window->winId()));
#endif
    };
    QObject::connect(&instance, &SingleInstance::activateRequested, &app, [&shownWindow, &showWhenReady, bringForward] {
        if (shownWindow)
            bringForward(shownWindow);
        else
            showWhenReady = true;
    });
    const bool memoryTest = parser.isSet(memoryOpt) && parser.isSet(demoOpt);
    if (!loadFonts() && smoke)
        return 1;

    // Before the engine, so they outlive it: it holds them as singletons
    Language language;
    Studio studio(&language);
    AppInfo appInfo; // before the engine, so it outlives it
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
    qmlRegisterSingletonType<AppInfo>("FloStudio.Backend", 1, 0, "AppInfo", [&appInfo](QQmlEngine *, QJSEngine *) -> QObject * {
        QJSEngine::setObjectOwnership(&appInfo, QJSEngine::CppOwnership);
        return &appInfo;
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

    // A small window at once, while the main one is made: on a cold start
    // (first run, Windows checking the new program) that takes a moment.
    // Not for a smoke test or a screenshot, which look at the main window.
    QQuickWindow *splash = nullptr;
    if (!smoke && !memoryTest && !parser.isSet(shotOpt)) {
        engine.load(QUrl(QStringLiteral("qrc:/qt/qml/FloStudio/App/Splash.qml")));
        if (!engine.rootObjects().isEmpty())
            splash = qobject_cast<QQuickWindow *>(engine.rootObjects().constLast());
        if (splash) {
            // Let it reach the screen before the main window keeps the
            // thread busy (at most a quarter of a second)
            QEventLoop drawn;
            QObject::connect(splash, &QQuickWindow::frameSwapped, &drawn, &QEventLoop::quit, Qt::QueuedConnection);
            QTimer::singleShot(250, &drawn, &QEventLoop::quit);
            drawn.exec();
        }
    }

    const qsizetype before = engine.rootObjects().size();
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/FloStudio/App/Main.qml")));
    if (engine.rootObjects().size() == before) {
        qCritical("Could not load the main window.");
        return 1;
    }
    auto *mainWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().constLast());

    // The splash goes once the main window has drawn its first frame
    if (splash && mainWindow) {
        // Also after a few seconds, and when the main window is minimized:
        // a window started minimized (or on another macOS Space) never
        // draws, and the frameless splash would stay. Closing twice is fine.
        QTimer::singleShot(5000, splash, [splash] { splash->close(); });
        QObject::connect(mainWindow, &QWindow::windowStateChanged, splash, [splash](Qt::WindowState state) {
            if (state == Qt::WindowMinimized)
                splash->close();
        });
        auto closeSplash = std::make_shared<QMetaObject::Connection>();
        *closeSplash = QObject::connect(mainWindow, &QQuickWindow::frameSwapped, splash, [splash, closeSplash] {
            QObject::disconnect(*closeSplash);
            splash->close();
            splash->deleteLater();
        }, Qt::QueuedConnection);
    } else if (splash) {
        splash->close();
    }

    // Started again: this window comes to the front
    shownWindow = mainWindow;
    if (mainWindow && std::exchange(showWhenReady, false))
        bringForward(mainWindow);

    // Memory management: after Studio freed screens or data, the QML
    // engine's collector, then the freed memory back to the system
    QObject::connect(&studio, &Studio::memoryFreed, &engine, [&engine] {
        engine.collectGarbage();
        FloStudio::Core::returnFreeMemory();
    });

    if (memoryTest)
        runMemoryTest(studio);

    if (smoke || parser.isSet(shotOpt)) {
        auto *window = mainWindow;
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
        auto shown = std::make_shared<int>(0); // when the first frame was on screen
        QObject::connect(poll, &QTimer::timeout, &app, [=] {
            *waited += poll->interval();
            if (swapped->load() < 1 && *waited < 15000)
                return; // keep waiting, up to 15 seconds
            // A screenshot waits 1.6 s after the first frame, so layouts
            // that settle after it (a page opened on a section) are in it
            if (*shown == 0 && swapped->load() >= 1)
                *shown = *waited;
            if (!smoke && swapped->load() >= 1 && *waited - *shown < 1600)
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
