#include "studio.h"

#include <flostudio/core/preview.h>

#include "language.h"
#include "projectserver.h"

#include <flostudio/core/buildinfo.h>
#include <flostudio/core/dbserver.h>
#include <flostudio/core/process.h>
#include <flostudio/core/project.h>
#include <flostudio/core/server.h>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QScopeGuard>
#include <QTimer>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStyleHints>
#include <QtConcurrent>

#include <algorithm>
#include <memory>
#include <utility>

using namespace FloStudio::Core;

namespace {

// What the worker thread finds; turned into the screens' lists on the
// interface thread.
struct Found
{
    PhpStatus php;
    ToolStatus composer;
    DatabaseServerStatus database;
    QVariantList phpChoices;
};

Found findTools(const Settings &settings, const QList<ToolPackage> &packages)
{
    Found found;
    found.php = detectPhp(settings);
    found.composer = detectComposer(settings);
    found.database = detectDatabaseServer();

    // The PHPs Settings offers: the one on PATH, Studio's downloads, and
    // the one chosen by hand. Each is asked for its version.
    QStringList seen;
    const auto add = [&](const QString &path, const QString &origin) {
        if (path.isEmpty() || seen.contains(path))
            return;
        seen << path;
        Settings only;
        only.phpPath = path;
        const PhpStatus php = detectPhp(only);
        found.phpChoices << QVariantMap{{QStringLiteral("path"), path},
                                        {QStringLiteral("version"), php.info.version},
                                        {QStringLiteral("origin"), origin}};
    };
    // A plain `php` may be one Studio downloaded (its folders are last on
    // the search path): then it is shown as Studio's
    const QString onPath = findTool(QStringLiteral("php"));
    const bool fromStudio = !onPath.isEmpty()
                            && QDir::cleanPath(QDir::fromNativeSeparators(onPath))
                                   .startsWith(QDir::cleanPath(toolsFolder()) + u'/');
    if (!fromStudio)
        add(onPath, QStringLiteral("system"));
    for (const ToolPackage &package : packages) {
        if (package.tool == u"php")
            add(installedProgram(package), QStringLiteral("studio"));
    }
    add(settings.phpPath.isEmpty() ? QString() : findTool(settings.phpPath), QStringLiteral("chosen"));
    return found;
}

QVariantMap toolRow(const QString &tool, const ToolStatus &status, bool supported = true)
{
    QString state = QStringLiteral("found");
    if (!status.found)
        state = QStringLiteral("missing");
    else if (!status.error.isEmpty())
        state = QStringLiteral("failed");
    else if (!supported)
        state = QStringLiteral("unsupported");
    return {{QStringLiteral("tool"), tool},
            {QStringLiteral("state"), state},
            {QStringLiteral("version"), status.version},
            {QStringLiteral("path"), status.path}};
}

QVariantList toolRows(const Found &found)
{
    QVariantMap database{{QStringLiteral("tool"), QStringLiteral("mariadb")},
                         {QStringLiteral("version"), found.database.product.version},
                         {QStringLiteral("product"), found.database.product.name},
                         {QStringLiteral("path"), found.database.clientPath}};
    database.insert(QStringLiteral("state"), found.database.running ? QStringLiteral("running")
                                             : found.database.product.name.isEmpty() ? QStringLiteral("optional")
                                                                                     : QStringLiteral("found"));
    return {toolRow(QStringLiteral("php"), found.php.tool, found.php.supported),
            toolRow(QStringLiteral("composer"), found.composer), database};
}

QVariantList packageList(const QList<Package> &packages)
{
    QVariantList list;
    for (const Package &package : packages)
        list << QVariantMap{{QStringLiteral("name"), package.name}, {QStringLiteral("version"), package.version}};
    return list;
}

QVariantMap infoMap(const ProjectInfo &info)
{
    const DatabaseConfig &db = info.database;
    return {
        {QStringLiteral("name"), info.summary.folderName},
        {QStringLiteral("path"), info.summary.path},
        {QStringLiteral("type"), ProjectsModel::typeKey(projectType(info.summary))},
        {QStringLiteral("appName"), info.appName},
        {QStringLiteral("appEnv"), info.appEnv},
        {QStringLiteral("appUrl"), info.appUrl},
        {QStringLiteral("appDebug"), info.appDebug},
        {QStringLiteral("production"), info.production},
        {QStringLiteral("skeletonVersion"), info.skeletonVersion},
        {QStringLiteral("languages"), info.languages},
        {QStringLiteral("defaultLanguage"), info.defaultLanguage},
        {QStringLiteral("database"),
         QVariantMap{{QStringLiteral("driver"), db.driver},
                     {QStringLiteral("name"), db.usesDsn ? QString() : db.name},
                     {QStringLiteral("host"), db.host},
                     {QStringLiteral("port"), db.port},
                     {QStringLiteral("usesDsn"), db.usesDsn},
                     {QStringLiteral("configured"), db.configured}}},
        {QStringLiteral("packages"), packageList(info.packages)},
        {QStringLiteral("otherPackages"), static_cast<int>(info.otherPackages.size())},
        {QStringLiteral("others"), packageList(info.otherPackages)},
        {QStringLiteral("hasEnv"), info.hasEnv},
        {QStringLiteral("hasVendor"), info.hasVendor},
        {QStringLiteral("hasLock"), info.hasLock},
        {QStringLiteral("duplicateEnvKeys"), info.duplicateEnvKeys},
        {QStringLiteral("envByteOrderMark"), info.envByteOrderMark},
    };
}

QList<ProjectSummary> demoProjects(const QJsonArray &list)
{
    QList<ProjectSummary> projects;
    for (const auto &item : list) {
        const QJsonObject json = item.toObject();
        ProjectSummary project;
        project.path = json.value(QStringLiteral("path")).toString();
        project.folderName = json.value(QStringLiteral("name")).toString();
        const QString home = json.value(QStringLiteral("home")).toString();
        project.home = home == u"admin" ? Home::Admin : home == u"api" ? Home::Api : Home::Site;
        project.hasAdmin = json.value(QStringLiteral("hasAdmin")).toBool();
        project.hasApi = json.value(QStringLiteral("hasApi")).toBool();
        project.missing = json.value(QStringLiteral("missing")).toBool();
        projects << project;
    }
    return projects;
}

} // namespace

