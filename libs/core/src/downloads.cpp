#include <flostudio/core/downloads.h>

#include <QCryptographicHash>
#include <QDirIterator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QVersionNumber>
#include <QtConcurrent>

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>

namespace FloStudio::Core {

namespace {

// The marker file in an installed package folder: the package's SHA-256.
QString markerName()
{
    return QStringLiteral(".flostudio-package");
}

bool isWindowsPlatform(const QString &platform)
{
    return platform.startsWith(QStringLiteral("windows"));
}

QString programName(const QString &name, const QString &platform)
{
    return isWindowsPlatform(platform) ? name + QStringLiteral(".exe") : name;
}

// The program inside an installed package folder.
QString programIn(const ToolPackage &package, const QString &folder)
{
    const QDir dir(folder);
    if (package.tool == u"php")
        return dir.filePath(programName(QStringLiteral("php"), package.platform));
    if (package.tool == u"composer")
        return dir.filePath(QStringLiteral("composer.phar"));
    if (package.tool == u"mariadb")
        return dir.filePath(QStringLiteral("bin/") + programName(QStringLiteral("mariadbd"), package.platform));
    return {};
}

QString readMarker(const QString &folder)
{
    QFile marker(QDir(folder).filePath(markerName()));
    if (!marker.open(QIODevice::ReadOnly))
        return {};
    return QString::fromLatin1(marker.readAll()).trimmed();
}

// The system's own tar: on Windows the one in System32 (bsdtar, reads zip),
// not another tar that happens to be on PATH (Git's GNU tar can't).
QString tarProgram()
{
#ifdef Q_OS_WIN
    const QString system = qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows"));
    return QDir(system).filePath(QStringLiteral("System32/tar.exe"));
#else
    return QStringLiteral("tar");
#endif
}

// Never through a link: a package's link could point anywhere.
void makeExecutable(const QString &path)
{
    QFile file(path);
    if (file.exists() && !QFileInfo(path).isSymLink())
        file.setPermissions(file.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser | QFileDevice::ExeGroup
                            | QFileDevice::ExeOther);
}

// A rename on Windows can fail for a moment while antivirus or the search
// indexer has a file open: tried for a few seconds (worker thread only).
bool renameWithRetry(const QString &from, const QString &to)
{
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (QDir().rename(from, to))
            return true;
#ifndef Q_OS_WIN
        return false;
#else
        QThread::msleep(250);
#endif
    }
    return false;
}

// Every link inside folder points inside it (D15: nothing unpacked may
// reach out of the tools folder).
bool linksStayInside(const QString &folder)
{
    const QString root = QFileInfo(folder).canonicalFilePath() + u'/';
    QDirIterator it(folder, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo entry(it.next());
        if (!entry.isSymLink() && !entry.isJunction())
            continue;
        // A link to something missing has no canonical path: also refused
        const QString target = QFileInfo(entry.symLinkTarget()).canonicalFilePath();
        if (target.isEmpty() || !(target + u'/').startsWith(root))
            return false;
    }
    return true;
}

bool isWorkFolder(const QString &name)
{
    return name.endsWith(QStringLiteral(".new")) || name.endsWith(QStringLiteral(".old")) || name.startsWith(u'.');
}

// One download: the addresses are tried in order; the file is hashed as it
// arrives, then checked and installed on a worker thread.
class Install : public QObject
{
public:
    Install(ToolPackage package, QString toolsDir, DownloadProgressHandler onProgress, InstallDoneHandler onDone,
            QObject *parent)
        : QObject(parent)
        , m_package(std::move(package))
        , m_toolsDir(std::move(toolsDir))
        , m_onProgress(std::move(onProgress))
        , m_onDone(std::move(onDone))
        , m_network(this)
    {
        setObjectName(QStringLiteral("flostudio-install"));
    }

