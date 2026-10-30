#include <flostudio/core/dbserver.h>
#include <flostudio/core/tools.h>

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QTimer>
#include <QtConcurrent>

#include <algorithm>
#include <memory>
#include <utility>

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

namespace FloStudio::Core {

namespace {

bool isWindowsPlatform(const QString &platform)
{
    return platform.startsWith(QStringLiteral("windows"));
}

// The first of the names that exists in one of the folders.
QString firstExisting(const QStringList &folders, const QStringList &names)
{
    for (const QString &name : names) {
        for (const QString &folder : folders) {
            const QString path = QDir(folder).filePath(name);
            if (QFileInfo(path).isFile())
                return path;
        }
    }
    return {};
}

// A path in a MariaDB option file: forward slashes (\ is an escape there),
// quoted (spaces), with " and \ escaped.
QString optionPath(const QString &path, const QString &platform)
{
    QString text = path;
    if (isWindowsPlatform(platform))
        text.replace(u'\\', u'/');
    text.replace(u'\\', QStringLiteral("\\\\")).replace(u'"', QStringLiteral("\\\""));
    return u'"' + text + u'"';
}

QString markerPath(const DatabaseServerFiles &files)
{
    return QDir(files.folder).filePath(QStringLiteral(".flostudio-root"));
}

// In the marker on Windows until root's password is on every root account
constexpr QByteArrayView accountsPending = "accounts\n";

QByteArray markerContents(const DatabaseServerFiles &files)
{
    QFile marker(markerPath(files));
    return marker.open(QIODevice::ReadOnly) ? marker.read(64) : QByteArray();
}

bool runningAsRoot()
{
#ifdef Q_OS_WIN
    return false;
#else
    return ::geteuid() == 0;
#endif
}

// What every MariaDB program Studio runs here gets: on Linux the install's
// lib/ on the library path (the libaio link, D4).
void addLibraryPath(Command &command, const DatabaseServerFiles &files)
{
    if (!files.platform.startsWith(QStringLiteral("linux")))
        return;
    const QString lib = QDir(files.baseDir).filePath(QStringLiteral("lib"));
    const QString current = qEnvironmentVariable("LD_LIBRARY_PATH");
    command.environment.insert(QStringLiteral("LD_LIBRARY_PATH"), current.isEmpty() ? lib : lib + u':' + current);
}

Command make(const QString &program, const QStringList &arguments, const DatabaseServerFiles &files)
{
    Command command;
    command.program = program;
    command.arguments = arguments;
    command.workingDir = files.folder;
    command.shown = shownCommandLine(QFileInfo(program).fileName(), arguments);
    // Never a password from the inherited environment
    command.removeEnvironment << QStringLiteral("MYSQL_PWD") << QStringLiteral("MYSQL_HOST")
                              << QStringLiteral("MYSQL_TCP_PORT") << QStringLiteral("MYSQL_UNIX_PORT");
    addLibraryPath(command, files);
    return command;
}

QStringList clientConnection(int port, const QString &user)
{
    // --no-defaults: no my.cnf of another server; TCP to 127.0.0.1 only
    return {QStringLiteral("--no-defaults"), QStringLiteral("--protocol=TCP"), QStringLiteral("--host=127.0.0.1"),
            QStringLiteral("--port=%1").arg(port), QStringLiteral("--user=") + user};
}

void setPassword(Command &command, const QString &password)
{
    if (!password.isEmpty())
        command.environment.insert(QStringLiteral("MYSQL_PWD"), password);
}

QString rootPasswordSql(const QString &password)
{
    const QString value = sqlString(password);
    QString sql;
    for (const char *host : {"localhost", "127.0.0.1", "::1"})
        sql += QStringLiteral("ALTER USER IF EXISTS 'root'@'%1' IDENTIFIED BY %2;\n").arg(QLatin1String(host), value);
    // Never anonymous logins (an empty user matches before a named one):
    // the set-up may make them for localhost and this computer's name
    sql += QStringLiteral("DROP USER IF EXISTS ''@'localhost';\n");
    const QString host = QSysInfo::machineHostName();
    if (!host.isEmpty() && !host.contains(u'\'') && !host.contains(u'\\'))
        sql += QStringLiteral("DROP USER IF EXISTS ''@'%1';\n").arg(host);
    return sql + QStringLiteral("FLUSH PRIVILEGES;\n");
}

Error capturedError(const QString &code, const Captured &captured)
{
    QString detail = (captured.err + u'\n' + captured.out).trimmed();
    if (!captured.started)
        detail = QStringLiteral("could not start %1").arg(captured.shown);
    else if (captured.timedOut)
        detail = QStringLiteral("%1: no answer in time").arg(captured.shown);
    return Error{code, detail};
}

// SQL through the server's socket (macOS, Linux): the first start runs
// without networking, so root has no password only where nothing but the
// socket in Studio's own folder can reach it (D6)
Command socketSqlCommand(const DatabaseServerFiles &files, const QString &user, const QString &password,
                         const QString &sql)
{
    Command command = make(files.client,
                           {QStringLiteral("--no-defaults"), QStringLiteral("--protocol=SOCKET"),
                            QStringLiteral("--socket=") + files.socket, QStringLiteral("--user=") + user,
                            QStringLiteral("--batch"), QStringLiteral("--skip-column-names")},
                           files);
    setPassword(command, password);
    command.input = sql.toUtf8();
    return command;
}

// The pid in the server's pid file, 0 when there is none
qint64 pidInFile(const DatabaseServerFiles &files)
{
    QFile file(files.pidFile);
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    return QString::fromLatin1(file.readAll()).trimmed().toLongLong();
}

qint64 fileSize(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() ? info.size() : 0;
}

// One start, then the running server (see startDatabaseServer).
//
// First start of a data folder: set up (mariadb-install-db, as a stopped-
// able child); on Windows its tool sets root's password right away; on
// macOS and Linux the server first runs without networking, root's
// password is set through the socket, and it starts again with networking.
// Root is only ever logged into once the server is proven ours: our
// process is alive and its pid file names it.
class Server : public QObject
{
public:
    Server(DatabaseServerStart start, QObject *owner, LineHandler onLine, DatabaseStartedHandler onStarted,
           ExitHandler onExit)
        : QObject(owner)
        , m_start(std::move(start))
        , m_onLine(std::move(onLine))
        , m_onStarted(std::move(onStarted))
        , m_onExit(std::move(onExit))
    {
        setObjectName(QStringLiteral("flostudio-database-server"));
    }