Studio::Studio(Language *language, QObject *parent)
    : QObject(parent)
    , m_language(language)
{
    // A new project is listed (in a project folder, or on its own); the
    // wizard's page stays until the person opens it
    m_wizard.setMariadb(&m_mariadb);
    connect(this, &Studio::settingsChanged, &m_wizard, &NewProjectWizard::serverChoiceChanged);
    connect(&m_wizard, &NewProjectWizard::stepsChanged, this, &Studio::commandRunningChanged);
    connect(&m_wizard, &NewProjectWizard::created, this, [this](const QString &path) {
        const QString folderOfIt = QDir::toNativeSeparators(QFileInfo(path).absolutePath());
        const bool inFolder = std::any_of(m_settings.projectFolders.begin(), m_settings.projectFolders.end(),
                                          [&folderOfIt](const QString &folder) { return QDir(folder) == QDir(folderOfIt); });
        if (!inFolder)
            changeSettings([&path](Settings &s) { s.projects << path; });
        refreshProjects();
    });
    connect(&m_wizard, &NewProjectWizard::problem, this, [this](const QString &code, const QString &detail) { setProblem(code, detail); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_settings.theme == u"system")
            emit darkChanged();
    });
#endif
}

Studio::~Studio()
{
    // Nothing more starts; the install (a child) is deleted with this, and
    // its result, which would come from the event loop, never arrives
    m_closing = true;
    m_queue.clear();
    if (m_install)
        cancelToolInstall(m_install);
}

void Studio::start()
{
    m_packages = bundledToolPackages();
    const auto loaded = loadSettings(settingsFolder());
    if (loaded) {
        m_settings = loaded.value();
    } else {
        // Never save over a settings file that is there but can't be read
        m_canSave = false;
        setProblem(loaded.error().code, loaded.error().detail);
    }
    m_language->set(m_settings.language);
    emit settingsChanged();
    emit darkChanged();
    refreshProjects();
    m_mariadb.attach(&m_settings, m_packages, m_platform);
    connect(&m_mariadb, &MariadbServer::settingsChanged, this, &Studio::saveMariadbSettings);
    // "Start the server when Studio opens" (D8), once the programs are known
    connect(&m_mariadb, &MariadbServer::programsFound, this, [this] {
        if (!std::exchange(m_autoStartDone, true) && m_settings.databaseAutoStart)
            m_mariadb.start();
    });
    refreshTools(); // and Studio's MariaDB programs
    m_clock.start();
    m_sweep.setInterval(60 * 1000);
    connect(&m_sweep, &QTimer::timeout, this, [this] { releaseUnused(false); });
    m_sweep.start();
    m_databasePoll.setInterval(5000);
    connect(&m_databasePoll, &QTimer::timeout, this, &Studio::askDatabasePort);
    if (!m_demo)
        m_databasePoll.start();
    // Studio's own server just stopped (or didn't start): look at once, so
    // the row doesn't show it as another server until the next look
    connect(&m_mariadb, &MariadbServer::changed, this, [this] {
        const QString state = m_mariadb.state();
        if (std::exchange(m_mariadbState, state) != state && (state == QLatin1String("stopped") || state == QLatin1String("failed")))
            askDatabasePort();
    });
    // Servers left running by a Studio that crashed (macOS, Linux): asking
    // `ps` takes a moment, so on a worker thread
    auto *leftovers = new QFutureWatcher<QList<LeftoverServer>>(this); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks): a child of this
    connect(leftovers, &QFutureWatcher<QList<LeftoverServer>>::finished, this, [this, leftovers] {
        const QList<LeftoverServer> stopped = leftovers->result();
        leftovers->deleteLater();
        for (const LeftoverServer &left : stopped) {
            if (m_projects.indexOf(left.projectDir) >= 0) // a project still listed
                serverFor(left.projectDir)->noteLeftover(left.group);
        }
    });
    leftovers->setFuture(QtConcurrent::run([folder = settingsFolder()] { return stopLeftoverServers(folder); }));
    // Preview copies a Studio left behind when it closed while previewing
    (void)QtConcurrent::run([] { removeLeftoverPreviews(previewsFolder()); });
}

