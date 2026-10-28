# Troubleshooting

When something goes wrong, Studio says what happened and, where it can, what to do. Most messages appear in the red bar at the top of the window, with the path or command it is about under it; **Dismiss** closes the bar. Commands that fail show their own output. This page lists the messages and what to do about them.

## Settings

**"Studio could not read its settings file, so it runs with the defaults and won't save over it."** Another program holds the file. Close it, then start Studio again.

**"Studio could not save its settings."** Studio could not write to your user's settings folder. Check that the folder is writable.

A settings file that isn't valid JSON is kept aside as `settings.broken-<date-time>.json`, and Studio starts with the defaults.

## PHP or Composer

**"PHP was not found."** or **"Composer was not found."** Download it with **Download…** under **This computer** in the sidebar, or choose it in [Settings > PHP and tools](settings.md#php-and-tools).

**PHP is too old.** The sidebar shows "8.0.30 is too old", for example. FloCMS needs PHP 8.1 or newer. Download a newer PHP or choose another one.

**"Doesn't run".** Studio found the program, but it did not answer. Choose another one in Settings.

**A PHP extension is missing.** The [Doctor](doctor.md) tab lists it. Turn it on in your `php.ini`, or use the PHP Studio downloads.

**"PHP is a `.bat` or `.cmd` file, and the project folder's name has characters `cmd.exe` would read itself ( ) & ! %."** Choose `php.exe` in **Settings > PHP and tools**, or rename the folder.

**"The project has no `flo` launcher yet: run `composer install` in it."** or **"The project has no `vendor/` folder yet: run `composer install` in it."** Run this in the project folder:

```bash
composer install
```

## Downloads

**"The download failed."** Studio tried every address for the file. The detail says why. Check your internet connection. Studio uses your system's proxy settings for downloads.

**"The downloaded file is not the one Studio expects (its SHA-256 differs), so it was deleted."** The file changed on the way or at its source. Try again later.

**"The download could not be unpacked."** or **"The download could not be installed."** Check that your disk has space and that Studio's folder is writable. A failed install leaves the tool you had before as it was.

**MariaDB for macOS.** MariaDB makes no builds for macOS. Install it with `brew install mariadb`, then click **Search again**.

**MariaDB for another system.** When Studio can't download MariaDB for your system, install it with your package manager, for example `sudo apt install mariadb-server`.

## The website server

**"The server did not start."** The detail shows what `php flo serve` printed.

**"The server said it started, but nothing answers at its address."** Another program may hold the port. Try another port in [Settings > Server](settings.md#server).

**"The server stopped by itself."** The detail shows its last message or exit code. The [Logs](logs.md) tab has the rest.

## Studio's MariaDB

These show in Settings, on the **Overview**'s **Database** card and in the **Database** tab, with **Try again**.

**"Port `3306` is in use by another program."** Often XAMPP's or Laragon's server. Click **Start on `3307`**, or choose another port in **Settings > Database server**. See [XAMPP, Laragon and other stacks](other-stacks.md#studios-mariadb-and-a-stacks-server-together).

**"It can't start without `libaio.so.1`."** (Linux) Install the library once with the command Studio shows, then click **Try again**:

```bash
sudo apt install libaio1t64
```

On Ubuntu older than 24.04 and Debian older than 13, the package is `libaio1`. Ubuntu 24.04 and newer call the file `libaio.so.1t64`; Studio points its own copy of MariaDB at it, inside Studio's folder. Studio doesn't change system folders itself. For another missing library, install it with your package manager.

**"Studio's MariaDB doesn't run on this computer."** The program does not start on this system.

**"Studio could not set up the databases folder."** A folder left half set up is moved aside, never deleted. Click **Try again**.

**"MariaDB stopped while starting."**, **"MariaDB didn't answer in time."** or **"MariaDB stopped unexpectedly."** Studio shows the end of MariaDB's log. **Open log** in Settings shows all of it.

**"The password of `root` is missing from the system's secure store, so Studio can't use these databases."** Delete the databases in **Settings > Database server** to start again.

**"Studio could not use the system's secure store for the password of `root`."** On Linux, Studio uses the system's keyring through libsecret. Check that a keyring runs in your session.

## The database of a project

**"Studio could not read the database. Is the database server running, and are the settings in `.env` right?"** Start the server, check the `DB_` keys in `.env` (**Edit in `.env`**), then click **Refresh**. For a project on Studio's MariaDB, the tab offers **Start the database server**.

**"The project has no `.env` file. Copy `.env.example` to `.env`."**

**"`.env` sets `DB_NAME` twice."** The website uses the first value, some `php flo` commands the last. Remove one.

**"Studio could not read the migrations."** or **"Studio could not read what the check printed."** **Show the output** shows what the command printed.

## A new project

A failed step shows the reason, or "The command ended with exit code 1." with the command's output. Fix what it says, then click **Try again**: Studio continues from that step. See [Your first project](first-project.md#when-a-step-fails).

**"That folder is there already."** Choose another name or folder.

**"Studio could not delete the folder it had made."** Close what uses it (an editor, a terminal), then try again.

**"Studio could not make the database and its user on its MariaDB."** Check that Studio's MariaDB runs, then try again.

## Packages

**"Composer could not say what is newer. It asks Packagist, so this needs the internet."** Check your connection, then click **Check for updates**.

**"A command failed (exit code 1). What ran before it stays done."** Read the output, fix the problem, and run the update again.

## The editor

**"The editor in Settings > Editor was not found."** Set the **Editor command** in [Settings > Editor](settings.md#editor) to a program on this computer.

**"The editor did not start."** Check the command.

**"The editor is a `.cmd` file, and the folder's name has characters `cmd.exe` would read itself."** Choose the editor's `.exe` in **Settings > Editor**.

## Removing and deleting projects

**"A command is still running in this project. Remove or delete it when it has finished."**

**"Studio doesn't delete this folder: it is, holds or is inside a folder Studio or your computer needs."** Delete it yourself if you mean to.

**"This project's folder is a link to another folder, so Studio doesn't delete it."** Delete the folder itself, then remove the project from Studio.

**"The project's website server didn't stop, so its folder wasn't deleted."** Stop the server, then try again.

**"The folder could not be moved to the Recycle Bin."** (or Trash) A program may be using a file in it, or the drive has no Recycle Bin. Nothing was deleted.

**"The project was deleted, but Studio could not delete its database on its MariaDB."** Delete it with a database tool (HeidiSQL, DBeaver…) as `root`. **Settings > Database server** has its password.

## Generator previews

**"The project is too large to copy for a preview (over 400 MB or 40,000 files, without uploads and logs)."** Create works without a preview.

**"Studio could not remove the preview's copy."** Studio tries again when it next starts.

## Quitting while a command runs

When you close Studio while a command runs, Studio asks first ("A command is still running"). Quitting stops it halfway, which can leave the database half migrated. Choose **Quit anyway** only if you mean it.

## Report a problem

Click the version at the bottom of the sidebar to open the About page. **Copy version info** copies the version and build details; paste them into a bug report. **Report a problem** opens the [issue tracker](https://github.com/hostkurd/flocms-studio/issues).
