#include "language.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlEngine>

namespace {
constexpr QChar kLri{0x2066}; // LEFT-TO-RIGHT ISOLATE
constexpr QChar kPdi{0x2069}; // POP DIRECTIONAL ISOLATE
constexpr QChar kRlm{0x200F}; // RIGHT-TO-LEFT MARK
} // namespace

Language::Language(QQmlEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

QString Language::uiFont() const
{
    return rtl() ? QStringLiteral("Noto Sans Arabic") : QStringLiteral("Noto Sans");
}

QLocale Language::localeFor(const QString &code)
{
    if (code == QLatin1String("ar"))
        return QLocale(QLocale::Arabic, QLocale::Iraq);
    if (code == QLatin1String("ckb"))
        return QLocale(QLocale::CentralKurdish, QLocale::Iraq);
    return QLocale(QLocale::English, QLocale::UnitedStates);
}

bool Language::set(const QString &code)
{
    if (code != QLatin1String("en") && code != QLatin1String("ar") && code != QLatin1String("ckb"))
        return false;

    // English has a file too: it holds only the plural forms
    // ("1 table", "24 tables"), since the source strings say "table(s)".
    QCoreApplication::removeTranslator(&m_translator);
    if (!m_translator.load(QStringLiteral(":/i18n/rtlcheck_%1.qm").arg(code)))
        return false;
    QCoreApplication::installTranslator(&m_translator);

    m_code = code;
    QLocale::setDefault(localeFor(code));
    QGuiApplication::setLayoutDirection(rtl() ? Qt::RightToLeft : Qt::LeftToRight);
    m_engine->retranslate();
    emit changed();
    return true;
}

QString Language::rich(const QString &text) const
{
    QString out;
    out.reserve(text.size() + 32);
    if (rtl())
        out += kRlm;

    bool inCode = false;
    for (const QChar c : text) {
        if (c == QLatin1Char('`')) {
            out += inCode ? QString(kPdi) + QStringLiteral("</span>")
                          : QStringLiteral("<span style=\"font-family:'JetBrains Mono'\">") + kLri;
            inCode = !inCode;
            continue;
        }
        switch (c.unicode()) {
        case '<': out += QStringLiteral("&lt;"); break;
        case '>': out += QStringLiteral("&gt;"); break;
        case '&': out += QStringLiteral("&amp;"); break;
        default: out += c;
        }
    }
    if (inCode) // unbalanced backtick: close it so the markup stays valid
        out += QString(kPdi) + QStringLiteral("</span>");
    return out;
}

QString Language::num(int value) const
{
    return localeFor(m_code).toString(value);
}

QString Language::zeroDigit() const
{
    return localeFor(m_code).zeroDigit();
}