Result<Done> Studio::startDemo(const QString &file)
{
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly))
        return Error{QStringLiteral("demo_file"), file};
    QJsonParseError error{};
    const QJsonObject demo = QJsonDocument::fromJson(in.readAll(), &error).object();
    if (error.error != QJsonParseError::NoError)
        return Error{QStringLiteral("demo_file"), error.errorString()};
    m_demo = true;
    m_canSave = false;
    m_settings = settingsFromJson(demo.value(QStringLiteral("settings")).toObject());
    m_language->set(m_settings.language);
    m_platform = demo.value(QStringLiteral("platform")).toString(m_platform);
    m_demoDialog = demo.value(QStringLiteral("dialog")).toString();
    m_demoTab = demo.value(QStringLiteral("tab")).toString();
    m_demoToolsFolder = demo.value(QStringLiteral("toolsFolder")).toString();
    // Packages the demo has downloaded already (their ids)
    m_demoDownloaded.clear();
    for (const auto &id : demo.value(QStringLiteral("downloaded")).toArray())
        m_demoDownloaded << id.toString();
    QFile list(QStringLiteral(":/flostudio/core/tools.json"));
    if (list.open(QIODevice::ReadOnly)) {
        const auto packages = toolPackages(list.readAll(), m_platform);
        if (packages)
            m_packages = packages.value();
    }
    m_projects.setProjects(demoProjects(demo.value(QStringLiteral("projects")).toArray()));
    m_mariadb.attach(&m_settings, m_packages, m_platform);
    m_mariadb.showDemo(demo.value(QStringLiteral("mariadb")).toObject().toVariantMap());
    m_tools = demo.value(QStringLiteral("tools")).toArray().toVariantList();
    m_phpChoices = demo.value(QStringLiteral("phpChoices")).toArray().toVariantList();
    m_downloads = demo.value(QStringLiteral("downloads")).toArray().toVariantList();
    m_current = demo.value(QStringLiteral("current")).toObject().toVariantMap();
    m_demoDeleteChoices = demo.value(QStringLiteral("deleteChoices")).toObject().toVariantMap();
    m_currentPath = m_current.value(QStringLiteral("path")).toString();
    m_page = demo.value(QStringLiteral("page")).toString(QStringLiteral("project"));
    m_settingsSection = demo.value(QStringLiteral("settingsSection")).toString(QStringLiteral("general"));
    if (demo.contains(QStringLiteral("problem")))
        m_problem = demo.value(QStringLiteral("problem")).toObject().toVariantMap();
    if (demo.contains(QStringLiteral("wizard")))
        m_wizard.showDemo(demo.value(QStringLiteral("wizard")).toObject().toVariantMap());
    // The open project's Doctor and database, server and log, and which
    // others are running
    if (!m_currentPath.isEmpty()) {
        checksFor(m_currentPath)->showDemo(demo.value(QStringLiteral("checks")).toObject().toVariantMap());
        generateFor(m_currentPath)->showDemo(demo.value(QStringLiteral("generate")).toObject().toVariantMap());
        ProjectPackages *packages = packagesFor(m_currentPath);
        packages->setProject(m_current);
        packages->showDemo(demo.value(QStringLiteral("packages")).toObject().toVariantMap());
    }
    if (!m_currentPath.isEmpty())
        serverFor(m_currentPath)->showDemo(demo.value(QStringLiteral("server")).toObject().toVariantMap(),
                                           demo.value(QStringLiteral("logs")).toArray().toVariantList());
    QSet<QString> running;
    for (const auto &path : demo.value(QStringLiteral("running")).toArray())
        running << path.toString();
    m_projects.setRunning(running);
    if (m_page == u"project")
        emit serverChanged();
    emit settingsChanged();
    emit darkChanged();
    emit currentChanged();
    emit pageChanged();
    emit toolsChanged();
    emit downloadsChanged();
    emit problemChanged();
    return Done{};
}

