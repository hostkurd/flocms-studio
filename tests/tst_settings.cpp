#include <flostudio/core/settings.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

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
        QCOMPARE(loadSettings(dir.path()).value(), Settings{});
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
        QCOMPARE(loadSettings(dir.path()).value(), settings);
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
        const Settings settings = loadSettings(dir.path()).value();
        QCOMPARE(settings.language, QStringLiteral("system"));
        QCOMPARE(settings.theme, QStringLiteral("system"));
        QCOMPARE(settings.serverPort, 8000);
        QCOMPARE(settings.editorCommand, Settings{}.editorCommand);
        QCOMPARE(settings.projects, QStringList{QStringLiteral("a")});
        QCOMPARE(settings.serverNetwork, false);
    }

    void floServerSettings()
    {
        // Design D14: off by default; saved and loaded
        QCOMPARE(Settings{}.useFloServer, false);
        const QTemporaryDir dir;
        Settings settings;
        settings.useFloServer = true;
        settings.phpPath = QStringLiteral("/usr/bin/php8.3"); // kept: turning FloServer off brings it back
        QVERIFY(saveSettings(settings, dir.path()).ok());
        QCOMPARE(loadSettings(dir.path()).value(), settings);
        // Only true and false count
        write(dir.path(), R"({"useFloServer":"yes"})");
        const Settings loaded = loadSettings(dir.path()).value();
        QCOMPARE(loaded.useFloServer, false);
        QVERIFY(!loaded.unknown.contains(QStringLiteral("useFloServer")));
    }

    void databaseServerSettings()
    {
        // MariaDB design note D9, D5-D8
        QCOMPARE(Settings{}.databaseServer, QStringLiteral("auto"));
        QCOMPARE(Settings{}.databasePort, 3306);
        QCOMPARE(Settings{}.databaseAutoStart, false);
        const QTemporaryDir dir;
        Settings settings;
        settings.databaseServer = QStringLiteral("found");
        settings.databaseFound = QStringLiteral("D:\\xampp\\mysql\\bin\\mysql.exe");
        settings.databaseSeries = QStringLiteral("12.3");
        settings.databasePort = 3307;
        settings.databaseAutoStart = true;
        settings.databaseHost = QStringLiteral("db.local");
        settings.databaseOtherPort = 3310;
        settings.databaseUser = QStringLiteral("dev");
        settings.databaseProjects = {QStringLiteral("C:\\Sites\\shop")};
        settings.hiddenProjects = {QStringLiteral("C:\\Sites\\old"), QStringLiteral(" "), QStringLiteral("C:\\Sites\\old")};
        QVERIFY(saveSettings(settings, dir.path()).ok());
        settings.hiddenProjects = {QStringLiteral("C:\\Sites\\old")}; // blanks and twice: once
        QCOMPARE(loadSettings(dir.path()).value(), settings);

        write(dir.path(), R"({"databaseServer":"found","databaseFound":"  ","databaseSeries":"latest",
                              "databasePort":0,"databaseOtherPort":"x","databaseHost":" ","databaseUser":"",
                              "databaseAutoStart":"yes"})");
        const Settings clean = loadSettings(dir.path()).value();
        QCOMPARE(clean.databaseServer, QStringLiteral("auto")); // found, but nothing found
        QCOMPARE(clean.databaseSeries, QString());
        QCOMPARE(clean.databasePort, 3306);
        QCOMPARE(clean.databaseOtherPort, 3306);
        QCOMPARE(clean.databaseHost, QStringLiteral("127.0.0.1"));
        QCOMPARE(clean.databaseUser, QStringLiteral("root"));
        QCOMPARE(clean.databaseAutoStart, false);
    }

    void keysFromANewerStudioAreKept()
    {
        const QTemporaryDir dir;
        write(dir.path(), R"({"language":"ar","sidebarWidth":240,"later":{"x":[1,2]}})");
        const Settings settings = loadSettings(dir.path()).value();
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
        for (const QByteArray &text : {QByteArray("{not json"), QByteArray("{again")}) {
            write(dir.path(), text);
            QCOMPARE(loadSettings(dir.path()).value(), Settings{});
        }
        // Both kept, each under its own name
        const QStringList kept = QDir(dir.path()).entryList({QStringLiteral("settings.broken-*.json")});
        QCOMPARE(kept.size(), 2);
        QStringList texts;
        for (const QString &name : kept) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            texts << QString::fromUtf8(file.readAll());
        }
        texts.sort();
        QCOMPARE(texts, (QStringList{QStringLiteral("{again"), QStringLiteral("{not json")}));
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("settings.json"))));
    }

    void anUnreadableFileIsNotReplaced()
    {
#ifdef Q_OS_UNIX
        if (::geteuid() == 0)
            QSKIP("root can read any file");
#endif
        const QTemporaryDir dir;
        write(dir.path(), R"({"language":"ar"})");
        QFile file(dir.filePath(QStringLiteral("settings.json")));
        if (!file.setPermissions(QFileDevice::Permissions()) || file.open(QIODevice::ReadOnly))
            QSKIP("the file can't be made unreadable for its owner here (Windows)");
        QCOMPARE(loadSettings(dir.path()).error().code, QStringLiteral("settings_read"));
        QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    }

    void aJsonArrayIsBrokenToo()
    {
        const QTemporaryDir dir;
        write(dir.path(), "[1,2]");
        QCOMPARE(loadSettings(dir.path()).value(), Settings{});
        QCOMPARE(QDir(dir.path()).entryList({QStringLiteral("settings.broken-*.json")}).size(), 1);
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
