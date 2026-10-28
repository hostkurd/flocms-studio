# Your first project

Studio creates a FloCMS project in four steps: **Project**, **Database**, **First admin** and **Create**. In the last step it runs the same `composer` and `php flo` commands you would type yourself, one after the other, and shows each command and its output.

## Download the tools

Studio needs PHP and Composer to create and run projects. When it can't find them, the welcome page says "Studio could not find PHP or Composer on this computer, so projects can't run yet." The sidebar lists the tools under **This computer**, with a **Download…** button when one is missing.

**Download…** opens the **Download the tools Studio needs** dialog, with one tab for each tool:

- **PHP**: choose a version, or **No PHP** to keep the PHP this computer has. Windows downloads come from windows.php.net; macOS and Linux downloads come from static-php-cli (static-php.dev). On Windows, Studio adds a `php.ini` that turns on the extensions FloCMS needs.
- **Composer**: from getcomposer.org. When this computer has an older Composer, Studio offers its own copy and leaves the other one as it is.
- **MariaDB**: optional. Only projects that use MySQL need one; SQLite needs nothing. Windows and Linux downloads come from archive.mariadb.org. MariaDB makes no builds for macOS: install it with `brew install mariadb`, and Studio uses that one.

The tools go into Studio's own folder for your user. Nothing else on the computer changes, and no administrator rights are needed. Studio downloads only the files in the list built into it, and checks each file's SHA-256 and size before it uses it. A file that doesn't match is deleted.

To use a PHP you already have, choose **Choose my own PHP…**. It opens [Settings > PHP and tools](settings.md#php-and-tools).

## Start a new project

Click **New project** on the welcome page or in the sidebar. The **New FloCMS project** page opens. **Cancel** closes it.

## Step 1: Project

- **Project name**: lower case, used for the folder and the database. Studio turns what you type into a folder name: "My Shop" becomes `my-shop`. Letters, digits, `-` and `_` are allowed.
- **Folder**: where the project folder is made. It starts as the first folder in [Settings > Project folders](settings.md#project-folders), or `Sites` in your home folder. **Browse…** chooses another one.
- **Languages**: click a language to add it. Click a chosen language again to make it the default. This sets `LANGUAGES` and `DEFAULT_LANG` in `.env`. The choices are English, Arabic, Kurdish, Greek, Turkish, Persian, German and French.
- **What are you building?**: one of five setups. You can add the admin panel or the API later from [Packages](packages.md).

| Choice | Setup |
|---|---|
| **Website + admin panel** | `site-admin` |
| **Website only** | `site` |
| **Admin panel only** | `admin` |
| **API + admin panel** | `api-admin` |
| **API only** | `api` |

For the two website setups, **Also add the JSON API** adds the API too.

## Step 2: Database

Choose **SQLite: a file in the project** or **MySQL / MariaDB**.

With SQLite, the database is the file `storage/database.sqlite`, made by the first migration. Nothing else is asked.

With MySQL or MariaDB, what you see depends on the server in [Settings > Database server](server-and-database.md#choose-the-database-server):

- **On Studio's MariaDB**: Studio shows its server, its address and its state. Under **The project's database**, choose:
  - **Studio makes a new database and a user for this project**. You set the **Database name** and the **User**. Studio makes a password, writes it into the project's `.env` and keeps it in the system's secure store. The user can only reach this database.
  - **Use a database that exists already**. You give the name, user and password.
- **On another server** (XAMPP, Laragon, or one you set), you give the **Database name** (it must exist already), the **Server** and **Port**, the **User** and the **Password**. The password is written to `DB_PASSWORD` in `.env`. Leave it empty for XAMPP's `root`. **Remember the password in…** saves it in the system's secure store for your next project on the same server, and **Use saved** fills it in.

## Step 3: First admin

This step is only for setups with the admin panel. It asks for the **Name**, **Email** (also the login name) and **Password** (at least 8 characters) of the first user of the admin panel. This user has every right (super admin).

Studio passes the password to FloCMS on the command's input. It never puts it on a command line or in a log.

## Step 4: Create

The **Create** step lists what Studio will do. Click **Create the project** to start. Studio runs these steps in order; the steps that don't apply to your choices are left out:

| Step | What runs |
|---|---|
| Create the project | `composer create-project hostkurd/flocms <name>` in the parent folder |
| Write `.env` | Studio writes the name, address, languages and database (see below) |
| Make the database | On Studio's MariaDB only: `CREATE DATABASE`, `CREATE USER` and `GRANT` for this database. Studio starts its MariaDB first if it isn't running. |
| Choose the setup | `php flo setup --as=<setup>` (not for **Website only**, which is what `create-project` makes) |
| Add the API | `composer require hostkurd/flocms-api`, then `php flo api:install` (website with **Also add the JSON API**) |
| Check the database | `php flo db:check` (MySQL / MariaDB only) |
| Run the migrations | `php flo migrate --force` (this also makes the SQLite file) |
| Create the first admin | `php flo user:create --name=<name> --email=<email> --role=super-admin --password-stdin` |

Studio adds `--no-interaction` and `--no-ansi` to every command, so no command stops on a question. Composer's `create-project` and `require` also get `--no-progress`.

The `.env` values Studio writes:

| Key | Value |
|---|---|
| `APP_NAME`, `SITE_NAME`, `SITE_NAME_ADMIN` | The project name |
| `APP_ENV` | `local` |
| `APP_DEBUG` | `true` |
| `APP_URL` | `http://127.0.0.1:<port>`, with the first port from [Settings > Server](settings.md#server) |
| `LANGUAGES`, `DEFAULT_LANG` | Your languages |
| `DB_TYPE` | `sqlite` or `mysql` |
| `DB_NAME` | `storage/database.sqlite`, or the database name |
| `DB_HOST`, `DB_PORT`, `DB_USERNAME`, `DB_PASSWORD` | MySQL / MariaDB only |

**Stop** stops the step that is running.

When every step is done, Studio says the project is ready. Click **Open the project**, then start its server. See [The server and the database](server-and-database.md).

## When a step fails

The step shows why it failed: Studio's own message, or what the command printed. Then:

- Fix what it says. **Back** leads to the database and admin settings, which you can still change.
- Click **Try again**. Studio continues from the step that failed. If `create-project` stopped halfway, Studio deletes the half-made folder and runs it again.
- Or click **Start over**. Studio deletes the folder it made, and everything in it, and the form starts empty.

See [Troubleshooting](troubleshooting.md) for common causes.

## Next steps

- [Open an existing project](open-project.md)
- [Migrations and seeding](migrations.md)
- [Studio and the command line](command-line.md)
