#include "proc.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 128

int proc_list_push(ProcessList *list, const ProcessInfo *info) {
    if (list->count == list->capacity) {
        size_t next = list->capacity == 0 ? INITIAL_CAPACITY : list->capacity * 2;
        ProcessInfo *grown = (ProcessInfo *)realloc(list->items, next * sizeof(ProcessInfo));
        if (grown == NULL) {
            return 0;
        }
        list->items = grown;
        list->capacity = next;
    }
    list->items[list->count++] = *info;
    return 1;
}

void proc_list_free(ProcessList *list) {
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    list->wall_ns = 0;
}

int proc_list_copy(ProcessList *dst, const ProcessList *src) {
    size_t i;

    dst->count = 0;
    dst->wall_ns = src->wall_ns;
    for (i = 0; i < src->count; i++) {
        if (!proc_list_push(dst, &src->items[i])) {
            return 0;
        }
    }
    return 1;
}

static int contains_fold(const char *haystack, const char *needle) {
    size_t n = strlen(needle);
    size_t h = strlen(haystack);
    size_t i;
    size_t j;

    if (n == 0) {
        return 1;
    }
    if (n > h) {
        return 0;
    }
    for (i = 0; i + n <= h; i++) {
        for (j = 0; j < n; j++) {
            int a = tolower((unsigned char)haystack[i + j]);
            int b = tolower((unsigned char)needle[j]);
            if (a != b) {
                break;
            }
        }
        if (j == n) {
            return 1;
        }
    }
    return 0;
}

void proc_list_filter(ProcessList *list, const char *needle) {
    size_t kept = 0;
    size_t i;

    if (needle == NULL || needle[0] == '\0') {
        return;
    }
    for (i = 0; i < list->count; i++) {
        if (contains_fold(list->items[i].name, needle)) {
            list->items[kept++] = list->items[i];
        }
    }
    list->count = kept;
}

static int compare_name(const void *lhs, const void *rhs) {
    const ProcessInfo *a = (const ProcessInfo *)lhs;
    const ProcessInfo *b = (const ProcessInfo *)rhs;
    size_t i;

    for (i = 0; a->name[i] != '\0' && b->name[i] != '\0'; i++) {
        int ca = tolower((unsigned char)a->name[i]);
        int cb = tolower((unsigned char)b->name[i]);
        if (ca != cb) {
            return ca < cb ? -1 : 1;
        }
    }
    if (a->name[i] == b->name[i]) {
        return a->pid < b->pid ? -1 : (a->pid > b->pid ? 1 : 0);
    }
    return a->name[i] == '\0' ? -1 : 1;
}

static int compare_cpu(const void *lhs, const void *rhs) {
    const ProcessInfo *a = (const ProcessInfo *)lhs;
    const ProcessInfo *b = (const ProcessInfo *)rhs;

    if (a->cpu_percent > b->cpu_percent) {
        return -1;
    }
    if (a->cpu_percent < b->cpu_percent) {
        return 1;
    }
    return a->pid < b->pid ? -1 : (a->pid > b->pid ? 1 : 0);
}

static int compare_mem(const void *lhs, const void *rhs) {
    const ProcessInfo *a = (const ProcessInfo *)lhs;
    const ProcessInfo *b = (const ProcessInfo *)rhs;

    if (a->mem_bytes > b->mem_bytes) {
        return -1;
    }
    if (a->mem_bytes < b->mem_bytes) {
        return 1;
    }
    return a->pid < b->pid ? -1 : (a->pid > b->pid ? 1 : 0);
}

static int compare_pid(const void *lhs, const void *rhs) {
    const ProcessInfo *a = (const ProcessInfo *)lhs;
    const ProcessInfo *b = (const ProcessInfo *)rhs;

    return a->pid < b->pid ? -1 : (a->pid > b->pid ? 1 : 0);
}

void proc_list_sort(ProcessList *list, SortKey key) {
    int (*cmp)(const void *, const void *) = compare_cpu;

    if (key == SORT_MEM) {
        cmp = compare_mem;
    } else if (key == SORT_PID) {
        cmp = compare_pid;
    } else if (key == SORT_NAME) {
        cmp = compare_name;
    }
    if (list->count > 1) {
        qsort(list->items, list->count, sizeof(ProcessInfo), cmp);
    }
}

const ProcessInfo *proc_list_find(const ProcessList *list, uint32_t pid) {
    size_t i;

    for (i = 0; i < list->count; i++) {
        if (list->items[i].pid == pid) {
            return &list->items[i];
        }
    }
    return NULL;
}

void proc_apply_cpu(ProcessList *current, const ProcessList *previous, unsigned cpu_count) {
    uint64_t wall_delta;
    size_t i;

    if (previous == NULL || previous->count == 0 || current->wall_ns <= previous->wall_ns) {
        return;
    }
    if (cpu_count == 0) {
        cpu_count = 1;
    }
    wall_delta = current->wall_ns - previous->wall_ns;

    for (i = 0; i < current->count; i++) {
        ProcessInfo *item = &current->items[i];
        const ProcessInfo *old = proc_list_find(previous, item->pid);
        double busy;

        if (old == NULL || !old->cpu_known || !item->cpu_known) {
            continue;
        }
        if (item->cpu_time_ns < old->cpu_time_ns) {
            continue;
        }
        busy = (double)(item->cpu_time_ns - old->cpu_time_ns);
        item->cpu_percent = busy * 100.0 / ((double)wall_delta * (double)cpu_count);
        if (item->cpu_percent > 100.0) {
            item->cpu_percent = 100.0;
        }
    }
}