    void start()
    {
        if (*m_cancelled) {
            finish(Error{QStringLiteral("tool_cancelled"), m_package.id});
            return;
        }
        if (!QDir().mkpath(QDir(m_toolsDir).filePath(QStringLiteral(".downloads")))) {
            finish(Error{QStringLiteral("tool_download"), m_toolsDir});
            return;
        }
        m_file.setFileName(QDir(m_toolsDir).filePath(QStringLiteral(".downloads/%1.part").arg(m_package.id)));
        tryNext();
    }

    // Before the download: nothing starts. During it: the reply stops.
    // While unpacking: the worker stops before the folder is swapped in.
    void cancel()
    {
        *m_cancelled = true;
        if (m_reply)
            m_reply->abort(); // finished() follows
    }

private:
    void tryNext()
    {
        if (*m_cancelled) {
            finish(Error{QStringLiteral("tool_cancelled"), m_package.id});
            return;
        }
        if (m_next >= m_package.urls.size()) {
            finish(Error{QStringLiteral("tool_download"), m_errors.join(QStringLiteral("; "))});
            return;
        }
        const QUrl url(m_package.urls.at(m_next++));
        m_hash.reset();
        m_received = 0;
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            finish(Error{QStringLiteral("tool_download"), m_file.errorString()});
            return;
        }
        QNetworkRequest request(url);
        // Redirects only to HTTPS (the default policy allows no downgrade)
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FloCMS Studio"));
        m_reply = m_network.get(request);
        connect(m_reply, &QNetworkReply::readyRead, this, [this] { take(); });
        connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
            if (m_onProgress)
                m_onProgress(received, total > 0 ? total : m_package.size);
        });
        connect(m_reply, &QNetworkReply::finished, this, [this] { downloaded(); });
    }

    void take()
    {
        if (!m_reply->isOpen()) // aborted
            return;
        const QByteArray data = m_reply->readAll();
        m_hash.addData(data);
        m_received += data.size();
        // More than the list says: a wrong file, stopped before it fills the disk
        if (m_received > m_package.size) {
            m_tooBig = true;
            m_reply->abort();
            return;
        }
        if (m_file.write(data) != data.size())
            m_reply->abort();
    }

    void downloaded()
    {
        take();
        m_file.close();
        const QNetworkReply::NetworkError error = m_reply->error();
        const QString reason = m_reply->errorString();
        const bool https = m_reply->url().scheme() == u"https";
        m_reply->deleteLater();
        m_reply = nullptr;
        if (*m_cancelled) {
            m_file.remove();
            finish(Error{QStringLiteral("tool_cancelled"), m_package.id});
            return;
        }
        if (std::exchange(m_tooBig, false)) {
            m_errors << QStringLiteral("%1: larger than %2 bytes").arg(m_package.urls.at(m_next - 1)).arg(m_package.size);
            m_file.remove();
            tryNext();
            return;
        }
        if (error != QNetworkReply::NoError || !https) {
            m_errors << QStringLiteral("%1: %2").arg(m_package.urls.at(m_next - 1), https ? reason : QStringLiteral("not HTTPS"));
            m_file.remove();
            tryNext();
            return;
        }
        const QString hash = QString::fromLatin1(m_hash.result().toHex());
        if (hash != m_package.sha256 || m_received != m_package.size) {
            m_file.remove();
            finish(Error{QStringLiteral("tool_hash"), QStringLiteral("%1: %2").arg(m_package.urls.at(m_next - 1), hash)});
            return;
        }
        // Unpacking takes a few seconds: off the interface thread
        auto *watcher = new QFutureWatcher<Result<QString>>(this); // owned by this
        connect(watcher, &QFutureWatcher<Result<QString>>::finished, this, [this, watcher] {
            const Result<QString> result = watcher->result();
            QFile::remove(m_file.fileName());
            finish(result);
        });
        watcher->setFuture(QtConcurrent::run([package = m_package, file = m_file.fileName(), dir = m_toolsDir,
                                              cancelled = m_cancelled] {
            return installDownloaded(package, file, dir, cancelled.get());
        }));
    }

    // Always from the event loop, never from inside cancel(): the owner may
    // be cancelling from its destructor. A queued call to a deleted Install
    // is dropped.
    void finish(const Result<QString> &result)
    {
        if (m_finished)
            return;
        m_finished = true;
        m_result = result;
        QTimer::singleShot(0, this, [this] {
            if (m_onDone)
                m_onDone(m_result);
        });
    }

    ToolPackage m_package;
    QString m_toolsDir;
    DownloadProgressHandler m_onProgress;
    InstallDoneHandler m_onDone;
    QNetworkAccessManager m_network;
    QNetworkReply *m_reply = nullptr; // belongs to m_network; cleared when it finishes
    QFile m_file;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    qint64 m_received = 0;
    qsizetype m_next = 0;
    QStringList m_errors;
    // Shared with the worker, which may outlive this object
    std::shared_ptr<std::atomic<bool>> m_cancelled = std::make_shared<std::atomic<bool>>(false);
    bool m_tooBig = false;
    Result<QString> m_result = Error{};
    bool m_finished = false;
};

} // namespace

