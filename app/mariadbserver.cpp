#include "mariadbserver.h"

#include <flostudio/core/process.h>
#include <flostudio/core/tools.h>
#include <flostudio/security/secrets.h>

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QJsonArray>
#include <QUrl>
#include <QtConcurrent>

#include <algorithm>
#include <utility>

using namespace FloStudio::Core;
namespace Security = FloStudio::Security;

namespace {

constexpr int keptLines = 200;

// The programs Studio can run as its server, newest series first: its own
// downloads, and on macOS Homebrew's (MariaDB builds nothing for macOS).
QVariantList findPrograms(const QList<ToolPackage> &packages, const QString &platform)
{
    QVariantList programs;
    for (const ToolPackage &package : packages) {
        if (package.tool != u"mariadb")
            continue;
        const QString program = installedProgram(package);
        if (!program.isEmpty())
            programs << QVariantMap{{QStringLiteral("series"), mariadbSeries(package.version)},
                                    {QStringLiteral("version"), package.version},
                                    {QStringLiteral("path"), QDir::toNativeSeparators(program)},
                                    {QStringLiteral("origin"), QStringLiteral("studio")}};
    }
    if (platform.startsWith(QStringLiteral("macos"))) {
        QStringList candidates;
        for (const QString &prefix : {QStringLiteral("/opt/homebrew/opt"), QStringLiteral("/usr/local/opt")}) {
            const QDir opt(prefix);
            for (const QString &name : opt.entryList({QStringLiteral("mariadb"), QStringLiteral("mariadb@*")}, QDir::Dirs))
                candidates << opt.filePath(name + QStringLiteral("/bin/mariadbd"));
        }
        for (const QString &path : std::as_const(candidates)) {
            if (!QFileInfo(path).isExecutable())
                continue;
            Command version;
            version.program = path;
            version.arguments = {QStringLiteral("--version")};
            const Captured out = runCaptured(version, 20000);
            const DatabaseProduct product = databaseVersion(out.out + out.err);
            const QString series = mariadbSeries(product.version);
            const bool known = std::any_of(programs.cbegin(), programs.cend(), [&series](const QVariant &p) {
                return p.toMap().value(QStringLiteral("series")).toString() == series;
            });
            if (out.succeeded() && !series.isEmpty() && !known)
                programs << QVariantMap{{QStringLiteral("series"), series},
                                        {QStringLiteral("version"), product.version},
                                        {QStringLiteral("path"), path},
                                        {QStringLiteral("origin"), QStringLiteral("homebrew")}};
        }
    }
    std::sort(programs.begin(), programs.end(), [](const QVariant &a, const QVariant &b) {
        return QVersionNumber::fromString(a.toMap().value(QStringLiteral("series")).toString())
               > QVersionNumber::fromString(b.toMap().value(QStringLiteral("series")).toString());
    });
    return programs;
}

} // namespace

MariadbServer::MariadbServer(QObject *parent)
    : QObject(parent)
{
}

MariadbServer::~MariadbServer()
{
    // Studio is closing: a clean shutdown, blocking (D8)
    if (m_server)
        stopDatabaseServerNow(m_server, m_password);
}

void MariadbServer::attach(Settings *settings, const QList<ToolPackage> &packages, const QString &platform)
{
    m_settings = settings;
    m_packages = packages;
    m_platform = platform;
}

void MariadbServer::refresh()
{
    if (m_demo)
        return;
    auto *watcher = new QFutureWatcher<QVariantList>(this); // NOLINT(cppcoreguidelines-owning-memory): child
    connect(watcher, &QFutureWatcher<QVariantList>::finished, this, [this, watcher] {
        m_programs = watcher->result();
        watcher->deleteLater();
        if (m_state == u"none" && !m_programs.isEmpty())
            m_state = QStringLiteral("stopped");
        else if (m_programs.isEmpty() && !active())
            m_state = QStringLiteral("none");
        emit changed();
        emit programsFound();
    });
    watcher->setFuture(QtConcurrent::run(findPrograms, m_packages, m_platform));
}

