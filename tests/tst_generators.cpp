// The Generate tab's generators and the preview copy (libs/core
// generators.h, preview.h).

#include <flostudio/core/commands.h>
#include <flostudio/core/generators.h>
#include <flostudio/core/preview.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace FloStudio::Core;

namespace {

void writeFile(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QStringList fields(const QList<FieldProblem> &problems)
{
    QStringList list;
    for (const FieldProblem &problem : problems)
        list << problem.field + u'=' + problem.code;
    return list;
}

QString kinds(const QList<DiffLine> &lines)
{
    QString text;
    for (const DiffLine &line : lines)
        text += QLatin1Char(line.kind);
    return text;
}

bool isLinked(const QString &path)
{
    const QFileInfo info(path);
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    if (info.isJunction())
        return true;
#endif
    return info.isSymLink();
}

// A project with what the copy treats differently
void makeProject(const QString &dir)
{
    writeFile(dir + QStringLiteral("/composer.json"), "{\"require\":{\"hostkurd/flocms-core\":\"^2.9\"}}");
    writeFile(dir + QStringLiteral("/flo"), "<?php\n");
    writeFile(dir + QStringLiteral("/config/admin.php"), "<?php\nreturn [\n    'widgets' => [\n    ],\n];\n");
    writeFile(dir + QStringLiteral("/admin/Resources/PostResource.php"), "<?php // posts\n");
    writeFile(dir + QStringLiteral("/public/index.php"), "<?php\n");
    writeFile(dir + QStringLiteral("/public/uploads/big.jpg"), QByteArray(1000, 'x'));
    writeFile(dir + QStringLiteral("/storage/logs/app.log"), "log\n");
    writeFile(dir + QStringLiteral("/storage/database.sqlite"), QByteArray("SQLite format 3\0", 16));
    writeFile(dir + QStringLiteral("/node_modules/x/index.js"), "x\n");
    writeFile(dir + QStringLiteral("/.git/HEAD"), "ref\n");
    writeFile(dir + QStringLiteral("/vendor/autoload.php"), "<?php require __DIR__ . '/composer/autoload_real.php';\n");
    writeFile(dir + QStringLiteral("/vendor/composer/autoload_real.php"), "<?php // __DIR__\n");
    writeFile(dir + QStringLiteral("/vendor/hostkurd/flocms-cli/src/Kernel.php"), "<?php // the cli\n");
    writeFile(dir + QStringLiteral("/vendor/hostkurd/flocms-core/src/App.php"), "<?php // core\n");
    writeFile(dir + QStringLiteral("/vendor/bin/flo"), "proxy\n");
}

} // namespace

class TestGenerators : public QObject
{
    Q_OBJECT

private slots:
    void names()
    {
        for (const QString &name : {QStringLiteral("admin-resource"), QStringLiteral("admin-page"), QStringLiteral("block"),
                                    QStringLiteral("widget"), QStringLiteral("controller"), QStringLiteral("migration")}) {
            Generator generator{};
            QVERIFY(generatorFromName(name, &generator));
            QCOMPARE(generatorName(generator), name);
            QVERIFY(generatorCommand(generator).startsWith(QStringLiteral("make:")));
        }
        Generator none{};
        QVERIFY(!generatorFromName(QStringLiteral("model"), &none));
        QCOMPARE(generatorCommand(Generator::Block), QStringLiteral("make:admin-block"));
        QVERIFY(generatorNeedsAdmin(Generator::Widget));
        QVERIFY(!generatorNeedsAdmin(Generator::Controller));
    }

