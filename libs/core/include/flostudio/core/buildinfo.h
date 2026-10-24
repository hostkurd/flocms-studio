#pragma once

#include <flostudio/core/export.h>

#include <QString>

namespace FloStudio::Core {

// What was built and how. Shown in the app's About box and printed by
// `Studio --version`, so bug reports say exactly which build they are about.
// A plain struct: only the functions below are exported, so the DLL build
// (FLOSTUDIO_SHARED_LIBS) has no class with members crossing the boundary.
struct BuildInfo
{
    QString version;              // FloCMS Studio version, e.g. "0.0.1"
    QString qtBuilt;              // Qt version Studio was compiled against
    QString qtRunning;            // Qt version loaded at run time
    bool qtStatic = false;        // Qt linked into the program (D6)
    bool sharedLibraries = false; // FloStudio libraries built as DLLs
    QString compiler;             // e.g. "MSVC 19.44", "Clang 17.0.0"
    QString architecture;         // e.g. "x86_64", "arm64"
};

FLOSTUDIOCORE_EXPORT BuildInfo currentBuildInfo();

// One line per field, for --version.
FLOSTUDIOCORE_EXPORT QString toText(const BuildInfo &info);

// The library's name, used by the other libraries' tests to prove the link
// direction (Database and Security use Core, never the reverse).
FLOSTUDIOCORE_EXPORT QString libraryName();

} // namespace FloStudio::Core