bool Studio::dark() const
{
    if (m_settings.theme == u"dark")
        return true;
    if (m_settings.theme == u"light")
        return false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

QString Studio::settingsFile() const
{
    return QDir::toNativeSeparators(QDir(settingsFolder()).filePath(QStringLiteral("settings.json")));
}

bool Studio::toolsMissing() const
{
    for (const QVariant &row : m_tools) {
        const QVariantMap tool = row.toMap();
        const QString state = tool.value(QStringLiteral("state")).toString();
        if (tool.value(QStringLiteral("tool")).toString() != u"mariadb" && (state == u"missing" || state == u"unsupported"))
            return true;
    }
    return false;
}

QVariantList Studio::downloadOffer() const
{
    QVariantList offer;
    for (const ToolPackage &package : m_packages) {
        const bool installed = m_demo ? m_demoDownloaded.contains(package.id) : !installedProgram(package).isEmpty();
        offer << QVariantMap{{QStringLiteral("id"), package.id},         {QStringLiteral("tool"), package.tool},
                             {QStringLiteral("version"), package.version}, {QStringLiteral("source"), package.source},
                             {QStringLiteral("size"), package.size},       {QStringLiteral("installed"), installed}};
    }
    return offer;
}

bool Studio::downloading() const
{
    return std::any_of(m_downloads.begin(), m_downloads.end(), [](const QVariant &row) {
        const QString state = row.toMap().value(QStringLiteral("state")).toString();
        return state == u"waiting" || state == u"downloading" || state == u"installing";
    });
}

QString Studio::toolsFolder() const
{
    return m_demo && !m_demoToolsFolder.isEmpty() ? m_demoToolsFolder : QDir::toNativeSeparators(FloStudio::Core::toolsFolder());
}

bool Studio::mariadbFromHomebrew() const
{
    return m_platform.startsWith(QStringLiteral("macos")); // MariaDB builds nothing for macOS: Homebrew has it
}

QString Studio::version()
{
    return currentBuildInfo().version;
}

void Studio::changeSettings(const std::function<void(Settings &)> &change)
{
    Settings next = m_settings;
    change(next);
    next = normalized(next);
    if (next == m_settings)
        return;
    m_settings = next;
    if (m_canSave) {
        if (const auto saved = saveSettings(m_settings, settingsFolder()); !saved)
            setProblem(saved.error().code, saved.error().detail);
    }
    emit settingsChanged();
}

void Studio::saveMariadbSettings()
{
    // MariadbServer changed m_settings itself: normalized and saved here
    m_settings = normalized(m_settings);
    if (m_canSave && !m_demo) {
        if (const auto saved = saveSettings(m_settings, settingsFolder()); !saved)
            setProblem(saved.error().code, saved.error().detail);
    }
    emit settingsChanged();
}

void Studio::setDatabaseServer(const QString &kind, const QString &found)
{
    changeSettings([&](Settings &s) {
        s.databaseServer = kind;
        if (kind == u"found")
            s.databaseFound = found;
    });
}

void Studio::setOtherDatabase(const QString &host, int port, const QString &user)
{
    changeSettings([&](Settings &s) {
        s.databaseHost = host;
        s.databaseOtherPort = port;
        s.databaseUser = user;
    });
}

void Studio::setLanguage(const QString &code)
{
    changeSettings([&code](Settings &s) { s.language = code; });
    m_language->set(m_settings.language);
}

void Studio::setTheme(const QString &theme)
{
    changeSettings([&theme](Settings &s) { s.theme = theme; });
    emit darkChanged();
}

void Studio::setServerPort(int port)
{
    changeSettings([port](Settings &s) { s.serverPort = port; });
}

void Studio::setServerNetwork(bool network)
{
    changeSettings([network](Settings &s) { s.serverNetwork = network; });
}

void Studio::setEditorCommand(const QString &command)
{
    changeSettings([&command](Settings &s) { s.editorCommand = command; });
}

void Studio::setPhpPath(const QString &path)
{
    changeSettings([&path](Settings &s) { s.phpPath = path; });
    refreshTools();
}

void Studio::setComposerPath(const QString &path)
{
    changeSettings([&path](Settings &s) { s.composerPath = path; });
    refreshTools();
}

void Studio::addProjectFolder(const QUrl &folder)
{
    const QString path = localPath(folder);
    if (path.isEmpty())
        return;
    changeSettings([&path](Settings &s) { s.projectFolders << path; });
    refreshProjects();
}

void Studio::removeProjectFolder(const QString &folder)
{
    changeSettings([&folder](Settings &s) { s.projectFolders.removeAll(folder); });
    refreshProjects();
}

void Studio::addProject(const QUrl &folder)
{
    const QString path = localPath(folder);
    if (path.isEmpty())
        return;
    if (!isProject(path)) {
        setProblem(QStringLiteral("not_flocms"), path);
        return;
    }
    // Opened again after "Remove from Studio": shown again
    changeSettings([&path](Settings &s) {
        s.projects << path;
        s.hiddenProjects.removeIf([&path](const QString &hidden) { return sameFolder(hidden, path); });
    });
    refreshProjects();
    selectProject(projectSummary(path).path);
}

bool Studio::projectBusy(const QString &path) const
{
    const ProjectGenerate *generate = m_generate.value(path);
    const ProjectPackages *packages = m_packagesTab.value(path);
    const ProjectChecks *checks = m_checks.value(path);
    return (checks && checks->busy()) || (generate && generate->busy()) || (packages && packages->busy());
}

void Studio::removeProject(const QString &path)
{
    if (!m_deleting.isEmpty()) {
        setProblem(QStringLiteral("delete_running"), m_deleting);
        return;
    }
    if (projectBusy(path)) {
        setProblem(QStringLiteral("project_busy"), path);
        return;
    }
    forgetProject(path, false);
}

void Studio::forgetProject(const QString &path, bool deleted)
{
    // Its website server: Studio no longer shows it, so it doesn't run on
    if (ProjectServer *server = m_servers.value(path); server && server->active())
        server->stop();
    const bool inFolder = std::any_of(m_settings.projectFolders.begin(), m_settings.projectFolders.end(),
                                      [&path](const QString &folder) {
                                          return sameFolder(QFileInfo(path).absolutePath(), folder);
                                      });
    const auto same = [&path](const QString &other) { return sameFolder(other, path) || other == path; };
    changeSettings([&](Settings &s) {
        s.projects.removeIf(same);
        if (deleted)
            s.hiddenProjects.removeIf(same);
        else if (inFolder && std::none_of(s.hiddenProjects.begin(), s.hiddenProjects.end(), same))
            s.hiddenProjects << path; // else the folder's scan shows it again
    });
    if (m_currentPath == path)
        m_currentPath.clear();
    m_lastShown.remove(path);
    refreshProjects();
    if (m_page == u"project" && m_currentPath.isEmpty())
        showProjects();
}

QVariantMap Studio::deleteChoices(const QString &path) const
{
    if (m_demo)
        return m_demoDeleteChoices;
    QVariantMap choices{{QStringLiteral("onStudioServer"), false}, {QStringLiteral("databaseShared"), false}};
    const auto info = projectInfo(path);
    if (!info)
        return choices;
    const DatabaseConfig &db = info.value().database;
    const auto onStudioServer = [this](const DatabaseConfig &config) {
        const bool local = config.host == u"127.0.0.1" || config.host == u"localhost";
        return m_mariadb.state() != u"none" && config.driver == u"mysql" && local && !config.usesDsn
               && config.port == m_mariadb.port();
    };
    // Only a database the wizard made for this project on Studio's server
    const bool ours = m_mariadb.usedBy(path) && onStudioServer(db)
                      && !dropProjectDatabaseSql(db.name, db.username, false).isEmpty();
    // Kept when any other project Studio knows uses the same database
    QStringList others = m_settings.databaseProjects;
    for (const auto &project : m_projects.projects())
        others << project.path;
    bool shared = false;
    for (const QString &other : std::as_const(others)) {
        if (shared || sameFolder(other, path))
            continue;
        if (const auto otherInfo = projectInfo(other);
            otherInfo && onStudioServer(otherInfo.value().database) && otherInfo.value().database.name == db.name)
            shared = true;
    }
    choices.insert(QStringLiteral("onStudioServer"), ours && !shared);
    choices.insert(QStringLiteral("databaseShared"), ours && shared);
    choices.insert(QStringLiteral("database"), db.name);
    choices.insert(QStringLiteral("user"), db.username);
    choices.insert(QStringLiteral("configured"), db.configured);
    return choices;
}

void Studio::deleteProject(const QString &path, bool database)
{
    if (m_demo)
        return;
    if (!m_deleting.isEmpty()) {
        setProblem(QStringLiteral("delete_running"), m_deleting);
        return;
    }
    if (projectBusy(path)) {
        setProblem(QStringLiteral("project_busy"), path);
        return;
    }
    // Read what to drop now: the .env goes with the folder. Only the
    // database: its user may be older than the project (the wizard's
    // CREATE USER IF NOT EXISTS takes one that is there), so it stays.
    QString sql;
    if (database) {
        const QVariantMap choices = deleteChoices(path);
        if (choices.value(QStringLiteral("onStudioServer")).toBool())
            sql = dropProjectDatabaseSql(choices.value(QStringLiteral("database")).toString(), {}, false);
    }
    m_deleting = path;
    emit deletingChanged();
    stopServerThen(path, [this, path, sql] { trashProject(path, sql); });
}

// The analyzer misses that connect() hands its slot object to Qt, and
// reports it where a line NOLINT can't reach
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void Studio::stopServerThen(const QString &path, const std::function<void()> &next)
{
    ProjectServer *server = m_servers.value(path);
    if (!server || !server->active()) {
        next();
        return;
    }
    // Once, whichever comes first: stopped, freed, or 30 s. `alive` says
    // whether the server is still there (it is freed with its project).
    auto alive = std::make_shared<bool>(true);
    auto done = std::make_shared<bool>(false);
    const auto go = [this, done, next](bool stopped) {
        if (*done)
            return;
        *done = true;
        QObject::disconnect(m_deleteWait);
        m_deleteWait = {};
        if (stopped)
            next();
        else
            deleteDone(QStringLiteral("delete_server"), {});
    };
    QObject::disconnect(m_deleteWait);
    m_deleteWait = connect(server, &ProjectServer::changed, this, [server, alive, go] {
        if (*alive && !server->active())
            go(true);
    });
    connect(server, &QObject::destroyed, this, [alive, go] {
        *alive = false;
        go(true);
    });
    QTimer::singleShot(30000, this, [server, alive, go] { go(!*alive || !server->active()); });
    if (server->state() != u"stopping")
        server->stop();
}

// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

void Studio::trashProject(const QString &path, const QString &dropSql)
{
    // A command started while the server stopped: not now
    if (projectBusy(path)) {
        deleteDone(QStringLiteral("project_busy"), path);
        return;
    }
    // Never a folder of Studio's own (settings, downloads, database data),
    // and never one that is or holds the home folder, Documents, Desktop,
    // Downloads, a project folder or another project
    const QStringList studio{settingsFolder(), toolsFolder(),
                             QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)};
    QStringList keep = m_settings.projectFolders;
    for (const auto location : {QStandardPaths::HomeLocation, QStandardPaths::DocumentsLocation,
                                QStandardPaths::DesktopLocation, QStandardPaths::DownloadLocation})
        keep << QStandardPaths::writableLocation(location);
    for (const auto &project : m_projects.projects()) {
        if (!sameFolder(project.path, path))
            keep << project.path;
    }
    if (const auto moved = moveProjectToTrash(path, studio, keep); !moved) {
        deleteDone(moved.error().code, moved.error().detail);
        return;
    }
    forgetProject(path, true);
    if (dropSql.isEmpty()) {
        forgetDatabaseOf(path);
        deleteDone();
        return;
    }
    dropDatabase(path, dropSql);
}