    // The arguments of board 3 and of `php flo help make:...`'s examples
    void arguments()
    {
        GeneratorForm form;
        form.name = QStringLiteral("Property");
        form.table = QStringLiteral("properties");
        form.slug = QStringLiteral("listings");
        form.migration = true;
        QCOMPARE(generatorArguments(Generator::AdminResource, form),
                 (QStringList{QStringLiteral("make:admin-resource"), QStringLiteral("Property"), QStringLiteral("--table=properties"),
                              QStringLiteral("--slug=listings"), QStringLiteral("--migration")}));
        form = {};
        form.name = QStringLiteral("City");
        form.formMode = QStringLiteral("modal");
        form.reorderable = true;
        form.force = true;
        QCOMPARE(generatorArguments(Generator::AdminResource, form),
                 (QStringList{QStringLiteral("make:admin-resource"), QStringLiteral("City"), QStringLiteral("--form=modal"),
                              QStringLiteral("--reorderable"), QStringLiteral("--force")}));
        form.formMode = QStringLiteral("page"); // FloCMS's default
        form.force = false;
        QCOMPARE(generatorArguments(Generator::AdminResource, form).size(), 3);

        form = {};
        form.name = QStringLiteral("SalesReport");
        form.pageKind = QStringLiteral("table");
        form.table = QStringLiteral("listings");
        form.group = QStringLiteral("Reports");
        form.view = true;
        form.resource = QStringLiteral("Post"); // not for a table page
        QCOMPARE(generatorArguments(Generator::AdminPage, form),
                 (QStringList{QStringLiteral("make:admin-page"), QStringLiteral("SalesReport"), QStringLiteral("--view"),
                              QStringLiteral("--table=listings"), QStringLiteral("--group=Reports")}));
        form.pageKind = QStringLiteral("form");
        form.view = false;
        form.group = QStringLiteral("Sales and stock");
        QCOMPARE(generatorArguments(Generator::AdminPage, form),
                 (QStringList{QStringLiteral("make:admin-page"), QStringLiteral("SalesReport"), QStringLiteral("--form"),
                              QStringLiteral("--group=Sales and stock")}));

        form = {};
        form.name = QStringLiteral("RecentPosts");
        form.widgetType = QStringLiteral("records");
        form.resource = QStringLiteral("Post");
        QCOMPARE(generatorArguments(Generator::Widget, form),
                 (QStringList{QStringLiteral("make:admin-widget"), QStringLiteral("RecentPosts"), QStringLiteral("--type=records"),
                              QStringLiteral("--resource=Post")}));
        form.widgetType = QStringLiteral("stats");
        form.resource.clear();
        QCOMPARE(generatorArguments(Generator::Widget, form), (QStringList{QStringLiteral("make:admin-widget"), QStringLiteral("RecentPosts")}));

        form = {};
        form.name = QStringLiteral("BlogPost");
        form.controllerKind = QStringLiteral("resource");
        form.model = true;
        QCOMPARE(generatorArguments(Generator::Controller, form),
                 (QStringList{QStringLiteral("make:controller"), QStringLiteral("BlogPost"), QStringLiteral("--resource"), QStringLiteral("--model")}));
        form.controllerKind = QStringLiteral("api");
        QCOMPARE(generatorArguments(Generator::Controller, form),
                 (QStringList{QStringLiteral("make:controller"), QStringLiteral("BlogPost"), QStringLiteral("--api")}));

        form = {};
        form.name = QStringLiteral("add_slug_to_posts");
        form.migrationKind = QStringLiteral("change");
        form.table = QStringLiteral("posts");
        QCOMPARE(generatorArguments(Generator::Migration, form),
                 (QStringList{QStringLiteral("make:migration"), QStringLiteral("add_slug_to_posts"), QStringLiteral("--table=posts")}));
        form.migrationKind = QStringLiteral("create");
        QCOMPARE(generatorArguments(Generator::Migration, form).at(2), QStringLiteral("--create=posts"));

        form = {};
        form.name = QStringLiteral(" LatestProjects ");
        QCOMPARE(generatorArguments(Generator::Block, form),
                 (QStringList{QStringLiteral("make:admin-block"), QStringLiteral("LatestProjects")}));
        // Every one is a command Core runs
        QVERIFY(isAllowed(Program::Flo, generatorArguments(Generator::Block, form)));
    }

