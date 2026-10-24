#pragma once

#include <QLocale>
#include <QObject>
#include <QString>
#include <QTranslator>

class QQmlEngine;

// The current UI language and everything that depends on it: the
// translator, the default QLocale (digits), the layout direction, and the
// display-time handling of code words (D9; checked in stage 0).
class Language : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(bool rtl READ rtl NOTIFY changed)
    Q_PROPERTY(QString uiFont READ uiFont NOTIFY changed)
    Q_PROPERTY(QString displayFont READ displayFont NOTIFY changed)

public:
    explicit Language(QObject *parent = nullptr);

    // The engine to retranslate on a switch. Language must outlive it,
    // because it is registered as a QML singleton instance.
    void setEngine(QQmlEngine *engine) { m_engine = engine; }

    QString code() const { return m_code; }
    bool rtl() const { return m_code == QLatin1String("ar") || m_code == QLatin1String("ckb"); }
    QString uiFont() const;

    // Switch language while the app runs: "en", "ar", "ckb", or "system"
    // (the computer's language when it is one of those, else English).
    Q_INVOKABLE bool set(const QString &code);

    // The language "system" stands for on this computer.
    static QString systemLanguage();

    // The font of headings: the same as the labels, as in native programs.
    QString displayFont() const;
    // The system's UI font (the labels in English).
    static QString systemFont();

    // Turns a translated string into rich text (HTML) for display:
    //  - `code words` become JetBrains Mono, wrapped in LRI ... PDI so
    //    they keep their left-to-right order inside RTL sentences;
    //  - in RTL languages, a leading RLM makes the paragraph RTL even
    //    when it starts with a Latin word.
    // The marks are added here only; translation files never hold them.
    Q_INVOKABLE QString rich(const QString &text) const;

    // A number in the current language's digits (Arabic-Indic for ar, ckb).
    Q_INVOKABLE QString num(qint64 value) const;

    // A size in bytes for people: "31.5 MB", in the language's digits.
    Q_INVOKABLE QString size(qint64 bytes) const;

signals:
    void changed();

private:
    static QLocale localeFor(const QString &code);

    QQmlEngine *m_engine = nullptr;
    QTranslator m_translator;
    QString m_code = QStringLiteral("en");
};
