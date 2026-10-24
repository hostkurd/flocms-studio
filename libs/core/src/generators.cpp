#include <flostudio/core/generators.h>

#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <utility>

namespace FloStudio::Core {

namespace {

struct Names
{
    Generator generator;
    const char *name;
    const char *command;
};

constexpr std::array<Names, 6> kGenerators{{
    {Generator::AdminResource, "admin-resource", "make:admin-resource"},
    {Generator::AdminPage, "admin-page", "make:admin-page"},
    {Generator::Block, "block", "make:admin-block"},
    {Generator::Widget, "widget", "make:admin-widget"},
    {Generator::Controller, "controller", "make:controller"},
    {Generator::Migration, "migration", "make:migration"},
}};

bool matches(const char *pattern, const QString &text)
{
    return QRegularExpression(QLatin1String(pattern)).match(text).hasMatch();
}

// The checks flocms-admin's commands make (MakeResourceCommand, ...)
constexpr const char *kClass = "^[A-Z][A-Za-z0-9]*$";
constexpr const char *kTable = "^[A-Za-z_][A-Za-z0-9_]{0,63}$";
constexpr const char *kSlug = "^[a-z0-9]+(-[a-z0-9]+)*$";
// flocms-cli makes these snake/Studly case itself (Name::snake, studly)
constexpr const char *kController = "^[A-Za-z][A-Za-z0-9_-]*$";
constexpr const char *kMigration = "^[A-Za-z][A-Za-z0-9_]*$";

QString option(const char *name, const QString &value)
{
    return QStringLiteral("--%1=%2").arg(QLatin1String(name), value.trimmed());
}

} // namespace

QString generatorName(Generator generator)
{
    for (const Names &names : kGenerators) {
        if (names.generator == generator)
            return QLatin1String(names.name);
    }
    return {};
}

bool generatorFromName(const QString &name, Generator *generator)
{
    for (const Names &names : kGenerators) {
        if (name == QLatin1String(names.name)) {
            *generator = names.generator;
            return true;
        }
    }
    return false;
}

QString generatorCommand(Generator generator)
{
    for (const Names &names : kGenerators) {
        if (names.generator == generator)
            return QLatin1String(names.command);
    }
    return {};
}

bool generatorNeedsAdmin(Generator generator)
{
    return generator == Generator::AdminResource || generator == Generator::AdminPage || generator == Generator::Block
           || generator == Generator::Widget;
}

QList<FieldProblem> checkGenerator(Generator generator, const GeneratorForm &form)
{
    QList<FieldProblem> problems;
    const auto add = [&problems](const char *field, const char *code) {
        problems << FieldProblem{QLatin1String(field), QLatin1String(code)};
    };
    const QString name = form.name.trimmed();
    if (name.isEmpty())
        add("name", "name_empty");
    else if (generator == Generator::Controller && !matches(kController, name))
        add("name", "controller_name");
    else if (generator == Generator::Migration && !matches(kMigration, name))
        add("name", "migration_name");
    else if (generator != Generator::Controller && generator != Generator::Migration && !matches(kClass, name))
        add("name", "class_name");

    const QString table = form.table.trimmed();
    const QString resource = form.resource.trimmed();
    switch (generator) {
    case Generator::AdminResource:
        if (!table.isEmpty() && !matches(kTable, table))
            add("table", "table_name");
        if (!form.slug.trimmed().isEmpty() && !matches(kSlug, form.slug.trimmed()))
            add("slug", "slug_name");
        break;
    case Generator::AdminPage:
        if (form.pageKind == u"table" && !matches(kTable, table))
            add("table", table.isEmpty() ? "table_needed" : "table_name");
        if (form.pageKind == u"resource" && resource.isEmpty())
            add("resource", "resource_needed");
        else if (form.pageKind == u"resource" && !matches(kClass, resource))
            add("resource", "resource_name");
        if (form.group.contains(u'\n') || form.group.contains(u'\r'))
            add("group", "group_invalid");
        break;
    case Generator::Widget:
        if (resource.isEmpty() && form.widgetType == u"records")
            add("resource", "resource_needed");
        else if (!resource.isEmpty() && !matches(kClass, resource))
            add("resource", "resource_name");
        break;
    case Generator::Migration:
        if (form.migrationKind != u"plain" && !matches(kTable, table))
            add("table", table.isEmpty() ? "table_needed" : "table_name");
        break;
    case Generator::Block: {
        // MakeBlockCommand: the snake case type is at most 41 characters
        const auto capitals = std::count_if(name.begin() + (name.isEmpty() ? 0 : 1), name.end(), [](QChar c) { return c.isUpper(); });
        if (problems.isEmpty() && name.size() + capitals > 41)
            add("name", "block_name_long");
        break;
    }
    case Generator::Controller:
        break;
    }
    return problems;
}

QStringList generatorArguments(Generator generator, const GeneratorForm &form)
{
    QStringList arguments{generatorCommand(generator), form.name.trimmed()};
    const QString table = form.table.trimmed();
    const QString resource = form.resource.trimmed();
    switch (generator) {
    case Generator::AdminResource:
        if (!table.isEmpty())
            arguments << option("table", table);
        if (!form.slug.trimmed().isEmpty())
            arguments << option("slug", form.slug);
        // page is FloCMS's default: only the others are passed
        if (form.formMode == u"modal" || form.formMode == u"slide-over")
            arguments << option("form", form.formMode);
        if (form.migration)
            arguments << QStringLiteral("--migration");
        if (form.reorderable)
            arguments << QStringLiteral("--reorderable");
        break;
    case Generator::AdminPage:
        if (form.view)
            arguments << QStringLiteral("--view");
        if (form.pageKind == u"form")
            arguments << QStringLiteral("--form");
        else if (form.pageKind == u"table")
            arguments << option("table", table);
        else if (form.pageKind == u"resource")
            arguments << option("resource", resource);
        if (!form.group.trimmed().isEmpty())
            arguments << option("group", form.group);
        break;
    case Generator::Widget:
        if (form.widgetType == u"records" || form.widgetType == u"chart")
            arguments << option("type", form.widgetType);
        if (!resource.isEmpty())
            arguments << option("resource", resource);
        break;
    case Generator::Controller:
        if (form.controllerKind == u"resource")
            arguments << QStringLiteral("--resource");
        else if (form.controllerKind == u"admin")
            arguments << QStringLiteral("--admin");
        else if (form.controllerKind == u"api")
            arguments << QStringLiteral("--api");
        if (form.controllerKind == u"api" && form.apiResource)
            arguments << QStringLiteral("--resource");
        // An API controller has no model of its own (make:api-controller)
        if (form.model && form.controllerKind != u"api")
            arguments << QStringLiteral("--model");
        break;
    case Generator::Migration:
        if (form.migrationKind == u"create")
            arguments << option("create", table);
        else if (form.migrationKind == u"change")
            arguments << option("table", table);
        break;
    case Generator::Block:
        break;
    }
    if (form.force)
        arguments << QStringLiteral("--force");
    return arguments;
}

} // namespace FloStudio::Core