    void checks()
    {
        GeneratorForm form;
        QCOMPARE(fields(checkGenerator(Generator::Block, form)), QStringList{QStringLiteral("name=name_empty")});
        form.name = QStringLiteral("latest-projects");
        QCOMPARE(fields(checkGenerator(Generator::Block, form)), QStringList{QStringLiteral("name=class_name")});
        QVERIFY(checkGenerator(Generator::Controller, form).isEmpty()); // blog-post is fine for a controller
        QCOMPARE(fields(checkGenerator(Generator::Migration, form)), QStringList{QStringLiteral("name=migration_name")});

        form.name = QStringLiteral("Property");
        form.table = QStringLiteral("2nd table");
        form.slug = QStringLiteral("Listings");
        QCOMPARE(fields(checkGenerator(Generator::AdminResource, form)),
                 (QStringList{QStringLiteral("table=table_name"), QStringLiteral("slug=slug_name")}));
        form.table.clear();
        form.slug.clear();
        QVERIFY(checkGenerator(Generator::AdminResource, form).isEmpty()); // FloCMS's defaults

        form.widgetType = QStringLiteral("records");
        QCOMPARE(fields(checkGenerator(Generator::Widget, form)), QStringList{QStringLiteral("resource=resource_needed")});
        form.resource = QStringLiteral("post");
        QCOMPARE(fields(checkGenerator(Generator::Widget, form)), QStringList{QStringLiteral("resource=resource_name")});

        form = {};
        form.name = QStringLiteral("SalesReport");
        form.pageKind = QStringLiteral("table");
        QCOMPARE(fields(checkGenerator(Generator::AdminPage, form)), QStringList{QStringLiteral("table=table_needed")});
        form.pageKind = QStringLiteral("resource");
        QCOMPARE(fields(checkGenerator(Generator::AdminPage, form)), QStringList{QStringLiteral("resource=resource_needed")});

        form = {};
        form.name = QStringLiteral("create_posts_table");
        QVERIFY(checkGenerator(Generator::Migration, form).isEmpty());
        form.migrationKind = QStringLiteral("change");
        QCOMPARE(fields(checkGenerator(Generator::Migration, form)), QStringList{QStringLiteral("table=table_needed")});
    }

    void theCopy()
    {
        const QTemporaryDir dir;
        const QString project = dir.filePath(QStringLiteral("my-shop"));
        makeProject(project);
        const QString previews = dir.filePath(QStringLiteral("previews"));
        const auto made = makePreviewCopy(project, previews);
        QVERIFY2(made.ok(), qUtf8Printable(made ? QString() : made.error().code));
        const QString copy = made.value();
        QCOMPARE(QFileInfo(copy).fileName(), QStringLiteral("my-shop"));
        // The project's files, without what generators don't need
        QCOMPARE(readFile(copy + QStringLiteral("/config/admin.php")), readFile(project + QStringLiteral("/config/admin.php")));
        QVERIFY(QFileInfo::exists(copy + QStringLiteral("/storage/database.sqlite")));
        QVERIFY(!QFileInfo::exists(copy + QStringLiteral("/public/uploads")));
        QVERIFY(!QFileInfo::exists(copy + QStringLiteral("/storage/logs")));
        QVERIFY(!QFileInfo::exists(copy + QStringLiteral("/node_modules")));
        QVERIFY(!QFileInfo::exists(copy + QStringLiteral("/.git")));
        // vendor/: the autoloader copied, the packages linked
        QVERIFY(!isLinked(copy + QStringLiteral("/vendor")));
        QVERIFY(!isLinked(copy + QStringLiteral("/vendor/composer")));
        QVERIFY(!isLinked(copy + QStringLiteral("/vendor/composer/autoload_real.php")));
        QVERIFY(!isLinked(copy + QStringLiteral("/vendor/hostkurd")));
        QVERIFY(isLinked(copy + QStringLiteral("/vendor/hostkurd/flocms-cli")));
        QCOMPARE(readFile(copy + QStringLiteral("/vendor/hostkurd/flocms-cli/src/Kernel.php")), QByteArray("<?php // the cli\n"));
        QVERIFY(!QFileInfo::exists(copy + QStringLiteral("/vendor/bin")));

        // What a generator does there
        writeFile(copy + QStringLiteral("/admin/Widgets/PostStats.php"), "<?php // stats\n");
        writeFile(copy + QStringLiteral("/config/admin.php"),
                  "<?php\nreturn [\n    'widgets' => [\n        \\App\\Admin\\Widgets\\PostStats::class,\n    ],\n];\n");
        writeFile(copy + QStringLiteral("/storage/logs/new.log"), "not compared\n");
        QVERIFY(QFile::remove(copy + QStringLiteral("/public/index.php")));
        const QList<FileChange> changes = previewChanges(project, copy);
        QCOMPARE(changes.size(), 3);
        QCOMPARE(changes.at(0).path, QStringLiteral("admin/Widgets/PostStats.php"));
        QCOMPARE(changes.at(0).kind, QStringLiteral("added"));
        QCOMPARE(changes.at(0).after, QByteArray("<?php // stats\n"));
        QCOMPARE(changes.at(1).path, QStringLiteral("config/admin.php"));
        QCOMPARE(changes.at(1).kind, QStringLiteral("changed"));
        QVERIFY(changes.at(1).before.contains("'widgets' => [\n    ],"));
        QCOMPARE(changes.at(2).kind, QStringLiteral("removed"));
        // Over the limit, or not text: no contents
        const QList<FileChange> small = previewChanges(project, copy, 10);
        QVERIFY(small.at(0).tooBig);
        QVERIFY(small.at(0).after.isEmpty());

        // Removed, links first: the project's packages stay
        QVERIFY(removePreviewCopy(copy).ok());
        QVERIFY(!QFileInfo::exists(copy));
        QVERIFY(!QFileInfo::exists(QFileInfo(copy).absolutePath()));
        QCOMPARE(readFile(project + QStringLiteral("/vendor/hostkurd/flocms-cli/src/Kernel.php")), QByteArray("<?php // the cli\n"));
        QVERIFY(QFileInfo::exists(project + QStringLiteral("/vendor/hostkurd/flocms-core/src/App.php")));
        QVERIFY(QFileInfo::exists(project + QStringLiteral("/public/index.php")));
        // Never a folder without the mark
        QCOMPARE(removePreviewCopy(project).error().code, QStringLiteral("preview_remove"));
        QVERIFY(QFileInfo::exists(project + QStringLiteral("/composer.json")));
    }

