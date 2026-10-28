# Open an existing project

Studio works with any FloCMS project on your computer, also one you didn't create in Studio. You can add one project at a time, or a folder that holds your projects; Studio then lists every FloCMS project inside it.

## Add projects

On the welcome page:

- **Add a projects folder…** adds a folder your projects are in. Studio lists every FloCMS project in its sub-folders. You manage these folders in [Settings > Project folders](settings.md#project-folders).
- **Open one project…** adds one project folder.

In the sidebar, **Open existing folder…** also adds one project.

## What Studio checks

A folder is a FloCMS project when its `composer.json` is the FloCMS skeleton (`hostkurd/flocms`) or requires `hostkurd/flocms-core`. When it is neither, Studio says: "This folder is not a FloCMS project: its `composer.json` neither is the FloCMS skeleton nor requires `hostkurd/flocms-core`."

Studio reads the project from its files, without PHP, so the list and the **Overview** appear at once:

- `.env`: the name, environment, address, languages and database settings
- `config/config.php`: what the home page shows (site, admin panel or API)
- `composer.json`: whether the admin panel (`hostkurd/flocms-admin`) and the API (`hostkurd/flocms-api`) are in the project
- `composer.lock`: the installed package versions
- `vendor/`: whether `composer install` has run

Everything else (Doctor, the database, the migrations) comes from `php flo` commands, which need PHP.

## The project list

The sidebar lists your projects under **Projects**, sorted by name. A green dot and **Running** mark a project whose server runs. **Missing** marks a project whose folder is gone or is no longer a FloCMS project. Click a project to open it.

When you open a missing project, Studio says why ("The project folder is gone.", for example) and offers **Remove from Studio**.

## The project page

The page shows the project's name and folder, these buttons, and six tabs:

- **Open site**: opens the site in your browser. It works while the server runs.
- **Open admin**: opens `/admin` in your browser, for projects with the admin panel, while the server runs.
- **Open folder**: opens the project folder in your file manager.
- **Open in editor**: opens the folder in your editor. Set the editor in [Settings > Editor](settings.md#editor).
- **Start server** / **Stop server**: see [The server and the database](server-and-database.md#the-website-server).
- **More**: **Remove from Studio** and **Delete project…**.

The tabs are **Overview**, **Generate**, **Database**, **Logs**, **Packages** and **Doctor**.

### Overview

The **Overview** shows, at a glance:

- **Server**: its state and address, and **Start server** / **Stop server**.
- **Database**: the database, its tables and waiting migrations, and **Run migrations** when some wait. For a project on Studio's MariaDB, it also shows that server's state, with **Start** and **Stop**.
- **Doctor**: the result of the checks. See [Doctor](doctor.md).
- **Project**: **Name**, **Environment**, **Address**, **Started from** (the skeleton version) and **Languages**. When `APP_ENV` is not set, the environment shows as **production (not set)**, because FloCMS then treats the site as production.
- **Quick commands**: **Admin resource…** and **Migration…** open the [Generate](generators.md) tab; **Clear cache** runs `php flo cache:clear`; **Optimize** runs `php flo optimize`.
- **Packages**: the FloCMS packages from `composer.lock`. See [Packages and updates](packages.md).

## Remove a project from Studio

**More > Remove from Studio** takes the project out of Studio's list. The files stay. If the project's server runs, Studio stops it.

A project inside one of your project folders stays hidden after you remove it. Open it again with **Open existing folder…** to show it again.

## Delete a project

**More > Delete project…** asks first. The project's folder goes to the Recycle Bin (Windows) or the Trash (macOS, Linux), so you can still bring it back from there. Its website server stops, and the project leaves Studio.

For a project whose database Studio made on its own MariaDB, the dialog offers **Also delete its database on Studio's MariaDB**. This deletes the database with all its data and can't be undone. The database user stays. When another project uses the same database, the database stays. Databases on other servers are never deleted.

Studio refuses to delete:

- a folder that is a link to another folder;
- a folder that is, holds or is inside a folder Studio or your computer needs: Studio's own files, your home or Documents folder, a project folder or another project;
- a project while a command still runs in it.

See [Troubleshooting](troubleshooting.md#removing-and-deleting-projects) for the messages.
