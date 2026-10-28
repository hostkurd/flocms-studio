# Packages and updates

The **Packages** tab shows the project's FloCMS packages, which of them have updates, and updates one package or adds the admin panel or the API. Studio reads the installed versions from `composer.lock` and asks Composer what is newer.

## What Studio reads

- **Installed versions** come from `composer.lock`, without PHP. The **Overview**'s **Packages** card lists the `hostkurd/*` packages and how many others there are.
- **What is newer** comes from Composer. When you open the **Packages** tab, and when you click **Check for updates**, Studio runs:

```bash
composer outdated --direct --all --format=json
```

Composer asks Packagist for every package, so this needs the internet. It sends the names of your project's packages, not your code.

## The packages table

**FloCMS packages** lists each package with its **Installed** and **Latest** version and a **Status**:

| Status | Meaning |
|---|---|
| **Up to date** | No newer version |
| **Update** | A newer version within the version constraint |
| **New major version** | A newer major version: it may change how things work |
| **New 0.x version** | Before 1.0, a new minor version may change how things work |
| **Through another package** | Not in `composer.json` itself: another package requires it |
| **Abandoned** | The package is abandoned |
| **Not installed** | The admin panel or the API, which you can add |

The other packages are under **Other packages**.

## Update a package

Click **Review update** on a package. Studio shows the update and the commands it will run. For a new major or 0.x version, read the changelog first: **Read the changelog** opens it, and for `flocms-admin` **Read UPGRADING.md** opens its upgrade notes.

Click **Update** to run the commands, in order:

```bash
composer require "hostkurd/flocms-core:^2.9.0" -W
php flo admin:install     # flocms-admin only
php flo api:install       # flocms-api only
php flo migrate --force
```

- `admin:install` and `api:install` only add what is missing.
- `migrate` runs the update's migrations. When `.env` names no database, it doesn't run; run it from the **Database** tab once one is set.
- On a production site (see [Migrations and seeding](migrations.md#production-sites)), Composer leaves out the development packages: Studio adds `--update-no-dev`.
- When Composer is a `.bat` or `.cmd` file on Windows, Studio requires the package without a version (`composer require hostkurd/flocms-core -W`), and Composer picks the newest version.

When it is done, Studio says "Updated to 2.9.0." If a command fails, Studio says so with its exit code. What ran before it stays done.

## Add the admin panel or the API

For a project without them, the table offers **Add the admin panel** (`hostkurd/flocms-admin`) and **Add the API** (`hostkurd/flocms-api`). Click **Add** to run:

```bash
composer require hostkurd/flocms-admin -W
php flo admin:install
php flo migrate --force
```

For the API, the commands are `composer require hostkurd/flocms-api -W` and `php flo api:install`. When it is done, the project's tabs show the new part.

## Notices

- "No `composer.lock` yet: run `composer install` in the project."
- "The packages are not installed (no `vendor/`): run `composer install`." Studio can't check for updates until then.
- "Composer could not say what is newer. It asks Packagist, so this needs the internet."

See the framework's [framework and packages](/docs/2.0/packages#versions-in-this-guide), [add the admin panel later](/docs/2.0/setups#add-the-admin-panel-later) and [add an API later](/docs/2.0/setups#add-an-api-later).
