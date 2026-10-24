// Stage 0 of FloCMS Studio native: a throwaway check that Qt Quick
// renders Kurdish and Arabic correctly. See README.md for what it checks.

#include "language.h"
#include "selftest.h"

#include <QCommandLineParser>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

namespace {

bool loadFonts()
{
    bool all = true;
    const char *files[] = {
        ":/fonts/NotoSans-Regular.ttf",       ":/fonts/NotoSans-Bold.ttf",
        ":/fonts/NotoSansArabic-Regular.ttf", ":/fonts/NotoSansArabic-Bold.ttf",
        ":/fonts/JetBrainsMono-Regular.ttf",
    };
    for (const char *file : files) {
        if (QFontDatabase::addApplicationFont(QString::fromLatin1(file)) < 0) {
            qWarning("Could not load font %s", file);
            all = false;
        }
    }
    // Each UI font falls back to the other for the script it lacks, so a
    // Latin word in Kurdish text (or Arabic in English text) never falls
    // back to whatever font the system happens to have.
    QFont::insertSubstitution(QStringLiteral("Noto Sans Arabic"), QStringLiteral("Noto Sans"));
    QFont::insertSubstitution(QStringLiteral("Noto Sans"), QStringLiteral("Noto Sans Arabic"));
    return all;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("FloCMS Studio RTL check"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    const bool fontsLoaded = loadFonts();

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption langOpt(QStringLiteral("lang"), QStringLiteral("en, ar or ckb"),
                                     QStringLiteral("code"), QStringLiteral("en"));
    const QCommandLineOption pageOpt(QStringLiteral("page"), QStringLiteral("overview or doctor"),
                                     QStringLiteral("name"), QStringLiteral("overview"));
    const QCommandLineOption darkOpt(QStringLiteral("dark"), QStringLiteral("Use the dark theme."));
    const QCommandLineOption shotOpt(QStringLiteral("screenshot"),
                                     QStringLiteral("Save a PNG of the window and quit."),
                                     QStringLiteral("file"));
    const QCommandLineOption testOpt(QStringLiteral("self-test"),
                                     QStringLiteral("Run the automatic checks and quit."));
    parser.addOptions({langOpt, pageOpt, darkOpt, shotOpt, testOpt});
    parser.process(app);

    // Declared before the engine so it is destroyed after it: the engine
    // holds it as a QML singleton instance.
    Language language;
    if (parser.isSet(testOpt))
        return runSelfTest(language, fontsLoaded);

    QQmlApplicationEngine engine;
    language.setEngine(&engine);
    qmlRegisterSingletonInstance("RtlCheck.Backend", 1, 0, "Lang", &language);

    if (!language.set(parser.value(langOpt))) {
        qCritical("Unknown language or missing translation: %s", qPrintable(parser.value(langOpt)));
        return 2;
    }

    engine.setInitialProperties({
        {QStringLiteral("page"), parser.value(pageOpt)},
        {QStringLiteral("dark"), parser.isSet(darkOpt)},
    });
    engine.load(QUrl(QStringLiteral("qrc:/RtlCheck/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (parser.isSet(shotOpt)) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const QString file = parser.value(shotOpt);
        // Give the scene graph a few frames to lay out and render.
        QTimer::singleShot(1500, &app, [window, file] {
            const bool saved = window && window->grabWindow().save(file);
            if (!saved)
                qCritical("Could not save the screenshot to %s", qPrintable(file));
            QCoreApplication::exit(saved ? 0 : 3);
        });
    }

    return QGuiApplication::exec();
}
