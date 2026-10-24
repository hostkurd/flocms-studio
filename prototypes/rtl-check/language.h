#pragma once

#include <QLocale>
#include <QObject>
#include <QString>
#include <QTranslator>

class QQmlEngine;

// Holds the current UI language and everything that depends on it:
// the installed translator, the default QLocale (digits), the layout
// direction, and the display-time handling of code words.
class Language : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(bool rtl READ rtl NOTIFY changed)
    Q_PROPERTY(QString uiFont READ uiFont NOTIFY changed)

public:
    explicit Language(QQmlEngine *engine, QObject *parent = nullptr);

    QString code() const { return m_code; }
    bool rtl() const { return m_code == QLatin1String("ar") || m_code == QLatin1String("ckb"); }
    QString uiFont() const;

    // Switch language while the app runs: "en", "ar" or "ckb".
    Q_INVOKABLE bool set(const QString &code);

    // Turns a translated string into rich text (HTML) for display:
    //  - `code words` become JetBrains Mono, wrapped in LRI ... PDI so
    //    they keep their left-to-right order inside RTL sentences;
    //  - in RTL languages, a leading RLM makes the paragraph RTL even
    //    when it starts with a Latin word.
    // The marks are added here only; translation files never hold them.
    Q_INVOKABLE QString rich(const QString &text) const;

    // A number in the current language's digits (Arabic-Indic for ar, ckb).
    Q_INVOKABLE QString num(int value) const;

    // The digits Qt's locale data gives for this language, for the check.
    Q_INVOKABLE QString zeroDigit() const;

signals:
    void changed();

private:
    static QLocale localeFor(const QString &code);

    QQmlEngine *m_engine;
    QTranslator m_translator;
    QString m_code = QStringLiteral("en");
};
