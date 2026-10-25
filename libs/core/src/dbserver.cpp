#include <flostudio/core/dbserver.h>
#include <flostudio/core/tools.h>

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
#include <QTcpServer>
#include <QTimer>
#include <QtConcurrent>

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

// One start, then the running server (see startDatabaseServer).
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
        if (m_process && m_process->state() != QProcess::NotRunning) {
            QObject::disconnect(m_process, nullptr, this, nullptr);
            m_process->kill();
            m_process->waitForFinished(5000);
        }
    }
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    void begin() { QTimer::singleShot(0, this, [this] { checkProgram(); }); }

    void stopNow(const QString &rootPassword, int graceMs)
    {
        if (!m_process || m_process->state() == QProcess::NotRunning)
            return;
        m_stopping = true;
        m_onExit = nullptr; // Studio is closing: nobody to tell
        if (m_started)
            runCaptured(databaseShutdownCommand(m_start.files, m_start.port, rootPassword), 15000);
        if (!m_process->waitForFinished(graceMs)) {
            QObject::disconnect(m_process, nullptr, this, nullptr);
            m_process->kill();
            m_process->waitForFinished(5000);
        }
    }

    bool running() const { return m_started && m_process && m_process->state() != QProcess::NotRunning; }

    void stop(const QString &rootPassword, std::function<void()> done, int graceMs)
    {
        if (!m_process || m_process->state() == QProcess::NotRunning) {
            if (done)
                QTimer::singleShot(0, this, std::move(done));
            return;
        }
        if (done)
            m_onStopped.push_back(std::move(done));
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
            if (m_process && m_process->state() != QProcess::NotRunning) {
                line(true, QStringLiteral("mariadbd did not end in time: stopped"));
                stopProcess(m_process, 0);
            }
        });
    }