    void leftovers()
    {
        const QTemporaryDir dir;
        const QString project = dir.filePath(QStringLiteral("site"));
        makeProject(project);
        const QString previews = dir.filePath(QStringLiteral("previews"));
        QVERIFY(makePreviewCopy(project, previews).ok());
        QVERIFY(makePreviewCopy(project, previews).ok());
        writeFile(previews + QStringLiteral("/not-ours/keep.txt"), "x");
        QCOMPARE(QDir(previews).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 3);
        removeLeftoverPreviews(previews);
        QCOMPARE(QDir(previews).entryList(QDir::Dirs | QDir::NoDotAndDotDot), QStringList{QStringLiteral("not-ours")});
        QVERIFY(QFileInfo::exists(project + QStringLiteral("/vendor/hostkurd/flocms-cli/src/Kernel.php")));
    }

    void noVendor()
    {
        const QTemporaryDir dir;
        const QString project = dir.filePath(QStringLiteral("site"));
        makeProject(project);
        QVERIFY(QDir(project + QStringLiteral("/vendor")).removeRecursively());
        const QString previews = dir.filePath(QStringLiteral("previews"));
        const auto made = makePreviewCopy(project, previews);
        QCOMPARE(made.error().code, QStringLiteral("no_vendor"));
        QVERIFY(QDir(previews).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()); // nothing left
    }

    void cancelled()
    {
        const QTemporaryDir dir;
        const QString project = dir.filePath(QStringLiteral("site"));
        makeProject(project);
        const std::atomic<bool> stop{true};
        const auto made = makePreviewCopy(project, dir.filePath(QStringLiteral("previews")), &stop);
        QCOMPARE(made.error().code, QStringLiteral("preview_cancelled"));
    }

    void diffs()
    {
        // config/admin.php as make:admin-widget changes it
        const QString before = QStringLiteral("<?php\nreturn [\n    'a' => 1,\n    'b' => 2,\n    'c' => 3,\n    'widgets' => [\n    ],\n    'd' => 4,\n    'e' => 5,\n    'f' => 6,\n    'g' => 7,\n];\n");
        QString after = before;
        after.replace(QStringLiteral("'widgets' => [\n"), QStringLiteral("'widgets' => [\n        PostStats::class,\n"));
        const QList<DiffLine> lines = diffLines(before, after, 2);
        QCOMPARE(kinds(lines), QStringLiteral("~  +  ~"));
        QCOMPARE(lines.at(0).text, QStringLiteral("4"));
        QCOMPARE(lines.at(3).text, QStringLiteral("        PostStats::class,"));
        QCOMPARE(lines.at(6).text, QStringLiteral("5"));
        // A line changed: - then +
        QCOMPARE(kinds(diffLines(QStringLiteral("a\nb\nc"), QStringLiteral("a\nB\nc"), 1)), QStringLiteral(" -+ "));
        QCOMPARE(kinds(diffLines(QStringLiteral("same"), QStringLiteral("same"))), QString());
    }
};

QTEST_GUILESS_MAIN(TestGenerators)
#include "tst_generators.moc"
