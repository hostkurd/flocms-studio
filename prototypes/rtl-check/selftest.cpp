#include "selftest.h"

#include "language.h"

#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QFont>
#include <QGlyphRun>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSet>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>

#include <cstdio>
#include <memory>
#include <optional>
#include <vector>

namespace {

struct Sample {
    const char *context;
    const char *source;
};

// Every string in the app that holds `code words`.
const Sample kSamples[] = {
    {"Main", "Open in `VS Code`"},
    {"Sidebar", "Running · `%1`"},
    {"Overview", "The `.env` file and the app key"},
    {"Overview", "The slug `properties` clashes with `PropertiesController`."},
    {"Overview", "Mail settings are missing, so password reset won't work. Set `MAIL_HOST` in `.env`."},
    {"Overview", "Open `.env`"},
    {"Doctor", "`PHP` version"},
    {"Doctor", "The `pdo_mysql` extension"},
    {"Doctor", "The `storage/cache/` folder"},
    {"Doctor", "The `.env` file"},
};

int g_failures = 0;
const bool g_github = qEnvironmentVariableIsSet("GITHUB_ACTIONS");

void report(bool ok, const QString &what)
{
    if (ok) {
        std::printf("PASS  %s\n", qUtf8Printable(what));
    } else {
        ++g_failures;
        std::printf("%s  %s\n", g_github ? "::error::FAIL" : "FAIL", qUtf8Printable(what));
    }
}

QStringList codeWords(const QString &text)
{
    QStringList words;
    static const QRegularExpression re(QStringLiteral("`([^`]*)`"));
    for (auto it = re.globalMatch(text); it.hasNext();)
        words << it.next().captured(1);
    return words;
}

// Left edge, in layout coordinates, of the glyphs for one character.
std::optional<qreal> glyphX(QTextLayout *layout, int pos)
{
    const QList<QGlyphRun> runs = layout->glyphRuns(pos, 1);
    if (runs.isEmpty())
        return std::nullopt;
    qreal x = runs.constFirst().boundingRect().left();
    for (const QGlyphRun &run : runs)
        x = qMin(x, run.boundingRect().left());
    return x;
}

// The font family Qt actually used for one character (after fallback).
QString glyphFamily(QTextLayout *layout, int pos)
{
    const QList<QGlyphRun> runs = layout->glyphRuns(pos, 1);
    return runs.isEmpty() ? QString() : runs.constFirst().rawFont().familyName();
}

// Lays out HTML the way a QML Text with textFormat RichText does: no text
// direction is forced, so the paragraph direction comes from the text
// itself (the RLM that rich() adds for RTL languages).
struct Laid {
    std::unique_ptr<QTextDocument> doc = std::make_unique<QTextDocument>();
    QString plain;
    QTextLayout *layout = nullptr;
};

Laid layOut(const Language &language, const QString &html)
{
    Laid laid;
    laid.doc->setDefaultFont(QFont(language.uiFont(), 20));
    laid.doc->setHtml(html);
    laid.doc->setTextWidth(4000); // one line
    laid.doc->documentLayout()->documentSize(); // force layout
    laid.plain = laid.doc->toPlainText();
    QTextLayout *layout = laid.doc->firstBlock().layout();
    if (layout && layout->lineCount() == 1)
        laid.layout = layout;
    return laid;
}

// Checks the visual order and fonts of one translated string: each code
// word left to right in JetBrains Mono, Arabic-script prose in Noto Sans
// Arabic, and a sentence-final period at the far left in RTL.
void checkLayout(Language &language, const QString &label, const QString &shown)
{
    const Laid laid = layOut(language, language.rich(shown));
    if (!laid.layout) {
        report(false, label + QStringLiteral(": could not lay out the text"));
        return;
    }
    const QString &plain = laid.plain;
    QTextLayout *layout = laid.layout;

    QList<bool> inCode(plain.size(), false);
    for (const QString &word : codeWords(shown)) {
        // rich() puts an LRI right before each code word, so this finds the
        // word itself, not the same letters somewhere in the prose.
        int start = plain.indexOf(QChar(0x2066) + word);
        if (start < 0) {
            report(false, label + QStringLiteral(": `%1` missing after rendering").arg(word));
            continue;
        }
        ++start;
        bool ordered = true;
        bool mono = true;
        int letters = 0;
        int measured = 0;
        std::optional<qreal> previous;
        for (int i = start; i < start + word.size(); ++i) {
            inCode[i] = true;
            if (plain.at(i).isSpace())
                continue;
            ++letters;
            const std::optional<qreal> x = glyphX(layout, i);
            if (!x)
                continue;
            ++measured;
            if (previous && *x <= *previous)
                ordered = false;
            previous = x;
            if (glyphFamily(layout, i) != QLatin1String("JetBrains Mono"))
                mono = false;
        }
        report(ordered && measured == letters && letters > 0,
               label + QStringLiteral(": `%1` reads left to right (%2 of %3 letters measured)")
                           .arg(word).arg(measured).arg(letters));
        report(mono, label + QStringLiteral(": `%1` uses JetBrains Mono").arg(word));
    }

    int arabic = 0;
    int arabicInFont = 0;
    for (int i = 0; i < plain.size(); ++i) {
        if (inCode[i] || plain.at(i).script() != QChar::Script_Arabic)
            continue;
        ++arabic;
        if (glyphFamily(layout, i) == QLatin1String("Noto Sans Arabic"))
            ++arabicInFont;
    }
    if (language.rtl())
        report(arabic > 0 && arabicInFont == arabic,
               label + QStringLiteral(": Arabic-script text uses Noto Sans Arabic (%1 of %2)")
                           .arg(arabicInFont).arg(arabic));

    if (language.rtl() && shown.endsWith(QLatin1Char('.'))) {
        const int period = plain.lastIndexOf(QLatin1Char('.'));
        const std::optional<qreal> periodX = glyphX(layout, period);
        bool leftmost = periodX.has_value();
        for (int i = 0; leftmost && i < plain.size(); ++i) {
            if (i == period || plain.at(i).isSpace())
                continue;
            const std::optional<qreal> x = glyphX(layout, i);
            if (x && *x < *periodX)
                leftmost = false;
        }
        report(leftmost, label + QStringLiteral(": the final period ends the sentence on the left"));
    }
}

// A string that starts with a code word has no strong RTL letter first;
// without the RLM from rich() the whole line would read left to right.
void checkLeadingCodeWord(Language &language)
{
    const QString lang = language.code();
    const Laid laid = layOut(language, language.rich(QStringLiteral("`.env` فایلە")));
    if (!laid.layout) {
        report(false, lang + QStringLiteral(": could not lay out the leading-code-word sample"));
        return;
    }
    std::optional<qreal> codeLeft;
    std::optional<qreal> proseRight;
    for (int i = 0; i < laid.plain.size(); ++i) {
        const QList<QGlyphRun> runs = laid.layout->glyphRuns(i, 1);
        if (runs.isEmpty() || laid.plain.at(i).isSpace())
            continue;
        const QRectF box = runs.constFirst().boundingRect();
        if (laid.plain.at(i).script() == QChar::Script_Arabic)
            proseRight = proseRight ? qMax(*proseRight, box.right()) : box.right();
        else if (laid.plain.at(i).isLetter() || laid.plain.at(i) == QLatin1Char('.'))
            codeLeft = codeLeft ? qMin(*codeLeft, box.left()) : box.left();
    }
    report(codeLeft && proseRight && *codeLeft >= *proseRight - 0.5,
           lang + QStringLiteral(": a sentence that starts with a code word still reads right to left"));
}

} // namespace

