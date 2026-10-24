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
        std::printf("PASS  %s\n", qPrintable(what));
    } else {
        ++g_failures;
        std::printf("%s  %s\n", g_github ? "::error::FAIL" : "FAIL", qPrintable(what));
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

// Lays out rich() output the way a QML Text with RichText does and checks
// the visual order: each code word left to right, sentence-final period
// at the far left in RTL.
void checkLayout(Language &language, const QString &label, const QString &translated)
{
    QTextDocument doc;
    doc.setDefaultFont(QFont(language.uiFont(), 20));
    QTextOption option;
    option.setTextDirection(language.rtl() ? Qt::RightToLeft : Qt::LeftToRight);
    doc.setDefaultTextOption(option);
    doc.setHtml(language.rich(translated));
    doc.setTextWidth(4000); // one line
    doc.documentLayout()->documentSize(); // force layout

    const QString plain = doc.toPlainText();
    QTextLayout *layout = doc.firstBlock().layout();
    if (!layout || layout->lineCount() != 1) {
        report(false, label + QStringLiteral(": could not lay out the text"));
        return;
    }

    for (const QString &word : codeWords(translated)) {
        const int start = plain.indexOf(word);
        if (start < 0) {
            report(false, label + QStringLiteral(": `%1` missing after rendering").arg(word));
            continue;
        }
        bool ordered = true;
        std::optional<qreal> previous;
        for (int i = start; i < start + word.size(); ++i) {
            if (word.at(i - start).isSpace())
                continue;
            const std::optional<qreal> x = glyphX(layout, i);
            if (!x)
                continue;
            if (previous && *x <= *previous)
                ordered = false;
            previous = x;
        }
        report(ordered, label + QStringLiteral(": `%1` reads left to right").arg(word));
    }

    if (language.rtl() && translated.endsWith(QLatin1Char('.'))) {
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

} // namespace

int runSelfTest(Language &language)
{
    for (const char *code : {"ar", "ckb"}) {
        const QString lang = QString::fromLatin1(code);
        report(language.set(lang), lang + QStringLiteral(": translation file loads"));
        report(QGuiApplication::layoutDirection() == Qt::RightToLeft,
               lang + QStringLiteral(": layout direction is right to left"));

        const QString zero = language.zeroDigit();
        report(zero == QString(QChar(0x0660)),
               lang + QStringLiteral(": locale uses Arabic-Indic digits (zero is U+%1)")
                          .arg(zero.isEmpty() ? 0 : zero.at(0).unicode(), 4, 16, QLatin1Char('0')));
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
            checkLayout(language, label, translated);
        }
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
