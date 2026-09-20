#include "render.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define NAME_COLUMN 30
#define BYTES_PER_MB (1024.0 * 1024.0)

void render_init(void) {
#ifdef _WIN32
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;

    if (console != INVALID_HANDLE_VALUE && GetConsoleMode(console, &mode)) {
        SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    SetConsoleOutputCP(CP_UTF8);
#endif
}

void render_clear(void) {
    fputs("\x1b[2J\x1b[H", stdout);
}

static void truncate_utf8(const char *source, char *target, size_t limit) {
    size_t used = 0;
    size_t i = 0;

    while (source[i] != '\0' && used < limit) {
        unsigned char lead = (unsigned char)source[i];
        size_t width = 1;

        if (lead >= 0xF0) {
            width = 4;
        } else if (lead >= 0xE0) {
            width = 3;
        } else if (lead >= 0xC0) {
            width = 2;
        }
        if (strlen(source + i) < width) {
            break;
        }
        memcpy(target + used, source + i, width);
        used += width;
        i += width;
    }
    target[used] = '\0';
}

void render_table(const ProcessList *list, const char *filter, unsigned top) {
    size_t shown = list->count < (size_t)top ? list->count : (size_t)top;
    size_t i;

    printf("%-7s %-*s %10s %8s %7s\n", "PID", NAME_COLUMN, "NAME", "MEM(MB)", "THREADS", "CPU%");
    for (i = 0; i < 7 + 1 + NAME_COLUMN + 1 + 10 + 1 + 8 + 1 + 7; i++) {
        fputc('-', stdout);
    }
    fputc('\n', stdout);

    for (i = 0; i < shown; i++) {
        const ProcessInfo *item = &list->items[i];
        char name[PROC_NAME_MAX];
        char mem[16];
        char cpu[16];

        truncate_utf8(item->name, name, NAME_COLUMN);
        if (item->mem_known) {
            snprintf(mem, sizeof(mem), "%.1f", (double)item->mem_bytes / BYTES_PER_MB);
        } else {
            snprintf(mem, sizeof(mem), "-");
        }
        if (item->cpu_known) {
            snprintf(cpu, sizeof(cpu), "%.1f", item->cpu_percent);
        } else {
            snprintf(cpu, sizeof(cpu), "-");
        }
        printf("%-7u %-*s %10s %8u %7s\n", item->pid, NAME_COLUMN, name, mem, item->threads, cpu);
    }

    printf("\ntoplam %zu surec", list->count);
    if (filter != NULL && filter[0] != '\0') {
        printf(" (filtre: %s)", filter);
    }
    printf(", gosterilen %zu\n", shown);
}
