#include <flostudio/core/preview.h>

#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QRegularExpression>
#include <QMap>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

#include <algorithm>
#include <cstring>
#include <vector>

#ifdef Q_OS_WIN
// windows.h without its min and max macros (std::max below)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#else
#include <cerrno>
#include <csignal>
#endif

namespace FloStudio::Core {

namespace {

constexpr qint64 kMaxBytes = 400LL * 1024 * 1024;
constexpr int kMaxFiles = 40000;
constexpr const char *kMark = ".flostudio-preview";

// Not copied, and not compared: what generators never read or write, and
// what can be large
bool skipped(const QString &relative)
{
    static const QStringList folders{
        QStringLiteral(".git"),          QStringLiteral("node_modules"),           QStringLiteral("vendor"),
        QStringLiteral("storage/logs"),  QStringLiteral("storage/cache"),          QStringLiteral("storage/framework/cache"),
        QStringLiteral("storage/framework/sessions"), QStringLiteral("storage/framework/views"),
        QStringLiteral("public/uploads"), QStringLiteral("storage/uploads"),
    };
    return std::any_of(folders.begin(), folders.end(), [&relative](const QString &folder) {
        return relative == folder || relative.startsWith(folder + u'/');
    });
}

QFileInfoList entries(const QString &dir)
{
    return QDir(dir).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
}

// A link or a junction: never followed, never copied
bool isLink(const QFileInfo &info)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    if (info.isJunction())
        return true;
#endif
    return info.isSymLink();
}

struct Copying
{
    const std::atomic<bool> *cancelled = nullptr;
    qint64 bytes = 0;
    int files = 0;
};

Result<Done> copyTree(const QString &from, const QString &to, const QString &relative, Copying *copying)
{
    if (!QDir().mkpath(to))
        return Error{QStringLiteral("preview_copy"), QDir::toNativeSeparators(to)};
    for (const QFileInfo &info : entries(from)) {
        if (copying->cancelled && copying->cancelled->load())
            return Error{QStringLiteral("preview_cancelled"), {}};
        const QString path = relative.isEmpty() ? info.fileName() : relative + u'/' + info.fileName();
        if (skipped(path) || isLink(info))
            continue;
        const QString target = to + u'/' + info.fileName();
        if (info.isDir()) {
            if (auto copied = copyTree(info.filePath(), target, path, copying); !copied)
                return copied;
            continue;
        }
        copying->bytes += info.size();
        if (++copying->files > kMaxFiles || copying->bytes > kMaxBytes)
            return Error{QStringLiteral("preview_too_big"), {}};
        if (!QFile::copy(info.filePath(), target))
            return Error{QStringLiteral("preview_copy"), QDir::toNativeSeparators(info.filePath())};
    }
    return Done{};
}

#ifdef Q_OS_WIN
// A junction (a mount point), which needs no rights a symbolic link needs.
// REPARSE_DATA_BUFFER is in the driver kit only, so its mount point part
// is written out here.
bool makeJunction(const QString &link, const QString &target)
{
    if (!QDir().mkdir(link))
        return false;
    const std::wstring path = QDir::toNativeSeparators(link).toStdWString();
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        QDir().rmdir(link);
        return false;
    }
    const std::wstring print = QDir::toNativeSeparators(target).toStdWString();
    const std::wstring substitute = L"\\??\\" + print;
    const auto substituteBytes = static_cast<WORD>(substitute.size() * sizeof(wchar_t));
    const auto printBytes = static_cast<WORD>(print.size() * sizeof(wchar_t));
    // The four name fields, then both names with their terminating nulls
    const auto dataLength = static_cast<WORD>(4 * sizeof(WORD) + substituteBytes + sizeof(wchar_t) + printBytes + sizeof(wchar_t));
    const auto total = static_cast<DWORD>(sizeof(DWORD) + 2 * sizeof(WORD) + dataLength);
    std::vector<BYTE> buffer(total, 0);
    BYTE *at = buffer.data();
    const auto put = [&at](const void *data, size_t size) {
        std::memcpy(at, data, size);
        at += size;
    };
    const DWORD tag = IO_REPARSE_TAG_MOUNT_POINT;
    const WORD reserved = 0;
    const WORD substituteOffset = 0;
    const auto printOffset = static_cast<WORD>(substituteBytes + sizeof(wchar_t));
    const wchar_t null = 0;
    put(&tag, sizeof(tag));
    put(&dataLength, sizeof(dataLength));
    put(&reserved, sizeof(reserved));
    put(&substituteOffset, sizeof(substituteOffset));
    put(&substituteBytes, sizeof(substituteBytes));
    put(&printOffset, sizeof(printOffset));
    put(&printBytes, sizeof(printBytes));
    put(substitute.data(), substituteBytes);
    put(&null, sizeof(null));
    put(print.data(), printBytes);
    put(&null, sizeof(null));
    DWORD returned = 0;
    const BOOL ok = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer.data(), total, nullptr, 0, &returned, nullptr);
    CloseHandle(handle);
    if (!ok) {
        QDir().rmdir(link);
        return false;
    }
    return true;
}
#endif

