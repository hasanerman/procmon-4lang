#ifndef PROC_H
#define PROC_H

#include <stddef.h>
#include <stdint.h>

#define PROC_NAME_MAX 256

typedef struct {
    uint32_t pid;
    char name[PROC_NAME_MAX];
    uint64_t mem_bytes;
    uint32_t threads;
    uint64_t cpu_time_ns;
    double cpu_percent;
    int mem_known;
    int cpu_known;
} ProcessInfo;

typedef struct {
    ProcessInfo *items;
    size_t count;
    size_t capacity;
    uint64_t wall_ns;
} ProcessList;

typedef enum {
    PROC_OK = 0,
    PROC_ERR_SNAPSHOT,
    PROC_ERR_MEMORY,
    PROC_ERR_DENIED,
    PROC_ERR_NOT_FOUND,
    PROC_ERR_PROTECTED
} ProcStatus;

typedef enum { SORT_CPU, SORT_MEM, SORT_PID, SORT_NAME } SortKey;

ProcStatus proc_snapshot(ProcessList *out);
ProcStatus proc_kill(uint32_t pid);
uint32_t proc_self_pid(void);
uint64_t proc_monotonic_ns(void);
unsigned proc_cpu_count(void);
int proc_is_protected(uint32_t pid);
void proc_sleep_ms(unsigned ms);

int proc_list_push(ProcessList *list, const ProcessInfo *info);
void proc_list_free(ProcessList *list);
int proc_list_copy(ProcessList *dst, const ProcessList *src);
void proc_list_filter(ProcessList *list, const char *needle);
void proc_list_sort(ProcessList *list, SortKey key);
void proc_apply_cpu(ProcessList *current, const ProcessList *previous, unsigned cpu_count);
const ProcessInfo *proc_list_find(const ProcessList *list, uint32_t pid);

#endif
