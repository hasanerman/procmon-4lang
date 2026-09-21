#ifndef _WIN32

#define _POSIX_C_SOURCE 200809L

#include "proc.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STAT_BUFFER 4096
#define LINUX_INIT_PID 1u
#define NS_PER_SEC 1000000000ull
#define TERM_GRACE_MS 200u

static long clock_ticks(void) {
    static long ticks;

    if (ticks == 0) {
        ticks = sysconf(_SC_CLK_TCK);
        if (ticks <= 0) {
            ticks = 100;
        }
    }
    return ticks;
}

static long page_size(void) {
    static long size;

    if (size == 0) {
        size = sysconf(_SC_PAGESIZE);
        if (size <= 0) {
            size = 4096;
        }
    }
    return size;
}

static int read_file(const char *path, char *buffer, size_t size) {
    FILE *file = fopen(path, "r");
    size_t read_bytes;

    if (file == NULL) {
        return 0;
    }
    read_bytes = fread(buffer, 1, size - 1, file);
    fclose(file);
    buffer[read_bytes] = '\0';
    return read_bytes > 0;
}

static int parse_stat(const char *text, ProcessInfo *info) {
    const char *open_paren = strchr(text, '(');
    const char *close_paren = strrchr(text, ')');
    const char *cursor;
    unsigned long long utime = 0;
    unsigned long long stime = 0;
    long threads = 0;
    long rss_pages = 0;
    size_t name_len;
    int field;

    if (open_paren == NULL || close_paren == NULL || close_paren < open_paren) {
        return 0;
    }
    name_len = (size_t)(close_paren - open_paren - 1);
    if (name_len >= sizeof(info->name)) {
        name_len = sizeof(info->name) - 1;
    }
    memcpy(info->name, open_paren + 1, name_len);
    info->name[name_len] = '\0';

    cursor = close_paren + 1;
    for (field = 3; field <= 24; field++) {
        while (*cursor == ' ') {
            cursor++;
        }
        if (*cursor == '\0') {
            return 0;
        }
        if (field == 14) {
            utime = strtoull(cursor, NULL, 10);
        } else if (field == 15) {
            stime = strtoull(cursor, NULL, 10);
        } else if (field == 20) {
            threads = strtol(cursor, NULL, 10);
        } else if (field == 24) {
            rss_pages = strtol(cursor, NULL, 10);
        }
        while (*cursor != ' ' && *cursor != '\0') {
            cursor++;
        }
    }

    info->cpu_time_ns = (uint64_t)((utime + stime) * (unsigned long long)NS_PER_SEC / (unsigned long long)clock_ticks());
    info->cpu_known = 1;
    info->threads = threads > 0 ? (uint32_t)threads : 0;
    if (rss_pages > 0) {
        info->mem_bytes = (uint64_t)rss_pages * (uint64_t)page_size();
        info->mem_known = 1;
    }
    return 1;
}

static int is_numeric(const char *text) {
    size_t i;

    for (i = 0; text[i] != '\0'; i++) {
        if (!isdigit((unsigned char)text[i])) {
            return 0;
        }
    }
    return i > 0;
}

ProcStatus proc_snapshot(ProcessList *out) {
    DIR *dir;
    struct dirent *entry;
    ProcStatus status = PROC_OK;

    out->count = 0;
    out->wall_ns = proc_monotonic_ns();

    dir = opendir("/proc");
    if (dir == NULL) {
        return PROC_ERR_SNAPSHOT;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[64];
        char buffer[STAT_BUFFER];
        ProcessInfo info;

        if (!is_numeric(entry->d_name)) {
            continue;
        }
        memset(&info, 0, sizeof(info));
        info.pid = (uint32_t)strtoul(entry->d_name, NULL, 10);

        snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
        if (!read_file(path, buffer, sizeof(buffer))) {
            continue;
        }
        if (!parse_stat(buffer, &info)) {
            continue;
        }
        if (!proc_list_push(out, &info)) {
            status = PROC_ERR_MEMORY;
            break;
        }
    }

    closedir(dir);
    return status;
}

int proc_is_protected(uint32_t pid) {
    return pid == 0 || pid == LINUX_INIT_PID;
}

ProcStatus proc_kill(uint32_t pid) {
    if (proc_is_protected(pid)) {
        return PROC_ERR_PROTECTED;
    }
    if (kill((pid_t)pid, SIGTERM) != 0) {
        return errno == EPERM ? PROC_ERR_DENIED : PROC_ERR_NOT_FOUND;
    }
    proc_sleep_ms(TERM_GRACE_MS);
    if (kill((pid_t)pid, 0) == 0 && kill((pid_t)pid, SIGKILL) != 0 && errno == EPERM) {
        return PROC_ERR_DENIED;
    }
    return PROC_OK;
}

uint32_t proc_self_pid(void) {
    return (uint32_t)getpid();
}

uint64_t proc_monotonic_ns(void) {
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * NS_PER_SEC + (uint64_t)now.tv_nsec;
}

unsigned proc_cpu_count(void) {
    long count = sysconf(_SC_NPROCESSORS_ONLN);

    return count > 0 ? (unsigned)count : 1u;
}

void proc_sleep_ms(unsigned ms) {
    struct timespec request;

    request.tv_sec = (time_t)(ms / 1000u);
    request.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&request, NULL);
}

#endif