bool makeLink(const QString &link, const QString &target)
{
#ifdef Q_OS_WIN
    return makeJunction(link, target);
#else
    return QFile::link(target, link);
#endif
}

// A link (or junction) itself, never what it leads to
bool removeLink(const QString &path)
{
#ifdef Q_OS_WIN
    // RemoveDirectoryW removes a junction, not its target's contents
    return RemoveDirectoryW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(path).utf16())) != 0;
#else
    return QFile::remove(path);
#endif
}

Result<Done> makeVendor(const QString &projectDir, const QString &copyDir, Copying *copying)
{
    const QString vendor = projectDir + QStringLiteral("/vendor");
    const QString copyVendor = copyDir + QStringLiteral("/vendor");
    if (!QFileInfo::exists(vendor + QStringLiteral("/autoload.php")))
        return Error{QStringLiteral("no_vendor"), QDir::toNativeSeparators(projectDir)};
    if (!QDir().mkpath(copyVendor))
        return Error{QStringLiteral("preview_copy"), QDir::toNativeSeparators(copyVendor)};
    for (const QFileInfo &info : entries(vendor)) {
        const QString target = copyVendor + u'/' + info.fileName();
        if (info.fileName() == u"bin")
            continue; // proxies for the packages' programs: not used
        if (info.fileName() == u"composer" && info.isDir()) {
            // The autoloader: real files, so its __DIR__ is the copy's
            if (auto copied = copyTree(info.filePath(), target, QStringLiteral("vendor-composer"), copying); !copied)
                return copied;
            continue;
        }
        if (!info.isDir()) {
            if (!QFile::copy(info.filePath(), target))
                return Error{QStringLiteral("preview_copy"), QDir::toNativeSeparators(info.filePath())};
            continue;
        }
        // vendor/<vendor>: a real folder of links to the packages
        if (!QDir().mkpath(target))
            return Error{QStringLiteral("preview_copy"), QDir::toNativeSeparators(target)};
        for (const QFileInfo &package : entries(info.filePath())) {
            if (copying->cancelled && copying->cancelled->load())
                return Error{QStringLiteral("preview_cancelled"), {}};
            const QString link = target + u'/' + package.fileName();
            const QString real = package.canonicalFilePath();
            if (real.isEmpty())
                continue; // a link that leads nowhere
            if (!package.isDir()) {
                if (!QFile::copy(real, link))
                    return Error{QStringLiteral("preview_copy"), QDir::toNativeSeparators(package.filePath())};
                continue;
            }
            if (!makeLink(link, real))
                return Error{QStringLiteral("preview_copy"), QDir::toNativeSeparators(link)};
        }
    }
    return Done{};
}