QString currentPlatform()
{
    const QString kernel = QSysInfo::kernelType();
    const QString cpu = QSysInfo::currentCpuArchitecture();
    const QString arch = cpu.startsWith(QStringLiteral("arm")) ? QStringLiteral("arm64") : QStringLiteral("x86_64");
    if (kernel == u"winnt")
        return QStringLiteral("windows-x86_64"); // no ARM builds of PHP for Windows; x64 runs on ARM
    if (kernel == u"darwin")
        return QStringLiteral("macos-") + arch;
    return QStringLiteral("linux-") + arch;
}

Result<QList<ToolPackage>> toolPackages(const QByteArray &json, const QString &platform)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.object().value(QStringLiteral("tools")).isArray())
        return Error{QStringLiteral("tools_list"), error.errorString()};
    static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{64}$"));
    QList<ToolPackage> packages;
    const QJsonArray tools = document.object().value(QStringLiteral("tools")).toArray();
    for (const auto &item : tools) {
        const QJsonObject entry = item.toObject();
        ToolPackage package;
        package.id = entry.value(QStringLiteral("id")).toString();
        package.tool = entry.value(QStringLiteral("tool")).toString();
        package.version = entry.value(QStringLiteral("version")).toString();
        package.platform = entry.value(QStringLiteral("platform")).toString();
        package.sha256 = entry.value(QStringLiteral("sha256")).toString();
        package.size = static_cast<qint64>(entry.value(QStringLiteral("size")).toDouble());
        package.archive = entry.value(QStringLiteral("archive")).toString();
        package.source = entry.value(QStringLiteral("source")).toString();
        for (const auto &url : entry.value(QStringLiteral("urls")).toArray())
            package.urls << url.toString();
        const bool https = !package.urls.isEmpty() && std::all_of(package.urls.begin(), package.urls.end(), [](const QString &url) {
            return url.startsWith(QStringLiteral("https://"));
        });
        static const QStringList archives{QStringLiteral("zip"), QStringLiteral("tar.gz"), QStringLiteral("phar")};
        if (!https || package.size <= 0 || !hex.match(package.sha256).hasMatch() || !archives.contains(package.archive) || package.id.isEmpty()
            || package.id.contains(u'/') || package.id.contains(u'\\') || package.id.startsWith(u'.'))
            continue;
        if (package.platform == platform || package.platform == u"any")
            packages << package;
    }
    const auto rank = [](const QString &tool) { return tool == u"php" ? 0 : tool == u"composer" ? 1 : 2; };
    std::stable_sort(packages.begin(), packages.end(), [&rank](const ToolPackage &a, const ToolPackage &b) {
        if (rank(a.tool) != rank(b.tool))
            return rank(a.tool) < rank(b.tool);
        return QVersionNumber::fromString(a.version) > QVersionNumber::fromString(b.version);
    });
    return packages;
}

QList<ToolPackage> bundledToolPackages()
{
    QFile file(QStringLiteral(":/flostudio/core/tools.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const auto packages = toolPackages(file.readAll(), currentPlatform());
    return packages ? packages.value() : QList<ToolPackage>{};
}

QString toolsFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("tools"));
}

