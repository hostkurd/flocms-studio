#include <flostudio/core/downloads.h>
#include <flostudio/core/floserver.h>
#include <flostudio/core/process.h>
#include <flostudio/core/tools.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QVersionNumber>

#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
// winsock2.h before windows.h; iphlpapi.h for the listening sockets
// (its IPv6 table needs ws2tcpip.h's address types first)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#include <vector>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace FloStudio::Core {

namespace {

#ifdef Q_OS_WIN
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseInsensitive;
#else
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseSensitive;
#endif

// Only absolute folders: a relative PATH entry (".") would mean wherever
// Studio happens to be started.
void addFolder(QStringList &list, const QString &folder)
{
    const QString clean = QDir::cleanPath(folder);
    if (!clean.isEmpty() && QDir::isAbsolutePath(clean) && !list.contains(clean, kPathCase))
        list << clean;
}

#ifndef Q_OS_WIN
// The login shell's PATH, printed between markers so what the shell's
// start files print (greetings, prompts) is ignored. `printenv PATH` gives
// the exported, colon-separated form in every shell, fish included. csh and
// tcsh can't take -l with -c, so they are not asked (PATH and the usual
// folders still apply). Up to 3 seconds: toolSearchPath() asks once.
QStringList loginShellPath()
{
    const QString shell = qEnvironmentVariable("SHELL");
    const QString name = QFileInfo(shell).fileName();
    if (shell.isEmpty() || !QFileInfo(shell).isExecutable() || name == u"csh" || name == u"tcsh")
        return {};
    QProcess process;
    process.setProgram(shell);
    process.setArguments({QStringLiteral("-i"), QStringLiteral("-l"), QStringLiteral("-c"),
                          QStringLiteral("printf '\\n<flostudio-path>'; printenv PATH; printf '</flostudio-path>\\n'")});
    process.setStandardInputFile(QProcess::nullDevice());
    process.start();
    if (!process.waitForFinished(3000)) {
        process.kill();
        process.waitForFinished(1000);
        return {};
    }
    static const QRegularExpression marked(QStringLiteral("<flostudio-path>(.*?)\\s*</flostudio-path>"),
                                           QRegularExpression::DotMatchesEverythingOption);
    const QString path = marked.match(QString::fromUtf8(process.readAllStandardOutput())).captured(1);
    return path.split(QDir::listSeparator(), Qt::SkipEmptyParts);
}
#endif

// php -> php.exe, composer -> composer.bat: QStandardPaths adds the Windows
// extensions itself. A name that has an extension Windows doesn't run
// (composer.phar) is looked for as it is.
QString lookUp(const QString &name)
{
    QString found = QStandardPaths::findExecutable(name, toolSearchPath());
    if (!found.isEmpty() || QFileInfo(name).suffix().isEmpty())
        return found;
    for (const QString &folder : toolSearchPath()) {
        const QFileInfo file(QDir(folder).filePath(name));
        if (file.isFile())
            return file.absoluteFilePath();
    }
    return {};
}

bool isBatchFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == u"bat" || suffix == u"cmd";
}

// A file `php <file>` can run: a .phar, or a PHP script (Homebrew's and
// most Linux `composer` files are the phar with a #!/usr/bin/env php line).
bool runsWithPhp(const QString &path)
{
    if (QFileInfo(path).suffix().compare(u"phar", Qt::CaseInsensitive) == 0)
        return true;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray head = file.read(128);
    const QByteArray firstLine = head.left(head.indexOf('\n'));
    return head.startsWith("<?php") || (firstLine.startsWith("#!") && firstLine.contains("php"));
}

ToolStatus status(const QString &path, const QString &version, const QString &missing)
{
    ToolStatus tool;
    tool.found = !path.isEmpty();
    tool.path = QDir::toNativeSeparators(path);
    tool.version = version;
    if (!tool.found)
        tool.error = missing;
    else if (version.isEmpty())
        tool.error = QStringLiteral("tool_failed");
    return tool;
}

} // namespace

