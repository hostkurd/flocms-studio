#include <flostudio/database/actions.h>

namespace FloStudio::Database {

ActionPlan planAction(Action action, bool production)
{
    ActionPlan plan;
    switch (action) {
    case Action::Migrate:
        plan.arguments = {QStringLiteral("migrate")};
        plan.confirm = production ? Confirm::Ask : Confirm::None;
        plan.force = production;
        break;
    case Action::Rollback:
        plan.arguments = {QStringLiteral("migrate:rollback")};
        plan.confirm = Confirm::Ask;
        plan.force = production;
        break;
    case Action::Fresh:
        // Without interaction flocms-cli refuses it everywhere unless --force
        plan.arguments = {QStringLiteral("migrate:fresh")};
        plan.confirm = Confirm::TypeDatabase;
        plan.force = true;
        break;
    case Action::Seed:
        plan.arguments = {QStringLiteral("db:seed")};
        plan.confirm = Confirm::Ask;
        plan.force = production;
        break;
    }
    if (plan.force)
        plan.arguments << QStringLiteral("--force");
    plan.changesData = true;
    return plan;
}

QStringList showArguments()
{
    return {QStringLiteral("db:show"), QStringLiteral("--format=json")};
}

QStringList statusArguments()
{
    return {QStringLiteral("migrate:status")};
}

QString actionCommand(Action action)
{
    return QStringLiteral("php flo ") + planAction(action, false).arguments.constFirst();
}

QString actionName(Action action)
{
    switch (action) {
    case Action::Migrate:
        return QStringLiteral("migrate");
    case Action::Rollback:
        return QStringLiteral("rollback");
    case Action::Fresh:
        return QStringLiteral("fresh");
    case Action::Seed:
        return QStringLiteral("seed");
    }
    return {};
}

bool actionFromName(const QString &name, Action *action)
{
    for (const Action candidate : {Action::Migrate, Action::Rollback, Action::Fresh, Action::Seed}) {
        if (actionName(candidate) == name) {
            *action = candidate;
            return true;
        }
    }
    return false;
}

bool typedNameMatches(const QString &typed, const QString &database)
{
    const QString name = database.trimmed();
    return !name.isEmpty() && typed.trimmed() == name;
}

} // namespace FloStudio::Database
