# FloCMS Studio

A desktop app that creates, runs and changes [FloCMS](https://github.com/HostKurd/FloCMS) projects. Every action runs a real `php flo` or `composer` command in the project and shows it.

This repo is the native app, written in C++ with Qt 6, for Windows and macOS first and Linux later. It is being built in stages; nothing here is ready to use yet.

The first version, built with Tauri, is [`hostkurd/flocms-studio-lite`](https://github.com/hostkurd/flocms-studio-lite). It is the reference for this app's behaviour.

## License

MIT, see [LICENSE](LICENSE). Studio uses Qt under the LGPL v3; the third-party notices will ship with the app.