void Studio::forgetDatabaseOf(const QString &path)
{
    changeSettings([&path](Settings &s) {
        s.databaseProjects.removeIf([&path](const QString &other) {
            return other == QDir::toNativeSeparators(path) || sameFolder(other, path);
        });
    });
}

void Studio::dropDatabase(const QString &path, const QString &sql)
{
    // Studio's server first, if it isn't running (as the wizard does)
    if (m_mariadb.state() != u"running") {
        QObject::disconnect(m_deleteWait);
        m_deleteWait = connect(&m_mariadb, &MariadbServer::changed, this, [this, path, sql] {
            const QString state = m_mariadb.state();
            if (state == u"running") {
                QObject::disconnect(m_deleteWait);
                m_deleteWait = {};
                dropDatabase(path, sql);
            } else if (state != u"starting") {
                QObject::disconnect(m_deleteWait);
                m_deleteWait = {};
                deleteDone(QStringLiteral("db_drop"), m_mariadb.detail());
            }
        });
        if (m_mariadb.state() != u"starting")
            m_mariadb.start();
        // start() refused (no program, a port in use): nothing will change
        if (m_mariadb.state() != u"starting" && m_mariadb.state() != u"running") {
            QObject::disconnect(m_deleteWait);
            m_deleteWait = {};
            deleteDone(QStringLiteral("db_drop"), m_mariadb.detail());
        }
        return;
    }
    const Command command = m_mariadb.rootSqlCommand(sql);
    if (command.program.isEmpty()) {
        deleteDone(QStringLiteral("db_drop"), {});
        return;
    }
    auto errors = std::make_shared<QStringList>();
    startStreaming(
        command, this,
        [errors](const OutputLine &line) {
            if (line.error)
                *errors << line.text;
        },
        [this, path, errors](int exitCode, bool crashed) {
            if (exitCode != 0 || crashed) {
                deleteDone(QStringLiteral("db_drop"), errors->join(u'\n'));
                return;
            }
            // Only now: a database that wasn't dropped stays on record
            forgetDatabaseOf(path);
            deleteDone();
        });
}

void Studio::deleteDone(const QString &code, const QString &detail)
{
    if (!code.isEmpty())
        setProblem(code, detail);
    m_deleting.clear();
    emit deletingChanged();
}

void Studio::selectProject(const QString &path)
{
    // The one left and the one opened were both just in use
    if (!m_currentPath.isEmpty())
        m_lastShown.insert(m_currentPath, m_clock.elapsed());
    m_lastShown.insert(path, m_clock.elapsed());
    m_currentPath = path;
    m_page = QStringLiteral("project");
    loadCurrent();
    emit pageChanged();
}

ProjectServer *Studio::serverFor(const QString &path)
{
    if (ProjectServer *server = m_servers.value(path))
        return server;
    // A child of this: deleting Studio stops every server
    auto *server = new ProjectServer(path, m_demo ? QString() : settingsFolder(), this);
    connect(server, &ProjectServer::changed, this, [this, server] {
        serversChanged();
        if (server->projectDir() == m_currentPath)
            emit serverChanged();
    });
    m_servers.insert(path, server);
    if (!m_demo)
        server->learnTimeZone(m_settings);
    return server;
}

ProjectChecks *Studio::checksFor(const QString &path)
{
    const bool hasAdmin = currentHasAdmin();
    if (ProjectChecks *checks = m_checks.value(path)) {
        checks->setHasAdmin(hasAdmin);
        return checks;
    }
    // A child of this, which outlives it, so the settings stay valid
    auto *checks = new ProjectChecks(path, m_demo ? nullptr : &m_settings, hasAdmin, this);
    m_checks.insert(path, checks);
    connect(checks, &ProjectChecks::outputChanged, this, &Studio::commandRunningChanged);
    if (!m_demo)
        checks->refreshAll();
    return checks;
}

ProjectGenerate *Studio::generateFor(const QString &path)
{
    const bool hasAdmin = currentHasAdmin();
    if (ProjectGenerate *generate = m_generate.value(path)) {
        generate->setHasAdmin(hasAdmin);
        return generate;
    }
    // A child of this, which outlives it, so the settings stay valid
    auto *generate = new ProjectGenerate(path, m_demo ? nullptr : &m_settings, hasAdmin, this);
    m_generate.insert(path, generate);
    connect(generate, &ProjectGenerate::busyChanged, this, &Studio::commandRunningChanged);
    // What Create wrote: a migration to run, a resource to link
    connect(generate, &ProjectGenerate::changedProject, this, [this, path] {
        if (ProjectChecks *checks = m_checks.value(path)) {
            checks->refreshDatabase();
            checks->runDoctor();
        }
    });
    return generate;
}

