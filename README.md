# FloCMS Studio

A desktop app that creates, runs and changes [FloCMS](https://github.com/HostKurd/FloCMS) projects. Every action runs a real `php flo` or `composer` command in the project and shows it.

This repo is the native app, written in C++ with Qt 6, for Windows and macOS first and Linux later. It is being built in stages; nothing here is ready to use yet. Stage 1 set up the build; stage 2 adds the Core library (settings, finding PHP, Composer and MariaDB, reading projects, running commands, reading their output). The window still only shows build details.

The first version, built with Tauri, is [`hostkurd/flocms-studio-lite`](https://github.com/hostkurd/flocms-studio-lite). This app's logic is written from FloCMS's own code and tested against real FloCMS output (`tests/fixtures/`), not ported from it.

## Layout

```
app/             Studio: the program (QML screens, C++ view models)
libs/core/       FloStudioCore: settings, tools, projects, commands, parsers
libs/database/   FloStudioDatabase: the Database tab's logic (uses Core)
libs/security/   FloStudioSecurity: passwords and secrets (uses Core)
tests/           Qt Test suites, and real FloCMS command output (fixtures/)
installer/       Windows installer (Inno Setup)
tools/           clang-tidy runner, release notices, fixture capture
prototypes/      stage 0 Kurdish and RTL check (throwaway)
```

Until the final release, Studio ships as a folder: `Studio.exe` with the FloStudio and Qt DLLs on Windows, a `FloCMS Studio.app` bundle on macOS. The final release links everything into one `Studio.exe`. Each library can only use the ones it depends on: Database and Security use Core, Core uses only Qt.

## Building

See [BUILDING.md](BUILDING.md). CI builds every pull request on Windows, macOS and Ubuntu; the release builds (a portable Windows folder, its installer, and a `.dmg`) are in each CI run's artifacts.

## License

MIT, see [LICENSE](LICENSE). Studio uses Qt under the GNU LGPL v3 (texts in `licenses/`); `THIRD-PARTY-NOTICES.txt` ships with every build and BUILDING.md explains how to use your own Qt. The stage 0 prototype's bundled fonts are under the SIL Open Font License 1.1 (see `prototypes/rtl-check/fonts/OFL.txt`).