int runSelfTest(Language &language, bool fontsLoaded)
{
    report(fontsLoaded, QStringLiteral("all bundled fonts load"));

    for (const char *code : {"ar", "ckb"}) {
        const QString lang = QString::fromLatin1(code);
        report(language.set(lang), lang + QStringLiteral(": translation file loads"));
        report(QGuiApplication::layoutDirection() == Qt::RightToLeft,
               lang + QStringLiteral(": layout direction is right to left"));

        const QString zero = language.zeroDigit();
        report(zero == QString(QChar(0x0660)),
               lang + QStringLiteral(": locale uses Arabic-Indic digits (zero is U+%1)")
                          .arg(uint(zero.isEmpty() ? 0 : zero.at(0).unicode()), 4, 16, QLatin1Char('0')));
        report(language.num(24) == QStringLiteral("٢٤"),
               lang + QStringLiteral(": 24 shows as ٢٤"));

        const QString tables = QCoreApplication::translate("Overview", "%Ln table(s)", nullptr, 24);
        report(tables.contains(QStringLiteral("٢٤")) && !tables.contains(QLatin1String("table")),
               lang + QStringLiteral(": plural string \"%Ln table(s)\" is translated with local digits"));

        for (const Sample &sample : kSamples) {
            const QString source = QString::fromUtf8(sample.source);
            const QString translated = QCoreApplication::translate(sample.context, sample.source);
            const QString label = lang + QStringLiteral(": \"") + source + QLatin1Char('"');
            report(translated != source, label + QStringLiteral(" is translated"));
            const QStringList sourceWords = codeWords(source);
            const QStringList translatedWords = codeWords(translated);
            report(translatedWords.size() == sourceWords.size()
                       && QSet<QString>(translatedWords.cbegin(), translatedWords.cend())
                              == QSet<QString>(sourceWords.cbegin(), sourceWords.cend()),
                   label + QStringLiteral(" keeps its code words unchanged"));
            // The screen shows %1 filled in, and digits take part in bidi.
            const QString shown = source.contains(QLatin1String("%1"))
                ? translated.arg(QStringLiteral("11.8")) : translated;
            checkLayout(language, label, shown);
        }
        checkLeadingCodeWord(language);
    }

    report(language.set(QStringLiteral("en")), QStringLiteral("en: translation file loads"));
    report(QGuiApplication::layoutDirection() == Qt::LeftToRight,
           QStringLiteral("en: switching back restores left to right"));
    report(QCoreApplication::translate("Overview", "%Ln table(s)", nullptr, 24) == QStringLiteral("24 tables")
               && QCoreApplication::translate("Overview", "%Ln table(s)", nullptr, 1) == QStringLiteral("1 table"),
           QStringLiteral("en: plural forms read \"1 table\" and \"24 tables\""));

    std::printf("\n%s: %d failure(s)\n", g_failures ? "FAILED" : "OK", g_failures);
    return g_failures ? 1 : 0;
}
