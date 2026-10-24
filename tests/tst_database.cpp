#include <flostudio/database/library.h>

#include <flostudio/core/buildinfo.h>

#include <QTest>

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void libraryName()
    {
        QCOMPARE(FloStudio::Database::libraryName(), QStringLiteral("FloStudioDatabase"));
    }

    // D5: Database uses Core, and Core comes through that link only.
    void dependsOnCore()
    {
        QCOMPARE(FloStudio::Database::dependencies(), QStringList{FloStudio::Core::libraryName()});
    }
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
