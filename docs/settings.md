# Settings

**Settings**, at the bottom of the sidebar, holds Studio's own settings. The sidebar then lists seven sections: **FloServer**, **General**, **PHP and tools**, **Database server**, **Server**, **Editor** and **Project folders**. **Back to projects** returns to your projects. Studio saves each change at once.

## FloServer

When [FloServer](https://github.com/hostkurd/floserver) is installed on this computer, the card says **FloServer `<version>` found**, and **Use FloServer's PHP and Composer** (off at first) makes Studio run every `php` and `composer` command through FloServer's shims. Each project then gets the PHP version in its `floserver.json`, and FloServer's shims come first on the `PATH` of the commands Studio starts. "Turning this off brings back the PHP and Composer you had before." Studio's database server is not changed by it.

When FloServer isn't installed, the card says so and the switch can't be turned on. Studio finds FloServer by its `integration.json` file, in `%LOCALAPPDATA%\FloServer` on Windows, `~/Library/Application Support/FloServer` on macOS and `~/.local/share/floserver` on Linux (or the folder in `FLOSERVER_DATA_DIR`).

## General

- **Language**: English, Arabic (العربية), Central Kurdish (کوردی) or **System**, which follows the computer's language. It changes right away. Arabic and Kurdish turn the whole window right to left.
- **Theme**: **Light**, **Dark** or **System**. System follows the computer's dark mode.

Command output, log lines and FloCMS's own messages stay in English and read left to right in every language.

## PHP and tools

- **PHP to use**: every PHP Studio found, each marked **downloaded by Studio**, **found on this computer** or **chosen by you**. Choose one, or **Choose another…** to pick a PHP program.
- **Composer**: the Composer Studio uses. **Choose…** picks another one (`composer`, `composer.phar` or `composer.bat`); **Use the one found** goes back to the one Studio found.
- **Search again** looks for the tools again.
- **Download PHP, Composer or MariaDB…** opens the downloads dialog. See [Your first project](first-project.md#download-the-tools).

The section also shows the folder downloads go into, for your user only.

While Studio uses FloServer, **PHP to use** says "FloServer, per project (the version in each project's floserver.json)" with the PHP versions FloServer has, and **Composer** shows FloServer's Composer. **Choose another…** and **Search again** are hidden then, and the download button only offers MariaDB.

Studio looks for tools on your `PATH`, then (on macOS and Linux) your login shell's `PATH`, then the usual install folders of Homebrew, Herd, XAMPP, Laragon and Composer-Setup, then its own downloads. Studio needs PHP 8.1 or newer; an older one shows as too old in the sidebar.

## Database server

- **Server new projects use**: **Studio's MariaDB**, the server found on this computer (XAMPP, Laragon or another), or **Another server** with its **Host**, **Port** and **User**. "Existing projects keep the server in their `.env`."
- **Studio's MariaDB**, once you have one: **Start** / **Stop**, **Open log**, **Version** (when you have more than one), **Databases folder** with **Open folder**, **Port**, the password of **User `root`** (**Show**, **Copy**), **Start the server when Studio opens**, **Delete its databases…** and **Remove MariaDB…**.

See [The server and the database](server-and-database.md#studios-mariadb).

## Server

- **First port to try**: the port `php flo serve` tries first, 8000 by default (1024 to 65535). It takes the next free port when this one is in use. New projects also get this port in `APP_URL`.
- **Let other devices on the network open the site (phones for testing)**: the site listens on `0.0.0.0`. Only do this on networks you trust.

## Editor

**Editor command** is how **Open in editor** starts your editor. `{folder}` becomes the project's folder; quotes group words. Without `{folder}`, Studio adds the folder at the end. The default is:

```text
code "{folder}"
```

For PhpStorm, for example: `phpstorm {folder}`. Studio also opens a project's `.env` with this command (**Edit in `.env`** in the **Database** tab).

## Project folders

Studio lists every FloCMS project inside these folders. **Add a folder…** adds one; **Remove** takes one out of the list (the projects stay on disk). The first folder is where new projects go.

## The settings file

Studio keeps its settings in `settings.json`, in your user's settings folder for FloCMS Studio. Passwords are never in it: they are in the system's secure store.

- A value Studio doesn't accept becomes the default.
- A file that isn't valid JSON is kept aside as `settings.broken-<date-time>.json`, and Studio starts with the defaults.
- Keys a newer Studio wrote are kept when an older Studio saves.
