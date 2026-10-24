#include <flostudio/core/server.h>

#include <flostudio/core/process.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

#include <algorithm>

#ifndef Q_OS_WIN
#include <csignal>
#include <sys/types.h>
#endif

namespace FloStudio::Core {

QStringList serveArguments(int port, bool network)
{
    return {QStringLiteral("serve"), QStringLiteral("--port=%1").arg(port),
            QStringLiteral("--host=%1").arg(network ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1"))};
}

QString localUrl(const QString &serveUrl)
{
    QUrl url(serveUrl);
    if (!url.isValid() || url.host().isEmpty())
        return serveUrl;
    if (url.host() == u"0.0.0.0" || url.host() == u"::")
        url.setHost(QStringLiteral("127.0.0.1"));
    return url.toString();
}

QStringList networkUrls(const QString &serveUrl)
{
    const QUrl url(serveUrl);
    if (!url.isValid() || url.port() <= 0)
        return {};
    QStringList urls;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack))
            continue;
        const QList<QNetworkAddressEntry> entries = interface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            const QHostAddress address = entry.ip();
            if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isLoopback() || address.isLinkLocal())
                continue;
            QUrl other = url;
            other.setHost(address.toString());
            if (!urls.contains(other.toString()))
                urls << other.toString();
        }
    }
    return urls;
}

int urlPort(const QString &url)
{
    return std::max(0, QUrl(url).port());
}

QString appLogPath(const QString &projectDir)
{
    return QDir(projectDir).filePath(QStringLiteral("storage/logs/app.log"));
}

namespace {

constexpr qint64 kHeadBytes = 64;

QStringList splitLines(const QByteArray &bytes)
{
    QStringList lines = QString::fromUtf8(bytes).split(u'\n');
    for (QString &line : lines) {
        if (line.endsWith(u'\r'))
            line.chop(1);
    }
    return lines;
}

} // namespace

LogRead readLogTail(const QString &path, int count)
{
    LogRead read;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        read.missing = !QFileInfo::exists(path);
        return read;
    }
    const qint64 size = file.size();
    read.head = file.read(kHeadBytes);
    // Back from the end in blocks until there are enough line ends
    QByteArray buffer;
    qint64 position = size;
    while (position > 0 && buffer.count('\n') <= count) {
        const qint64 block = std::min<qint64>(qint64{64} * 1024, position);
        position -= block;
        if (!file.seek(position))
            break;
        buffer.prepend(file.read(block));
    }
    // A last line still being written waits for the next read
    const qsizetype end = buffer.lastIndexOf('\n');
    if (end < 0) {
        read.offset = position;
        return read;
    }
    read.offset = position + end + 1;
    QStringList lines = splitLines(buffer.left(end));
    if (position > 0 && !lines.isEmpty())
        lines.removeFirst(); // cut in the middle
    if (lines.size() > count)
        lines = lines.mid(lines.size() - count);
    read.lines = lines;
    return read;
}

LogRead readLogFrom(const QString &path, qint64 offset, qint64 maxBytes, const QByteArray &head)
{
    LogRead read;
    read.offset = offset;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        read.missing = !QFileInfo::exists(path);
        read.restarted = read.missing && offset > 0;
        read.offset = 0;
        return read;
    }
    const qint64 size = file.size();
    read.head = file.read(kHeadBytes);
    // Shorter, or other first bytes: not the file read before
    const bool sameStart = head.isEmpty() || read.head.startsWith(head) || head.startsWith(read.head);
    if (size < offset || !sameStart) {
        read.restarted = true;
        offset = 0;
    }
    if (size == offset || !file.seek(offset)) {
        read.offset = offset;
        return read;
    }
    const QByteArray chunk = file.read(std::min(size - offset, maxBytes));
    const qsizetype end = chunk.lastIndexOf('\n');
    if (end < 0) {
        // One line longer than maxBytes is taken as it is; a shorter one waits
        if (chunk.size() >= maxBytes) {
            read.lines = splitLines(chunk);
            read.offset = offset + chunk.size();
        } else {
            read.offset = offset;
        }
        return read;
    }
    read.lines = splitLines(chunk.left(end));
    read.offset = offset + end + 1;
    return read;
}

// --- Leftover servers ----------------------------------------------------

QString serverRecordFile(const QString &settingsDir)
{
    return QDir(settingsDir).filePath(QStringLiteral("servers.json"));
}