void MariadbServer::showDemo(const QVariantMap &demo)
{
    m_demo = true;
    m_demoInfo = demo;
    m_programs = demo.value(QStringLiteral("programs")).toList();
    m_state = demo.value(QStringLiteral("state"), m_programs.isEmpty() ? QStringLiteral("none") : QStringLiteral("stopped")).toString();
    m_phase = demo.value(QStringLiteral("phase")).toString();
    m_error = demo.value(QStringLiteral("error")).toString();
    m_detail = demo.value(QStringLiteral("detail")).toString();
    m_freePort = demo.value(QStringLiteral("freePort")).toInt();
    const qint64 minutes = demo.value(QStringLiteral("runningMinutes")).toLongLong();
    if (m_state == u"running")
        m_startedAt = QDateTime::currentDateTime().addSecs(-60 * minutes);
    m_lines = demo.value(QStringLiteral("lines")).toStringList();
    emit changed();
}

QVariantMap MariadbServer::current() const
{
    const QString wanted = m_settings ? m_settings->databaseSeries : QString();
    for (const QVariant &program : m_programs) {
        if (program.toMap().value(QStringLiteral("series")).toString() == wanted)
            return program.toMap();
    }
    return m_programs.isEmpty() ? QVariantMap() : m_programs.constFirst().toMap();
}

QString MariadbServer::series() const { return current().value(QStringLiteral("series")).toString(); }
QString MariadbServer::version() const { return current().value(QStringLiteral("version")).toString(); }
QString MariadbServer::program() const { return current().value(QStringLiteral("path")).toString(); }
QString MariadbServer::origin() const { return current().value(QStringLiteral("origin")).toString(); }

DatabaseServerFiles MariadbServer::files() const
{
    return databaseServerFiles(QDir::fromNativeSeparators(program()), series(), m_platform);
}

QString MariadbServer::dataFolder() const
{
    if (m_demo)
        return m_demoInfo.value(QStringLiteral("dataFolder")).toString();
    return series().isEmpty() ? QString() : QDir::toNativeSeparators(files().folder);
}

int MariadbServer::port() const
{
    return m_settings ? m_settings->databasePort : 3306;
}

bool MariadbServer::autoStart() const
{
    return m_settings && m_settings->databaseAutoStart;
}

bool MariadbServer::initialized() const
{
    if (m_demo)
        return m_demoInfo.value(QStringLiteral("initialized"), true).toBool();
    return !series().isEmpty() && databaseServerInitialized(files());
}

bool MariadbServer::secureStore()
{
    return Security::secureStoreAvailable();
}

QString MariadbServer::passwordAccount(const QString &series)
{
    return QStringLiteral("flocms-studio-mariadb-%1://root").arg(series);
}

void MariadbServer::setState(const QString &state, const QString &error, const QString &detail)
{
    m_state = state;
    m_error = error;
    m_detail = detail;
    if (state != u"starting")
        m_phase.clear();
    emit changed();
}

void MariadbServer::addLine(const QString &text)
{
    m_lines << text;
    if (m_lines.size() > keptLines)
        m_lines.erase(m_lines.begin(), m_lines.begin() + (m_lines.size() - keptLines));
    // The phase, from the command Core runs (D11: the steps on screen)
    if (text.startsWith(QStringLiteral("› "))) {
        QString program = text.mid(2).section(u' ', 0, 0);
        program.remove(QStringLiteral(".exe"));
        if (program.contains(QStringLiteral("install-db")))
            m_phase = QStringLiteral("setup");
        else if (program == u"mariadbd" && text.contains(QStringLiteral("--defaults-file=")))
            m_phase = QStringLiteral("start");
        else if (program == u"mariadb" || program == u"mysql")
            m_phase = QStringLiteral("password");
    }
    emit changed();
}

void MariadbServer::start()
{
    if (m_demo || active() || program().isEmpty())
        return;
    m_lines.clear();
    m_freePort = 0;
    m_phase = QStringLiteral("check");
    setState(QStringLiteral("starting"));
    const QString account = passwordAccount(series());
    const int run = ++m_run;
    Security::readPassword(account, this, [this, run, account](const Result<QString> &secret) {
        if (run != m_run)
            return;
        if (secret)
            return launch(secret.value());
        const QString code = secret.error().code;
        if (code != u"keychain_missing")
            return setState(QStringLiteral("failed"), QStringLiteral("db_keychain"), secret.error().detail);
        if (databaseRootPasswordSet(files()))
            return setState(QStringLiteral("failed"), QStringLiteral("db_root_lost"), account);
        // A new data folder: a new password, saved before the server gets it
        const QString password = generateDatabasePassword();
        Security::savePassword(account, password, this, [this, run, password](const Result<QString> &saved) {
            if (run != m_run)
                return;
            if (!saved)
                return setState(QStringLiteral("failed"), QStringLiteral("db_keychain"), saved.error().detail);
            launch(password);
        });
    });
}

