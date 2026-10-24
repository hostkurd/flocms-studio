#pragma once

#include <flostudio/core/export.h>
#include <flostudio/core/wizard.h>

#include <QList>
#include <QString>
#include <QStringList>

// The Generate tab (stage 7, board 3): FloCMS's generators with a form
// each, and the `php flo make:...` arguments the form gives. The options
// are flocms-cli's and flocms-admin's own (`php flo help make:... `):
//
//   make:admin-resource <Name> [--table=] [--slug=] [--form=page|modal|slide-over] [--migration] [--reorderable]
//   make:admin-page <Name> [--view] [--form | --table=<table> | --resource=<Resource>] [--group=]
//   make:admin-block <Name>
//   make:admin-widget <Name> [--type=stats|records|chart] [--resource=]   (records needs a resource)
//   make:controller <Name> [--resource | --admin | --api] [--model]
//   make:migration <name> [--create=<table> | --table=<table>]
//
// and --force on each to overwrite what is there. The admin panel's four
// need the admin panel in the project.
namespace FloStudio::Core {

enum class Generator { AdminResource, AdminPage, Block, Widget, Controller, Migration };

struct GeneratorForm
{
    QString name; // the class (Property, SalesReport) or the migration (create_posts_table)

    // Admin resource
    QString table;     // also: a page's table, a migration's table
    QString slug;      // the URL segment
    QString formMode;  // page (FloCMS's default), modal, slide-over
    bool migration = false;
    bool reorderable = false;

    // Admin page: what it shows
    QString pageKind = QStringLiteral("plain"); // plain, form, table, resource
    bool view = false;                          // also its view
    QString group;                              // the sidebar group

    QString resource; // a page's or a widget's resource (Post or PostResource)

    // Widget
    QString widgetType = QStringLiteral("stats"); // stats, records, chart

    // Controller
    QString controllerKind = QStringLiteral("plain"); // plain, resource, admin, api
    bool model = false;

    // Migration
    QString migrationKind = QStringLiteral("plain"); // plain, create, change

    bool force = false; // overwrite files that are there
};

// admin-resource, admin-page, block, widget, controller, migration and back.
FLOSTUDIOCORE_EXPORT QString generatorName(Generator generator);
FLOSTUDIOCORE_EXPORT bool generatorFromName(const QString &name, Generator *generator);
// make:admin-resource, ...
FLOSTUDIOCORE_EXPORT QString generatorCommand(Generator generator);
// The admin panel's generators.
FLOSTUDIOCORE_EXPORT bool generatorNeedsAdmin(Generator generator);

// What is wrong, field by field ({field, code}): name_empty, class_name
// (StudlyCase: Property, BlogPost), controller_name, migration_name
// (lower case with _), table_name, slug_name, resource_name,
// resource_needed (a records widget, a resource page), table_needed (a
// table page, a migration that creates or changes one), group_invalid.
FLOSTUDIOCORE_EXPORT QList<FieldProblem> checkGenerator(Generator generator, const GeneratorForm &form);

// `make:...` and its arguments for the form (only the options that apply).
FLOSTUDIOCORE_EXPORT QStringList generatorArguments(Generator generator, const GeneratorForm &form);

} // namespace FloStudio::Core
