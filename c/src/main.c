#include "args.h"
#include "proc.h"
#include "render.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>

#define EXIT_OK 0
#define EXIT_USAGE 1
#define EXIT_DENIED 2
#define EXIT_NOT_FOUND 3

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int signal_number) {
    (void)signal_number;
    g_running = 0;
}

static int confirm_kill(uint32_t pid, const char *name) {
    int answer;

    printf("pid %u (%s) sonlandirilsin mi? [y/N] ", pid, name);
    fflush(stdout);
    answer = fgetc(stdin);
    if (answer == 'y' || answer == 'Y') {
        return 1;
    }
    return 0;
}

static int kill_exit_code(ProcStatus status) {
    if (status == PROC_OK) {
        return EXIT_OK;
    }
    if (status == PROC_ERR_DENIED || status == PROC_ERR_PROTECTED) {
        return EXIT_DENIED;
    }
    return EXIT_NOT_FOUND;
}

static int run_kill(const Options *options) {
    ProcessList list;
    const ProcessInfo *target;
    ProcStatus status;
    int code;

    memset(&list, 0, sizeof(list));

    if (options->kill_pid == proc_self_pid()) {
        fprintf(stderr, "kendi surecini sonlandiramazsin\n");
        return EXIT_DENIED;
    }
    if (proc_is_protected(options->kill_pid)) {
        fprintf(stderr, "pid %u korumali bir sistem sureci\n", options->kill_pid);
        return EXIT_DENIED;
    }
    if (proc_snapshot(&list) != PROC_OK) {
        fprintf(stderr, "surec listesi alinamadi\n");
        proc_list_free(&list);
        return EXIT_NOT_FOUND;
    }
    target = proc_list_find(&list, options->kill_pid);
    if (target == NULL) {
        fprintf(stderr, "pid %u bulunamadi\n", options->kill_pid);
        proc_list_free(&list);
        return EXIT_NOT_FOUND;
    }
    if (!confirm_kill(target->pid, target->name)) {
        printf("iptal edildi\n");
        proc_list_free(&list);
        return EXIT_OK;
    }

    status = proc_kill(options->kill_pid);
    code = kill_exit_code(status);
    if (code == EXIT_OK) {
        printf("pid %u sonlandirildi\n", options->kill_pid);
    } else if (code == EXIT_DENIED) {
        fprintf(stderr, "pid %u icin yetki yok\n", options->kill_pid);
    } else {
        fprintf(stderr, "pid %u sonlandirilamadi\n", options->kill_pid);
    }

    proc_list_free(&list);
    return code;
}

static int run_monitor(const Options *options) {
    ProcessList current;
    ProcessList previous;
    ProcessList shown;
    unsigned cpu_count = proc_cpu_count();
    int code = EXIT_OK;

    memset(&current, 0, sizeof(current));
    memset(&previous, 0, sizeof(previous));
    memset(&shown, 0, sizeof(shown));

    render_init();

    while (g_running) {
        if (proc_snapshot(&current) != PROC_OK) {
            fprintf(stderr, "surec listesi alinamadi\n");
            code = EXIT_NOT_FOUND;
            break;
        }
        proc_apply_cpu(&current, &previous, cpu_count);

        if (!proc_list_copy(&shown, &current)) {
            fprintf(stderr, "bellek yetersiz\n");
            code = EXIT_NOT_FOUND;
            break;
        }
        proc_list_filter(&shown, options->filter);
        proc_list_sort(&shown, options->sort);

        if (!options->once) {
            render_clear();
        }
        render_table(&shown, options->filter, options->top);

        proc_list_free(&previous);
        previous = current;
        memset(&current, 0, sizeof(current));

        if (options->once) {
            break;
        }
        proc_sleep_ms(options->interval_ms);
    }

    proc_list_free(&shown);
    proc_list_free(&current);
    proc_list_free(&previous);
    return code;
}

int main(int argc, char **argv) {
    Options options;
    char error[128];
    ArgsStatus status;

    signal(SIGINT, handle_signal);

    status = args_parse(argc, argv, &options, error, sizeof(error));
    if (status == ARGS_HELP) {
        fputs(args_usage(), stdout);
        return EXIT_OK;
    }
    if (status == ARGS_ERROR) {
        fprintf(stderr, "hata: %s\n%s", error, args_usage());
        return EXIT_USAGE;
    }

    if (options.has_kill) {
        return run_kill(&options);
    }
    return run_monitor(&options);
}
