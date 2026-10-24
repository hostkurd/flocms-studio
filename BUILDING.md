# Building FloCMS Studio

Studio is C++20 with Qt 6 and CMake. Releases use Qt 6.12, linked
statically (one `Studio.exe`); for development a normal Qt install is enough.

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

Run `windeployqt build\bin\Studio.exe` once if Windows can't find the Qt
DLLs, or add `C:\Qt\6.12.0\msvc2022_64\bin` to `PATH`.

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
| `FLOSTUDIO_SHARED_LIBS` | `OFF` | Build FloStudioCore, FloStudioDatabase and FloStudioSecurity as DLLs / shared libraries instead of inside Studio |
| `FLOSTUDIO_WARNINGS_AS_ERRORS` | `ON` | Compiler warnings fail the build (our own files only) |
| `FLOSTUDIO_SANITIZE` | `OFF` | AddressSanitizer + UndefinedBehaviorSanitizer (GCC, Clang) |
| `FLOSTUDIO_BUILD_TESTS` | `ON` | Build the Qt Test suites in `tests/` |

clang-tidy, as CI runs it: build with Clang, then
`python3 tools/run_clang_tidy.py build`.

## Release builds: static Qt

Qt doesn't publish static builds. `.github/scripts/build_static_qt.py`
builds one from Qt's official source archives (qtbase, qtshadertools,
qtsvg, qtdeclarative, qttools), checking each against Qt's checksums:

```
python .github/scripts/build_static_qt.py --version 6.12.0 --prefix C:\Qt\6.12.0-static --work C:\q
```

On Windows run it in the VS 2022 command prompt; it builds Qt with the
static C++ runtime (`/MT`), so `Studio.exe` needs no Visual C++ DLLs. On
macOS add `--universal` for Intel and Apple Silicon. It takes a few hours;
CI does it once and caches the result.

Then build Studio with Qt's toolchain file:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\Qt\6.12.0-static\lib\cmake\Qt6\qt.toolchain.cmake
cmake --build build
```

The result is one `build\bin\Studio.exe` (or one universal
`FloCMS Studio.app`). The Windows installer is built from
`installer/windows/studio.iss` with Inno Setup 6; see the `release` job
in `.github/workflows/ci.yml` for the exact steps.

## Using your own Qt (LGPL)

Release builds contain Qt under the GNU LGPL v3. You may change Qt and
relink Studio with it: build your Qt (static or shared), then build Studio
from the source of the same release with the steps above, pointing
`CMAKE_PREFIX_PATH` or `CMAKE_TOOLCHAIN_FILE` at your Qt.
`THIRD-PARTY-NOTICES.txt`, shipped with every release, lists the Qt
version, where its source is, and the licenses.
