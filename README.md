# FloCMS Studio

A desktop app that creates, runs and changes [FloCMS](https://github.com/HostKurd/FloCMS) projects. Every action runs a real `php flo` or `composer` command in the project and shows it.

This repo is the native app, written in C++ with Qt 6, for Windows and macOS first and Linux later. It is being built in stages; nothing here is ready to use yet. Stage 1 sets up the build: the window you get only shows build details.

The first version, built with Tauri, is [`hostkurd/flocms-studio-lite`](https://github.com/hostkurd/flocms-studio-lite). It is the reference for this app's behaviour.

## Layout

```
app/             Studio: the program (QML screens, C++ view models)
libs/core/       FloStudioCore: settings, tools, projects, commands, parsers
libs/database/   FloStudioDatabase: the Database tab's logic (uses Core)
libs/security/   FloStudioSecurity: passwords and secrets (uses Core)
tests/           Qt Test suites
installer/       Windows installer (Inno Setup)
tools/           clang-tidy runner, release notices
prototypes/      stage 0 Kurdish and RTL check (throwaway)
```

The three libraries are built into the program by default, so a release is one `Studio.exe` on Windows and one `FloCMS Studio.app` on macOS. Each library can only use the ones it depends on: Database and Security use Core, Core uses only Qt.

## Building

See [BUILDING.md](BUILDING.md). CI builds every pull request on Windows, macOS and Ubuntu; the release builds (a static `Studio.exe`, its installer, and a `.dmg`) are in each CI run's artifacts.

## License

MIT, see [LICENSE](LICENSE). Release builds contain Qt, used under the GNU LGPL v3; `THIRD-PARTY-NOTICES.txt` ships with them and BUILDING.md explains how to rebuild with your own Qt. The stage 0 prototype's bundled fonts are under the SIL Open Font License 1.1 (see `prototypes/rtl-check/fonts/OFL.txt`).