void MariadbServer::launch(const QString &password)
{
    DatabaseServerStart start;
    start.files = files();
    start.port = port();
    start.rootPassword = password;
    const int run = m_run;
    QObject *server = startDatabaseServer(
        start, this,
        [this, run](const OutputLine &line) {
            if (run == m_run)
                addLine(line.text);
        },
        [this, run, password](const Result<Done> &started) {
            if (run != m_run)
                return;
            if (started) {
                m_password = password;
                m_startedAt = QDateTime::currentDateTime();
                return setState(QStringLiteral("running"));
            }
            if (started.error().code == u"db_port_busy")
                m_freePort = freeDatabasePort(port() + 1, 50);
            if (m_server) {
                m_server->deleteLater();
                m_server = nullptr;
            }
            setState(QStringLiteral("failed"), started.error().code, started.error().detail);
        },
        [this, run](int, bool crashed) {
            if (run != m_run)
                return;
            m_password.clear();
            m_shownPassword.clear();
            m_startedAt = {};
            if (m_server) {
                m_server->deleteLater();
                m_server = nullptr;
            }
            if (crashed)
                setState(QStringLiteral("failed"), QStringLiteral("db_crashed"),
                         databaseErrorLogTail(files()).join(u'\n'));
            else
                setState(QStringLiteral("stopped"));
        });
    m_server = server;
    connect(server, &QObject::destroyed, this, [this, server] {
        if (m_server == server)
            m_server = nullptr;
    });
}

void MariadbServer::stop()
{
    if (m_demo || !m_server || m_state != u"running")
        return;
    setState(QStringLiteral("stopping"));
    stopDatabaseServer(m_server, m_password, [this] {
        if (m_state == u"stopping")
            setState(QStringLiteral("stopped"));
    });
}

void MariadbServer::startOnFreePort()
{
    if (m_freePort <= 0)
        return;
    setPort(m_freePort);
    start();
}

void MariadbServer::setPort(int port)
{
    if (!m_settings || port < 1 || port > 65535 || port == m_settings->databasePort)
        return;
    m_settings->databasePort = port;
    emit settingsChanged();
    emit changed();
}

void MariadbServer::setAutoStart(bool on)
{
    if (!m_settings || m_settings->databaseAutoStart == on)
        return;
    m_settings->databaseAutoStart = on;
    emit settingsChanged();
    emit changed();
}

void MariadbServer::setSeries(const QString &series)
{
    if (!m_settings || active() || m_settings->databaseSeries == series)
        return;
    m_settings->databaseSeries = series;
    emit settingsChanged();
    emit changed();
}

void MariadbServer::showPassword(bool on)
{
    if (!on || m_password.isEmpty()) {
        m_shownPassword.clear();
        emit changed();
        return;
    }
    m_shownPassword = m_password;
    emit changed();
}

void MariadbServer::copyPassword()
{
    if (!m_password.isEmpty()) {
        copyText(m_password);
        return;
    }
    // Not running: read from the secure store
    Security::readPassword(passwordAccount(series()), this, [this](const Result<QString> &secret) {
        if (secret)
            copyText(secret.value());
    });
}

void MariadbServer::copyText(const QString &text)
{
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(text);
}

void MariadbServer::openLog()
{
    if (!m_demo && !series().isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(files().errorLog));
}

void MariadbServer::openDataFolder()
{
    if (!m_demo && !series().isEmpty()) {
        QDir().mkpath(files().folder);
        QDesktopServices::openUrl(QUrl::fromLocalFile(files().folder));
    }
}

void MariadbServer::remove(bool databasesToo)
{
    if (m_demo || active() || origin() != u"studio")
        return;
    const QString wanted = series();
    const DatabaseServerFiles serverFiles = files();
    for (const ToolPackage &package : std::as_const(m_packages)) {
        if (package.tool == u"mariadb" && mariadbSeries(package.version) == wanted)
            QDir(installFolder(package)).removeRecursively();
    }
    if (databasesToo) {
        QDir(serverFiles.folder).removeRecursively();
        Security::forgetPassword(passwordAccount(wanted), this, [](const Result<QString> &) {});
    }
    m_lines.clear();
    setState(QStringLiteral("stopped"));
    refresh();
}
