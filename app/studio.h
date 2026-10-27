#pragma once

#include "logmodel.h"
#include "mariadbserver.h"
#include "newproject.h"
#include "projectchecks.h"
#include "projectgenerate.h"
#include "projectpackages.h"
#include "projectsmodel.h"

#include <flostudio/core/downloads.h>
#include <flostudio/core/settings.h>
#include <flostudio/core/tools.h>

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class Language;
class ProjectServer;

// The app's state for the screens (D7): settings, the project list and the
// open project, the tools on this computer, and tool downloads. QML only
// lays this out and calls the Q_INVOKABLEs. Registered as the QML
// singleton FloStudio.Backend/Studio.
//
// A demo (--demo FILE) fills everything from a JSON file instead, finds no
// tools and saves nothing: CI uses it for screenshots of every screen.
class Studio : public QObject
{
    Q_OBJECT
    // Settings
    Q_PROPERTY(QString language READ language NOTIFY settingsChanged)
    Q_PROPERTY(QString theme READ theme NOTIFY settingsChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY darkChanged)
    Q_PROPERTY(int serverPort READ serverPort NOTIFY settingsChanged)
    Q_PROPERTY(bool serverNetwork READ serverNetwork NOTIFY settingsChanged)
    Q_PROPERTY(QString editorCommand READ editorCommand NOTIFY settingsChanged)
    Q_PROPERTY(QString phpPath READ phpPath NOTIFY settingsChanged)
    Q_PROPERTY(QString composerPath READ composerPath NOTIFY settingsChanged)
    Q_PROPERTY(QStringList projectFolders READ projectFolders NOTIFY settingsChanged)
    Q_PROPERTY(QString settingsFile READ settingsFile CONSTANT)
    // An error the screen shows until dismissed: {code, detail}
    Q_PROPERTY(QVariantMap problem READ problem NOTIFY problemChanged)
    // The project "Delete project…" is working on, empty when none
    Q_PROPERTY(QString deleting READ deleting NOTIFY deletingChanged)
    // Pages and projects
    Q_PROPERTY(QString page READ page NOTIFY pageChanged)          // welcome, project, settings, new, about
    Q_PROPERTY(QString settingsSection READ settingsSection NOTIFY pageChanged)
    Q_PROPERTY(ProjectsModel *projects READ projects CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    // The open project's server (stage 4): {state, url, networkUrls,
    // startedAt, php, error, detail}; state: stopped, starting, running,
    // stopping, failed
    Q_PROPERTY(QVariantMap server READ server NOTIFY serverChanged)
    Q_PROPERTY(LogModel *logs READ logs NOTIFY serverChanged)
    // The open project's Doctor and Database (stage 5)
    Q_PROPERTY(ProjectChecks *checks READ checks NOTIFY currentChanged)
    // The open project's Generate tab (stage 7)
    Q_PROPERTY(ProjectGenerate *generate READ generate NOTIFY currentChanged)
    // The open project's Packages tab (stage 8)
    Q_PROPERTY(ProjectPackages *packagesTab READ packagesTab NOTIFY currentChanged)
    // A command that changes a database or caches still runs (quitting asks)
    Q_PROPERTY(bool commandRunning READ commandRunning NOTIFY commandRunningChanged)
    // The new project wizard (stage 6); page "new" shows it
    Q_PROPERTY(NewProjectWizard *wizard READ wizard CONSTANT)
    // Tools: [{tool, state, version, path, detail}]; state: checking, found,
    // missing, failed, unsupported (PHP older than FloCMS needs), optional
    Q_PROPERTY(QVariantList tools READ tools NOTIFY toolsChanged)
    Q_PROPERTY(bool checkingTools READ checkingTools NOTIFY toolsChanged)
    // The PHPs to choose from in Settings: [{path, version, origin}]; origin:
    // system (found on PATH), studio (downloaded by Studio), chosen (picked)
    Q_PROPERTY(QVariantList phpChoices READ phpChoices NOTIFY toolsChanged)
    Q_PROPERTY(bool toolsMissing READ toolsMissing NOTIFY toolsChanged)
    // Downloads (D15)
    Q_PROPERTY(QVariantList downloadOffer READ downloadOffer NOTIFY downloadsChanged)
    Q_PROPERTY(QVariantList downloads READ downloads NOTIFY downloadsChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadsChanged)
    Q_PROPERTY(QString toolsFolder READ toolsFolder NOTIFY downloadsChanged)
    Q_PROPERTY(bool mariadbFromHomebrew READ mariadbFromHomebrew NOTIFY downloadsChanged)
    // Studio's own MariaDB server (MariaDB design note D5–D12)
    Q_PROPERTY(MariadbServer *mariadb READ mariadb CONSTANT)
    // This computer in the tool list's terms (a demo's own): windows-x86_64, macos-arm64, ...
    Q_PROPERTY(QString platform READ platform NOTIFY settingsChanged)
    // The server new projects use (D9): auto, studio, found, other
    Q_PROPERTY(QString databaseServer READ databaseServer NOTIFY settingsChanged)
    Q_PROPERTY(QString databaseFound READ databaseFound NOTIFY settingsChanged)
    Q_PROPERTY(QString databaseHost READ databaseHost NOTIFY settingsChanged)
    Q_PROPERTY(int databaseOtherPort READ databaseOtherPort NOTIFY settingsChanged)
    Q_PROPERTY(QString databaseUser READ databaseUser NOTIFY settingsChanged)
    Q_PROPERTY(bool demo READ demo CONSTANT)
    // A demo can open on a dialog ("downloads")
    Q_PROPERTY(QString demoDialog READ demoDialog CONSTANT)
    // A demo can open a project tab: overview, generate, database, logs, packages, doctor
    Q_PROPERTY(QString demoTab READ demoTab CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    Studio(Language *language, QObject *parent = nullptr);
    ~Studio() override;
    Studio(const Studio &) = delete;
    Studio &operator=(const Studio &) = delete;

    // Loads the settings and the projects, then looks for the tools (on a
    // worker thread). Not called for a demo.
    void start();
    // A demo instead. Error codes: demo_file.
    FloStudio::Core::Result<FloStudio::Core::Done> startDemo(const QString &file);

    QString language() const { return m_settings.language; }
    QString theme() const { return m_settings.theme; }
    bool dark() const;
    int serverPort() const { return m_settings.serverPort; }
    bool serverNetwork() const { return m_settings.serverNetwork; }
    QString editorCommand() const { return m_settings.editorCommand; }
    QString phpPath() const { return m_settings.phpPath; }
    QString composerPath() const { return m_settings.composerPath; }
    QStringList projectFolders() const { return m_settings.projectFolders; }
    QString settingsFile() const;
    QVariantMap problem() const { return m_problem; }
    QString deleting() const { return m_deleting; }
    QString page() const { return m_page; }
    QString settingsSection() const { return m_settingsSection; }
    ProjectsModel *projects() { return &m_projects; }
    QString currentPath() const { return m_currentPath; }
    QVariantMap current() const { return m_current; }
    QVariantMap server() const;
    LogModel *logs() const;
    ProjectChecks *checks() const { return m_checks.value(m_currentPath); }
    ProjectGenerate *generate() const { return m_generate.value(m_currentPath); }
    ProjectPackages *packagesTab() const { return m_packagesTab.value(m_currentPath); }
    bool commandRunning() const;
    NewProjectWizard *wizard() { return &m_wizard; }
    QVariantList tools() const { return m_tools; }
    bool checkingTools() const { return m_checking; }
    QVariantList phpChoices() const { return m_phpChoices; }
    bool toolsMissing() const;
    QVariantList downloadOffer() const;
    QVariantList downloads() const { return m_downloads; }
    bool downloading() const;
    QString toolsFolder() const;
    bool mariadbFromHomebrew() const;
    MariadbServer *mariadb() { return &m_mariadb; }
    QString platform() const { return m_platform; }
    QString databaseServer() const { return m_settings.databaseServer; }
    QString databaseFound() const { return m_settings.databaseFound; }
    QString databaseHost() const { return m_settings.databaseHost; }
    int databaseOtherPort() const { return m_settings.databaseOtherPort; }
    QString databaseUser() const { return m_settings.databaseUser; }
    bool demo() const { return m_demo; }
    QString demoDialog() const { return m_demoDialog; }
    QString demoTab() const { return m_demoTab; }
    static QString version();

    Q_INVOKABLE void setLanguage(const QString &code);
    Q_INVOKABLE void setTheme(const QString &theme);
    Q_INVOKABLE void setServerPort(int port);
    Q_INVOKABLE void setServerNetwork(bool network);
    Q_INVOKABLE void setEditorCommand(const QString &command);
    Q_INVOKABLE void setPhpPath(const QString &path);
    Q_INVOKABLE void setComposerPath(const QString &path);
    // D9: studio, found (with the found server's client program), other
    Q_INVOKABLE void setDatabaseServer(const QString &kind, const QString &found = {});
    Q_INVOKABLE void setOtherDatabase(const QString &host, int port, const QString &user);
    Q_INVOKABLE void addProjectFolder(const QUrl &folder);
    Q_INVOKABLE void removeProjectFolder(const QString &folder);
    // "Open existing folder…": a FloCMS project is added and opened;
    // anything else is a problem (not_flocms).
    Q_INVOKABLE void addProject(const QUrl &folder);
    // "Remove from Studio": the project leaves the list (its website
    // server stops); its files stay. One found in a project folder stays
    // hidden until it is opened again with "Open existing folder…".
    Q_INVOKABLE void removeProject(const QString &path);
    // What "Delete project…" can do besides moving the folder:
    // {database, user, onStudioServer (its database is on Studio's own
    // MariaDB, made by the wizard, and no other project uses it),
    // databaseShared (it is, but another project uses it), configured}
    Q_INVOKABLE QVariantMap deleteChoices(const QString &path) const;
    // "Delete project…": its website server stopped, the folder moved to the
    // Recycle Bin / Trash, the project removed from Studio; with database,
    // its database on Studio's server dropped too (its user stays).
    // Problems: delete_running, project_busy, delete_server, delete_link,
    // delete_protected, trash_failed, db_drop.
    Q_INVOKABLE void deleteProject(const QString &path, bool database);
    Q_INVOKABLE void selectProject(const QString &path);
    Q_INVOKABLE void showSettings(const QString &section = QStringLiteral("general"));
    Q_INVOKABLE void showAbout();
    Q_INVOKABLE void showProjects();
    Q_INVOKABLE void showNewProject();
    // From the wizard ("Change in Settings"): New project keeps its form
    Q_INVOKABLE void showSettingsFromWizard(const QString &section);
    // The wizard made its project: open it, and the wizard starts again
    Q_INVOKABLE void openNewProject();
    Q_INVOKABLE void refreshProjects();
    Q_INVOKABLE void refreshTools();
    Q_INVOKABLE void openInEditor(const QString &path);
    Q_INVOKABLE void openFolder(const QString &path);
    Q_INVOKABLE void dismissProblem();
    // The open project's server and log
    Q_INVOKABLE void startServer();
    Q_INVOKABLE void stopServer();
    Q_INVOKABLE void openSite();
    Q_INVOKABLE void openAdmin();
    Q_INVOKABLE void openLogFile();
    // The open project's .env in the editor (Settings > Editor), else the
    // system's program for it
    Q_INVOKABLE void openEnvFile();
    // An address of the running site (Open in admin)
    Q_INVOKABLE void openUrl(const QString &url);
    // A hostkurd package's page on GitHub (its changelog): only those
    Q_INVOKABLE void openRepository(const QString &url);
    // Memory management: frees what hasn't been used for a while (the
    // least recently used first, never what is shown or busy). Screens
    // hidden for kScreenIdleMs go (QML ShownOnce, on releaseIdle); the
    // checks, package results and stopped server (with its log lines) of a
    // project not shown for kProjectIdleMs go, and are read again when it
    // is opened. A project with a running server or command, its
    // generator forms and the project list stay. `now` frees at once
    // whatever is idle at all (the memory test).
    static constexpr int kScreenIdleMs = 5 * 60 * 1000;
    static constexpr qint64 kProjectIdleMs = qint64(15) * 60 * 1000;
    void releaseUnused(bool now);
    // A screen freed itself on releaseIdle (ShownOnce)
    Q_INVOKABLE void screenReleased() { m_freed = true; }
    // Milliseconds since Studio started, on a clock that a change of the
    // computer's time or a sleep doesn't move (ShownOnce's hidden time)
    Q_INVOKABLE qint64 uptimeMs() const { return m_clock.elapsed(); }
    // How many projects have their data loaded (tests)
    int loadedProjects() const;
    // The project tab to show (the memory test opens each one)
    void requestTab(const QString &name);
    QStringList projectPaths() const;

    // ids from downloadOffer
    Q_INVOKABLE void startDownloads(const QStringList &ids);
    Q_INVOKABLE void cancelDownloads();
    Q_INVOKABLE void clearDownloads();
    // A local file the user picked (FileDialog) as a native path.
    Q_INVOKABLE static QString localPath(const QUrl &url);

signals:
    void settingsChanged();
    void darkChanged();
    void problemChanged();
    void deletingChanged();
    void pageChanged();
    void currentChanged();
    void toolsChanged();
    void downloadsChanged();
    void serverChanged();
    void commandRunningChanged();
    // QML screens hidden long enough (or at all, with `now`) are freed
    void releaseIdle(bool now, int screenIdleMs);
    void tabRequested(const QString &name);
    // Something was freed a moment ago: main.cpp runs the QML engine's
    // garbage collector, then gives the memory back to the system
    void memoryFreed();

private:
    void changeSettings(const std::function<void(FloStudio::Core::Settings &)> &change);
    void setProblem(const QString &code, const QString &detail);
    void loadCurrent();
    void downloadNext();
    void saveMariadbSettings();
    ProjectServer *serverFor(const QString &path);
    bool projectBusy(const QString &path) const;
    // Off the list: settings, the open page, its objects; a server stopped
    void forgetProject(const QString &path, bool deleted);
    // Then, once its website server has stopped
    void stopServerThen(const QString &path, const std::function<void()> &next);
    void trashProject(const QString &path, const QString &dropSql);
    void dropDatabase(const QString &path, const QString &sql);
    void forgetDatabaseOf(const QString &path);
    void deleteDone(const QString &code = {}, const QString &detail = {});
    ProjectChecks *checksFor(const QString &path);
    ProjectGenerate *generateFor(const QString &path);
    ProjectPackages *packagesFor(const QString &path);
    bool currentHasAdmin() const;
    void serversChanged();
    void askDatabasePort();
    void forgetIdle();

    Language *m_language;
    FloStudio::Core::Settings m_settings;
    bool m_canSave = true; // false when settings.json could not be read, or in a demo
    bool m_demo = false;
    QVariantMap m_problem;
    QString m_page = QStringLiteral("welcome");
    QString m_settingsSection = QStringLiteral("general");
    ProjectsModel m_projects;
    QString m_currentPath;
    QVariantMap m_current;
    QVariantList m_tools;
    QVariantList m_phpChoices;
    bool m_checking = false;
    bool m_recheck = false;
    // Another MariaDB (XAMPP, Laragon…) started or stopped after the tools
    // were looked for: port 3306 looked at every few seconds (without
    // connecting). Only the newest look counts: each one, and each full
    // look for the tools, takes the next number.
    QTimer m_databasePoll;
    // Memory management: a sweep every minute; when each project was last
    // shown (m_clock's time)
    QTimer m_sweep;
    QElapsedTimer m_clock;
    QHash<QString, qint64> m_lastShown;
    bool m_freed = false; // this sweep freed a screen or a project's data
    quint64 m_databaseLook = 0;
    QString m_mariadbState; // Studio's own server's last state, to see it stop
    QList<FloStudio::Core::ToolPackage> m_packages;
    QVariantList m_downloads;
    QStringList m_queue;
    QObject *m_install = nullptr; // a child of this; cleared when it reports back
    bool m_closing = false;
    QString m_platform = FloStudio::Core::currentPlatform();
    MariadbServer m_mariadb;
    QString m_deleting;
    QMetaObject::Connection m_deleteWait; // waiting for a server to stop or start
    bool m_autoStartDone = false;
    bool m_keepWizard = false;
    QString m_demoDialog;
    QVariantMap m_demoDeleteChoices; // a demo's "Delete project…" choices
    QString m_demoTab;
    QStringList m_demoDownloaded;
    QString m_demoToolsFolder;
    QHash<QString, ProjectServer *> m_servers; // children of this, one per project opened
    QHash<QString, ProjectChecks *> m_checks;  // children of this, one per project opened
    QHash<QString, ProjectGenerate *> m_generate; // the same
    QHash<QString, ProjectPackages *> m_packagesTab; // the same
    NewProjectWizard m_wizard{&m_settings};
};
