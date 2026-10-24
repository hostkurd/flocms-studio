#include <flostudio/security/library.h>

#include <flostudio/core/buildinfo.h>

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
};

QTEST_GUILESS_MAIN(TestSecurity)
#include "tst_security.moc"
