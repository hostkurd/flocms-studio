# Install Studio

FloCMS Studio runs on Windows, macOS and Linux. Each release on the [releases page](https://github.com/hostkurd/flocms-studio/releases) has one file per system and a `SHA256SUMS.txt` file to check them. Installing Studio needs no administrator rights on Windows, and Studio installs PHP, Composer and MariaDB for your user only, when you ask it to.

## Windows

Studio runs on 64-bit Windows 10 and 11. Download one of these:

- `FloCMS-Studio-<version>-x64.msi`, the installer. It installs Studio for your user only, without administrator rights, into `%LOCALAPPDATA%\Programs\FloCMS Studio`, and adds Studio to the Start menu.
- `FloCMS-Studio-<version>-windows-x64-portable.zip`, the portable folder. Unzip it and run `Studio.exe`. Nothing is installed.

A newer installer replaces the installed version. An older installer stops with the message "A newer version of FloCMS Studio is already installed."

### Code signing

The Windows files (`Studio.exe` and the `.msi`) are signed through the SignPath Foundation program for open source projects. Windows shows the publisher as **SignPath Foundation**. Each signed file also names FloCMS Studio and its version in **Properties > Details**.

A release made before signing was set up says in its notes that the files are not signed (releases up to 0.1.2 also had "(unsigned)" in their title). Windows SmartScreen then warns you before the first start. See [Privacy and code signing](privacy.md).

## macOS

Studio runs on macOS 14.4 or newer, on Intel and Apple Silicon. Download `FloCMS-Studio-<version>-macos-universal.dmg`, open it, and drag **FloCMS Studio** to **Applications**.

The macOS app is not signed by the SignPath program and is not notarized yet. The first time you open it, macOS refuses. To open it:

1. Try to open **FloCMS Studio** once.
2. Open **System Settings > Privacy & Security**.
3. Choose **Open Anyway** for FloCMS Studio.

## Linux

Studio is built for 64-bit x86 (x86_64 / amd64) Linux.

### Ubuntu 24.04+ and Debian 13+

Download `FloCMS-Studio-<version>-amd64.deb` and install it:

```bash
sudo apt install ./FloCMS-Studio-<version>-amd64.deb
```

You can also open the file in your software center. FloCMS Studio is then in the app menu, and the `flocms-studio` command starts it. The package installs Studio into `/opt/flocms-studio`.

### Other Linux systems

Download `FloCMS-Studio-<version>-linux-x86_64.AppImage`, make it executable and run it:

```bash
chmod +x FloCMS-Studio-<version>-linux-x86_64.AppImage
./FloCMS-Studio-<version>-linux-x86_64.AppImage
```

In a file manager you can instead right-click the file, open **Properties**, and allow it to run as a program.

## Check a download

`SHA256SUMS.txt` lists the SHA-256 of every file in the release. Compare the hash of your file with its line in that file.

Windows (PowerShell):

```powershell
Get-FileHash .\FloCMS-Studio-<version>-x64.msi -Algorithm SHA256
```

macOS:

```bash
shasum -a 256 FloCMS-Studio-<version>-macos-universal.dmg
```

Linux, with `SHA256SUMS.txt` in the same folder as the download:

```bash
sha256sum -c --ignore-missing SHA256SUMS.txt
```

## System requirements

What the release builds show:

| System | Versions | Architectures |
|---|---|---|
| Windows | 10 and 11, 64-bit | x64 |
| macOS | 14.4 or newer | Intel and Apple Silicon (one universal app) |
| Linux `.deb` | Ubuntu 24.04+, Debian 13+ | amd64 |
| Linux AppImage | other distributions | x86_64 |

Studio is built with Qt 6.12. On Windows and macOS, Qt is linked into the program, so it needs nothing else. The Linux packages carry their own Qt, with X11 and Wayland support.

To create and run projects, Studio needs PHP 8.1 or newer and Composer. Studio can download both for you. See [Your first project](first-project.md#download-the-tools).

## Run one Studio at a time

Studio runs once per user. When you start it again, the running Studio comes to the front.

## Uninstall

- **Windows installer:** remove FloCMS Studio in **Settings > Apps**.
- **Windows portable folder:** delete the folder.
- **macOS:** delete FloCMS Studio from **Applications**.
- **Linux `.deb`:** run `sudo apt remove flocms-studio`.
- **AppImage:** delete the file.

Uninstalling leaves your settings, the tools Studio downloaded, Studio's databases and your projects in place. Delete them yourself if you want them gone.

## Where Studio keeps its files

All of Studio's files are in folders of your user:

- **Settings:** a file named `settings.json` in your user's settings folder for FloCMS Studio.
- **Downloaded tools:** **Settings > PHP and tools** shows the folder (see [Settings](settings.md#php-and-tools)).
- **Studio's MariaDB databases:** **Settings > Database server** shows the **Databases folder** and has an **Open folder** button.
- **Passwords:** the database passwords Studio saves are in the system's secure store: Windows Credential Manager, the macOS Keychain, or the system's keyring on Linux.

Your projects stay where you created them.
