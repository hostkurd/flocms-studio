#pragma once

#include <flostudio/core/export.h>

#include <QtGlobal>

namespace FloStudio::Core {

// This process's memory in bytes (0 when the system doesn't say).
// `inUse`: Windows the working set (Task Manager's "Working set" column;
// its default "Memory" column shows only the private part of it), macOS
// the physical footprint (Activity Monitor's "Memory"), Linux the resident
// set (`ps` RSS). `own`: the part only this program uses: Windows private
// bytes, macOS the footprint, Linux the anonymous resident memory.
struct ProcessMemory
{
    qint64 inUse = 0;
    qint64 own = 0;
};

FLOSTUDIOCORE_EXPORT ProcessMemory currentProcessMemory();

// Gives memory the program freed back to the system. Allocators keep freed
// blocks for reuse, so after freeing screens or data the system's monitor
// shows no change until this runs: Linux malloc_trim, macOS the malloc
// zones' pressure relief, Windows HeapCompact on the process's heaps.
// Takes a few milliseconds; call it after freeing something, not often.
FLOSTUDIOCORE_EXPORT void returnFreeMemory();

} // namespace FloStudio::Core
