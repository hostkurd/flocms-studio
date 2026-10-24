#pragma once

#include <flostudio/core/export.h>
#include <flostudio/core/result.h>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <atomic>

// The generator preview (D5): the real generator runs in a temporary copy
// of the project, Studio shows what it wrote, and the copy goes. Create
// then runs the same command in the project itself.
//
// The copy, in <previews>/<id>/<project folder name>:
// - the project's files, without .git, node_modules, the logs, caches,
//   sessions, compiled views and uploads, and without links;
// - vendor/ is a real folder: vendor/autoload.php and vendor/composer/ are
//   copied, every package folder (vendor/<vendor>/<package>) is a link to
//   the project's (a junction on Windows). Composer's autoloader works
//   from __DIR__, and PHP resolves links there, so a linked vendor/ would
//   load the project's own classes and write into the project.
// - <previews>/<id>/.flostudio-preview marks it as Studio's and names the
//   Studio that made it: only such a folder is ever removed, never while
//   that Studio runs (but by itself), and removing never follows a link
//   or a junction, so nothing is followed into the project's packages.
namespace FloStudio::Core {

// Where the copies go: Studio's cache folder, previews/.
FLOSTUDIOCORE_EXPORT QString previewsFolder();

// A new copy of the project. Returns the copy's project folder. Codes:
// no_vendor (composer install first), preview_too_big (over 400 MB or
// 40,000 files), preview_copy (with the path that failed),
// preview_cancelled.
FLOSTUDIOCORE_EXPORT Result<QString> makePreviewCopy(const QString &projectDir, const QString &previewsDir,
                                                     const std::atomic<bool> *cancelled = nullptr);

// Removes a copy (the folder makePreviewCopy returned). Code
// preview_remove when part of it stays; never a folder without the mark.
FLOSTUDIOCORE_EXPORT Result<Done> removePreviewCopy(const QString &copyDir);

// Removes every copy a Studio left behind (it closed while previewing).
FLOSTUDIOCORE_EXPORT void removeLeftoverPreviews(const QString &previewsDir);

// A file the generator added, changed or removed in the copy.
struct FileChange
{
    QString path;      // relative, with /
    QString kind;      // added, changed, removed
    QByteArray before; // the project's (changed, removed)
    QByteArray after;  // the copy's (added, changed)
    bool binary = false;
    bool tooBig = false; // over maxBytes: no contents
    bool operator==(const FileChange &) const = default;
};

// The files under dir that are compared (not the skipped folders, no
// links), one "path\tsize\ttime" line each.
FLOSTUDIOCORE_EXPORT QStringList fileSnapshot(const QString &dir);
// The paths added, removed or changed (size or time) between two snapshots.
FLOSTUDIOCORE_EXPORT QStringList changedSince(const QStringList &before, const QStringList &after);

// What the generator did in the copy: the copy now against `copied`, its
// snapshot right after it was made (so changes in the project meanwhile
// don't count). `before` comes from the project. In path order.
FLOSTUDIOCORE_EXPORT QList<FileChange> previewChanges(const QString &projectDir, const QString &copyDir,
                                                      const QStringList &copied, qint64 maxBytes = 256LL * 1024);

// A setting in the copy's config/admin.php (admin_pages_path, ...) that
// is an absolute path: generators would write there, outside the copy.
// Empty when there is none.
FLOSTUDIOCORE_EXPORT QString absoluteConfigPath(const QString &copyDir);

// A changed file's lines: kind ' ' (the same), '+', '-', or '~' for a run
// of unchanged lines left out (its text says how many).
struct DiffLine
{
    char kind = ' ';
    QString text;
    bool operator==(const DiffLine &) const = default;
};
FLOSTUDIOCORE_EXPORT QList<DiffLine> diffLines(const QString &before, const QString &after, int context = 3);

} // namespace FloStudio::Core
