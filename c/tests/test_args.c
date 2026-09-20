#include "../src/args.h"

#include <stdio.h>
#include <string.h>

static int g_failures;

#define CHECK(condition)                                               \
    do {                                                               \
        if (!(condition)) {                                            \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); \
            g_failures++;                                              \
        }                                                              \
    } while (0)

static ArgsStatus parse(char **argv, int argc, Options *options) {
    char error[128];

    error[0] = '\0';
    return args_parse(argc, argv, options, error, sizeof(error));
}

static void test_defaults(void) {
    Options options;
    char *argv[] = {"procmon"};

    CHECK(parse(argv, 1, &options) == ARGS_OK);
    CHECK(options.interval_ms == 1000);
    CHECK(options.top == 25);
    CHECK(options.sort == SORT_CPU);
    CHECK(options.once == 0);
    CHECK(options.has_kill == 0);
    CHECK(options.filter[0] == '\0');
}

static void test_full_command_line(void) {
    Options options;
    char *argv[] = {"procmon", "--interval", "250", "--filter", "chrome",
                    "--sort", "mem", "--top", "5", "--once"};

    CHECK(parse(argv, 10, &options) == ARGS_OK);
    CHECK(options.interval_ms == 250);
    CHECK(strcmp(options.filter, "chrome") == 0);
    CHECK(options.sort == SORT_MEM);
    CHECK(options.top == 5);
    CHECK(options.once == 1);
}

static void test_sort_keys(void) {
    Options options;
    char *cpu[] = {"procmon", "--sort", "cpu"};
    char *pid[] = {"procmon", "--sort", "pid"};
    char *name[] = {"procmon", "--sort", "name"};
    char *bad[] = {"procmon", "--sort", "disk"};

    CHECK(parse(cpu, 3, &options) == ARGS_OK && options.sort == SORT_CPU);
    CHECK(parse(pid, 3, &options) == ARGS_OK && options.sort == SORT_PID);
    CHECK(parse(name, 3, &options) == ARGS_OK && options.sort == SORT_NAME);
    CHECK(parse(bad, 3, &options) == ARGS_ERROR);
}

static void test_rejects_bad_values(void) {
    Options options;
    char *unknown[] = {"procmon", "--zoom"};
    char *missing[] = {"procmon", "--interval"};
    char *small[] = {"procmon", "--interval", "10"};
    char *large[] = {"procmon", "--interval", "999999"};
    char *zero_top[] = {"procmon", "--top", "0"};
    char *text_top[] = {"procmon", "--top", "abc"};
    char *trailing[] = {"procmon", "--top", "12x"};

    CHECK(parse(unknown, 2, &options) == ARGS_ERROR);
    CHECK(parse(missing, 2, &options) == ARGS_ERROR);
    CHECK(parse(small, 3, &options) == ARGS_ERROR);
    CHECK(parse(large, 3, &options) == ARGS_ERROR);
    CHECK(parse(zero_top, 3, &options) == ARGS_ERROR);
    CHECK(parse(text_top, 3, &options) == ARGS_ERROR);
    CHECK(parse(trailing, 3, &options) == ARGS_ERROR);
}

static void test_kill_and_help(void) {
    Options options;
    char *kill_args[] = {"procmon", "--kill", "4321"};
    char *help_args[] = {"procmon", "--help"};

    CHECK(parse(kill_args, 3, &options) == ARGS_OK);
    CHECK(options.has_kill == 1 && options.kill_pid == 4321);
    CHECK(parse(help_args, 2, &options) == ARGS_HELP);
}

int main(void) {
    test_defaults();
    test_full_command_line();
    test_sort_keys();
    test_rejects_bad_values();
    test_kill_and_help();

    if (g_failures == 0) {
        printf("test_args: tum testler gecti\n");
        return 0;
    }
    printf("test_args: %d test basarisiz\n", g_failures);
    return 1;
}