bool isText(const QByteArray &bytes)
{
    return !bytes.contains('\0');
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

// Every file under dir that is compared, relative, with its size
// Every file under dir that is compared, relative, with its size and time
void listFiles(const QString &dir, const QString &relative, QStringList *files)
{
    for (const QFileInfo &info : entries(relative.isEmpty() ? dir : dir + u'/' + relative)) {
        const QString path = relative.isEmpty() ? info.fileName() : relative + u'/' + info.fileName();
        if (skipped(path) || isLink(info))
            continue;
        if (info.isDir())
            listFiles(dir, path, files);
        else
            *files << path + u'\t' + QString::number(info.size()) + u'\t'
                          + QString::number(info.lastModified().toMSecsSinceEpoch());
    }
}

// "path\tsize\ttime" → path: "size\ttime"
QMap<QString, QString> byPath(const QStringList &snapshot)
{
    QMap<QString, QString> map;
    for (const QString &line : snapshot) {
        const qsizetype tab = line.indexOf(u'\t');
        map.insert(line.left(tab), line.mid(tab + 1));
    }
    return map;
}

qint64 sizeOf(const QString &state)
{
    return state.section(u'\t', 0, 0).toLongLong();
}

// Removes dir and what is in it, never following a link or a junction:
// those are removed themselves (Qt's removeRecursively only stops at
// symbolic links, and walks into junctions)
bool removeTree(const QString &dir)
{
    bool removed = true;
    for (const QFileInfo &info : entries(dir)) {
        if (isLink(info)) {
            removed = removeLink(info.filePath()) && removed;
        } else if (info.isDir()) {
            removed = removeTree(info.filePath()) && removed;
        } else {
            QFile file(info.filePath());
            file.setPermissions(file.permissions() | QFileDevice::WriteOwner);
            removed = file.remove() && removed;
        }
    }
    return removed && QDir().rmdir(dir);
}

// The mark: the Studio that made the copy, then the project
bool ownerRuns(const QString &markPath)
{
    QFile mark(markPath);
    if (!mark.open(QIODevice::ReadOnly))
        return false;
    const qint64 pid = QString::fromUtf8(mark.readLine()).trimmed().toLongLong();
    if (pid <= 0)
        return false;
    if (pid == QCoreApplication::applicationPid())
        return true; // this Studio's own: it removes them itself
#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!process)
        return false;
    DWORD code = 0;
    const bool running = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    CloseHandle(process);
    return running;
#else
    return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

} // namespace

QString previewsFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/previews");
}

Result<QString> makePreviewCopy(const QString &projectDir, const QString &previewsDir, const std::atomic<bool> *cancelled)
{
    const QString project = QDir(projectDir).absolutePath();
    const QString root = QDir(previewsDir).filePath(QUuid::createUuid().toString(QUuid::Id128));
    if (!QDir().mkpath(root))
        return Error{QStringLiteral("preview_copy"), QDir::toNativeSeparators(root)};
    // The mark first, so a copy that stops half way is still removed
    QFile mark(root + u'/' + QLatin1String(kMark));
    const QByteArray marked = QByteArray::number(QCoreApplication::applicationPid()) + '\n' + project.toUtf8();
    if (!mark.open(QIODevice::WriteOnly) || mark.write(marked) < 0) {
        QDir().rmdir(root);
        return Error{QStringLiteral("preview_copy"), QDir::toNativeSeparators(root)};
    }
    mark.close();
    const QString copy = root + u'/' + QFileInfo(project).fileName();
    Copying copying;
    copying.cancelled = cancelled;
    auto made = copyTree(project, copy, {}, &copying);
    if (made)
        made = makeVendor(project, copy, &copying);
    if (!made) {
        removePreviewCopy(copy);
        return made.error();
    }
    return copy;
}

