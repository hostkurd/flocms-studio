# XAMPP, Laragon and other stacks

Studio works beside XAMPP, Laragon, Herd, Homebrew and the PHP or MySQL your system already has. It finds their PHP, Composer and database programs and uses them, but it never starts, stops or changes their servers.

## What Studio finds

Studio looks for PHP, Composer and MySQL/MariaDB on your `PATH` and in the usual install folders:

| System | Folders |
|---|---|
| Windows | `C:\xampp\php` and `C:\xampp\mysql\bin` (also `xampp` on another fixed drive, such as `D:\xampp`), `C:\laragon\bin\php\php*`, `C:\laragon\bin\mysql\*\bin`, Herd, Composer-Setup, `Program Files\MariaDB *` and `Program Files\MySQL\MySQL Server *` |
| macOS | Homebrew (`/opt/homebrew/bin`, `/usr/local/bin`), Herd, Composer's global folder, `/Applications/XAMPP/xamppfiles/bin` |
| Linux | `/usr/local/bin`, Composer's global folder, `~/.local/bin`, `/opt/lampp/bin` |

On macOS and Linux, Studio also asks your login shell for its `PATH`, because apps started from the Finder or a menu get a short one.

The downloads dialog shows where each tool comes from: **From XAMPP**, **From Laragon**, **From Herd**, **Installed on this computer** or **Downloaded by Studio**. To choose a PHP, use [Settings > PHP and tools](settings.md#php-and-tools).

## Use the stack's database server

In [Settings > Database server](settings.md#database-server), choose the server Studio found, for example "XAMPP · MariaDB 10.4". Its note says "start and stop it in XAMPP" (or Laragon). For any other server found, it says "Studio doesn't start or stop it". The sidebar shows the server as running when something listens on its port.

For a new project on that server, the [Database step](first-project.md#step-2-database) starts with server `127.0.0.1`, port `3306` and user `root`, with an empty password, as XAMPP has. When the database doesn't exist yet, step 4 offers **Create database "…"**, or create it in phpMyAdmin, HeidiSQL or `mysql` first. Studio writes what you give into the project's `.env`.

With **Another server**, Studio fills in the host, port and user you set in Settings.

## Studio's MariaDB and a stack's server together

Both use port 3306 by default. When XAMPP's or Laragon's server already holds the port, Studio's MariaDB can't start there. Studio says so and offers **Start on `3307`** (the next free port). You can also set its **Port** in Settings. See [The server and the database](server-and-database.md#when-the-port-is-in-use).

The **Database** tab offers to start Studio's MariaDB only for projects whose database Studio made on it, not for every project that uses `127.0.0.1:3306`.

## Composer-Setup and `.bat` programs on Windows

Composer-Setup installs `composer.bat`, a wrapper around `composer.phar`. Studio runs the `composer.phar` beside it through PHP instead.

When PHP or Composer is a `.bat` or `.cmd` file, `cmd.exe` reads the command line itself. Studio then refuses arguments with characters `cmd.exe` would read itself (`" % ^ & | < > ! ( )`), such as a project folder named `shop (old)`, and asks you to choose `php.exe` (or the editor's `.exe`) instead. See [Troubleshooting](troubleshooting.md#php-or-composer).