QStringList commonToolFolders(const QString &home)
{
    QStringList candidates;
#if defined(Q_OS_WIN)
    const QString programData = qEnvironmentVariable("ProgramData", QStringLiteral("C:/ProgramData"));
    const QString appData = qEnvironmentVariable("APPDATA");
    const QString programFiles = qEnvironmentVariable("ProgramFiles", QStringLiteral("C:/Program Files"));
    candidates << programData + QStringLiteral("/ComposerSetup/bin") // Composer-Setup
               << appData + QStringLiteral("/Composer/vendor/bin")
               << home + QStringLiteral("/.config/herd/bin")          // Herd for Windows
               << QStringLiteral("C:/xampp/php") << QStringLiteral("C:/xampp/mysql/bin");
    // XAMPP on another fixed drive (D:\xampp); network and removable
    // drives are not asked: they can take long to answer
    for (const QFileInfo &drive : QDir::drives()) {
        const std::wstring root = QDir::toNativeSeparators(drive.absolutePath()).toStdWString();
        if (GetDriveTypeW(root.c_str()) != DRIVE_FIXED)
            continue;
        const QString xampp = drive.absolutePath() + QStringLiteral("xampp");
        candidates << xampp + QStringLiteral("/php") << xampp + QStringLiteral("/mysql/bin");
    }
    // Laragon and MariaDB keep a folder per version: newest name first
    const auto versioned = [&candidates](const QString &parent, const QString &pattern, const QString &sub) {
        QStringList names = QDir(parent).entryList({pattern}, QDir::Dirs | QDir::NoDotAndDotDot);
        std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
            return QString::compare(a, b, Qt::CaseInsensitive) > 0;
        });
        for (const QString &name : names)
            candidates << parent + u'/' + name + sub;
    };
    versioned(QStringLiteral("C:/laragon/bin/php"), QStringLiteral("php*"), QString());
    versioned(QStringLiteral("C:/laragon/bin/mysql"), QStringLiteral("*"), QStringLiteral("/bin"));
    versioned(programFiles, QStringLiteral("MariaDB *"), QStringLiteral("/bin"));
    versioned(programFiles + QStringLiteral("/MySQL"), QStringLiteral("MySQL Server *"), QStringLiteral("/bin"));
#elif defined(Q_OS_MACOS)
    candidates << QStringLiteral("/opt/homebrew/bin") << QStringLiteral("/usr/local/bin")
               << home + QStringLiteral("/Library/Application Support/Herd/bin")
               << home + QStringLiteral("/.config/herd-lite/bin")
               << home + QStringLiteral("/.composer/vendor/bin") << home + QStringLiteral("/.config/composer/vendor/bin")
               << QStringLiteral("/Applications/XAMPP/xamppfiles/bin");
#else
    candidates << QStringLiteral("/usr/local/bin") << home + QStringLiteral("/.config/composer/vendor/bin")
               << home + QStringLiteral("/.composer/vendor/bin") << home + QStringLiteral("/.local/bin")
               << QStringLiteral("/opt/lampp/bin");
#endif
    QStringList folders;
    for (const QString &candidate : std::as_const(candidates)) {
        if (QFileInfo(candidate).isDir())
            addFolder(folders, candidate);
    }
    return folders;
}