Result<Done> removePreviewCopy(const QString &copyDir)
{
    const QString root = QFileInfo(copyDir).absolutePath();
    if (!QFileInfo::exists(root + u'/' + QLatin1String(kMark)))
        return Error{QStringLiteral("preview_remove"), QDir::toNativeSeparators(copyDir)};
    bool removed = true;
    // The package links one by one: removing them as folders could follow
    // them into the project's own vendor/
    const QString vendor = copyDir + QStringLiteral("/vendor");
    for (const QFileInfo &folder : entries(vendor)) {
        if (!folder.isDir() || isLink(folder) || folder.fileName() == u"composer")
            continue;
        for (const QFileInfo &package : entries(folder.filePath())) {
            if (isLink(package) && !removeLink(package.filePath()))
                removed = false;
        }
    }
    // Anything still a link stops the rest: nothing is removed through one
    for (const QFileInfo &folder : entries(vendor)) {
        if (!folder.isDir() || folder.fileName() == u"composer")
            continue;
        const QFileInfoList left = entries(folder.filePath());
        if (std::any_of(left.begin(), left.end(), [](const QFileInfo &info) { return isLink(info); }))
            return Error{QStringLiteral("preview_remove"), QDir::toNativeSeparators(folder.filePath())};
    }
    if (removed && removeTree(copyDir))
        removed = QFile::remove(root + u'/' + QLatin1String(kMark)) && QDir().rmdir(root);
    else
        removed = false;
    if (!removed)
        return Error{QStringLiteral("preview_remove"), QDir::toNativeSeparators(copyDir)};
    return Done{};
}

void removeLeftoverPreviews(const QString &previewsDir)
{
    for (const QFileInfo &root : entries(previewsDir)) {
        const QString mark = root.filePath() + u'/' + QLatin1String(kMark);
        // A copy whose Studio still runs is that Studio's (it may be making it)
        if (!root.isDir() || isLink(root) || !QFileInfo::exists(mark) || ownerRuns(mark))
            continue;
        bool any = false;
        for (const QFileInfo &copy : entries(root.filePath())) {
            if (copy.isDir() && !isLink(copy)) {
                any = true;
                removePreviewCopy(copy.filePath());
            }
        }
        // Stopped before the copy began
        if (!any) {
            QFile::remove(root.filePath() + u'/' + QLatin1String(kMark));
            QDir().rmdir(root.filePath());
        }
    }
}

QStringList fileSnapshot(const QString &dir)
{
    QStringList files;
    listFiles(dir, {}, &files);
    return files;
}