    ~Server() override
    {
        // Ended at once when nobody stopped it first (its job object or
        // process group would end it anyway): Qt then has no running
        // process to warn about
        for (QProcess *process : {m_process, m_install}) {
            if (process && process->state() != QProcess::NotRunning) {
                QObject::disconnect(process, nullptr, this, nullptr);
                process->kill();
                process->waitForFinished(5000);
            }
        }
    }
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    void begin() { QTimer::singleShot(0, this, [this] { checkProgram(); }); }

    bool running() const { return m_started && alive(m_process); }

    void stopNow(const QString &rootPassword, int graceMs)
    {
        m_cancelled = true;
        m_onExit = nullptr; // Studio is closing: nobody to tell
        m_onStarted = nullptr;
        endNow(m_install, 3000);
        if (!alive(m_process))
            return;
        m_stopping = true;
        if (m_started) {
            runCaptured(databaseShutdownCommand(m_start.files, m_start.port, rootPassword), 15000);
            if (m_process->waitForFinished(graceMs))
                return;
        }
        endNow(m_process, 10000);
    }

    void stop(const QString &rootPassword, std::function<void()> done, int graceMs)
    {
        if (done)
            m_onStopped.push_back(std::move(done));
        if (!m_started) {
            // Still starting: it stops here. A server already running ends
            // politely (SIGTERM is a clean shutdown); set-up is ended
            m_cancelled = true;
            if (alive(m_install)) {
                stopProcess(m_install, 3000);
            } else if (alive(m_process)) {
                m_stopping = true;
                stopProcess(m_process, graceMs);
            } else {
                finishCancelled();
            }
            return;
        }
        if (!alive(m_process))
            return finishStopped();
        if (m_stopping)
            return;
        m_stopping = true;
        const Command shutdown = databaseShutdownCommand(m_start.files, m_start.port, rootPassword);
        line(false, QStringLiteral("› ") + shutdown.shown);
        run([shutdown] { return runCaptured(shutdown, 20000); }, [this](const Captured &captured) {
            if (!captured.succeeded())
                line(true, capturedError(QStringLiteral("db_stop"), captured).detail);
        });
        // Whatever the shutdown said: ended after the grace time
        QTimer::singleShot(graceMs, this, [this] {
            if (alive(m_process)) {
                line(true, QStringLiteral("mariadbd did not end in time: stopped"));
                stopProcess(m_process, 3000);
            }
        });
    }

private:
    static bool alive(const QProcess *process) { return process && process->state() != QProcess::NotRunning; }