QStringList toolSearchPath()
{
    // The system's part is worked out once (thread-safe static): asking the
    // shell takes a moment. Studio's downloads are read each time, so a
    // tool downloaded a minute ago is found.
    static const QStringList system = [] {
        QStringList list;
        for (const QString &folder : qEnvironmentVariable("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts))
            addFolder(list, folder);
#ifndef Q_OS_WIN
        for (const QString &folder : loginShellPath())
            addFolder(list, folder);
#endif
        for (const QString &folder : commonToolFolders(QDir::homePath()))
            addFolder(list, folder);
        // A stack's PHP on PATH (XAMPP's D:\xampp\php): its database server
        // is beside it (D:\xampp\mysql\bin)
        const QStringList withPhp = list;
        for (const QString &folder : withPhp) {
            if (QFileInfo(folder).fileName().compare(QStringLiteral("php"), Qt::CaseInsensitive) != 0)
                continue;
            const QString mysql = QDir::cleanPath(folder + QStringLiteral("/../mysql/bin"));
            if (QFileInfo(mysql).isDir())
                addFolder(list, mysql);
        }
        return list;
    }();
    // Studio's own Composer first: it is downloaded to be used (an upgrade
    // of the computer's own); the rest of what Studio downloaded last (D15),
    // newest first, after the computer's own PHP and database server
    QStringList folders;
    const QStringList downloaded = installedToolFolders();
    for (const QString &folder : downloaded) {
        if (QFileInfo(folder).fileName().startsWith(QStringLiteral("composer")))
            addFolder(folders, QDir::fromNativeSeparators(folder));
    }
    for (const QString &folder : system)
        addFolder(folders, folder);
    for (const QString &folder : downloaded)
        addFolder(folders, QDir::fromNativeSeparators(folder));
    for (QString &folder : folders)
        folder = QDir::toNativeSeparators(folder);
    return folders;
}

QString findTool(const QString &nameOrPath)
{
    const QString name = nameOrPath.trimmed();
    if (name.isEmpty())
        return {};
    if (name.contains(u'/') || name.contains(u'\\')) {
        const QFileInfo file(name);
        return file.isFile() ? QDir::toNativeSeparators(file.absoluteFilePath()) : QString();
    }
    return QDir::toNativeSeparators(lookUp(name));
}

Result<QString> phpProgram(const Settings &settings)
{
    // FloServer's shim: each project gets the PHP in its floserver.json (D14)
    if (const auto floServer = floServerInUse(settings)) {
        const QString shim = floServerShim(*floServer, QStringLiteral("php"));
        if (shim.isEmpty())
            return Error{QStringLiteral("php_not_found"), floServerShimPath(*floServer, QStringLiteral("php"))};
        return shim;
    }
    const QString wanted = settings.phpPath.isEmpty() ? QStringLiteral("php") : settings.phpPath;
    const QString path = findTool(wanted);
    if (path.isEmpty())
        return Error{QStringLiteral("php_not_found"), wanted};
    return path;
}

Result<ComposerLaunch> composerLaunch(const Settings &settings)
{
    QString path;
    QString wanted;
    if (const auto floServer = floServerInUse(settings)) {
        // FloServer's shim, a program of its own that runs Composer with
        // the project's PHP (D14): it goes on as a native program below
        wanted = floServerShimPath(*floServer, QStringLiteral("composer"));
        path = floServerShim(*floServer, QStringLiteral("composer"));
    } else {
        wanted = settings.composerPath.isEmpty() ? QStringLiteral("composer") : settings.composerPath;
        path = findTool(wanted);
        if (path.isEmpty() && settings.composerPath.isEmpty())
            path = findTool(QStringLiteral("composer.phar"));
    }
    if (path.isEmpty())
        return Error{QStringLiteral("composer_not_found"), wanted};

    ComposerLaunch launch;
    launch.path = path;
    QString phar;
    if (isBatchFile(path)) {
        // Composer-Setup's composer.bat: @php "%~dp0composer.phar" %*
        const QString beside = QFileInfo(path).dir().filePath(QStringLiteral("composer.phar"));
        if (QFileInfo(beside).isFile())
            phar = QDir::toNativeSeparators(beside);
    } else if (runsWithPhp(path)) {
        phar = path;
    }
    if (!phar.isEmpty()) {
        const auto php = phpProgram(settings);
        if (!php)
            return php.error();
        launch.program = php.value();
        launch.arguments = {phar};
        launch.setupBinary = phar;
        return launch;
    }
    launch.program = path;
    launch.batchFile = isBatchFile(path);
    // The skeleton's setup runs a .bat directly and anything else as
    // `php <file>`: a .cmd would be read as a PHP file, so setup is left
    // to find `composer` itself.
    if (QFileInfo(path).suffix().compare(u"bat", Qt::CaseInsensitive) == 0)
        launch.setupBinary = path;
    return launch;
}

bool safeForBatchFile(const QString &argument)
{
    static const QString special = QStringLiteral("\"%^&|<>!()");
    return std::none_of(argument.begin(), argument.end(), [](QChar c) {
        return special.contains(c) || c.unicode() < 0x20 || c.unicode() == 0x7F;
    });
}

QString phpProbeCode()
{
    // One value per line; no double quotes (Windows command lines).
    return QStringLiteral(
        "echo PHP_VERSION, PHP_EOL, PHP_BINARY, PHP_EOL, (php_ini_loaded_file() ?: ''), PHP_EOL, "
        "implode(',', array_map('strtolower', get_loaded_extensions())), PHP_EOL;");
}

PhpInfo parsePhpProbe(const QString &output)
{
    const QStringList lines = QString(output).remove(u'\r').split(u'\n');
    PhpInfo info;
    static const QRegularExpression version(QStringLiteral(R"(^\d+\.\d+\.\d+)"));
    if (lines.size() < 4 || !version.match(lines.at(0)).hasMatch())
        return info;
    info.version = lines.at(0).trimmed();
    info.binary = lines.at(1).trimmed();
    info.iniFile = lines.at(2).trimmed();
    info.extensions = lines.at(3).trimmed().split(u',', Qt::SkipEmptyParts);
    return info;
}

bool phpVersionSupported(const QString &version)
{
    const QVersionNumber number = QVersionNumber::fromString(version);
    return !number.isNull() && number >= QVersionNumber(8, 1);
}

MissingExtensions missingExtensions(const QStringList &extensions, const QString &databaseDriver)
{
    // flocms-cli's DoctorCommand
    MissingExtensions missing;
    const QString pdo = databaseDriver == u"sqlite" ? QStringLiteral("pdo_sqlite") : QStringLiteral("pdo_mysql");
    for (const QString &name : {QStringLiteral("pdo"), QStringLiteral("mbstring"), QStringLiteral("fileinfo"),
                                QStringLiteral("gd"), pdo}) {
        if (!extensions.contains(name, Qt::CaseInsensitive))
            missing.required << name;
    }
    for (const QString &name : {QStringLiteral("openssl"), QStringLiteral("curl")}) {
        if (!extensions.contains(name, Qt::CaseInsensitive))
            missing.recommended << name;
    }
    return missing;
}

QString composerVersion(const QString &output)
{
    static const QRegularExpression line(QStringLiteral(R"(^Composer version (\S+))"),
                                         QRegularExpression::MultilineOption);
    return line.match(QString(output).remove(u'\r')).captured(1);
}

DatabaseProduct databaseVersion(const QString &output)
{
    const QString first = output.section(u'\n', 0, 0).trimmed();
    // "mysql  Ver 15.1 Distrib 10.11.14-MariaDB, for ..." and MariaDB 11's
    // "mariadb from 11.4.3-MariaDB, client 15.2 for ..."
    static const QRegularExpression maria(QStringLiteral(R"((\d+\.\d+\.\d+)-MariaDB)"));
    if (const auto match = maria.match(first); match.hasMatch())
        return {QStringLiteral("MariaDB"), match.captured(1)};
    // MySQL 5.7: "mysql  Ver 14.14 Distrib 5.7.44, for Linux"; 8.x: "mysql  Ver 8.0.39 for Linux"
    static const QRegularExpression distrib(QStringLiteral(R"(Distrib (\d+\.\d+\.\d+))"));
    if (const auto match = distrib.match(first); match.hasMatch())
        return {QStringLiteral("MySQL"), match.captured(1)};
    static const QRegularExpression ver(QStringLiteral(R"(\bVer (\d+\.\d+\.\d+)\b)"));
    if (const auto match = ver.match(first); match.hasMatch())
        return {QStringLiteral("MySQL"), match.captured(1)};
    return {};
}

namespace {

// Where detectPhp and detectComposer run FloServer's shims (D14): not
// wherever Studio was started (that may be inside a project, whose
// floserver.json would pick the PHP), but FloServer's own data folder,
// never in a project. There the shim runs FloServer's newest PHP (or the
// next php on the PATH when FloServer has none): Settings lists every
// version, and each project's own runs in its folder. Empty (Studio's
// folder, which doesn't matter then) without FloServer.
QString floServerProbeFolder(const Settings &settings)
{
    if (!floServerInUse(settings))
        return {};
    const QString folder = floServerDataFolder();
    return QFileInfo(folder).isDir() ? folder : QString();
}

} // namespace

PhpStatus detectPhp(const Settings &settings)
{
    PhpStatus php;
    const auto program = phpProgram(settings);
    if (!program) {
        php.tool = status({}, {}, program.error().code);
        return php;
    }
    Command command;
    command.program = program.value();
    command.arguments = {QStringLiteral("-r"), phpProbeCode()};
    command.environment.insert(QStringLiteral("PATH"), commandSearchPath(settings));
    command.workingDir = floServerProbeFolder(settings);
    const Captured out = runCaptured(command, 15000);
    php.info = out.succeeded() ? parsePhpProbe(out.out) : PhpInfo{};
    php.tool = status(program.value(), php.info.version, QStringLiteral("php_not_found"));
    php.supported = phpVersionSupported(php.info.version);
    return php;
}

ToolStatus detectComposer(const Settings &settings)
{
    const auto launch = composerLaunch(settings);
    if (!launch)
        return status({}, {}, launch.error().code);
    Command command;
    command.program = launch->program;
    command.arguments = launch->arguments + QStringList{QStringLiteral("--version"), QStringLiteral("--no-ansi"),
                                                        QStringLiteral("--no-interaction")};
    command.environment.insert(QStringLiteral("PATH"), commandSearchPath(settings));
    command.workingDir = floServerProbeFolder(settings);
    const Captured out = runCaptured(command, 30000);
    // Composer 2.8 prints the version on stdout and its notes on stderr;
    // read both in case a version prints it elsewhere.
    const QString version = out.succeeded() ? composerVersion(out.out + u'\n' + out.err) : QString();
    return status(launch->path, version, QStringLiteral("composer_not_found"));
}

bool portAnswers(const QString &host, int port, int timeoutMs)
{
    QTcpSocket socket;
    socket.connectToHost(host, static_cast<quint16>(port));
    const bool connected = socket.waitForConnected(timeoutMs);
    socket.abort();
    return connected;
}

#ifdef Q_OS_WIN
namespace {

// The table's ports are in network byte order, in a DWORD's low 16 bits
int tablePort(DWORD value)
{
    return static_cast<int>(((value & 0xFFU) << 8U) | ((value >> 8U) & 0xFFU));
}

// The listening sockets of one address family, from GetExtendedTcpTable
// (asked again when the table grew between the two calls)
std::vector<unsigned char> listeners(ULONG family)
{
    std::vector<unsigned char> buffer;
    for (int attempt = 0; attempt < 4; ++attempt) {
        DWORD size = static_cast<DWORD>(buffer.size());
        const DWORD result = GetExtendedTcpTable(buffer.empty() ? nullptr : buffer.data(), &size, FALSE, family,
                                                 TCP_TABLE_OWNER_PID_LISTENER, 0);
        if (result == NO_ERROR)
            return buffer;
        if (result != ERROR_INSUFFICIENT_BUFFER)
            return {};
        buffer.resize(size + 256);
    }
    return {};
}

} // namespace
#else
namespace {

// Binding refused with EADDRINUSE: something listens on that address (or
// on the wildcard covering it). SO_REUSEADDR so a closed connection still
// waiting (TIME_WAIT) doesn't count; the socket never listens or connects.
bool bindRefused(int family, const sockaddr *address, socklen_t length)
{
    const int fd = ::socket(family, SOCK_STREAM, 0);
    if (fd < 0)
        return false;
    const int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    if (family == AF_INET6)
        ::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof on);
    const bool refused = ::bind(fd, address, length) != 0 && errno == EADDRINUSE;
    ::close(fd);
    return refused;
}

} // namespace
#endif

bool portListening(int port)
{
    if (port <= 0 || port > 65535)
        return false;
#ifdef Q_OS_WIN
    {
        const std::vector<unsigned char> table = listeners(AF_INET);
        if (!table.empty()) {
            const auto *rows = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID *>(table.data());
            for (DWORD i = 0; i < rows->dwNumEntries; ++i) {
                if (tablePort(rows->table[i].dwLocalPort) == port)
                    return true;
            }
        }
    }
    {
        const std::vector<unsigned char> table = listeners(AF_INET6);
        if (!table.empty()) {
            const auto *rows = reinterpret_cast<const MIB_TCP6TABLE_OWNER_PID *>(table.data());
            for (DWORD i = 0; i < rows->dwNumEntries; ++i) {
                if (tablePort(rows->table[i].dwLocalPort) == port)
                    return true;
            }
        }
    }
    return false;
#else
    const auto networkPort = htons(static_cast<uint16_t>(port));
    for (const in_addr_t host : {htonl(INADDR_LOOPBACK), htonl(INADDR_ANY)}) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = networkPort;
        address.sin_addr.s_addr = host;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the sockets API takes a sockaddr
        if (bindRefused(AF_INET, reinterpret_cast<const sockaddr *>(&address), sizeof address))
            return true;
    }
    for (const in6_addr &host : {in6addr_loopback, in6addr_any}) {
        sockaddr_in6 address{};
        address.sin6_family = AF_INET6;
        address.sin6_port = networkPort;
        address.sin6_addr = host;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the sockets API takes a sockaddr
        if (bindRefused(AF_INET6, reinterpret_cast<const sockaddr *>(&address), sizeof address))
            return true;
    }
    return false;
#endif
}

