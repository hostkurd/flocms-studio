#include "testutil.h"

#include <flostudio/core/env.h>

#include <QTemporaryDir>
#include <QTest>

using namespace FloStudio::Core;

class TestEnv : public QObject
{
    Q_OBJECT

    using Values = QList<QPair<QString, QString>>;

private slots:
    void readsTheSkeletonsExample()
    {
        // The real .env.example of the skeleton (tests/fixtures/project)
        const auto env = parseEnv(TestUtil::fixture(QStringLiteral("project/env.example")));
        QCOMPARE(env.value(QStringLiteral("APP_NAME")), QStringLiteral("FloCMS"));
        QCOMPARE(env.value(QStringLiteral("SITE_NAME")), QStringLiteral("FLO CMS"));
        QCOMPARE(env.value(QStringLiteral("LANGUAGES")), QStringLiteral("en,ar,ku"));
        QCOMPARE(env.value(QStringLiteral("DB_NAME")), QString());
        QVERIFY(env.contains(QStringLiteral("DB_NAME")));
        QCOMPARE(env.value(QStringLiteral("API_CORS_ORIGINS")), QString());
        QVERIFY(!env.contains(QStringLiteral("# Template")));
    }

    void readsLikeCore_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("value");
        QTest::newRow("plain") << "A=b" << "b";
        QTest::newRow("trimmed") << "  A =  b c  " << "b c";
        QTest::newRow("double quotes") << "A=\"My Site\"" << "My Site";
        QTest::newRow("single quotes") << "A='p#ss'" << "p#ss";
        QTest::newRow("no inline comment") << "A=a #b" << "a #b";
        QTest::newRow("quote inside") << "A=\"a\"b\"" << "a\"b";
        QTest::newRow("mixed quotes stay") << "A=\"a'" << "\"a'";
        QTest::newRow("second equals") << "A=b=c" << "b=c";
        QTest::newRow("empty") << "A=" << "";
        QTest::newRow("Kurdish") << "A=کوردی" << "کوردی";
        QTest::newRow("CRLF") << "A=b\r" << "b";
    }

    void readsLikeCore()
    {
        QFETCH(QString, line);
        QFETCH(QString, value);
        QCOMPARE(parseEnv(line).value(QStringLiteral("A")), value);
        QCOMPARE(parseEnvForCommands(line).value(QStringLiteral("A")), value);
    }

    void skipsCommentsAndOtherLines()
    {
        const auto env = parseEnv(QStringLiteral("# A=1\n  # B=2\nno equals\n\n=nokey\nC=3\n"));
        QCOMPARE(env.keys(), (QStringList{QString(), QStringLiteral("C")}));
    }

    void aSingleQuoteCharacterDiffers()
    {
        // core strips `"` to nothing (starts and ends with "), the CLI needs two characters
        QCOMPARE(parseEnv(QStringLiteral("A=\"")).value(QStringLiteral("A")), QString());
        QCOMPARE(parseEnvForCommands(QStringLiteral("A=\"")).value(QStringLiteral("A")), QStringLiteral("\""));
    }

    void aKeyWrittenTwice()
    {
        const QString text = QStringLiteral("DB_NAME=first\nX=1\nDB_NAME=second\nX=1\nY=2\n");
        QCOMPARE(parseEnv(text).value(QStringLiteral("DB_NAME")), QStringLiteral("first"));
        QCOMPARE(parseEnvForCommands(text).value(QStringLiteral("DB_NAME")), QStringLiteral("second"));
        QCOMPARE(duplicateKeys(text), (QStringList{QStringLiteral("DB_NAME"), QStringLiteral("X")}));
        QVERIFY(duplicateKeys(TestUtil::fixture(QStringLiteral("project/env.example"))).isEmpty());
    }

    void flags_data()
    {
        QTest::addColumn<QString>("value");
        QTest::addColumn<bool>("fallback");
        QTest::addColumn<bool>("expected");
        QTest::newRow("true") << "true" << false << true;
        QTest::newRow("TRUE") << "TRUE" << false << true;
        QTest::newRow("1") << "1" << false << true;
        QTest::newRow("yes") << "yes" << false << true;
        QTest::newRow("on") << "on" << false << true;
        QTest::newRow("false") << "false" << true << false;
        QTest::newRow("0") << "0" << true << false;
        QTest::newRow("empty") << "" << true << false;
        QTest::newRow("other") << "maybe" << true << true;
        QTest::newRow("other2") << "maybe" << false << false;
    }

    void flags()
    {
        QFETCH(QString, value);
        QFETCH(bool, fallback);
        QFETCH(bool, expected);
        const QMap<QString, QString> env{{QStringLiteral("APP_DEBUG"), value}};
        QCOMPARE(envFlag(env, QStringLiteral("APP_DEBUG"), fallback), expected);
        QCOMPARE(envFlag({}, QStringLiteral("APP_DEBUG"), fallback), fallback);
    }

    void production()
    {
        const auto env = [](const QString &value) { return QMap<QString, QString>{{QStringLiteral("APP_ENV"), value}}; };
        QVERIFY(isProduction({}));
        QVERIFY(isProduction(env(QStringLiteral("production"))));
        QVERIFY(isProduction(env(QStringLiteral("Prod"))));
        QVERIFY(isProduction(env(QStringLiteral("live"))));
        QVERIFY(isProduction(env(QStringLiteral("null"))));
        QVERIFY(!isProduction(env(QStringLiteral("local"))));
        QVERIFY(!isProduction(env(QString())));
        QVERIFY(!isProduction(parseEnv(TestUtil::fixture(QStringLiteral("project/env.example")))));
    }

    void writesValuesBothReadersGiveBack_data()
    {
        QTest::addColumn<QString>("value");
        QTest::addColumn<QString>("written");
        QTest::newRow("plain") << "shop" << "shop";
        QTest::newRow("inner spaces") << "Studio Full" << "Studio Full";
        QTest::newRow("hash") << "p#ss" << "p#ss";
        QTest::newRow("inner quote") << "a\"b" << "a\"b";
        QTest::newRow("outer spaces") << " x " << "\" x \"";
        QTest::newRow("looks quoted") << "'x'" << "\"'x'\"";
        QTest::newRow("looks double quoted") << "\"x\"" << "\"\"x\"\"";
        QTest::newRow("one quote") << "\"" << "\"\"\"";
        QTest::newRow("both quotes") << "a'b\" c" << "a'b\" c";
        QTest::newRow("empty") << "" << "";
        QTest::newRow("Arabic") << "موقعي" << "موقعي";
    }

    void writesValuesBothReadersGiveBack()
    {
        QFETCH(QString, value);
        QFETCH(QString, written);
        const auto line = envLineValue(value);
        QVERIFY(line.ok());
        QCOMPARE(line.value(), written);
        const QString text = QStringLiteral("DB_PASSWORD=") + line.value();
        QCOMPARE(parseEnv(text).value(QStringLiteral("DB_PASSWORD")), value);
        QCOMPARE(parseEnvForCommands(text).value(QStringLiteral("DB_PASSWORD")), value);
    }

    void aLineBreakCannotBeWritten()
    {
        QCOMPARE(envLineValue(QStringLiteral("a\nb")).error().code, QStringLiteral("env_value"));
        QCOMPARE(envLineValue(QStringLiteral("a\rb")).error().code, QStringLiteral("env_value"));
    }

    void setReplacesAddsAndKeepsTheRest()
    {
        const QByteArray before =
            "APP_NAME=FloCMS\n# Database\nDB_TYPE=mysql\nDB_NAME=\n# DB_HOST=old\nAPP_DEBUG=true\nDB_NAME=dup\n";
        const auto after = setEnvValues(before, Values{{QStringLiteral("DB_NAME"), QStringLiteral("shop")},
                                                       {QStringLiteral("DB_HOST"), QStringLiteral("127.0.0.1")},
                                                       {QStringLiteral("APP_NAME"), QStringLiteral("My Shop")}});
        QVERIFY(after.ok());
        QCOMPARE(after.value(), QByteArray("APP_NAME=My Shop\n# Database\nDB_TYPE=mysql\nDB_NAME=shop\n"
                                           "# DB_HOST=old\nAPP_DEBUG=true\nDB_HOST=127.0.0.1\n"));
    }

    void setKeepsLineEndings()
    {
        QCOMPARE(setEnvValues("A=1\r\nDB_NAME=\r\n", Values{{QStringLiteral("DB_NAME"), QStringLiteral("x")}}).value(),
                 QByteArray("A=1\r\nDB_NAME=x\r\n"));
        // Mixed: each line keeps its own; an added line takes CRLF
        QCOMPARE(setEnvValues("A=1\nB=2\r\nDB_NAME=\n", Values{{QStringLiteral("DB_NAME"), QStringLiteral("x")},
                                                                {QStringLiteral("DB_HOST"), QStringLiteral("h")}}).value(),
                 QByteArray("A=1\nB=2\r\nDB_NAME=x\nDB_HOST=h\r\n"));
        // No final line break: one is added
        QCOMPARE(setEnvValues("A=1", Values{{QStringLiteral("DB_NAME"), QStringLiteral("x")}}).value(),
                 QByteArray("A=1\nDB_NAME=x\n"));
    }

    void setLeavesOtherBytesAlone()
    {
        // A line in Latin-1 (not UTF-8) and a byte order mark stay as they were
        const QByteArray before = "\xEF\xBB\xBF" "APP_NAME=x\nSITE_NAME=Caf\xE9\nDB_NAME=\n";
        const auto after = setEnvValues(before, Values{{QStringLiteral("DB_NAME"), QStringLiteral("shop")}});
        QCOMPARE(after.value(), QByteArray("\xEF\xBB\xBF" "APP_NAME=x\nSITE_NAME=Caf\xE9\nDB_NAME=shop\n"));
    }

    void aByteOrderMarkIsPartOfTheFirstKey()
    {
        // As PHP reads it: the first key is "\uFEFFAPP_NAME", not APP_NAME
        const QString text = envText("\xEF\xBB\xBF" "APP_NAME=Shop\nDB_NAME=x\n");
        QVERIFY(hasByteOrderMark(text));
        const auto env = parseEnv(text);
        QVERIFY(!env.contains(QStringLiteral("APP_NAME")));
        QCOMPARE(env.value(QChar(0xFEFF) + QStringLiteral("APP_NAME")), QStringLiteral("Shop"));
        QCOMPARE(env.value(QStringLiteral("DB_NAME")), QStringLiteral("x"));
        QVERIFY(!hasByteOrderMark(envText("A=1")));
    }

    void setOnTheRealExample()
    {
        const QByteArray example = TestUtil::fixture(QStringLiteral("project/env.example")).toUtf8();
        const auto after = setEnvValues(example, Values{{QStringLiteral("DB_NAME"), QStringLiteral("studio")},
                                                        {QStringLiteral("DB_USERNAME"), QStringLiteral("root")},
                                                        {QStringLiteral("APP_NAME"), QStringLiteral("سایتی من")}});
        QVERIFY(after.ok());
        const auto env = parseEnv(envText(after.value()));
        QCOMPARE(env.value(QStringLiteral("DB_NAME")), QStringLiteral("studio"));
        QCOMPARE(env.value(QStringLiteral("APP_NAME")), QStringLiteral("سایتی من"));
        // Only the three lines changed
        const QList<QByteArray> a = example.split('\n');
        const QList<QByteArray> b = after.value().split('\n');
        QCOMPARE(a.size(), b.size());
        int changed = 0;
        for (qsizetype i = 0; i < a.size(); ++i)
            changed += a.at(i) != b.at(i) ? 1 : 0;
        QCOMPARE(changed, 3);
    }

    void refusesOtherKeys()
    {
        QCOMPARE(setEnvValues(QByteArray(), Values{{QStringLiteral("APP_KEY"), QStringLiteral("x")}}).error().code,
                 QStringLiteral("env_key"));
        QCOMPARE(setEnvValues(QByteArray(), Values{{QStringLiteral("APP_NAME"), QStringLiteral("a\nb")}}).error().code,
                 QStringLiteral("env_value"));
    }

    void writesTheFileFromTheExample()
    {
        const QTemporaryDir dir;
        QFile example(dir.filePath(QStringLiteral(".env.example")));
        QVERIFY(example.open(QIODevice::WriteOnly));
        example.write("APP_NAME=FloCMS\nDB_NAME=\n");
        example.close();
        QVERIFY(writeEnvValues(dir.path(), Values{{QStringLiteral("DB_NAME"), QStringLiteral("shop")}}).ok());
        QCOMPARE(readEnvFile(dir.path()), QStringLiteral("APP_NAME=FloCMS\nDB_NAME=shop\n"));
        // Again: now from .env itself
        QVERIFY(writeEnvValues(dir.path(), Values{{QStringLiteral("APP_NAME"), QStringLiteral("Shop")}}).ok());
        QCOMPARE(readEnvFile(dir.path()), QStringLiteral("APP_NAME=Shop\nDB_NAME=shop\n"));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden).size(), 2); // no temporary file left
    }
};

QTEST_GUILESS_MAIN(TestEnv)
#include "tst_env.moc"
