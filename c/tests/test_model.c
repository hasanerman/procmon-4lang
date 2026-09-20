#include "../src/proc.h"

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

static void add(ProcessList *list, uint32_t pid, const char *name, uint64_t mem,
                uint32_t threads, uint64_t cpu_ns) {
    ProcessInfo info;

    memset(&info, 0, sizeof(info));
    info.pid = pid;
    snprintf(info.name, sizeof(info.name), "%s", name);
    info.mem_bytes = mem;
    info.mem_known = 1;
    info.threads = threads;
    info.cpu_time_ns = cpu_ns;
    info.cpu_known = 1;
    proc_list_push(list, &info);
}

static ProcessList sample(void) {
    ProcessList list;

    memset(&list, 0, sizeof(list));
    add(&list, 10, "Chrome.exe", 300u * 1024u * 1024u, 40, 1000000000ull);
    add(&list, 20, "code.exe", 900u * 1024u * 1024u, 60, 2000000000ull);
    add(&list, 5, "alpha", 10u * 1024u * 1024u, 2, 0);
    list.wall_ns = 0;
    return list;
}

static void test_filter_is_case_insensitive(void) {
    ProcessList list = sample();

    proc_list_filter(&list, "CHROME");
    CHECK(list.count == 1);
    CHECK(list.count == 1 && list.items[0].pid == 10);
    proc_list_free(&list);
}

static void test_filter_empty_keeps_everything(void) {
    ProcessList list = sample();

    proc_list_filter(&list, "");
    CHECK(list.count == 3);
    proc_list_filter(&list, "yok-boyle-bir-surec");
    CHECK(list.count == 0);
    proc_list_free(&list);
}

static void test_sort_orders(void) {
    ProcessList list = sample();

    proc_list_sort(&list, SORT_MEM);
    CHECK(list.items[0].pid == 20 && list.items[2].pid == 5);

    proc_list_sort(&list, SORT_PID);
    CHECK(list.items[0].pid == 5 && list.items[2].pid == 20);

    proc_list_sort(&list, SORT_NAME);
    CHECK(strcmp(list.items[0].name, "alpha") == 0);
    CHECK(strcmp(list.items[1].name, "Chrome.exe") == 0);
    CHECK(strcmp(list.items[2].name, "code.exe") == 0);

    proc_list_free(&list);
}

static void test_cpu_percent_uses_two_samples(void) {
    ProcessList previous;
    ProcessList current;

    memset(&previous, 0, sizeof(previous));
    memset(&current, 0, sizeof(current));

    add(&previous, 10, "worker", 0, 1, 0);
    previous.wall_ns = 0;

    add(&current, 10, "worker", 0, 1, 500000000ull);
    current.wall_ns = 1000000000ull;

    proc_apply_cpu(&current, &previous, 1);
    CHECK(current.items[0].cpu_percent > 49.9 && current.items[0].cpu_percent < 50.1);

    proc_apply_cpu(&current, &previous, 2);
    CHECK(current.items[0].cpu_percent > 24.9 && current.items[0].cpu_percent < 25.1);

    proc_list_free(&previous);
    proc_list_free(&current);
}

static void test_cpu_percent_without_previous_sample(void) {
    ProcessList current;

    memset(&current, 0, sizeof(current));
    add(&current, 10, "worker", 0, 1, 500000000ull);
    current.wall_ns = 1000000000ull;

    proc_apply_cpu(&current, NULL, 1);
    CHECK(current.items[0].cpu_percent == 0.0);

    proc_list_free(&current);
}

static void test_sort_and_copy_keep_data(void) {
    ProcessList list = sample();
    ProcessList copy;

    memset(&copy, 0, sizeof(copy));
    CHECK(proc_list_copy(&copy, &list) == 1);
    CHECK(copy.count == list.count);
    proc_list_sort(&copy, SORT_MEM);
    CHECK(list.items[0].pid == 10);
    CHECK(proc_list_find(&copy, 20) != NULL);
    CHECK(proc_list_find(&copy, 999) == NULL);

    proc_list_free(&copy);
    proc_list_free(&list);
}

static void test_live_snapshot_contains_self(void) {
    ProcessList list;

    memset(&list, 0, sizeof(list));
    CHECK(proc_snapshot(&list) == PROC_OK);
    CHECK(list.count > 0);
    CHECK(proc_list_find(&list, proc_self_pid()) != NULL);
    CHECK(proc_is_protected(0) == 1);
    proc_list_free(&list);
}

int main(void) {
    test_filter_is_case_insensitive();
    test_filter_empty_keeps_everything();
    test_sort_orders();
    test_cpu_percent_uses_two_samples();
    test_cpu_percent_without_previous_sample();
    test_sort_and_copy_keep_data();
    test_live_snapshot_contains_self();

    if (g_failures == 0) {
        printf("test_model: tum testler gecti\n");
        return 0;
    }
    printf("test_model: %d test basarisiz\n", g_failures);
    return 1;
}
