#include "args.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_INTERVAL_MS 1000u
#define DEFAULT_TOP 25u
#define MIN_INTERVAL_MS 50u
#define MAX_INTERVAL_MS 60000u
#define MAX_TOP 500u

static void set_err(char *err, size_t err_len, const char *msg) {
    if (err == NULL || err_len == 0) {
        return;
    }
    snprintf(err, err_len, "%s", msg);
}

static int parse_ulong(const char *text, unsigned long *out) {
    char *end = NULL;
    unsigned long value;

    if (text == NULL || *text == '\0') {
        return 0;
    }
    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }
    *out = value;
    return 1;
}

static int parse_sort(const char *text, SortKey *out) {
    if (strcmp(text, "cpu") == 0) {
        *out = SORT_CPU;
    } else if (strcmp(text, "mem") == 0) {
        *out = SORT_MEM;
    } else if (strcmp(text, "pid") == 0) {
        *out = SORT_PID;
    } else if (strcmp(text, "name") == 0) {
        *out = SORT_NAME;
    } else {
        return 0;
    }
    return 1;
}

void args_defaults(Options *out) {
    memset(out, 0, sizeof(*out));
    out->interval_ms = DEFAULT_INTERVAL_MS;
    out->sort = SORT_CPU;
    out->top = DEFAULT_TOP;
}

const char *args_usage(void) {
    return "procmon [--interval <ms>] [--filter <metin>] [--sort cpu|mem|pid|name]\n"
           "        [--top <n>] [--kill <pid>] [--once] [--help]\n";
}

ArgsStatus args_parse(int argc, char **argv, Options *out, char *err, size_t err_len) {
    int i;

    args_defaults(out);

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = NULL;
        unsigned long number = 0;
        int needs_value = strcmp(arg, "--interval") == 0 || strcmp(arg, "--filter") == 0 ||
                          strcmp(arg, "--sort") == 0 || strcmp(arg, "--top") == 0 ||
                          strcmp(arg, "--kill") == 0;

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            return ARGS_HELP;
        }
        if (strcmp(arg, "--once") == 0) {
            out->once = 1;
            continue;
        }
        if (!needs_value) {
            set_err(err, err_len, "bilinmeyen arguman");
            return ARGS_ERROR;
        }
        if (i + 1 >= argc) {
            set_err(err, err_len, "argumanin degeri eksik");
            return ARGS_ERROR;
        }
        value = argv[++i];

        if (strcmp(arg, "--interval") == 0) {
            if (!parse_ulong(value, &number) || number < MIN_INTERVAL_MS || number > MAX_INTERVAL_MS) {
                set_err(err, err_len, "interval 50 ile 60000 arasinda olmalidir");
                return ARGS_ERROR;
            }
            out->interval_ms = (unsigned)number;
        } else if (strcmp(arg, "--top") == 0) {
            if (!parse_ulong(value, &number) || number == 0 || number > MAX_TOP) {
                set_err(err, err_len, "top 1 ile 500 arasinda olmalidir");
                return ARGS_ERROR;
            }
            out->top = (unsigned)number;
        } else if (strcmp(arg, "--kill") == 0) {
            if (!parse_ulong(value, &number) || number > UINT_MAX) {
                set_err(err, err_len, "kill icin gecerli bir pid gerekir");
                return ARGS_ERROR;
            }
            out->kill_pid = (uint32_t)number;
            out->has_kill = 1;
        } else if (strcmp(arg, "--sort") == 0) {
            if (!parse_sort(value, &out->sort)) {
                set_err(err, err_len, "sort cpu, mem, pid veya name olmalidir");
                return ARGS_ERROR;
            }
        } else {
            if (strlen(value) >= FILTER_MAX) {
                set_err(err, err_len, "filtre metni cok uzun");
                return ARGS_ERROR;
            }
            snprintf(out->filter, sizeof(out->filter), "%s", value);
        }
    }

    return ARGS_OK;
}
