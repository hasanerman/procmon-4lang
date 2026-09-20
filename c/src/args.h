#ifndef ARGS_H
#define ARGS_H

#include <stddef.h>
#include <stdint.h>

#include "proc.h"

#define FILTER_MAX 128

typedef struct {
    unsigned interval_ms;
    char filter[FILTER_MAX];
    SortKey sort;
    unsigned top;
    uint32_t kill_pid;
    int has_kill;
    int once;
} Options;

typedef enum { ARGS_OK = 0, ARGS_ERROR = 1, ARGS_HELP = 2 } ArgsStatus;

void args_defaults(Options *out);
ArgsStatus args_parse(int argc, char **argv, Options *out, char *err, size_t err_len);
const char *args_usage(void);

#endif
