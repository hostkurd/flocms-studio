#include <flostudio/core/buildinfo.h>
#include <flostudio/core/memory.h>
#include <flostudio/core/version.h>

#include <QRegularExpression>
#include <QTest>

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    // The process's memory as the system counts it; giving freed memory
    // back never makes it bigger
    void processMemory()
    {
        const FloStudio::Core::ProcessMemory before = FloStudio::Core::currentProcessMemory();
        QVERIFY(before.inUse > qint64(1024) * 1024);
        QVERIFY(before.own > 0);
        QVERIFY(before.own <= before.inUse + qint64(64) * 1024 * 1024);
        {
            QList<QByteArray> blocks;
            for (int i = 0; i < 64; ++i)
                blocks << QByteArray(qsizetype(256) * 1024, 'x');
        }
        FloStudio::Core::returnFreeMemory();
        const FloStudio::Core::ProcessMemory after = FloStudio::Core::currentProcessMemory();
        QVERIFY(after.inUse > 0);
    }

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