DatabaseServerStatus detectDatabaseServer(int port)
{
    DatabaseServerStatus server;
    server.running = portListening(port);
    for (const QString &client : {QStringLiteral("mariadb"), QStringLiteral("mysql")}) {
        const QString path = findTool(client);
        if (path.isEmpty())
            continue;
        Command command;
        command.program = path;
        command.arguments = {QStringLiteral("--version")};
        const Captured out = runCaptured(command, 10000);
        const DatabaseProduct product = databaseVersion(out.out);
        if (out.succeeded() && !product.name.isEmpty()) {
            server.product = product;
            server.clientPath = path;
            break;
        }
    }
    return server;
}

QStringList editorCommandLine(const QString &editorTemplate, const QString &folder)
{
    QStringList parts;
    QString current;
    bool quoted = false;
    bool inWord = false;
    for (const QChar c : editorTemplate) {
        if (c == u'"') {
            quoted = !quoted;
            inWord = true; // "" is an empty argument
        } else if (c.isSpace() && !quoted) {
            if (inWord)
                parts << std::exchange(current, QString());
            inWord = false;
        } else {
            current += c;
            inWord = true;
        }
    }
    if (inWord)
        parts << current;
    if (parts.isEmpty() || parts.constFirst().trimmed().isEmpty())
        return {};
    const bool placed = editorTemplate.contains(QStringLiteral("{folder}"));
    for (QString &part : parts)
        part.replace(QStringLiteral("{folder}"), folder);
    if (!placed)
        parts << folder;
    return parts;
}

Result<Done> openInEditor(const Settings &settings, const QString &folder)
{
    const QString shownFolder = QDir::toNativeSeparators(folder);
    QStringList parts = editorCommandLine(settings.editorCommand, shownFolder);
    if (parts.isEmpty())
        return Error{QStringLiteral("editor_not_found"), settings.editorCommand};
    const QString program = findTool(parts.takeFirst());
    if (program.isEmpty())
        return Error{QStringLiteral("editor_not_found"), settings.editorCommand};
    if (isBatchFile(program)) {
        for (const QString &argument : std::as_const(parts)) {
            if (!safeForBatchFile(argument))
                return Error{QStringLiteral("unsafe_argument"), argument};
        }
    }
    QProcess process;
    process.setProgram(program);
    process.setArguments(parts);
    // A file (the project's .env) opens from its folder
    const QFileInfo target(folder);
    process.setWorkingDirectory(target.isDir() ? folder : target.absolutePath());
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"), commandSearchPath(settings));
    process.setProcessEnvironment(environment);
    if (!process.startDetached())
        return Error{QStringLiteral("editor_failed"), program};
    return Done{};
}

} // namespace FloStudio::Core
