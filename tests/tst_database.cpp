#include <flostudio/database/actions.h>
#include <flostudio/database/library.h>

#include <flostudio/core/commands.h>

#include <flostudio/core/buildinfo.h>

#include <QTest>

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void libraryName()
    {
        QCOMPARE(FloStudio::Database::libraryName(), QStringLiteral("FloStudioDatabase"));
    }

    // D5: Database uses Core, and Core comes through that link only.
    void dependsOnCore()
    {
        QCOMPARE(FloStudio::Database::dependencies(), QStringList{FloStudio::Core::libraryName()});
    }

    // --- Stage 5: what the Database tab asks and runs ------------------------

    void whatEachActionAsks()
    {
        using namespace FloStudio::Database;
        using FloStudio::Core::isAllowed;
        using FloStudio::Core::Program;
        // Migrate asks on production only, and then runs with --force
        QCOMPARE(planAction(Action::Migrate, false), (ActionPlan{{QStringLiteral("migrate")}, Confirm::None, false, true}));
        QCOMPARE(planAction(Action::Migrate, true),
                 (ActionPlan{{QStringLiteral("migrate"), QStringLiteral("--force")}, Confirm::Ask, true, true}));
        // Rolling back and seeding always ask
        QCOMPARE(planAction(Action::Rollback, false).confirm, Confirm::Ask);
        QVERIFY(!planAction(Action::Rollback, false).force);
        QVERIFY(planAction(Action::Rollback, true).arguments.contains(QStringLiteral("--force")));
        QCOMPARE(planAction(Action::Seed, false), (ActionPlan{{QStringLiteral("db:seed")}, Confirm::Ask, false, true}));
        QVERIFY(planAction(Action::Seed, true).force);
        // Fresh: the name typed, and --force everywhere (without interaction
        // flocms-cli refuses it otherwise)
        for (const bool production : {false, true}) {
            const ActionPlan fresh = planAction(Action::Fresh, production);
            QCOMPARE(fresh.confirm, Confirm::TypeDatabase);
            QCOMPARE(fresh.arguments, (QStringList{QStringLiteral("migrate:fresh"), QStringLiteral("--force")}));
        }
        // Everything they run is in Core's allowlist
        for (const Action action : {Action::Migrate, Action::Rollback, Action::Fresh, Action::Seed}) {
            for (const bool production : {false, true})
                QVERIFY(isAllowed(Program::Flo, planAction(action, production).arguments));
        }
        QVERIFY(isAllowed(Program::Flo, showArguments()));
        QVERIFY(isAllowed(Program::Flo, statusArguments()));
    }

    void namesAndCommands()
    {
        using namespace FloStudio::Database;
        for (const Action action : {Action::Migrate, Action::Rollback, Action::Fresh, Action::Seed}) {
            Action back{};
            QVERIFY(actionFromName(actionName(action), &back));
            QCOMPARE(back, action);
        }
        Action none{};
        QVERIFY(!actionFromName(QStringLiteral("drop"), &none));
        QCOMPARE(actionCommand(Action::Rollback), QStringLiteral("php flo migrate:rollback"));
    }

    void theTypedName()
    {
        using FloStudio::Database::typedNameMatches;
        QVERIFY(typedNameMatches(QStringLiteral("fectum"), QStringLiteral("fectum")));
        QVERIFY(typedNameMatches(QStringLiteral("  fectum "), QStringLiteral("fectum")));
        QVERIFY(!typedNameMatches(QStringLiteral("Fectum"), QStringLiteral("fectum")));
        QVERIFY(!typedNameMatches(QStringLiteral("fect"), QStringLiteral("fectum")));
        QVERIFY(!typedNameMatches(QString(), QString()));
        QVERIFY(!typedNameMatches(QStringLiteral(" "), QStringLiteral(" ")));
    }
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