namespace {

QJsonObject readRecord(const QString &settingsDir)
{
    QFile file(serverRecordFile(settingsDir));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

void writeRecord(const QString &settingsDir, const QJsonObject &record)
{
    if (!QDir().mkpath(settingsDir))
        return;
    QSaveFile file(serverRecordFile(settingsDir));
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(record).toJson(QJsonDocument::Compact));
    file.commit();
}

} // namespace

void recordServer(const QString &settingsDir, const QString &projectDir, qint64 group)
{
#ifdef Q_OS_WIN
    Q_UNUSED(settingsDir)
    Q_UNUSED(projectDir)
    Q_UNUSED(group)
#else
    QJsonObject record = readRecord(settingsDir);
    if (group > 0)
        record.insert(projectDir, QJsonObject{{QStringLiteral("group"), static_cast<double>(group)},
                                              {QStringLiteral("owner"), static_cast<double>(QCoreApplication::applicationPid())}});
    else
        record.remove(projectDir);
    writeRecord(settingsDir, record);
#endif
}

PsLine readPsLine(const QString &line)
{
    static const QRegularExpression pattern(QStringLiteral("^\\s*(\\d+)\\s+(.+)$"));
    const QRegularExpressionMatch match = pattern.match(line);
    if (!match.hasMatch())
        return {};
    return {match.captured(1).toLongLong(), match.captured(2).trimmed()};
}

bool isBuiltInServer(const QString &command)
{
    // php -S 127.0.0.1:8000 -t public router.php (any php program name)
    static const QRegularExpression pattern(QStringLiteral("(^|/)php[0-9.]*(\\s.*)?\\s-S\\s+\\S+:\\d+"));
    return pattern.match(command).hasMatch();
}

bool isBuiltInServerOf(const QString &command, const QString &projectDir)
{
    if (!isBuiltInServer(command))
        return false;
    // `php flo serve` serves <project>/public (-t), by the path it was given
    // or the real one (macOS's /tmp is /private/tmp)
    QStringList roots{QDir::cleanPath(projectDir)};
    const QString real = QFileInfo(projectDir).canonicalFilePath();
    if (!real.isEmpty() && !roots.contains(real))
        roots << real;
    return std::any_of(roots.begin(), roots.end(), [&command](const QString &root) {
        return command.contains(QStringLiteral(" -t ") + root + QStringLiteral("/public"));
    });
}

QList<LeftoverServer> stopLeftoverServers(const QString &settingsDir)
{
    QList<LeftoverServer> stopped;
#ifdef Q_OS_WIN
    Q_UNUSED(settingsDir)
#else
    const QJsonObject record = readRecord(settingsDir);
    if (record.isEmpty())
        return stopped;
    Command ps;
    ps.program = QStringLiteral("ps");
    ps.arguments = {QStringLiteral("-A"), QStringLiteral("-o"), QStringLiteral("pgid=,command=")};
    const Captured listed = runCaptured(ps, 5000);
    if (!listed.succeeded())
        return stopped; // the notes stay for the next start
    const QStringList lines = listed.out.split(u'\n', Qt::SkipEmptyParts);
    const auto self = static_cast<qint64>(QCoreApplication::applicationPid());
    QJsonObject kept;
    for (auto it = record.begin(); it != record.end(); ++it) {
        const QJsonObject entry = it.value().toObject();
        const auto group = static_cast<qint64>(entry.value(QStringLiteral("group")).toDouble());
        const auto owner = static_cast<qint64>(entry.value(QStringLiteral("owner")).toDouble());
        // Another Studio that still runs owns it: its server is left alone
        if (owner > 0 && owner != self && ::kill(static_cast<pid_t>(owner), 0) == 0) {
            kept.insert(it.key(), entry);
            continue;
        }
        // Only a group that still holds this project's PHP built-in server:
        // a group id used again by something else is left alone
        const QString projectDir = it.key();
        const bool server = group > 1 && std::any_of(lines.begin(), lines.end(), [group, &projectDir](const QString &line) {
            const PsLine row = readPsLine(line);
            return row.group == group && isBuiltInServerOf(row.command, projectDir);
        });
        if (server && ::kill(static_cast<pid_t>(-group), SIGTERM) == 0)
            stopped << LeftoverServer{projectDir, group};
    }
    writeRecord(settingsDir, kept);
#endif
    return stopped;
}

} // namespace FloStudio::Core
