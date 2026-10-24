#include <flostudio/core/downloads.h>
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
    // Worked out once (thread-safe static): asking the shell takes a moment.
    static const QStringList folders = [] {
        QStringList list;
        for (const QString &folder : qEnvironmentVariable("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts))
            addFolder(list, folder);
#ifndef Q_OS_WIN
        for (const QString &folder : loginShellPath())
            addFolder(list, folder);
#endif
        for (const QString &folder : commonToolFolders(QDir::homePath()))
            addFolder(list, folder);
        // Last: what Studio downloaded itself (D15)
        for (const QString &folder : installedToolFolders())
            addFolder(list, QDir::fromNativeSeparators(folder));
        for (QString &folder : list)
            folder = QDir::toNativeSeparators(folder);
        return list;
    }();
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
    const QString wanted = settings.phpPath.isEmpty() ? QStringLiteral("php") : settings.phpPath;
    const QString path = findTool(wanted);
    if (path.isEmpty())
        return Error{QStringLiteral("php_not_found"), wanted};
    return path;
}

Result<ComposerLaunch> composerLaunch(const Settings &settings)
{
    const QString wanted = settings.composerPath.isEmpty() ? QStringLiteral("composer") : settings.composerPath;
    QString path = findTool(wanted);
    if (path.isEmpty() && settings.composerPath.isEmpty())
        path = findTool(QStringLiteral("composer.phar"));
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

DatabaseServerStatus detectDatabaseServer(int port)
{
    DatabaseServerStatus server;
    server.running = portAnswers(QStringLiteral("127.0.0.1"), port, 400);
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
    process.setWorkingDirectory(folder);
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"), toolSearchPath().join(QDir::listSeparator()));
    process.setProcessEnvironment(environment);
    if (!process.startDetached())
        return Error{QStringLiteral("editor_failed"), program};
    return Done{};
}

} // namespace FloStudio::Core
