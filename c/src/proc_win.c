#ifdef _WIN32

#include "proc.h"

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <stdio.h>
#include <string.h>

#define FILETIME_TO_NS 100ull
#define WIN_SYSTEM_PID 4u
#define WIN_IDLE_PID 0u

static uint64_t filetime_ns(const FILETIME *ft) {
    ULARGE_INTEGER value;

    value.LowPart = ft->dwLowDateTime;
    value.HighPart = ft->dwHighDateTime;
    return value.QuadPart * FILETIME_TO_NS;
}

static void fill_from_handle(HANDLE process, ProcessInfo *info) {
    PROCESS_MEMORY_COUNTERS counters;
    FILETIME creation;
    FILETIME exit_time;
    FILETIME kernel;
    FILETIME user;

    if (GetProcessMemoryInfo(process, &counters, sizeof(counters))) {
        info->mem_bytes = (uint64_t)counters.WorkingSetSize;
        info->mem_known = 1;
    }
    if (GetProcessTimes(process, &creation, &exit_time, &kernel, &user)) {
        info->cpu_time_ns = filetime_ns(&kernel) + filetime_ns(&user);
        info->cpu_known = 1;
    }
}

ProcStatus proc_snapshot(ProcessList *out) {
    HANDLE snapshot = INVALID_HANDLE_VALUE;
    PROCESSENTRY32 entry;
    ProcStatus status = PROC_OK;

    out->count = 0;
    out->wall_ns = proc_monotonic_ns();

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return PROC_ERR_SNAPSHOT;
    }

    entry.dwSize = sizeof(entry);
    if (!Process32First(snapshot, &entry)) {
        status = PROC_ERR_SNAPSHOT;
        goto cleanup;
    }

    do {
        ProcessInfo info;
        HANDLE process;

        memset(&info, 0, sizeof(info));
        info.pid = (uint32_t)entry.th32ProcessID;
        info.threads = (uint32_t)entry.cntThreads;
        snprintf(info.name, sizeof(info.name), "%s", entry.szExeFile);

        process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (process != NULL) {
            fill_from_handle(process, &info);
            CloseHandle(process);
        }

        if (!proc_list_push(out, &info)) {
            status = PROC_ERR_MEMORY;
            goto cleanup;
        }
    } while (Process32Next(snapshot, &entry));

cleanup:
    CloseHandle(snapshot);
    return status;
}

int proc_is_protected(uint32_t pid) {
    return pid == WIN_IDLE_PID || pid == WIN_SYSTEM_PID;
}

ProcStatus proc_kill(uint32_t pid) {
    HANDLE process;
    ProcStatus status = PROC_OK;

    if (proc_is_protected(pid)) {
        return PROC_ERR_PROTECTED;
    }
    process = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (process == NULL) {
        DWORD code = GetLastError();
        if (code == ERROR_INVALID_PARAMETER) {
            return PROC_ERR_NOT_FOUND;
        }
        return PROC_ERR_DENIED;
    }
    if (!TerminateProcess(process, 1)) {
        status = GetLastError() == ERROR_ACCESS_DENIED ? PROC_ERR_DENIED : PROC_ERR_NOT_FOUND;
    }
    CloseHandle(process);
    return status;
}

uint32_t proc_self_pid(void) {
    return (uint32_t)GetCurrentProcessId();
}

uint64_t proc_monotonic_ns(void) {
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;

    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    QueryPerformanceCounter(&counter);
    return (uint64_t)((double)counter.QuadPart * 1e9 / (double)frequency.QuadPart);
}

unsigned proc_cpu_count(void) {
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    return info.dwNumberOfProcessors == 0 ? 1u : (unsigned)info.dwNumberOfProcessors;
}

void proc_sleep_ms(unsigned ms) {
    Sleep(ms);
}

#endif
