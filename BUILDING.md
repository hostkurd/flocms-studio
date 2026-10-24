# Building FloCMS Studio

Studio is C++20 with Qt 6 and CMake, built against Qt 6.12. Until the final
release, builds ship as a folder: `Studio.exe` with the Qt and FloStudio
DLLs (a `.app` bundle on macOS). The final release (stage 9) links
everything into one `Studio.exe`; see "The final single-file build" below.

## What you need

- CMake 3.24 or newer, and Ninja (recommended).
- Qt 6.12 with Qt Quick, Quick Controls and Qt Test. The Qt online
  installer (open-source) or [aqtinstall](https://github.com/miurahr/aqtinstall)
  both work. Qt 6.4 or newer also builds, for a quick start on Linux.
- A compiler: Visual Studio 2022 on Windows, Xcode on macOS, GCC 13 or
  Clang 17 on Linux.

## Build and run (development)

Windows, in a "x64 Native Tools Command Prompt for VS 2022":

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.12.0\msvc2022_64
cmake --build build
ctest --test-dir build --output-on-failure
build\bin\Studio.exe
```

Add `C:\Qt\6.12.0\msvc2022_64\bin` to `PATH` to run it from the build
folder. To make a folder you can copy to another PC, as CI does:
`windeployqt --qmldir app\qml build\bin\Studio.exe`.

macOS:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=~/Qt/6.12.0/macos
cmake --build build
ctest --test-dir build --output-on-failure
open "build/bin/FloCMS Studio.app"
```

Linux:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=~/Qt/6.12.0/gcc_64
cmake --build build
ctest --test-dir build --output-on-failure
build/bin/flocms-studio
```

Every build accepts `--version` (build details), `--smoke-test` (open the
window, draw it once, fail on any warning) and `--screenshot file.png`.

## Options

| CMake option | Default | Meaning |
|---|---|---|
| `FLOSTUDIO_SHARED_LIBS` | `ON` | Build FloStudioCore, FloStudioDatabase and FloStudioSecurity as DLLs / shared libraries; `OFF` builds them into Studio (the final release) |
| `FLOSTUDIO_WARNINGS_AS_ERRORS` | `ON` | Compiler warnings fail the build (our own files only) |
| `FLOSTUDIO_SANITIZE` | `OFF` | AddressSanitizer + UndefinedBehaviorSanitizer (GCC, Clang) |
| `FLOSTUDIO_BUILD_TESTS` | `ON` | Build the Qt Test suites in `tests/` |

clang-tidy, as CI runs it: build with Clang, then
`python3 tools/run_clang_tidy.py build`.

## Release builds

CI's `release` job (`.github/workflows/ci.yml`) shows the exact steps:
build, run the tests, collect Qt with `windeployqt` / `macdeployqt`, write
the notices with `tools/make_notices.py`, then build the Windows installer
from `installer/windows/studio.iss` with Inno Setup 6, or a `.dmg` with
`hdiutil` on macOS.

## The final single-file build (stage 9)

Qt doesn't publish static builds. `.github/scripts/build_static_qt.py`
builds one from Qt's official source archives (qtbase, qtshadertools,
qtsvg, qtdeclarative, qttools), checking each against Qt's checksums. On
Windows it uses the static C++ runtime (`/MT`), so `Studio.exe` needs no
Visual C++ DLLs; on macOS `--universal` builds for Intel and Apple Silicon.
It takes a few hours.

```
python .github/scripts/build_static_qt.py --version 6.12.0 --prefix C:\Qt\6.12.0-static --work C:\q
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DFLOSTUDIO_SHARED_LIBS=OFF -DCMAKE_TOOLCHAIN_FILE=C:\Qt\6.12.0-static\lib\cmake\Qt6\qt.toolchain.cmake
cmake --build build
```

The result is one `build\bin\Studio.exe` (or one universal
`FloCMS Studio.app`). CI will build it this way in stage 9.

## Using your own Qt (LGPL)

Studio uses Qt under the GNU LGPL v3. You may change Qt and use your
version with Studio: replace the Qt DLLs or frameworks that come with it,
or build Studio from the source of the same release with the steps above,
pointing `CMAKE_PREFIX_PATH` (or, for a static Qt, `CMAKE_TOOLCHAIN_FILE`)
at your Qt.
`THIRD-PARTY-NOTICES.txt`, shipped with every release, lists the Qt
version, where its source is, and the licenses.
