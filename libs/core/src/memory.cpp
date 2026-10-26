#include <flostudio/core/memory.h>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// psapi's GetProcessMemoryInfo is kernel32's K32GetProcessMemoryInfo
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#include <vector>
#elif defined(Q_OS_MACOS)
#include <mach/mach.h>
#include <malloc/malloc.h>
#else
#include <QFile>
#include <QList>
#include <malloc.h>
#endif

namespace FloStudio::Core {

#if defined(Q_OS_LINUX)
namespace {

// "VmRSS:   80576 kB" from /proc/self/status, in bytes
qint64 statusValue(const QByteArray &status, const QByteArray &key)
{
    for (const QByteArray &line : status.split('\n')) {
        if (!line.startsWith(key + ':'))
            continue;
        const QList<QByteArray> parts = line.mid(key.size() + 1).simplified().split(' ');
        return parts.isEmpty() ? 0 : parts.constFirst().toLongLong() * 1024;
    }
    return 0;
}

} // namespace
#endif

ProcessMemory currentProcessMemory()
{
    ProcessMemory memory;
#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof counters;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the API takes the base struct
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters), sizeof counters)) {
        memory.inUse = static_cast<qint64>(counters.WorkingSetSize);
        memory.own = static_cast<qint64>(counters.PrivateUsage);
    }
#elif defined(Q_OS_MACOS)
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the API takes an int array
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        memory.inUse = static_cast<qint64>(info.phys_footprint);
        memory.own = static_cast<qint64>(info.phys_footprint);
    }
#else
    QFile file(QStringLiteral("/proc/self/status"));
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray status = file.readAll();
        memory.inUse = statusValue(status, "VmRSS");
        memory.own = statusValue(status, "RssAnon");
    }
#endif
    return memory;
}

void returnFreeMemory()
{
#if defined(Q_OS_WIN)
    const DWORD count = GetProcessHeaps(0, nullptr);
    std::vector<HANDLE> heaps(count);
    const DWORD got = heaps.empty() ? 0 : GetProcessHeaps(count, heaps.data());
    for (DWORD i = 0; i < got && i < count; ++i)
        HeapCompact(heaps[i], 0);
#elif defined(Q_OS_MACOS)
    malloc_zone_pressure_relief(nullptr, 0);
#elif defined(__GLIBC__)
    malloc_trim(0);
#endif
}

} // namespace FloStudio::Core