QStringList changedSince(const QStringList &before, const QStringList &after)
{
    const QMap<QString, QString> was = byPath(before);
    const QMap<QString, QString> now = byPath(after);
    QStringList paths;
    for (auto it = now.begin(); it != now.end(); ++it) {
        if (was.value(it.key()) != it.value())
            paths << it.key();
    }
    for (auto it = was.begin(); it != was.end(); ++it) {
        if (!now.contains(it.key()))
            paths << it.key();
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

QList<FileChange> previewChanges(const QString &projectDir, const QString &copyDir, const QStringList &copied, qint64 maxBytes)
{
    // The copy as it was made against the copy now: what the generator
    // did, not what changed in the project meanwhile
    const QMap<QString, QString> was = byPath(copied);
    const QMap<QString, QString> now = byPath(fileSnapshot(copyDir));
    QList<FileChange> changes;
    for (const QString &path : changedSince(copied, fileSnapshot(copyDir))) {
        const bool before = was.contains(path);
        const bool after = now.contains(path);
        FileChange change;
        change.path = path;
        change.kind = !before ? QStringLiteral("added") : (!after ? QStringLiteral("removed") : QStringLiteral("changed"));
        // Written again with the same bytes: no change
        const QString original = projectDir + u'/' + path;
        if (before && after && sizeOf(was.value(path)) == sizeOf(now.value(path))
            && readAll(original) == readAll(copyDir + u'/' + path))
            continue;
        change.tooBig = (before && sizeOf(was.value(path)) > maxBytes) || (after && sizeOf(now.value(path)) > maxBytes);
        if (!change.tooBig) {
            if (before)
                change.before = readAll(original);
            if (after)
                change.after = readAll(copyDir + u'/' + path);
            change.binary = !isText(change.before) || !isText(change.after);
            if (change.binary) {
                change.before.clear();
                change.after.clear();
            }
        }
        changes << change;
    }
    return changes;
}

QString absoluteConfigPath(const QString &copyDir)
{
    // flocms-admin's *_path settings (admin_pages_path, widgets_path, ...)
    // are used as they are when absolute: a generator would write there
    static const QRegularExpression setting(
        QStringLiteral(R"(['"](\w+_path)['"]\s*=>\s*['"](/|\\\\|[A-Za-z]:[\\/]))"));
    QFile file(copyDir + QStringLiteral("/config/admin.php"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QString text = QString::fromUtf8(file.readAll());
    for (const QString &line : text.split(u'\n')) {
        if (line.trimmed().startsWith(QStringLiteral("//")))
            continue;
        if (const auto match = setting.match(line); match.hasMatch())
            return match.captured(1);
    }
    return {};
}

QList<DiffLine> diffLines(const QString &before, const QString &after, int context)
{
    const QStringList a = before.split(u'\n');
    const QStringList b = after.split(u'\n');
    // The common start and end, then the shortest edit of the middle
    qsizetype start = 0;
    while (start < a.size() && start < b.size() && a.at(start) == b.at(start))
        ++start;
    qsizetype endA = a.size();
    qsizetype endB = b.size();
    while (endA > start && endB > start && a.at(endA - 1) == b.at(endB - 1)) {
        --endA;
        --endB;
    }
    QList<DiffLine> all;
    for (qsizetype i = 0; i < start; ++i)
        all << DiffLine{' ', a.at(i)};
    const qsizetype n = endA - start;
    const qsizetype m = endB - start;
    if (n * m <= 4'000'000) {
        // Longest common subsequence of the middle
        std::vector<std::vector<int>> lcs(static_cast<size_t>(n + 1), std::vector<int>(static_cast<size_t>(m + 1), 0));
        for (qsizetype i = n - 1; i >= 0; --i) {
            for (qsizetype j = m - 1; j >= 0; --j) {
                const auto si = static_cast<size_t>(i);
                const auto sj = static_cast<size_t>(j);
                lcs[si][sj] = a.at(start + i) == b.at(start + j) ? lcs[si + 1][sj + 1] + 1
                                                                  : std::max(lcs[si + 1][sj], lcs[si][sj + 1]);
            }
        }
        qsizetype i = 0;
        qsizetype j = 0;
        while (i < n || j < m) {
            const auto si = static_cast<size_t>(i);
            const auto sj = static_cast<size_t>(j);
            if (i < n && j < m && a.at(start + i) == b.at(start + j)) {
                all << DiffLine{' ', a.at(start + i)};
                ++i;
                ++j;
            } else if (i < n && (j == m || lcs[si + 1][sj] >= lcs[si][sj + 1])) {
                // What goes before what comes in its place
                all << DiffLine{'-', a.at(start + i)};
                ++i;
            } else {
                all << DiffLine{'+', b.at(start + j)};
                ++j;
            }
        }
    } else {
        for (qsizetype i = start; i < endA; ++i)
            all << DiffLine{'-', a.at(i)};
        for (qsizetype j = start; j < endB; ++j)
            all << DiffLine{'+', b.at(j)};
    }
    for (qsizetype i = endA; i < a.size(); ++i)
        all << DiffLine{' ', a.at(i)};

    // Only the changes and their context
    std::vector<bool> keep(static_cast<size_t>(all.size()), false);
    for (qsizetype i = 0; i < all.size(); ++i) {
        if (all.at(i).kind == ' ')
            continue;
        for (qsizetype k = std::max<qsizetype>(0, i - context); k <= std::min<qsizetype>(all.size() - 1, i + context); ++k)
            keep[static_cast<size_t>(k)] = true;
    }
    QList<DiffLine> shown;
    qsizetype left = 0;
    for (qsizetype i = 0; i < all.size(); ++i) {
        if (keep[static_cast<size_t>(i)]) {
            if (left > 0)
                shown << DiffLine{'~', QString::number(left)};
            left = 0;
            shown << all.at(i);
        } else {
            ++left;
        }
    }
    if (left > 0 && !shown.isEmpty())
        shown << DiffLine{'~', QString::number(left)};
    return shown;
}

} // namespace FloStudio::Core
