#pragma once

#include "projectsmodel.h"

#include <flostudio/core/downloads.h>
#include <flostudio/core/settings.h>
#include <flostudio/core/tools.h>

#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class Language;

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
    // Pages and projects
    Q_PROPERTY(QString page READ page NOTIFY pageChanged)          // welcome, project, settings
    Q_PROPERTY(QString settingsSection READ settingsSection NOTIFY pageChanged)
    Q_PROPERTY(ProjectsModel *projects READ projects CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
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
    Q_PROPERTY(bool demo READ demo CONSTANT)
    // A demo can open on a dialog ("downloads")
    Q_PROPERTY(QString demoDialog READ demoDialog CONSTANT)
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
    QString page() const { return m_page; }
    QString settingsSection() const { return m_settingsSection; }
    ProjectsModel *projects() { return &m_projects; }
    QString currentPath() const { return m_currentPath; }
    QVariantMap current() const { return m_current; }
    QVariantList tools() const { return m_tools; }
    bool checkingTools() const { return m_checking; }
    QVariantList phpChoices() const { return m_phpChoices; }
    bool toolsMissing() const;
    QVariantList downloadOffer() const;
    QVariantList downloads() const { return m_downloads; }
    bool downloading() const;
    QString toolsFolder() const;
    bool mariadbFromHomebrew() const;
    bool demo() const { return m_demo; }
    QString demoDialog() const { return m_demoDialog; }
    static QString version();

    Q_INVOKABLE void setLanguage(const QString &code);
    Q_INVOKABLE void setTheme(const QString &theme);
    Q_INVOKABLE void setServerPort(int port);
    Q_INVOKABLE void setServerNetwork(bool network);
    Q_INVOKABLE void setEditorCommand(const QString &command);
    Q_INVOKABLE void setPhpPath(const QString &path);
    Q_INVOKABLE void setComposerPath(const QString &path);
    Q_INVOKABLE void addProjectFolder(const QUrl &folder);
    Q_INVOKABLE void removeProjectFolder(const QString &folder);
    // "Open existing folder…": a FloCMS project is added and opened;
    // anything else is a problem (not_flocms).
    Q_INVOKABLE void addProject(const QUrl &folder);
    Q_INVOKABLE void removeProject(const QString &path);
    Q_INVOKABLE void selectProject(const QString &path);
    Q_INVOKABLE void showSettings(const QString &section = QStringLiteral("general"));
    Q_INVOKABLE void showProjects();
    Q_INVOKABLE void refreshProjects();
    Q_INVOKABLE void refreshTools();
    Q_INVOKABLE void openInEditor(const QString &path);
    Q_INVOKABLE void openFolder(const QString &path);
    Q_INVOKABLE void dismissProblem();
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
    void pageChanged();
    void currentChanged();
    void toolsChanged();
    void downloadsChanged();

private:
    void changeSettings(const std::function<void(FloStudio::Core::Settings &)> &change);
    void setProblem(const QString &code, const QString &detail);
    void loadCurrent();
    void downloadNext();

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
    QList<FloStudio::Core::ToolPackage> m_packages;
    QVariantList m_downloads;
    QStringList m_queue;
    QPointer<QObject> m_install;
    QString m_platform = FloStudio::Core::currentPlatform();
    QString m_demoDialog;
    QString m_demoToolsFolder;
};
