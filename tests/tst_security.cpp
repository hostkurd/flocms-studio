#include <flostudio/security/library.h>
#include <flostudio/security/secrets.h>

#include "testutil.h"

#include <flostudio/core/buildinfo.h>

#include <QStandardPaths>
#include <QTest>

class TestSecurity : public QObject
{
    Q_OBJECT

private slots:
    void libraryName()
    {
        QCOMPARE(FloStudio::Security::libraryName(), QStringLiteral("FloStudioSecurity"));
    }

    // D5: Security uses Core, and Core comes through that link only.
    void dependsOnCore()
    {
        QCOMPARE(FloStudio::Security::dependencies(), QStringList{FloStudio::Core::libraryName()});
    }

    // --- Stage 6: passwords and keys --------------------------------------

    void redactsSecrets()
    {
        using FloStudio::Security::redact;
        QCOMPARE(redact(QStringLiteral("DB_PASSWORD=s3cret!pw and again s3cret!pw"), {QStringLiteral("s3cret!pw")}),
                 QStringLiteral("DB_PASSWORD=•••••••• and again ••••••••"));
        // A secret that holds another is replaced whole; short ones are left
        QCOMPARE(redact(QStringLiteral("key base64:abcdef123"), {QStringLiteral("abc"), QStringLiteral("base64:abcdef123")}),
                 QStringLiteral("key ••••••••"));
        QCOMPARE(redact(QStringLiteral("a b"), {QStringLiteral("a"), QString()}), QStringLiteral("a b"));
    }

    void findsTheSecretsOfAnEnv()
    {
        // The real skeleton's .env.example, with a key and passwords set
        QString env = TestUtil::fixture(QStringLiteral("project/env.example"));
        env += QStringLiteral("\nAPP_KEY=base64:Zm9vYmFyYmF6\nDB_PASSWORD=\"pa ss\"\nMAIL_PASSWORD=mailpw\nSTRIPE_SECRET=sk_live_1\nAPI_TOKEN=tok123\n");
        const QStringList secrets = FloStudio::Security::envSecrets(env);
        for (const char *expected : {"base64:Zm9vYmFyYmF6", "pa ss", "mailpw", "sk_live_1", "tok123"})
            QVERIFY2(secrets.contains(QString::fromLatin1(expected)), expected);
        QVERIFY(!secrets.contains(QStringLiteral("FloCMS"))); // APP_NAME isn't one
    }

    void accountsAndTheStore()
    {
        QCOMPARE(FloStudio::Security::databaseAccount(QStringLiteral(" 127.0.0.1 "), 3306, QStringLiteral("root")),
                 QStringLiteral("mysql://root@127.0.0.1:3306"));
        // Tests never touch the real store
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(!FloStudio::Security::secureStoreAvailable());
        QObject context;
        bool done = false;
        QString code;
        FloStudio::Security::readPassword(QStringLiteral("mysql://x@y:1"), &context, [&](const FloStudio::Core::Result<QString> &r) {
            done = true;
            code = r ? QString() : r.error().code;
        });
        QVERIFY(!done); // from the event loop
        QTRY_VERIFY(done);
        QCOMPARE(code, QStringLiteral("keychain_unavailable"));
    }
};

QTEST_GUILESS_MAIN(TestSecurity)
#include "tst_security.moc"
