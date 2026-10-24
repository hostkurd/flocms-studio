#include <flostudio/core/settings.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

using namespace FloStudio::Core;

class TestSettings : public QObject
{
    Q_OBJECT

    static void write(const QString &dir, const QByteArray &text)
    {
        QFile file(dir + QStringLiteral("/settings.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text);
    }

private slots:
    void missingFileGivesDefaults()
    {
        const QTemporaryDir dir;
        QCOMPARE(loadSettings(dir.path()), Settings{});
        QCOMPARE(Settings{}.serverPort, 8000);
        QCOMPARE(Settings{}.editorCommand, QStringLiteral("code \"{folder}\""));
    }

    void savesAndLoads()
    {
        const QTemporaryDir dir;
        Settings settings;
        settings.language = QStringLiteral("ckb");
        settings.theme = QStringLiteral("dark");
        settings.phpPath = QStringLiteral("C:\\php\\php.exe");
        settings.projectFolders = {QStringLiteral("C:\\Sites"), QStringLiteral("/Users/ali/سایتەکان")};
        settings.serverPort = 8080;
        settings.serverNetwork = true;
        QVERIFY(saveSettings(settings, dir.path()).ok());
        QCOMPARE(loadSettings(dir.path()), settings);
    }

    void savingCreatesTheFolder()
    {
        const QTemporaryDir dir;
        const QString nested = dir.path() + QStringLiteral("/a/b");
        QVERIFY(saveSettings(Settings{}, nested).ok());
        QVERIFY(QFile::exists(nested + QStringLiteral("/settings.json")));
    }

    void unknownValuesFallBack()
    {
        const QTemporaryDir dir;
        write(dir.path(), R"({"language":"fr","theme":"blue","serverPort":70000,"editorCommand":"  ",
                              "projects":["a"," a ","", 5],"serverNetwork":"yes"})");
        const Settings settings = loadSettings(dir.path());
        QCOMPARE(settings.language, QStringLiteral("system"));
        QCOMPARE(settings.theme, QStringLiteral("system"));
        QCOMPARE(settings.serverPort, 8000);
        QCOMPARE(settings.editorCommand, Settings{}.editorCommand);
        QCOMPARE(settings.projects, QStringList{QStringLiteral("a")});
        QCOMPARE(settings.serverNetwork, false);
    }

    void keysFromANewerStudioAreKept()
    {
        const QTemporaryDir dir;
        write(dir.path(), R"({"language":"ar","sidebarWidth":240,"later":{"x":[1,2]}})");
        const Settings settings = loadSettings(dir.path());
        QCOMPARE(settings.language, QStringLiteral("ar"));
        QVERIFY(saveSettings(settings, dir.path()).ok());
        QFile file(dir.path() + QStringLiteral("/settings.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(json.value(QStringLiteral("sidebarWidth")).toInt(), 240);
        QCOMPARE(json.value(QStringLiteral("later")).toObject().value(QStringLiteral("x")).toArray().size(), 2);
        QCOMPARE(json.value(QStringLiteral("language")).toString(), QStringLiteral("ar"));
    }

    void brokenFileIsKeptAside()
    {
        const QTemporaryDir dir;
        write(dir.path(), "{not json");
        QCOMPARE(loadSettings(dir.path()), Settings{});
        QFile broken(dir.path() + QStringLiteral("/settings.json.broken"));
        QVERIFY(broken.open(QIODevice::ReadOnly));
        QCOMPARE(broken.readAll(), QByteArray("{not json"));
        QVERIFY(!QFile::exists(dir.path() + QStringLiteral("/settings.json")));
    }

    void aJsonArrayIsBrokenToo()
    {
        const QTemporaryDir dir;
        write(dir.path(), "[1,2]");
        QCOMPARE(loadSettings(dir.path()), Settings{});
        QVERIFY(QFile::exists(dir.path() + QStringLiteral("/settings.json.broken")));
    }

    void normalizedTrimsAndDedupes()
    {
        Settings settings;
        settings.phpPath = QStringLiteral("  /usr/bin/php ");
        settings.projectFolders = {QStringLiteral("/a"), QStringLiteral("/a"), QStringLiteral(" /b"), QString()};
        settings.serverPort = 0;
        const Settings clean = normalized(settings);
        QCOMPARE(clean.phpPath, QStringLiteral("/usr/bin/php"));
        QCOMPARE(clean.projectFolders, (QStringList{QStringLiteral("/a"), QStringLiteral("/b")}));
        QCOMPARE(clean.serverPort, 8000);
    }

    void folderIsPerUser()
    {
        QVERIFY(!settingsFolder().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
