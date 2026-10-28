# FloCMS Studio

[Code signing policy](CODE_SIGNING.md) · [Releases](https://github.com/hostkurd/flocms-studio/releases)

A desktop app that creates, runs and changes [FloCMS](https://github.com/HostKurd/FloCMS) projects. Every action runs a real `php flo` or `composer` command in the project and shows it.

This repo is the native app, written in C++ with Qt 6, for Windows, macOS and Linux. With it you can:

- create a FloCMS project step by step, with PHP, Composer and MariaDB downloaded for you when they are missing (on macOS, Studio uses Homebrew's MariaDB)
- run the website and a MariaDB server of Studio's own, or use the one XAMPP, Laragon or another stack already runs
- see a project at a glance, run its migrations, read its logs, check it with Doctor, update its packages, and generate admin resources, pages, blocks and widgets, controllers and migrations
- use it in English, Arabic or Central Kurdish

The first version, built with Tauri, is [`hostkurd/flocms-studio-lite`](https://github.com/hostkurd/flocms-studio-lite). This app's logic is written from FloCMS's own code and tested against real FloCMS output (`tests/fixtures/`), not ported from it.

## User guide

The guide is in [`docs/`](docs/) and on [flocms.com/docs/studio](https://flocms.com/docs/studio): [install Studio](docs/install.md), [your first project](docs/first-project.md), and [which button runs which command](docs/command-line.md).

## Download

Releases are on the [releases page](https://github.com/hostkurd/flocms-studio/releases): a Windows installer (`.msi`, for your user only, no administrator rights) and a portable folder, a macOS disk image for Intel and Apple Silicon, and for Linux a `.deb` (Ubuntu 24.04+, Debian 13+) and an AppImage. The Windows files are signed through SignPath Foundation (from the first release it accepts; until then they are marked unsigned): see the [code signing policy](CODE_SIGNING.md), which also says what Studio connects to (nothing about you is collected or sent).

## Layout

```
app/             Studio: the program (QML screens, C++ view models)
libs/core/       FloStudioCore: settings, tools, projects, commands, parsers
libs/database/   FloStudioDatabase: the Database tab's logic (uses Core)
libs/security/   FloStudioSecurity: passwords and secrets (uses Core)
tests/           Qt Test suites, and real FloCMS command output (fixtures/)
installer/       Windows installer (WiX Toolset)
.signpath/       what SignPath signs for a release (CODE_SIGNING.md)
tools/           clang-tidy runner, release notices, fixture capture
prototypes/      stage 0 Kurdish and RTL check (throwaway)
```

Releases are one program: `Studio.exe` on Windows (with its license texts beside it), one `FloCMS Studio.app` on macOS, with Qt, the FloStudio libraries and on Windows the C++ runtime linked in. Day-to-day builds use Qt's shared libraries. Each library can only use the ones it depends on: Database and Security use Core, Core uses only Qt.

## Building

See [BUILDING.md](BUILDING.md). CI builds every pull request on Windows, macOS and Ubuntu; the release builds (a portable Windows folder, its installer, a `.dmg` and an AppImage) are in each CI run's artifacts. A version tag runs the release workflow, which signs the Windows files and makes a draft release (CODE_SIGNING.md).

## License

MIT, see [LICENSE](LICENSE). Studio uses Qt under the GNU LGPL v3 (texts in `licenses/`); `THIRD-PARTY-NOTICES.txt` ships with every build and BUILDING.md explains how to use your own Qt. The stage 0 prototype's bundled fonts are under the SIL Open Font License 1.1 (see `prototypes/rtl-check/fonts/OFL.txt`).