ProjectPackages *Studio::packagesFor(const QString &path)
{
    if (ProjectPackages *packages = m_packagesTab.value(path))
        return packages;
    // A child of this, which outlives it, so the settings stay valid
    auto *packages = new ProjectPackages(path, m_demo ? nullptr : &m_settings, this);
    m_packagesTab.insert(path, packages);
    connect(packages, &ProjectPackages::runChanged, this, &Studio::commandRunningChanged);
    // An update or a new package: composer.lock, the project's type, its
    // migrations and Doctor change
    connect(packages, &ProjectPackages::changedProject, this, [this, path] {
        if (path == m_currentPath)
            loadCurrent();
        refreshProjects();
        if (ProjectChecks *checks = m_checks.value(path)) {
            checks->refreshDatabase();
            checks->runDoctor();
        }
    });
    return packages;
}

bool Studio::currentHasAdmin() const
{
    const QString type = m_current.value(QStringLiteral("type")).toString();
    return type == u"siteAdmin" || type == u"siteAdminApi" || type == u"admin" || type == u"apiAdmin";
}

void Studio::releaseUnused(bool now)
{
    if (m_closing)
        return;
    m_freed = false;
    emit releaseIdle(now, kScreenIdleMs); // screens call screenReleased()
    // Once the freed screens are gone: the collector, then back to the
    // system (allocators keep freed memory for reuse otherwise)
    const auto giveBack = qScopeGuard([this] {
        if (m_freed)
            QTimer::singleShot(1000, this, [this] {
                if (!m_closing)
                    emit memoryFreed();
            });
    });
    // A demo's data comes from its file and isn't read again
    if (m_demo)
        return;
    const qint64 time = m_clock.elapsed();
    if (!m_currentPath.isEmpty())
        m_lastShown.insert(m_currentPath, time);
    QSet<QString> paths;
    for (const QString &path : m_checks.keys())
        paths << path;
    for (const QString &path : m_packagesTab.keys())
        paths << path;
    for (const QString &path : m_servers.keys())
        paths << path;
    for (const QString &path : std::as_const(paths)) {
        if (path == m_currentPath)
            continue;
        // Never shown (a server left from a crash, found at the start):
        // idle since Studio started
        const qint64 idle = time - m_lastShown.value(path, 0);
        if (!now && idle < kProjectIdleMs)
            continue;
        if (ProjectChecks *checks = m_checks.value(path); checks && !checks->busy()) {
            m_checks.remove(path);
            checks->disconnect(this);
            checks->deleteLater(); // stops a check still reading (they only read)
            m_freed = true;
        }
        if (ProjectPackages *packages = m_packagesTab.value(path); packages && !packages->busy()) {
            m_packagesTab.remove(path);
            packages->disconnect(this);
            packages->deleteLater();
            m_freed = true;
        }
        // A server that runs (or starts, or stops) stays, with its log
        if (ProjectServer *server = m_servers.value(path); server && !server->active()) {
            m_servers.remove(path);
            server->disconnect(this);
            server->deleteLater();
            m_freed = true;
        }
    }
    forgetIdle();
}

void Studio::forgetIdle()
{
    // Times of projects with nothing loaded any more
    for (auto it = m_lastShown.begin(); it != m_lastShown.end();) {
        const QString &path = it.key();
        if (path != m_currentPath && !m_checks.contains(path) && !m_packagesTab.contains(path)
            && !m_servers.contains(path) && !m_generate.contains(path))
            it = m_lastShown.erase(it);
        else
            ++it;
    }
}

int Studio::loadedProjects() const
{
    QSet<QString> paths;
    for (const QString &path : m_checks.keys())
        paths << path;
    for (const QString &path : m_packagesTab.keys())
        paths << path;
    for (const QString &path : m_servers.keys())
        paths << path;
    return static_cast<int>(paths.size());
}

void Studio::requestTab(const QString &name)
{
    emit tabRequested(name);
}

QStringList Studio::projectPaths() const
{
    QStringList paths;
    for (const auto &project : m_projects.projects())
        paths << project.path;
    return paths;
}

bool Studio::commandRunning() const
{
    return m_wizard.state() == u"running"
           || std::any_of(m_checks.cbegin(), m_checks.cend(), [](const ProjectChecks *checks) { return checks->busy(); })
           || std::any_of(m_generate.cbegin(), m_generate.cend(), [](const ProjectGenerate *generate) { return generate->busy(); })
           || std::any_of(m_packagesTab.cbegin(), m_packagesTab.cend(), [](const ProjectPackages *packages) { return packages->busy(); });
}

void Studio::serversChanged()
{
    if (m_demo)
        return;
    QSet<QString> running;
    for (auto it = m_servers.cbegin(); it != m_servers.cend(); ++it) {
        if (it.value()->active())
            running << it.key();
    }
    m_projects.setRunning(running);
}

QVariantMap Studio::server() const
{
    const ProjectServer *server = m_servers.value(m_currentPath);
    return server ? server->info() : QVariantMap{{QStringLiteral("state"), QStringLiteral("stopped")}};
}

LogModel *Studio::logs() const
{
    const ProjectServer *server = m_servers.value(m_currentPath);
    return server ? server->logs() : nullptr;
}

void Studio::startServer()
{
    if (m_demo || m_currentPath.isEmpty() || m_current.contains(QStringLiteral("error")))
        return;
    serverFor(m_currentPath)->start(m_settings);
}

void Studio::stopServer()
{
    if (ProjectServer *server = m_servers.value(m_currentPath))
        server->stop();
}

void Studio::openSite()
{
    const ProjectServer *server = m_servers.value(m_currentPath);
    if (server && !server->url().isEmpty())
        QDesktopServices::openUrl(QUrl(server->url()));
}

