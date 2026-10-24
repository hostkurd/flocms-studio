#include <flostudio/core/buildinfo.h>
#include <flostudio/core/version.h>

#include <QRegularExpression>
#include <QTest>

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void versionComesFromCMake()
    {
        const auto info = FloStudio::Core::currentBuildInfo();
        QCOMPARE(info.version, QStringLiteral(FLOSTUDIO_VERSION));
        QVERIFY(QRegularExpression(QStringLiteral("^\\d+\\.\\d+\\.\\d+$")).match(info.version).hasMatch());
    }

    void qtVersionsAreKnown()
    {
        const auto info = FloStudio::Core::currentBuildInfo();
        QCOMPARE(info.qtBuilt, QStringLiteral(QT_VERSION_STR));
        QCOMPARE(info.qtRunning, QString::fromLatin1(qVersion()));
        QVERIFY(!info.compiler.isEmpty());
        QVERIFY(!info.architecture.isEmpty());
    }

    void textHasOneLinePerPart()
    {
        const QString text = FloStudio::Core::toText(FloStudio::Core::currentBuildInfo());
        const QStringList lines = text.split(QLatin1Char('\n'));
        QCOMPARE(lines.size(), 4);
        QVERIFY(lines.at(0).startsWith(QStringLiteral("FloCMS Studio ")));
        QVERIFY(lines.at(1).startsWith(QStringLiteral("Qt ")));
    }

    void libraryName()
    {
        QCOMPARE(FloStudio::Core::libraryName(), QStringLiteral("FloStudioCore"));
    }
};

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"