private:
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

    void fail(const Error &error)
    {
        line(true, error.detail);
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_failing = true;
            stopProcess(m_process, 0);
        }
        if (auto started = std::exchange(m_onStarted, nullptr))
            started(error);
    }

    // 1-2: the port is free and the program runs here
    void checkProgram()
    {
        const DatabaseServerFiles files = m_start.files;
        const int port = m_start.port;
        if (!QFileInfo(files.server).isFile())
            return fail(Error{QStringLiteral("db_program"), files.server});
        // The programs run in Studio's folder for the series
        if (!QDir().mkpath(files.folder))
            return fail(Error{QStringLiteral("db_init"), files.folder});
        run([port] { return portAnswers(QStringLiteral("127.0.0.1"), port, 400); }, [this, files](bool busy) {
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
                    lines(false, captured.out);
                    if (!captured.succeeded()) {
                        const QString library = missingSharedLibrary(captured.err + captured.out);
                        if (!library.isEmpty())
                            return fail(Error{QStringLiteral("db_library"), library});
                        return fail(capturedError(QStringLiteral("db_program"), captured));
                    }
                    writeConfig();
                });
        });
    }

    // 3: Studio's config, then the data folder the first time
    void writeConfig()
    {
        const DatabaseServerFiles &files = m_start.files;
        if (!QDir().mkpath(files.folder))
            return fail(Error{QStringLiteral("db_init"), files.folder});
        QSaveFile config(files.config);
        const QByteArray text = databaseServerConfig(files, m_start.port).toUtf8();
        if (!config.open(QIODevice::WriteOnly) || config.write(text) != text.size() || !config.commit())
            return fail(Error{QStringLiteral("db_init"), QStringLiteral("%1: %2").arg(files.config, config.errorString())});
        if (databaseServerInitialized(files))
            return startServer();
        // A folder left by a set-up that failed half-way would stop the next one
        QDir(files.dataDir).removeRecursively();
        const Command install = databaseInstallCommand(files, m_start.port);
        line(false, QStringLiteral("› ") + install.shown);
        run([install] { return runCaptured(install, 5 * 60 * 1000); }, [this](const Captured &captured) {
            lines(false, captured.out);
            lines(true, captured.err);
            if (!captured.succeeded() || !databaseServerInitialized(m_start.files))
                return fail(capturedError(QStringLiteral("db_init"), captured));
            startServer();
        });
    }

    // 4: the server, until its port answers
    void startServer()
    {
        const Command server = databaseServerCommand(m_start.files);
        line(false, QStringLiteral("› ") + server.shown);
        m_process = startStreaming(
            server, this, [this](const OutputLine &output) { line(output.error, output.text); },
            [this](int exitCode, bool crashed) { ended(exitCode, crashed); });
        connect(m_process, &QObject::destroyed, this, [this] { m_process = nullptr; });
        m_waited.start();
        waitForPort();
    }

    void waitForPort()
    {
        if (!m_process || m_process->state() == QProcess::NotRunning)
            return; // ended() reports it
        const int port = m_start.port;
        run([port] { return portAnswers(QStringLiteral("127.0.0.1"), port, 300); }, [this](bool answers) {
            if (!m_onStarted)
                return; // failed or ended meanwhile
            if (answers)
                return setRootPassword();
            if (m_waited.elapsed() > m_start.timeoutMs)
                return fail(Error{QStringLiteral("db_timeout"),
                                  databaseErrorLogTail(m_start.files).join(u'\n')});
            QTimer::singleShot(250, this, [this] { waitForPort(); });
        });
    }

    // 5: root's password, the first time
    void setRootPassword()
    {
        if (databaseRootPasswordSet(m_start.files))
            return started();
        if (m_start.rootPassword.isEmpty())
            return fail(Error{QStringLiteral("db_root"), QStringLiteral("no password for root")});
        const DatabaseServerFiles files = m_start.files;
        const int port = m_start.port;
        const QString password = m_start.rootPassword;
        // As root with no password (a new data folder); if that is refused,
        // the password may be set already (the marker was lost): checked
        const Command alter = databaseSqlCommand(files, port, QStringLiteral("root"), QString(), rootPasswordSql(password));
        const Command check = databaseSqlCommand(files, port, QStringLiteral("root"), password, QStringLiteral("SELECT 1;\n"));
        line(false, QStringLiteral("› ") + alter.shown);
        run(
            [alter, check] {
                Captured captured = runCaptured(alter, 30000);
                if (!captured.succeeded())
                    captured = runCaptured(check, 30000);
                return captured;
            },
            [this](const Captured &captured) {
                if (!captured.succeeded())
                    return fail(capturedError(QStringLiteral("db_root"), captured));
                QFile marker(markerPath(m_start.files));
                if (!marker.open(QIODevice::WriteOnly))
                    return fail(Error{QStringLiteral("db_root"), marker.fileName()});
                marker.close();
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
        const bool wasStarted = std::exchange(m_started, false);
        if (m_onStarted && !m_failing)
            fail(Error{QStringLiteral("db_start"), databaseErrorLogTail(m_start.files).join(u'\n')});
        if (wasStarted && m_onExit)
            m_onExit(exitCode, crashed && !m_stopping);
        m_stopping = false;
        for (auto &done : std::exchange(m_onStopped, {}))
            done();
    }

    DatabaseServerStart m_start;
    LineHandler m_onLine;
    DatabaseStartedHandler m_onStarted;
    ExitHandler m_onExit;
    std::vector<std::function<void()>> m_onStopped;
    QProcess *m_process = nullptr; // a child of this; cleared when it goes
    QElapsedTimer m_waited;
    bool m_started = false;
    bool m_stopping = false;
    bool m_failing = false;
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
    return files;
}

QString mariadbSeries(const QString &version)
{
    static const QRegularExpression series(QStringLiteral(R"(^(\d+)\.(\d+)(?:\.\d+)?)"));
    const QRegularExpressionMatch match = series.match(version.trimmed());
    return match.hasMatch() ? match.captured(1) + u'.' + match.captured(2) : QString();
}

QString databaseServerConfig(const DatabaseServerFiles &files, int port)
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

Command databaseInstallCommand(const DatabaseServerFiles &files, int port)
{
    const QString data = QDir::toNativeSeparators(files.dataDir);
    QStringList arguments;
    if (isWindowsPlatform(files.platform)) {
        // The Windows tool: root without a password until Studio sets one
        arguments << QStringLiteral("--datadir=") + data << QStringLiteral("--port=%1").arg(port);
    } else {
        arguments << QStringLiteral("--no-defaults") << QStringLiteral("--basedir=") + files.baseDir
                  << QStringLiteral("--datadir=") + data << QStringLiteral("--auth-root-authentication-method=normal")
                  << QStringLiteral("--skip-test-db");
        if (runningAsRoot())
            arguments << QStringLiteral("--user=root");
    }
    return make(files.installDb, arguments, files);
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
    value.replace(u'\\', QStringLiteral("\\\\")).replace(u'\'', QStringLiteral("\\'"));
    return u'\'' + value + u'\'';
}

int freeDatabasePort(int from, int tries)
{
    for (int port = from; port < from + tries && port <= 65535; ++port) {
        if (portAnswers(QStringLiteral("127.0.0.1"), port, 200))
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
    const QString lib = QDir(files.baseDir).filePath(QStringLiteral("lib"));
    QString link = QDir(lib).filePath(QStringLiteral("libaio.so.1"));
    if (QFileInfo(link).isSymLink() && QFileInfo(link).symLinkTarget() == system)
        return link;
    QFile::remove(link);
    if (!QDir().mkpath(lib) || !QFile::link(system, link))
        return {};
    return link;
}

QStringList databaseErrorLogTail(const DatabaseServerFiles &files, int lines)
{
    QFile log(files.errorLog);
    if (!log.open(QIODevice::ReadOnly))
        return {};
    const qint64 size = log.size();
    const qint64 start = std::max<qint64>(0, size - qint64{64} * 1024);
    log.seek(start);
    QStringList all = QString::fromUtf8(log.readAll()).split(u'\n', Qt::SkipEmptyParts);
    if (start > 0 && !all.isEmpty())
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
