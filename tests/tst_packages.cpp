// The Packages tab's Composer output and commands (libs/core packages.h),
// read from real output (tests/fixtures/composer).

#include <flostudio/core/commands.h>
#include <flostudio/core/packages.h>

#include <QFile>
#include <QTest>

using namespace FloStudio::Core;

namespace {

QString fixture(const QString &name)
{
    QFile file(QStringLiteral(FLOSTUDIO_FIXTURES "/composer/") + name);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

const PackageStatus *find(const QList<PackageStatus> &packages, const QString &name)
{
    for (const PackageStatus &package : packages) {
        if (package.name == name)
            return &package;
    }
    return nullptr;
}

} // namespace

class TestPackages : public QObject
{
    Q_OBJECT

private slots:
    void readsOutdated()
    {
        // Right after create-project: flocms-core 2.8.0, with 2.9.0 out
        const auto all = parseOutdated(fixture(QStringLiteral("outdated-all.out")));
        QVERIFY(all);
        QCOMPARE(all->size(), 5);
        const PackageStatus *core = find(*all, QStringLiteral("hostkurd/flocms-core"));
        QVERIFY(core);
        QCOMPARE(core->version, QStringLiteral("2.8.0"));
        QCOMPARE(core->latest, QStringLiteral("2.9.0"));
        QCOMPARE(core->status, QStringLiteral("semver-safe-update"));
        QCOMPARE(core->repository, QStringLiteral("https://github.com/hostkurd/flocms-core"));
        QVERIFY(!core->abandoned);
        const PackageStatus *cli = find(*all, QStringLiteral("hostkurd/flocms-cli"));
        QCOMPARE(cli->status, QStringLiteral("up-to-date"));
        QCOMPARE(cli->repository, QStringLiteral("https://github.com/hostkurd/flocms-cli"));
        // Not hostkurd's: no repository link
        QCOMPARE(find(*all, QStringLiteral("phpunit/phpunit"))->repository, QString());

        // Without --all, only what is newer
        const auto older = parseOutdated(fixture(QStringLiteral("outdated-older.out")));
        QVERIFY(older);
        QCOMPARE(older->size(), 2);
        QCOMPARE(find(*older, QStringLiteral("hostkurd/flocms-uploader"))->version, QStringLiteral("1.2.0"));
        QCOMPARE(find(*older, QStringLiteral("hostkurd/flocms-uploader"))->latest, QStringLiteral("1.2.1"));

        QVERIFY(!parseOutdated(QStringLiteral("Could not fetch https://repo.packagist.org")));
        QVERIFY(parseOutdated(QStringLiteral("Warning: something\n{\"installed\": []}"))->isEmpty());
    }

    void versions()
    {
        QCOMPARE(plainVersion(QStringLiteral("v2.9.0")), QStringLiteral("2.9.0"));
        QCOMPARE(plainVersion(QStringLiteral("10.5.66")), QStringLiteral("10.5.66"));
        QCOMPARE(plainVersion(QStringLiteral("dev-main")), QStringLiteral("dev-main"));
        QCOMPARE(hostkurdRepository(QStringLiteral("https://github.com/hostkurd/flocms-api.git")), QStringLiteral("https://github.com/hostkurd/flocms-api"));
        QCOMPARE(hostkurdRepository(QStringLiteral("https://github.com/sebastianbergmann/phpunit/tree/10.5.66")), QString());
        QCOMPARE(hostkurdRepository(QStringLiteral("https://github.com.evil/hostkurd/x")), QString());
        QVERIFY(hasUpgradeNotes(QStringLiteral("hostkurd/flocms-admin")));
        QVERIFY(!hasUpgradeNotes(QStringLiteral("hostkurd/flocms-core")));
    }

    // Board 8's commands, and what the capture ran
    void steps()
    {
        QList<PackageStep> steps = updateSteps(QStringLiteral("hostkurd/flocms-admin"), QStringLiteral("v0.39.3"));
        QCOMPARE(steps.size(), 3);
        QCOMPARE(steps.at(0).program, Program::Composer);
        QCOMPARE(steps.at(0).arguments,
                 (QStringList{QStringLiteral("require"), QStringLiteral("hostkurd/flocms-admin:^0.39.3"), QStringLiteral("-W")}));
        QCOMPARE(steps.at(1).arguments, QStringList{QStringLiteral("admin:install")});
        QCOMPARE(steps.at(2).arguments, (QStringList{QStringLiteral("migrate"), QStringLiteral("--force")}));
        // A package without an installer: require and migrate
        steps = updateSteps(QStringLiteral("hostkurd/flocms-uploader"), QStringLiteral("1.2.1"));
        QCOMPARE(steps.size(), 2);
        QCOMPARE(steps.at(0).arguments.at(1), QStringLiteral("hostkurd/flocms-uploader:^1.2.1"));
        QCOMPARE(updateSteps(QStringLiteral("hostkurd/flocms-api"), QStringLiteral("1.3.0")).at(1).arguments,
                 QStringList{QStringLiteral("api:install")});

        steps = addSteps(QStringLiteral("hostkurd/flocms-api"));
        QCOMPARE(steps.size(), 3);
        QCOMPARE(steps.at(0).arguments, (QStringList{QStringLiteral("require"), QStringLiteral("hostkurd/flocms-api")}));
        QCOMPARE(addSteps(QStringLiteral("hostkurd/flocms-admin")).at(1).arguments, QStringList{QStringLiteral("admin:install")});
        QVERIFY(addSteps(QStringLiteral("psr/log")).isEmpty());
        // Every command is one Core runs
        for (const PackageStep &each : updateSteps(QStringLiteral("hostkurd/flocms-admin"), QStringLiteral("0.39.3")) + steps)
            QVERIFY(isAllowed(each.program, each.arguments));
        QVERIFY(isAllowed(Program::Composer, outdatedArguments()));
    }
};

QTEST_GUILESS_MAIN(TestPackages)
#include "tst_packages.moc"