void Studio::openAdmin()
{
    const ProjectServer *server = m_servers.value(m_currentPath);
    if (server && !server->url().isEmpty())
        QDesktopServices::openUrl(QUrl(server->url() + QStringLiteral("/admin")));
}

void Studio::openEnvFile()
{
    const QString env = QDir(m_currentPath).filePath(QStringLiteral(".env"));
    if (m_demo || !QFileInfo::exists(env)) {
        setProblem(QStringLiteral("no_env_file"), QDir::toNativeSeparators(env));
        return;
    }
    // The editor opens a file as it opens a folder; a .env has no program of its own on most systems
    if (FloStudio::Core::openInEditor(m_settings, QDir::toNativeSeparators(env)))
        return;
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(env)))
        setProblem(QStringLiteral("open_failed"), QDir::toNativeSeparators(env));
}

void Studio::openRepository(const QString &url)
{
    // Only hostkurd's own repositories (a package's changelog)
    if (url.startsWith(QStringLiteral("https://github.com/hostkurd/")) && !url.contains(QStringLiteral("..")))
        QDesktopServices::openUrl(QUrl(url));
}

void Studio::openUrl(const QString &url)
{
    const QUrl address(url);
    // Only the running site's own addresses
    const ProjectServer *server = m_servers.value(m_currentPath);
    if (!server || server->url().isEmpty() || !url.startsWith(server->url() + u'/'))
        return;
    QDesktopServices::openUrl(address);
}

void Studio::openLogFile()
{
    const QString log = appLogPath(m_currentPath);
    if (!QFileInfo::exists(log)) {
        setProblem(QStringLiteral("no_log_file"), QDir::toNativeSeparators(log));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(log)))
        setProblem(QStringLiteral("open_failed"), QDir::toNativeSeparators(log));
}

void Studio::showSettings(const QString &section)
{
    m_page = QStringLiteral("settings");
    m_settingsSection = section;
    emit pageChanged();
}

void Studio::showSettingsFromWizard(const QString &section)
{
    m_keepWizard = true; // coming back keeps what was typed
    showSettings(section);
}

void Studio::showNewProject()
{
    if (m_wizard.state() == u"done" || (m_wizard.state() == u"editing" && !std::exchange(m_keepWizard, false)))
        m_wizard.reset();
    m_page = QStringLiteral("new");
    emit pageChanged();
}

void Studio::openNewProject()
{
    if (m_wizard.state() != u"done")
        return;
    const QString path = projectSummary(m_wizard.projectPath()).path;
    m_wizard.reset();
    selectProject(path);
}

void Studio::showProjects()
{
    m_page = m_projects.count() > 0 ? QStringLiteral("project") : QStringLiteral("welcome");
    emit pageChanged();
}

void Studio::refreshProjects()
{
    if (m_demo)
        return;
    m_projects.setProjects(listProjects(m_settings.projectFolders, m_settings.projects, m_settings.hiddenProjects));
    // A project no longer listed: its server stops, its log goes
    for (const QString &path : m_servers.keys()) {
        if (m_projects.indexOf(path) < 0)
            m_servers.take(path)->deleteLater();
    }
    for (const QString &path : m_checks.keys()) {
        // One whose migration still runs stays until it ends: stopping it
        // halfway could leave the database half migrated
        if (m_projects.indexOf(path) < 0 && !m_checks.value(path)->busy())
            m_checks.take(path)->deleteLater();
    }
    for (const QString &path : m_generate.keys()) {
        if (m_projects.indexOf(path) < 0 && !m_generate.value(path)->busy())
            m_generate.take(path)->deleteLater();
    }
    for (const QString &path : m_packagesTab.keys()) {
        if (m_projects.indexOf(path) < 0 && !m_packagesTab.value(path)->busy())
            m_packagesTab.take(path)->deleteLater();
    }
    serversChanged();
    if (m_projects.indexOf(m_currentPath) < 0)
        m_currentPath = m_projects.count() > 0 ? m_projects.projects().constFirst().path : QString();
    if (m_page != u"settings" && m_page != u"new")
        m_page = m_currentPath.isEmpty() ? QStringLiteral("welcome") : QStringLiteral("project");
    loadCurrent();
    emit pageChanged();
}

void Studio::loadCurrent()
{
    if (m_demo)
        return;
    m_current.clear();
    if (!m_currentPath.isEmpty()) {
        if (const auto info = projectInfo(m_currentPath))
            m_current = infoMap(info.value());
        else
            m_current = {{QStringLiteral("name"), QFileInfo(m_currentPath).fileName()},
                         {QStringLiteral("path"), m_currentPath},
                         {QStringLiteral("error"), info.error().code}};
        // Its log is followed from now on, and the server is kept; Doctor and
        // the database are read the first time it opens
        if (!m_current.contains(QStringLiteral("error"))) {
            serverFor(m_currentPath);
            checksFor(m_currentPath);
            generateFor(m_currentPath);
            packagesFor(m_currentPath)->setProject(m_current);
        }
    }
    emit currentChanged();
    emit serverChanged();
}