QString installFolder(const ToolPackage &package, const QString &toolsDir)
{
    return QDir(toolsDir).filePath(package.id);
}

QString installedProgram(const ToolPackage &package, const QString &toolsDir)
{
    const QString folder = installFolder(package, toolsDir);
    if (readMarker(folder) != package.sha256)
        return {};
    const QString program = programIn(package, folder);
    return QFileInfo(program).isFile() ? QDir::toNativeSeparators(program) : QString();
}

QStringList installedToolFolders(const QString &toolsDir)
{
    QStringList folders;
    QFileInfoList entries = QDir(toolsDir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    // By tool, newest first (php-8.10, php-8.5, php-8.4), so a plain `php`
    // is the newest
    const auto split = [](const QString &name) {
        const qsizetype dash = name.lastIndexOf(u'-');
        return dash < 0 ? std::pair{name, QVersionNumber()}
                        : std::pair{name.left(dash), QVersionNumber::fromString(name.mid(dash + 1))};
    };
    std::sort(entries.begin(), entries.end(), [&split](const QFileInfo &a, const QFileInfo &b) {
        const auto [toolA, versionA] = split(a.fileName());
        const auto [toolB, versionB] = split(b.fileName());
        return toolA != toolB ? toolA < toolB : versionA > versionB;
    });
    for (const QFileInfo &entry : std::as_const(entries)) {
        if (isWorkFolder(entry.fileName()) || readMarker(entry.filePath()).isEmpty())
            continue;
        const QString bin = QDir(entry.filePath()).filePath(QStringLiteral("bin"));
        folders << QDir::toNativeSeparators(entry.fileName().startsWith(QStringLiteral("mariadb")) ? bin : entry.filePath());
    }
    return folders;
}

QString windowsPhpIni(const QString &extensionDir)
{
    QStringList lines{
        QStringLiteral("; Written by FloCMS Studio for the PHP it downloaded."),
        QStringLiteral("; The extensions are what `php flo doctor` checks, plus Composer's."),
        QStringLiteral("extension_dir = \"%1\"").arg(QDir::toNativeSeparators(extensionDir)),
    };
    for (const char *extension : {"curl", "fileinfo", "gd", "intl", "mbstring", "openssl", "pdo_mysql", "pdo_sqlite",
                                  "sqlite3", "zip"})
        lines << QStringLiteral("extension=%1").arg(QLatin1String(extension));
    lines << QStringLiteral("memory_limit = 512M") << QStringLiteral("upload_max_filesize = 64M")
          << QStringLiteral("post_max_size = 64M") << QStringLiteral("max_execution_time = 120")
          << QStringLiteral("display_errors = On") << QStringLiteral("error_reporting = E_ALL")
          << QStringLiteral("date.timezone = UTC");
    return lines.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n");
}

QString sha256OfFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

Result<Done> extractArchive(const QString &archive, const QString &type, const QString &folder)
{
    if (!QDir().mkpath(folder))
        return Error{QStringLiteral("tool_extract"), folder};
    QProcess tar;
    tar.setProgram(tarProgram());
    // -x extract, -f file, -C into; bsdtar and GNU tar both read .tar.gz with -z implied
    tar.setArguments({QStringLiteral("-x"), QStringLiteral("-f"), QDir::toNativeSeparators(archive), QStringLiteral("-C"),
                      QDir::toNativeSeparators(folder)});
    tar.start();
    if (!tar.waitForFinished(10 * 60 * 1000) || tar.exitStatus() != QProcess::NormalExit || tar.exitCode() != 0)
        return Error{QStringLiteral("tool_extract"),
                     QStringLiteral("%1 (%2): %3").arg(QFileInfo(archive).fileName(), type,
                                                       QString::fromLocal8Bit(tar.readAllStandardError()).trimmed())};
    if (!linksStayInside(folder))
        return Error{QStringLiteral("tool_extract"), QStringLiteral("%1: a link points outside the package").arg(QFileInfo(archive).fileName())};
    // One top folder only (mariadb-11.8.5-winx64/...): its contents move up
    const QDir dir(folder);
    const QFileInfoList top = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    if (top.size() == 1 && top.constFirst().isDir()) {
        const QDir inner(top.constFirst().filePath());
        for (const QString &name : inner.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
            if (!QDir().rename(inner.filePath(name), dir.filePath(name)))
                return Error{QStringLiteral("tool_extract"), name};
        }
        QDir().rmdir(inner.path());
    }
    return Done{};
}

Result<QString> installDownloaded(const ToolPackage &package, const QString &file, const QString &toolsDir,
                                  const std::atomic<bool> *cancelled)
{
    const QString target = installFolder(package, toolsDir);
    const QString staging = target + QStringLiteral(".new");
    const QString old = target + QStringLiteral(".old");
    // Any failure from here on leaves only what was installed before
    const auto fail = [&staging](const Error &error) -> Result<QString> {
        QDir(staging).removeRecursively();
        return error;
    };
    QDir(staging).removeRecursively();
    if (package.archive == u"phar") {
        if (!QDir().mkpath(staging) || !QFile::copy(file, QDir(staging).filePath(QStringLiteral("composer.phar"))))
            return fail(Error{QStringLiteral("tool_install"), staging});
    } else if (const auto extracted = extractArchive(file, package.archive, staging); !extracted) {
        return fail(extracted.error());
    }

    if (package.tool == u"php" && isWindowsPlatform(package.platform)) {
        QSaveFile ini(QDir(staging).filePath(QStringLiteral("php.ini")));
        const QByteArray text = windowsPhpIni(QDir(target).filePath(QStringLiteral("ext"))).toUtf8();
        if (!ini.open(QIODevice::WriteOnly) || ini.write(text) != text.size() || !ini.commit())
            return fail(Error{QStringLiteral("tool_install"), ini.errorString()});
    }
    if (!isWindowsPlatform(package.platform)) {
        makeExecutable(programIn(package, staging));
        if (package.tool == u"mariadb") {
            const QDir bin(QDir(staging).filePath(QStringLiteral("bin")));
            for (const QString &name : bin.entryList(QDir::Files))
                makeExecutable(bin.filePath(name));
        }
    }
    if (!QFileInfo(programIn(package, staging)).isFile())
        return fail(Error{QStringLiteral("tool_install"), QStringLiteral("no %1 in the package").arg(package.tool)});
    // Last: the marker says the folder is complete
    QFile marker(QDir(staging).filePath(markerName()));
    if (!marker.open(QIODevice::WriteOnly) || marker.write(package.sha256.toLatin1()) != package.sha256.size())
        return fail(Error{QStringLiteral("tool_install"), marker.errorString()});
    marker.close();
    if (cancelled && *cancelled)
        return fail(Error{QStringLiteral("tool_cancelled"), package.id});

    // The swap: the old folder moves aside first and comes back if the new
    // one can't move in, so there is always a working install
    QDir(old).removeRecursively();
    const bool hadOld = QFileInfo::exists(target);
    if (hadOld && !renameWithRetry(target, old))
        return fail(Error{QStringLiteral("tool_install"), QStringLiteral("%1 is in use").arg(QDir::toNativeSeparators(target))});
    if (!renameWithRetry(staging, target)) {
        if (hadOld)
            renameWithRetry(old, target);
        return fail(Error{QStringLiteral("tool_install"), QDir::toNativeSeparators(target)});
    }
    QDir(old).removeRecursively();
    return QDir::toNativeSeparators(programIn(package, target));
}

QObject *startToolInstall(const ToolPackage &package, QObject *owner, DownloadProgressHandler onProgress,
                          InstallDoneHandler onDone, const QString &toolsDir)
{
    auto *install = new Install(package, toolsDir, std::move(onProgress), std::move(onDone), owner); // owned by owner
    // Started from the event loop, so the handlers never run before this returns
    QTimer::singleShot(0, install, [install] { install->start(); });
    return install;
}

void cancelToolInstall(QObject *install)
{
    if (auto *running = dynamic_cast<Install *>(install))
        running->cancel();
}

} // namespace FloStudio::Core
