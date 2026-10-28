# Migrations and seeding

The **Database** tab shows a project's connection, its migrations and its tables, and runs the migration and seeding commands. Everything goes through `php flo`. The commands that change data ask first.

## What the tab reads

When you open a project, Studio runs these read-only commands:

```bash
php flo db:show --format=json
php flo migrate:status
```

For a project with the admin panel it also runs `php flo admin:list`, to link tables to their admin pages. Click **Refresh** to read again. Studio also reads again after a command in Studio changed the database, a generator wrote a file, or a package was updated.

The tab has three parts:

- **Connection**: the server (MariaDB, MySQL or SQLite, with its version), the **Database**, the **Host** and where the settings come from (`.env`). **Edit in `.env`** opens the file in your editor. See [The server and the database](server-and-database.md#database-settings-in-env).
- **Migrations**: every migration with its **Status**, **Package** and **Batch**.
- **Tables**: every table with its row count. Type in the filter box to find one. For a table that has an admin resource, **Open in admin** opens it in the admin panel while the server runs. To edit data, use the admin panel.

A migration's status is one of:

| Status | Meaning |
|---|---|
| **Waiting** | Not run yet |
| **Ran** | Run |
| **Ran · changed since** | Run, but its file changed since |
| **Ran · file gone** | Run, but its file is gone |

## Run the migrations

**Run 1 migration** (or **Run 3 migrations**, and so on) runs the waiting migrations. **Up to date** means none wait. The **Overview**'s **Database** card has a **Run migrations** button too.

```bash
php flo migrate
```

## Roll back the last batch

**Roll back last batch** undoes the migrations of the last batch. Tables or columns they made, and the data in them, are dropped. Studio always asks first ("Roll back the last batch?").

```bash
php flo migrate:rollback
```

## Seed the database

**Seed…** runs `database/seeders/DatabaseSeeder`, which adds rows to the tables. Studio always asks first ("Seed the database?").

```bash
php flo db:seed
```

## Drop every table and migrate again

**Drop all and migrate…** deletes every table in the database, with all its data, then runs every migration again. This can't be undone. Studio asks you to type the database's name first ("Type the database's name, `my_shop`, to go on:").

```bash
php flo migrate:fresh --force
```

## Production sites

Studio runs every command with `--no-interaction`, so FloCMS can't ask its own questions. On a production site FloCMS refuses these commands without `--force`. Studio asks first, in its own window, then adds `--force`.

A site is a production site when `APP_ENV` in `.env` is `production`, `prod` or `live`, or when `APP_ENV` is not set.

| Button | Local site | Production site |
|---|---|---|
| **Run migrations** | Runs at once: `php flo migrate` | Asks "Run the migrations on this production site?", then `php flo migrate --force` |
| **Roll back last batch** | Asks, then `php flo migrate:rollback` | Asks, then `php flo migrate:rollback --force` |
| **Seed…** | Asks, then `php flo db:seed` | Asks, then `php flo db:seed --force` |
| **Drop all and migrate…** | Type the name, then `php flo migrate:fresh --force` | The same |

The question shows the exact command it will run. Studio reads `APP_ENV` again when you click, so a change to `.env` counts at once.

## The output

The command's output appears under the migrations, with **Running…**, **Done** or **Failed (exit code 1)**. While a command runs, the other buttons wait. If you close Studio while a command runs, Studio asks first: quitting stops it halfway, which can leave the database half migrated.

## Create a migration

Use the [Generate](generators.md#migration) tab, or **Migration…** on the **Overview**.

See the framework's [migrations](/docs/2.0/migrations) and [seeding](/docs/2.0/seeding) guides.