// The analyzer misses the watcher's parent and reports it at the end of
// the function, where a line NOLINT can't reach
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void Studio::refreshTools()
{
    if (m_demo || m_closing)
        return;
    m_mariadb.refresh(); // Studio's own MariaDB programs, on their own worker
    if (m_checking) {
        m_recheck = true; // once this run ends
        return;
    }
    m_checking = true;
    ++m_databaseLook; // a look at 3306 already on its way is older than this one
    emit toolsChanged();
    auto *watcher = new QFutureWatcher<Found>(this); // a child of this, deleted when it finishes
    connect(watcher, &QFutureWatcher<Found>::finished, this, [this, watcher] {
        const Found found = watcher->result();
        watcher->deleteLater();
        m_tools = toolRows(found);
        m_phpChoices = found.phpChoices;
        m_checking = false;
        emit toolsChanged();
        emit downloadsChanged(); // "installed" in the offer
        if (std::exchange(m_recheck, false))
            refreshTools();
    });
    watcher->setFuture(QtConcurrent::run(findTools, m_settings, m_packages));
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

// Whether a server listens on 3306 now: the MariaDB row follows another
// server (XAMPP, Laragon…) started or stopped while Studio is open
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void Studio::askDatabasePort()
{
    if (m_demo || m_closing || m_checking)
        return;
    const quint64 look = ++m_databaseLook;
    auto *watcher = new QFutureWatcher<bool>(this); // a child of this, deleted when it finishes
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, look] {
        const bool running = watcher->result();
        watcher->deleteLater();
        // A newer look, or a full look for the tools, has the last word
        if (look != m_databaseLook || m_checking)
            return;
        // While Studio's own server runs (or starts, or stops) the row
        // shows that one: 3306 may be its own port
        const QString own = m_mariadb.state();
        if (own == QLatin1String("starting") || own == QLatin1String("running") || own == QLatin1String("stopping"))
            return;
        for (QVariant &item : m_tools) {
            QVariantMap row = item.toMap();
            if (row.value(QStringLiteral("tool")).toString() != QLatin1String("mariadb"))
                continue;
            const QString state = running ? QStringLiteral("running")
                                  : row.value(QStringLiteral("product")).toString().isEmpty() ? QStringLiteral("optional")
                                                                                              : QStringLiteral("found");
            if (row.value(QStringLiteral("state")).toString() == state)
                return;
            row.insert(QStringLiteral("state"), state);
            item = row;
            emit toolsChanged();
            return;
        }
    });
    watcher->setFuture(QtConcurrent::run([] { return portListening(3306); }));
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

void Studio::openInEditor(const QString &path)
{
    if (const auto opened = FloStudio::Core::openInEditor(m_settings, path); !opened)
        setProblem(opened.error().code, opened.error().detail);
}

void Studio::openFolder(const QString &path)
{
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        setProblem(QStringLiteral("open_failed"), path);
}

void Studio::setProblem(const QString &code, const QString &detail)
{
    m_problem = {{QStringLiteral("code"), code}, {QStringLiteral("detail"), detail}};
    emit problemChanged();
}

void Studio::dismissProblem()
{
    m_problem.clear();
    emit problemChanged();
}

QString Studio::localPath(const QUrl &url)
{
    return url.isLocalFile() ? QDir::toNativeSeparators(url.toLocalFile()) : QString();
}

void Studio::startDownloads(const QStringList &ids)
{
    if (m_demo || downloading())
        return;
    m_downloads.clear();
    m_queue.clear();
    for (const ToolPackage &package : std::as_const(m_packages)) {
        if (!ids.contains(package.id))
            continue;
        m_downloads << QVariantMap{{QStringLiteral("id"), package.id},
                                   {QStringLiteral("tool"), package.tool},
                                   {QStringLiteral("version"), package.version},
                                   {QStringLiteral("source"), package.source},
                                   {QStringLiteral("state"), QStringLiteral("waiting")},
                                   {QStringLiteral("received"), 0},
                                   {QStringLiteral("total"), package.size}};
        m_queue << package.id;
    }
    emit downloadsChanged();
    downloadNext();
}

void Studio::downloadNext()
{
    if (m_queue.isEmpty()) {
        refreshTools(); // and Studio's MariaDB programs
        return;
    }
    const QString id = m_queue.takeFirst();
    const auto package = std::find_if(m_packages.begin(), m_packages.end(), [&id](const ToolPackage &p) { return p.id == id; });
    if (package == m_packages.end()) {
        downloadNext();
        return;
    }
    const auto update = [this, id](const QVariantMap &changes) {
        for (QVariant &row : m_downloads) {
            QVariantMap entry = row.toMap();
            if (entry.value(QStringLiteral("id")).toString() != id)
                continue;
            for (auto it = changes.begin(); it != changes.end(); ++it)
                entry.insert(it.key(), it.value());
            row = entry;
        }
        emit downloadsChanged();
    };
    update({{QStringLiteral("state"), QStringLiteral("downloading")}});
    const ToolPackage chosen = *package;
    m_install = startToolInstall(
        chosen, this,
        [update](qint64 received, qint64 total) {
            update({{QStringLiteral("received"), received},
                    {QStringLiteral("total"), total},
                    // At the end: the SHA-256 check and unpacking
                    {QStringLiteral("state"), received >= total && total > 0 ? QStringLiteral("installing")
                                                                             : QStringLiteral("downloading")}});
        },
        [this, update, chosen](const Result<QString> &program) {
            m_install->deleteLater();
            m_install = nullptr;
            if (program) {
                update({{QStringLiteral("state"), QStringLiteral("done")}, {QStringLiteral("program"), program.value()}});
                // A tool this computer didn't have: use the download right away.
                // One it has stays the default (D15); Settings can switch.
                const bool missing = std::any_of(m_tools.begin(), m_tools.end(), [&chosen](const QVariant &row) {
                    const QVariantMap tool = row.toMap();
                    return tool.value(QStringLiteral("tool")).toString() == chosen.tool
                           && tool.value(QStringLiteral("state")).toString() != u"found";
                });
                if (missing && chosen.tool == u"php")
                    changeSettings([&program](Settings &s) { s.phpPath = program.value(); });
                if (missing && chosen.tool == u"composer")
                    changeSettings([&program](Settings &s) { s.composerPath = program.value(); });
            } else {
                update({{QStringLiteral("state"), program.error().code == u"tool_cancelled" ? QStringLiteral("cancelled")
                                                                                             : QStringLiteral("failed")},
                        {QStringLiteral("error"), program.error().code},
                        {QStringLiteral("detail"), program.error().detail}});
            }
            downloadNext();
        });
}

void Studio::cancelDownloads()
{
    for (QVariant &row : m_downloads) {
        QVariantMap entry = row.toMap();
        if (entry.value(QStringLiteral("state")).toString() == u"waiting") {
            entry.insert(QStringLiteral("state"), QStringLiteral("cancelled"));
            row = entry;
        }
    }
    m_queue.clear();
    if (m_install)
        cancelToolInstall(m_install);
    emit downloadsChanged();
}

void Studio::clearDownloads()
{
    if (downloading())
        return;
    m_downloads.clear();
    emit downloadsChanged();
}
