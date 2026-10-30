# The server and the database

Studio runs each project's website with `php flo serve`, PHP's built-in server. For MySQL projects it can also run a MariaDB server of its own, or use one that XAMPP, Laragon or another stack already runs. SQLite projects need no database server.

## The website server

Click **Start server** on the project page or on the **Overview**'s **Server** card. Studio runs:

```bash
php flo serve --port=8000 --host=127.0.0.1
```

The port is the **First port to try** from [Settings > Server](settings.md#server), 8000 by default. When that port is in use, `php flo serve` takes the next free port itself. Studio reads the address it prints, waits until the site answers there, then shows the server as **Running**, with its address.

**Stop server** stops it. The server also stops when you close Studio, remove the project from Studio, or delete it.

### Open the site from other devices

With **Let other devices on the network open the site (phones for testing)** turned on in Settings, Studio runs the server with `--host=0.0.0.0`. The **Server** card then lists the addresses under **From other devices on your network:**. Only do this on networks you trust.

### A server left from a crash

On macOS and Linux, a project's server can keep running after Studio crashes. At the next start, Studio stops such servers. The **Logs** tab then says: "Stopped a server of this project that was still running from the last time Studio closed unexpectedly."

## Choose the database server

[Settings > Database server](settings.md#database-server) sets the default server for new projects (**Default server for new projects**). Each project can pick its own when it is made (step 2 of the [new project](first-project.md#step-2-database) form) or set up (below):

- **Studio's MariaDB**: the MariaDB Studio downloaded, which Studio starts and stops.
- **A server found on this computer**, for example XAMPP's or Laragon's. Studio doesn't start or stop it: start and stop it in XAMPP or Laragon. See [XAMPP, Laragon and other stacks](other-stacks.md).
- **Another server**: a host, port and user you give.

"Existing projects keep the server in their `.env`." The choice only changes the defaults of the [new project](first-project.md#step-2-database) form.

When you have made no choice, new projects use Studio's MariaDB when you have one, else the server found on this computer.

## Set up a database for an existing project

A project with no database yet (no `DB_NAME` in `.env`) shows "Not set up yet" on its Overview and Database tab, with **Set up database**. Choose the server for this project:

- **Studio's MariaDB**: give the **Database** and **User** names (the project's folder name by default). Studio starts its MariaDB when it is stopped, makes the database and a user that can only reach it, writes the `DB_` lines into `.env` and keeps the password in the system's secure store. The project then counts as one of Studio's server's, so its Database tab can start that server.
- **Another server**: give the **Database**, **User**, **Server**, **Port** and **Password**. Studio writes the `DB_` lines into `.env` and runs `php flo db:check`. When the database isn't there, **Create database "…"** creates it with that login.

Then run the migrations on the Database tab, and create the first admin with `php flo user:create` in a terminal (**More > Open terminal**). To move a project that has a database to another server, change its `DB_` lines with **Edit in `.env`**; its data doesn't move with it.

## Studio's MariaDB

Download MariaDB in the **Download the tools Studio needs** dialog, or with **Download MariaDB…** in **Settings > Database server**. On macOS, install it with `brew install mariadb`; Studio then runs Homebrew's MariaDB on its own databases folder.

The first start sets the server up. Studio shows each part:

1. Downloaded and checked against its `SHA-256`
2. Databases folder made, `root` password saved in the system's secure store
3. Starting on port `3306`
4. Password of `root` set

How Studio runs it:

- It listens on `127.0.0.1` only, so only this computer can connect.
- Its databases are in a folder for your user, one per MariaDB version (series). **Settings > Database server** shows the **Databases folder**.
- The `root` user has a password. Studio keeps it in the system's secure store. **Show** and **Copy** in Settings give it to you, for a database tool such as HeidiSQL or phpMyAdmin.
- For each project it creates, Studio makes a database and a user that can only reach that database.
- **Start the server when Studio opens** starts it with Studio. Studio stops the server cleanly when it closes.

Start and stop Studio's MariaDB in **Settings > Database server**, on the **Overview**'s **Database** card, or with **Start the database server** in the **Database** tab when a project's database is on it.

The sidebar's **MariaDB** row shows the state of Studio's MariaDB, and its port while it runs. When Studio's server is stopped and another server answers, the row shows that one instead.

### When the port is in use

When another program uses the port, Studio says "Port `3306` is in use by another program. Studio can start its MariaDB on port `3307`." and offers **Start on `3307`**. You can also set another **Port** in Settings; it is used from the next start. When Studio's MariaDB starts on another port, a new project's `.env` follows it.

### Remove Studio's MariaDB

In **Settings > Database server**:

- **Remove MariaDB…** deletes the program. The databases stay; download the same version again to use them.
- **Delete its databases…** deletes every database in the databases folder, with the password of `root`. Projects that use them stop working.

## Database settings in `.env`

Every project reads its database settings from its `.env` file. Studio and the `php flo` commands read the same keys:

| Key | Meaning |
|---|---|
| `DB_TYPE` | `mysql` or `sqlite` |
| `DB_HOST` | The server (MySQL); `localhost` when not set |
| `DB_PORT` | The port (MySQL); `3306` when not set |
| `DB_NAME` | The database; for SQLite, the file, such as `storage/database.sqlite` |
| `DB_USERNAME` | The user (MySQL) |
| `DB_PASSWORD` | The password (MySQL) |
| `DB_DSN` | A full connection string, used instead of the keys above |

A project needs `DB_NAME`, and for MySQL `DB_USERNAME`, before `php flo` can connect. Until then the **Database** card says **Not set up yet**.

The **Database** tab shows the connection (**Connection**, **Database**, **Host**, **From** `.env`). **Edit in `.env`** opens the file in your editor. After you change it, click **Refresh**.

When `.env` sets a key twice, Studio warns: the website uses the first value, some `php flo` commands the last. Remove one.

See [Migrations and seeding](migrations.md) for the rest of the **Database** tab, and the framework's [database guide](/docs/2.0/database).
