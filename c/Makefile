CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
BUILD := build

COMMON := src/args.c src/proc_common.c src/render.c
PLATFORM := src/proc_linux.c
APP := $(BUILD)/procmon
TEST_ARGS := $(BUILD)/test_args
TEST_MODEL := $(BUILD)/test_model

.PHONY: all test asan clean

all: $(APP)

$(BUILD):
	mkdir -p $(BUILD)

$(APP): $(BUILD) src/main.c $(COMMON) $(PLATFORM)
	$(CC) $(CFLAGS) -o $@ src/main.c $(COMMON) $(PLATFORM)

$(TEST_ARGS): $(BUILD) tests/test_args.c src/args.c
	$(CC) $(CFLAGS) -o $@ tests/test_args.c src/args.c

$(TEST_MODEL): $(BUILD) tests/test_model.c src/proc_common.c $(PLATFORM)
	$(CC) $(CFLAGS) -o $@ tests/test_model.c src/proc_common.c $(PLATFORM)

test: $(TEST_ARGS) $(TEST_MODEL)
	./$(TEST_ARGS)
	./$(TEST_MODEL)

asan: CFLAGS += -fsanitize=address,undefined -g -O1
asan: clean test

clean:
	rm -rf $(BUILD)