    static void endNow(QProcess *process, int graceMs)
    {
        if (!alive(process))
            return;
        stopProcess(process, graceMs / 2);
        if (!process->waitForFinished(graceMs)) {
            process->kill();
            process->waitForFinished(5000);
        }
    }

    // The watcher is a child of this (deleted when it finishes, or with
    // this): the analyzer can't see Qt's parent ownership
    // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
    template <typename Work, typename Then> void run(Work work, Then then)
    {
        using T = decltype(work());
        auto *watcher = new QFutureWatcher<T>(this); // NOLINT(cppcoreguidelines-owning-memory): owned by this
        connect(watcher, &QFutureWatcher<T>::finished, this, [watcher, then = std::move(then)]() mutable {
            then(watcher->result());
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run(std::move(work)));
    }
    // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

    void line(bool error, const QString &text)
    {
        if (m_onLine)
            m_onLine(OutputLine{error, text});
    }

    void lines(bool error, const QString &text)
    {
        for (const QString &one : text.split(u'\n', Qt::SkipEmptyParts))
            line(error, cleanLine(one.trimmed()));
    }

    // Whether a continuation should go on (nothing stopped or failed it)
    bool going() const { return m_onStarted && !m_cancelled; }

    QStringList logSinceStart() const { return databaseErrorLogTail(m_start.files, 12, m_logFrom); }

    void fail(const Error &error)
    {
        line(true, error.detail);
        if (alive(m_process)) {
            m_failing = true;
            stopProcess(m_process, 10000); // politely: it may hold data already
        }
        if (auto started = std::exchange(m_onStarted, nullptr))
            started(error);
    }

    void finishCancelled()
    {
        if (auto started = std::exchange(m_onStarted, nullptr))
            started(Error{QStringLiteral("db_cancelled"), {}});
        finishStopped();
    }

    void finishStopped()
    {
        for (auto &done : std::exchange(m_onStopped, {}))
            done();
    }

    // 1-2: the port is free and the program runs here
    void checkProgram()
    {
        const DatabaseServerFiles files = m_start.files;
        const int port = m_start.port;
        if (!QFileInfo(files.server).isFile())
            return fail(Error{QStringLiteral("db_program"), files.server});
        // The programs run in Studio's folder for the series, the owner's only
        if (!QDir().mkpath(files.folder))
            return fail(Error{QStringLiteral("db_init"), files.folder});
        QFile::setPermissions(files.folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        if (!files.socket.isEmpty())
            QDir().mkpath(QFileInfo(files.socket).path());
        run([port] { return portAnswers(QStringLiteral("127.0.0.1"), port, 400); }, [this, files](bool busy) {
            if (!going())
                return;
            if (busy)
                return fail(Error{QStringLiteral("db_port_busy"), QString::number(m_start.port)});
            const Command version = databaseVersionCommand(files);
            line(false, QStringLiteral("› ") + version.shown);
            run(
                [version, files] {
                    Captured captured = runCaptured(version, 60000);
                    // Ubuntu 24.04's libaio.so.1t64 is all that's missing: link it, try again
                    if (!captured.succeeded()
                        && missingSharedLibrary(captured.err + captured.out) == u"libaio.so.1"
                        && !linkLibaioT64(files).isEmpty())
                        captured = runCaptured(version, 60000);
                    return captured;
                },
                [this](const Captured &captured) {
                    if (!going())
                        return;
                    lines(false, captured.out);
                    if (!captured.succeeded()) {
                        const QString library = missingSharedLibrary(captured.err + captured.out);
                        if (!library.isEmpty())
                            return fail(Error{QStringLiteral("db_library"), library});
                        return fail(capturedError(QStringLiteral("db_program"), captured));
                    }
                    prepare();
                });
        });
    }

    bool unixFirstStart() const
    {
        return !isWindowsPlatform(m_start.files.platform) && !databaseRootPasswordSet(m_start.files);
    }

    bool writeConfig(bool networking)
    {
        QSaveFile config(m_start.files.config);
        const QByteArray text = databaseServerConfig(m_start.files, m_start.port, networking).toUtf8();
        if (!config.open(QIODevice::WriteOnly) || config.write(text) != text.size() || !config.commit()) {
            fail(Error{QStringLiteral("db_init"), QStringLiteral("%1: %2").arg(m_start.files.config, config.errorString())});
            return false;
        }
        return true;
    }

    // 3: the data folder the first time, then the server
    void prepare()
    {
        const DatabaseServerFiles &files = m_start.files;
        if (databaseServerInitialized(files))
            return startServer();
        if (m_start.rootPassword.isEmpty())
            return fail(Error{QStringLiteral("db_root"), QStringLiteral("no password for root")});
        // A new data folder: root has no password yet, whatever the marker said
        QFile::remove(markerPath(files));
        // What a set-up left half-way (or a damaged folder) is kept aside,
        // never deleted: it may hold someone's databases
        if (QFileInfo::exists(files.dataDir)) {
            const QString aside = files.dataDir + QStringLiteral(".failed-")
                                  + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
            if (!QDir().rename(files.dataDir, aside))
                return fail(Error{QStringLiteral("db_init"), files.dataDir});
            line(true, QStringLiteral("An unfinished data folder was moved to %1").arg(QDir::toNativeSeparators(aside)));
        }
        // MariaDB's set-up script can't take a base folder with a space
        // (Studio's tools folder is in "FloCMS Studio"): it is reached
        // through a link in a private temporary folder of this user's
        DatabaseServerFiles setupFiles = files;
        if (!isWindowsPlatform(files.platform) && files.baseDir.contains(u' ')) {
            m_baseLink = std::make_unique<QTemporaryDir>(QDir(QDir::tempPath()).filePath(QStringLiteral("flostudio-XXXXXX")));
            const QString link = m_baseLink->filePath(QStringLiteral("mariadb"));
            if (!m_baseLink->isValid() || m_baseLink->path().contains(u' ') || !QFile::link(files.baseDir, link))
                return fail(Error{QStringLiteral("db_init"), QStringLiteral("no folder without spaces for MariaDB's set-up")});
            setupFiles.baseDir = link;
            setupFiles.installDb = link + files.installDb.mid(files.baseDir.size());
        }
        const Command install = databaseInstallCommand(setupFiles, m_start.port, m_start.rootPassword);
        line(false, QStringLiteral("› ") + install.shown);
        m_install = startStreaming(
            install, this, [this](const OutputLine &output) { line(output.error, output.text); },
            [this](int exitCode, bool crashed) {
                m_install = nullptr;
                m_baseLink.reset();
                if (m_cancelled)
                    return finishCancelled();
                if (!going())
                    return;
                if (exitCode != 0 || crashed || !databaseServerInitialized(m_start.files))
                    return fail(Error{QStringLiteral("db_init"), QStringLiteral("mariadb-install-db: exit %1").arg(exitCode)});
                // Windows: its tool set root's password (for root@localhost);
                // the marker says every root account and the anonymous ones
                // still wait, until a start has done them
                if (isWindowsPlatform(m_start.files.platform)) {
                    QFile marker(markerPath(m_start.files));
                    if (!marker.open(QIODevice::WriteOnly) || marker.write(accountsPending.data(), accountsPending.size()) != accountsPending.size())
                        return fail(Error{QStringLiteral("db_root"), marker.fileName()});
                }
                startServer();
            });
    }

    // 4: the server, until it is ready and proven ours
    void startServer()
    {
        const bool networking = !unixFirstStart();
        if (!writeConfig(networking))
            return;
        // Still free? The set-up may have taken a minute
        if (networking && portAnswers(QStringLiteral("127.0.0.1"), m_start.port, 300))
            return fail(Error{QStringLiteral("db_port_busy"), QString::number(m_start.port)});
        QFile::remove(m_start.files.pidFile);
        m_logFrom = fileSize(m_start.files.errorLog);
        const Command server = databaseServerCommand(m_start.files);
        line(false, QStringLiteral("› ") + server.shown + (networking ? QString() : QStringLiteral(" (no networking)")));
        m_process = startStreaming(
            server, this, [this](const OutputLine &output) { line(output.error, output.text); },
            [this](int exitCode, bool crashed) { ended(exitCode, crashed); });
        connect(m_process, &QObject::destroyed, this, [this, process = m_process] {
            if (m_process == process)
                m_process = nullptr;
        });
        m_waited.start();
        waitUntilReady(networking);
    }

    void waitUntilReady(bool networking)
    {
        if (!alive(m_process) || !going())
            return; // ended() reports it
        const int port = m_start.port;
        const QString socket = m_start.files.socket;
        run([networking, port, socket] {
            return networking ? portAnswers(QStringLiteral("127.0.0.1"), port, 300) : QFileInfo::exists(socket);
        }, [this, networking](bool ready) {
            if (!going() || !alive(m_process))
                return;
            // Ours: our process wrote the pid file (another server on the
            // port would never get root's login)
            if (ready && pidInFile(m_start.files) == m_process->processId())
                return networking ? checkRoot() : setRootPassword();
            if (m_waited.elapsed() > m_start.timeoutMs)
                return fail(Error{QStringLiteral("db_timeout"), logSinceStart().join(u'\n')});
            QTimer::singleShot(250, this, [this, networking] { waitUntilReady(networking); });
        });
    }

    // 5 (macOS, Linux, first start): root's password through the socket,
    // then the server starts again with networking
    void setRootPassword(int attempt = 0)
    {
        const Command alter = socketSqlCommand(m_start.files, QStringLiteral("root"), QString(), rootPasswordSql(m_start.rootPassword));
        const Command check = socketSqlCommand(m_start.files, QStringLiteral("root"), m_start.rootPassword, QStringLiteral("SELECT 1;\n"));
        if (attempt == 0)
            line(false, QStringLiteral("› ") + alter.shown);
        run(
            [alter, check] {
                const Captured altered = runCaptured(alter, 30000);
                if (altered.succeeded())
                    return std::pair{altered, Captured{}};
                // Refused: the password may be set already (the marker was lost)
                return std::pair{altered, runCaptured(check, 30000)};
            },
            [this, attempt](const std::pair<Captured, Captured> &result) {
                if (!going() || !alive(m_process))
                    return;
                const auto &[altered, checked] = result;
                if (!altered.succeeded() && !checked.succeeded()) {
                    // The socket may not take logins for a moment yet
                    if (attempt < 8)
                        return void(QTimer::singleShot(500, this, [this, attempt] { setRootPassword(attempt + 1); }));
                    return fail(Error{QStringLiteral("db_root"),
                                      (altered.err + u'\n' + checked.err).trimmed()});
                }
                QFile marker(markerPath(m_start.files));
                if (!marker.open(QIODevice::WriteOnly))
                    return fail(Error{QStringLiteral("db_root"), marker.fileName()});
                marker.close();
                // Again, with networking: SIGTERM is a clean shutdown
                m_restarting = true;
                stopProcess(m_process, 30000);
            });
    }

    // 5: root's password works (an existing data folder), over TCP
    void checkRoot()
    {
        // A new Windows data folder: root's password on every root account,
        // anonymous ones dropped (as root, with its password)
        const bool accounts = markerContents(m_start.files) == accountsPending;
        const QString sql = accounts ? rootPasswordSql(m_start.rootPassword) : QStringLiteral("SELECT 1;\n");
        const Command check = databaseSqlCommand(m_start.files, m_start.port, QStringLiteral("root"), m_start.rootPassword, sql);
        run([check] { return runCaptured(check, 30000); }, [this, accounts](const Captured &captured) {
            if (!going() || !alive(m_process))
                return;
            if (!captured.succeeded())
                return fail(capturedError(QStringLiteral("db_root"), captured));
            if (accounts) {
                QFile marker(markerPath(m_start.files));
                if (!marker.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    return fail(Error{QStringLiteral("db_root"), marker.fileName()});
            }
            started();
        });
    }

    void started()
    {
        m_started = true;
        line(false, QStringLiteral("MariaDB %1 is running on 127.0.0.1:%2").arg(m_start.files.series).arg(m_start.port));
        if (auto done = std::exchange(m_onStarted, nullptr))
            done(Done{});
    }

    void ended(int exitCode, bool crashed)
    {
        if (std::exchange(m_restarting, false) && going()) {
            m_process = nullptr;
            return startServer();
        }
        const bool wasStarted = std::exchange(m_started, false);
        if (m_cancelled) {
            m_stopping = false;
            return finishCancelled();
        }
        if (m_onStarted && !m_failing)
            fail(Error{QStringLiteral("db_start"), logSinceStart().join(u'\n')});
        if (wasStarted && m_onExit)
            m_onExit(exitCode, crashed && !m_stopping);
        m_stopping = false;
        finishStopped();
    }

    DatabaseServerStart m_start;
    LineHandler m_onLine;
    DatabaseStartedHandler m_onStarted;
    ExitHandler m_onExit;
    std::vector<std::function<void()>> m_onStopped;
    QProcess *m_process = nullptr; // mariadbd, a child of this; cleared when it goes
    QProcess *m_install = nullptr; // mariadb-install-db while it runs, a child of this
    std::unique_ptr<QTemporaryDir> m_baseLink; // the install reached without spaces, during set-up
    QElapsedTimer m_waited;
    qint64 m_logFrom = 0; // the error log's size when this start began
    bool m_started = false;
    bool m_stopping = false;
    bool m_failing = false;
    bool m_cancelled = false;
    bool m_restarting = false;
};

} // namespace

QString databaseServersFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("mariadb"));
}

DatabaseServerFiles databaseServerFiles(const QString &server, const QString &series, const QString &platform,
                                        const QString &serversFolder)
{
    DatabaseServerFiles files;
    files.series = series;
    files.platform = platform;
    files.server = QDir::cleanPath(server);
    const QString bin = QFileInfo(files.server).absolutePath();
    files.baseDir = QDir::cleanPath(bin + QStringLiteral("/.."));
    const bool windows = isWindowsPlatform(platform);
    const auto exe = [windows](const char *name) {
        return windows ? QLatin1String(name) + QStringLiteral(".exe") : QString::fromLatin1(name);
    };
    // bin/ beside mariadbd; the install's bin/ too (a system's sbin/mariadbd);
    // scripts/ for the Linux bintar's mariadb-install-db
    const QString baseBin = QDir(files.baseDir).filePath(QStringLiteral("bin"));
    const QString scripts = QDir(files.baseDir).filePath(QStringLiteral("scripts"));
    files.installDb = firstExisting({bin, baseBin, scripts}, {exe("mariadb-install-db"), exe("mysql_install_db")});
    if (files.installDb.isEmpty())
        files.installDb = QDir(windows ? bin : scripts).filePath(exe("mariadb-install-db"));
    files.admin = firstExisting({bin, baseBin}, {exe("mariadb-admin"), exe("mysqladmin")});
    if (files.admin.isEmpty())
        files.admin = QDir(bin).filePath(exe("mariadb-admin"));
    files.client = firstExisting({bin, baseBin}, {exe("mariadb"), exe("mysql")});
    if (files.client.isEmpty())
        files.client = QDir(bin).filePath(exe("mariadb"));
    files.folder = QDir::cleanPath(QDir(serversFolder).filePath(series));
    const QDir folder(files.folder);
    files.dataDir = folder.filePath(QStringLiteral("data"));
    files.config = folder.filePath(windows ? QStringLiteral("my.ini") : QStringLiteral("my.cnf"));
    files.errorLog = folder.filePath(QStringLiteral("mariadbd.err"));
    files.pidFile = folder.filePath(QStringLiteral("mariadbd.pid"));
    if (!windows)
        files.socket = folder.filePath(QStringLiteral("mysqld.sock"));
    // A socket path has a limit (104 bytes on macOS, 108 on Linux): a long
    // one goes to this user's private runtime folder instead
    if (!windows && files.socket.toUtf8().size() > 100) {
        // Linux: the user's private runtime folder ($XDG_RUNTIME_DIR, 0700);
        // macOS: the user's own temporary folder (/var/folders/.../T, 0700)
        QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
        if (runtime.isEmpty() || !QFileInfo(runtime).isDir())
            runtime = QDir::tempPath();
        files.socket = QDir(runtime).filePath(QStringLiteral("flostudio-mariadb-%1.sock").arg(series));
    }
    return files;
}

QString mariadbSeries(const QString &version)
{
    static const QRegularExpression series(QStringLiteral(R"(^(\d+)\.(\d+)(?:\.\d+)?)"));
    const QRegularExpressionMatch match = series.match(version.trimmed());
    return match.hasMatch() ? match.captured(1) + u'.' + match.captured(2) : QString();
}

QString databaseServerConfig(const DatabaseServerFiles &files, int port, bool networking)
{
    QStringList lines{
        QStringLiteral("# Written by FloCMS Studio for its own MariaDB %1. Studio rewrites it at every start.").arg(files.series),
        QStringLiteral("[mysqld]"),
        QStringLiteral("basedir=") + optionPath(files.baseDir, files.platform),
        QStringLiteral("datadir=") + optionPath(files.dataDir, files.platform),
        QStringLiteral("port=%1").arg(port),
        QStringLiteral("bind-address=127.0.0.1"),
        QStringLiteral("pid-file=") + optionPath(files.pidFile, files.platform),
        QStringLiteral("log-error=") + optionPath(files.errorLog, files.platform),
    };
    if (!files.socket.isEmpty())
        lines << QStringLiteral("socket=") + optionPath(files.socket, files.platform);
    if (!networking)
        lines << QStringLiteral("skip-networking");
    if (runningAsRoot())
        lines << QStringLiteral("user=root");
    lines << QStringLiteral("character-set-server=utf8mb4") << QStringLiteral("collation-server=utf8mb4_unicode_ci")
          << QStringLiteral("innodb_buffer_pool_size=128M") << QString() << QStringLiteral("[client]")
          << QStringLiteral("port=%1").arg(port);
    if (!files.socket.isEmpty())
        lines << QStringLiteral("socket=") + optionPath(files.socket, files.platform);
    const QString end = isWindowsPlatform(files.platform) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    return lines.join(end) + end;
}

bool databaseServerInitialized(const DatabaseServerFiles &files)
{
    return QFileInfo(QDir(files.dataDir).filePath(QStringLiteral("mysql"))).isDir();
}

bool databaseRootPasswordSet(const DatabaseServerFiles &files)
{
    return QFileInfo::exists(markerPath(files));
}

Command databaseInstallCommand(const DatabaseServerFiles &files, int port, const QString &rootPassword)
{
    const QString data = QDir::toNativeSeparators(files.dataDir);
    QStringList arguments;
    QString hidden;
    if (isWindowsPlatform(files.platform)) {
        // The Windows tool sets root's password itself; it takes it only as
        // an argument (only the same user can read another process's
        // command line on Windows), never shown
        arguments << QStringLiteral("--datadir=") + data << QStringLiteral("--port=%1").arg(port);
        if (!rootPassword.isEmpty()) {
            arguments << QStringLiteral("--password=") + rootPassword;
            hidden = QStringLiteral("--password=") + rootPassword;
        }
    } else {
        arguments << QStringLiteral("--no-defaults") << QStringLiteral("--basedir=") + files.baseDir
                  << QStringLiteral("--datadir=") + data << QStringLiteral("--auth-root-authentication-method=normal")
                  << QStringLiteral("--skip-test-db")
                  // Never stopped by a computer name that doesn't resolve
                  // (the script's only use of --force): Studio's accounts
                  // are for localhost anyway
                  << QStringLiteral("--force");
        if (runningAsRoot())
            arguments << QStringLiteral("--user=root");
    }
    Command command = make(files.installDb, arguments, files);
    if (!hidden.isEmpty())
        command.shown.replace(hidden, QStringLiteral("--password=••••••••"));
    return command;
}

Command databaseServerCommand(const DatabaseServerFiles &files)
{
    return make(files.server, {QStringLiteral("--defaults-file=") + QDir::toNativeSeparators(files.config)}, files);
}

Command databaseShutdownCommand(const DatabaseServerFiles &files, int port, const QString &rootPassword)
{
    Command command = make(files.admin, clientConnection(port, QStringLiteral("root")) << QStringLiteral("shutdown"), files);
    setPassword(command, rootPassword);
    return command;
}

Command databaseSqlCommand(const DatabaseServerFiles &files, int port, const QString &user, const QString &password,
                           const QString &sql)
{
    Command command = make(files.client,
                           clientConnection(port, user) << QStringLiteral("--batch") << QStringLiteral("--skip-column-names"),
                           files);
    setPassword(command, password);
    command.input = sql.toUtf8();
    return command;
}

Command databaseVersionCommand(const DatabaseServerFiles &files)
{
    return make(files.server, {QStringLiteral("--version")}, files);
}

QString generateDatabasePassword(int length)
{
    static const QLatin1String alphabet("ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789");
    QString password;
    password.reserve(length);
    for (int i = 0; i < length; ++i)
        password += alphabet.at(static_cast<qsizetype>(QRandomGenerator::system()->bounded(int(alphabet.size()))));
    return password;
}

QString sqlIdentifier(const QString &name)
{
    QString text = name;
    return u'`' + text.replace(u'`', QStringLiteral("``")) + u'`';
}

QString sqlString(const QString &text)
{
    QString value = text;
    value.replace(u'\\', QStringLiteral("\\\\")).replace(u'\'', QStringLiteral("''"));
    return u'\'' + value + u'\'';
}

QString dropProjectDatabaseSql(const QString &database, const QString &user, bool dropUser)
{
    static const QStringList system{QStringLiteral("mysql"), QStringLiteral("information_schema"),
                                    QStringLiteral("performance_schema"), QStringLiteral("sys")};
    if (database.trimmed().isEmpty() || system.contains(database.trimmed().toLower()))
        return {};
    QString sql = QStringLiteral("DROP DATABASE IF EXISTS %1;\n").arg(sqlIdentifier(database));
    static const QStringList accounts{QStringLiteral("root"), QStringLiteral("mariadb.sys"), QStringLiteral("mysql"),
                                      QStringLiteral("PUBLIC")};
    if (dropUser && !user.trimmed().isEmpty() && !accounts.contains(user.trimmed(), Qt::CaseInsensitive)) {
        for (const char *host : {"localhost", "127.0.0.1"})
            sql += QStringLiteral("DROP USER IF EXISTS %1@'%2';\n").arg(sqlString(user), QLatin1String(host));
        sql += QStringLiteral("FLUSH PRIVILEGES;\n");
    }
    return sql;
}

int freeDatabasePort(int from, int tries)
{
    for (int port = from; port < from + tries && port <= 65535; ++port) {
        // Whether something listens (binding is refused), not whether a
        // connection succeeds: some machines (seen on GitHub's macOS 15
        // runners; VPNs and security tools do it too) accept connections on
        // closed localhost ports, and every port then looked taken.
        if (portListening(port))
            continue;
        QTcpServer probe;
        if (probe.listen(QHostAddress::LocalHost, static_cast<quint16>(port)))
            return port;
    }
    return 0;
}

QString missingSharedLibrary(const QString &output)
{
    // Linux: "error while loading shared libraries: libaio.so.1: cannot open shared object file"
    // macOS: "Library not loaded: /opt/homebrew/opt/pcre2/lib/libpcre2-8.0.dylib"
    static const QRegularExpression linux(QStringLiteral(R"(error while loading shared libraries: ([^\s:]+):)"));
    static const QRegularExpression macos(QStringLiteral(R"(Library not loaded: (\S+))"));
    for (const QRegularExpression *pattern : {&linux, &macos}) {
        const QRegularExpressionMatch match = pattern->match(output);
        if (match.hasMatch())
            return match.captured(1);
    }
    return {};
}

QString linkLibaioT64(const DatabaseServerFiles &files, const QStringList &libraryDirs)
{
    if (!files.platform.startsWith(QStringLiteral("linux")))
        return {};
    const QStringList dirs = libraryDirs.isEmpty()
                                 ? QStringList{QStringLiteral("/usr/lib/x86_64-linux-gnu"), QStringLiteral("/lib/x86_64-linux-gnu"),
                                               QStringLiteral("/usr/lib/aarch64-linux-gnu"), QStringLiteral("/usr/lib64"),
                                               QStringLiteral("/usr/lib")}
                                 : libraryDirs;
    const QString system = firstExisting(dirs, {QStringLiteral("libaio.so.1t64")});
    if (system.isEmpty())
        return {};
    // Only inside a MariaDB of Studio's own (never a system's /usr)
    const QString base = QDir::cleanPath(files.baseDir);
    for (const char *systemFolder : {"/", "/usr", "/usr/local", "/opt", "/lib", "/lib64", "/usr/lib"}) {
        if (base == QLatin1String(systemFolder))
            return {};
    }
    if (!QFileInfo(base).isWritable())
        return {};
    const QString lib = QDir(files.baseDir).filePath(QStringLiteral("lib"));
    QString link = QDir(lib).filePath(QStringLiteral("libaio.so.1"));
    if (QFileInfo(link).isSymLink() && QFileInfo(link).symLinkTarget() == system)
        return link;
    QFile::remove(link);
    if (!QDir().mkpath(lib) || !QFile::link(system, link))
        return {};
    return link;
}

QStringList databaseErrorLogTail(const DatabaseServerFiles &files, int lines, qint64 from)
{
    QFile log(files.errorLog);
    if (!log.open(QIODevice::ReadOnly))
        return {};
    const qint64 size = log.size();
    // From `from` (this start), at most the last 64 KiB
    const qint64 start = std::max<qint64>(std::clamp<qint64>(from, 0, size), size - qint64{64} * 1024);
    log.seek(start);
    QStringList all = QString::fromUtf8(log.readAll()).split(u'\n', Qt::SkipEmptyParts);
    if (start > from && !all.isEmpty())
        all.removeFirst(); // a line cut in half
    for (QString &line : all)
        line = line.trimmed();
    return all.mid(std::max<qsizetype>(0, all.size() - lines));
}

QObject *startDatabaseServer(const DatabaseServerStart &start, QObject *owner, LineHandler onLine,
                             DatabaseStartedHandler onStarted, ExitHandler onExit)
{
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): owned by owner (Qt parent)
    auto *server = new Server(start, owner, std::move(onLine), std::move(onStarted), std::move(onExit));
    server->begin();
    return server;
}

void stopDatabaseServer(QObject *server, const QString &rootPassword, std::function<void()> done, int graceMs)
{
    if (auto *own = dynamic_cast<Server *>(server))
        own->stop(rootPassword, std::move(done), graceMs);
    else if (done)
        done();
}

void stopDatabaseServerNow(QObject *server, const QString &rootPassword, int graceMs)
{
    if (auto *own = dynamic_cast<Server *>(server))
        own->stopNow(rootPassword, graceMs);
}

bool databaseServerRunning(const QObject *server)
{
    const auto *own = dynamic_cast<const Server *>(server);
    return own && own->running();
}

} // namespace FloStudio::Core
