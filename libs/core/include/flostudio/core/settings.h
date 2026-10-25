#pragma once

#include <flostudio/core/export.h>
#include <flostudio/core/result.h>

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace FloStudio::Core {

// Studio's own settings, kept as settings.json in the per-user config
// folder. Every field has a default, so a missing file, a missing key or a
// value Studio doesn't know all give a working Studio.
struct Settings
{
    QString language = QStringLiteral("system"); // system, en, ar, ckb
    QString theme = QStringLiteral("system");    // system, light, dark

    // The tools to run. Empty: found on the search path (see tools.h).
    QString phpPath;
    QString composerPath;

    // How to open a project in an editor. {folder} becomes the project's
    // folder; without it the folder is added as the last argument.
    QString editorCommand = QStringLiteral("code \"{folder}\"");

    QStringList projectFolders; // folders whose sub-folders are scanned
    QStringList projects;       // projects added one by one

    int serverPort = 8000;      // the first port `php flo serve` tries
    bool serverNetwork = false; // listen on 0.0.0.0 instead of 127.0.0.1

    // The database server new projects use (MariaDB design note D9):
    // auto (Studio's when it has one, else the one found on this computer),
    // studio, found (databaseFound: that server's client program), other.
    QString databaseServer = QStringLiteral("auto");
    QString databaseFound;
    // Studio's own server (D5–D8): the series (empty: the newest installed),
    // its port, and whether it starts when Studio opens.
    QString databaseSeries;
    int databasePort = 3306;
    bool databaseAutoStart = false;
    // The projects whose database is on Studio's server (the wizard made
    // it there): the Database tab offers to start the server only for them
    QStringList databaseProjects;
    // "Another server": its address and user (the password is in the
    // system's secure store).
    QString databaseHost = QStringLiteral("127.0.0.1");
    int databaseOtherPort = 3306;
    QString databaseUser = QStringLiteral("root");

    // Keys this version doesn't know, written back unchanged, so a newer
    // Studio's settings survive a run of an older one.
    QJsonObject unknown;

    bool operator==(const Settings &) const = default;
};

// The folder settings.json lives in (QStandardPaths::AppConfigLocation).
FLOSTUDIOCORE_EXPORT QString settingsFolder();

// Loads dir/settings.json. A missing file gives the defaults. A file that
// isn't valid JSON is kept aside as settings.broken-<date-time>.json and
// the defaults are used. A file that is there but can't be read (another
// program holds it) gives settings_read: don't save over it then.
FLOSTUDIOCORE_EXPORT Result<Settings> loadSettings(const QString &dir);

// Writes dir/settings.json in one step (a crash leaves the old file).
// Error codes: settings_write.
FLOSTUDIOCORE_EXPORT Result<Done> saveSettings(const Settings &settings, const QString &dir);

// Values Studio doesn't accept become the defaults; lists lose blank
// entries and duplicates (same folder written twice).
FLOSTUDIOCORE_EXPORT Settings normalized(Settings settings);

FLOSTUDIOCORE_EXPORT QJsonObject toJson(const Settings &settings);
FLOSTUDIOCORE_EXPORT Settings settingsFromJson(const QJsonObject &json);

} // namespace FloStudio::Core
