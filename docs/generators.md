# Generators

The **Generate** tab runs FloCMS's `php flo make:` generators from a form. It shows the command the form gives, lets you preview the files it would write, and runs it in the project when you click Create.

## Choose a generator

Choose one under **Generator**:

| Generator | Command | Needs |
|---|---|---|
| **Admin resource** | `php flo make:admin-resource` | The admin panel |
| **Admin page** | `php flo make:admin-page` | The admin panel |
| **Block** | `php flo make:admin-block` | The admin panel |
| **Widget** | `php flo make:admin-widget` | The admin panel |
| **Controller** | `php flo make:controller` | |
| **Migration** | `php flo make:migration` | |

The four admin generators need the admin panel (`hostkurd/flocms-admin`) in the project. Add it in [Packages](packages.md#add-the-admin-panel-or-the-api).

The **Overview**'s **Quick commands** open this tab: **Admin resource…** and **Migration…**.

Under the form, Studio shows the command exactly as it will run. **Overwrite files that are there** adds `--force`.

## Admin resource

A page in the admin panel to list, add, change and delete a table's records.

| Field | Option |
|---|---|
| **Name**: a class name, e.g. `Property` or `BlogPost` | `<Name>` |
| **Table**: empty for the plural of the name | `--table=` |
| **URL**: the URL segment | `--slug=` |
| **Create and Edit open as**: **A full page**, **A dialog** or **A slide-over panel** | `--form=modal` or `--form=slide-over` (a full page is the default) |
| **Also create a migration for the table** | `--migration` |
| **Editors can drag records into order (a `sort` column)** | `--reorderable` |

The resource starts with a title field. Add the other fields in the file.

## Admin page

A page of your own in the admin panel.

| Field | Option |
|---|---|
| **Name**: a class name, e.g. `SalesReport` | `<Name>` |
| **The page shows**: **Content of its own**, **A form with Save**, **A database table** or **A resource's records** | nothing, `--form`, `--table=<table>` or `--resource=<Resource>` |
| **Sidebar group**: e.g. Reports; empty for no group | `--group=` |
| **Also write its view, with the panel's parts** | `--view` |

## Block

A block for Builder fields. **Name** is a class name, e.g. `LatestProjects`. Its template name can't be over 41 characters.

## Widget

A dashboard widget.

| Field | Option |
|---|---|
| **Name**: a class name, e.g. `PostStats` or `RecentPosts` | `<Name>` |
| **Widget**: **Numbers**, **Latest records** or **A chart** | nothing, `--type=records` or `--type=chart` |
| **Resource**: the resource it counts; needed for latest records | `--resource=` |

## Controller

| Field | Option |
|---|---|
| **Name**: e.g. `Blog` or `BlogPost` | `<Name>` |
| **Actions**: **A page**, **Pages and admin actions (CRUD)**, **Admin actions** or **JSON API** | nothing, `--resource`, `--admin` or `--api` |
| **With the resource actions: list, show, add, change, delete** (JSON API) | `--resource` |
| **Also create its model** (not for a JSON API) | `--model` |

## Migration

| Field | Option |
|---|---|
| **Name**: e.g. `create_posts_table` or `add_slug_to_posts` | `<name>` |
| **Migration**: **Empty**, **Create a table** or **Change a table** | nothing, `--create=<table>` or `--table=<table>` |
| **Table** | the table to create or change |

A name like `create_posts_table` creates the `posts` table. After you create a migration, run it in the [Database](migrations.md#run-the-migrations) tab.

## Preview

**Preview** runs the generator in a temporary copy of the project and shows the files it writes. Your project changes only with Create.

Studio copies the project without `.git`, `node_modules`, logs, caches, sessions, compiled views and uploads. The packages in `vendor/` are linked, not copied. Studio runs the same command in the copy, lists the files it added, changed or removed, shows each one (a changed file as a diff), and deletes the copy.

The preview tells you when:

- the project has no `vendor/` folder yet: run `composer install` in it;
- the project is too large to copy (over 400 MB or 40,000 files, without uploads and logs). Create works without a preview;
- a path in `config/admin.php` is absolute, so the generator would write outside the copy. Create works;
- the form changed since the preview: preview again;
- the project itself changed while the preview ran: check the files it names.

## Create

**Create resource** (or **Create page**, **Create block**, **Create widget**, **Create controller**, **Create migration**) runs the command in the project. Studio then reads the database and runs Doctor again, so a new migration or resource shows at once.

See the framework's [generators and stubs](/docs/2.0/generators) and [command list](/docs/2.0/cli#generators), and the admin panel's [resources](/docs/admin/0.39/overview#resources), [custom pages](/docs/admin/0.39/custom-pages), [widgets](/docs/admin/0.39/widgets) and [Builder blocks](/docs/admin/0.39/builder).
