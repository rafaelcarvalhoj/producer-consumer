CC ?= cc
CPPFLAGS ?= -Isrc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -g -pthread
LDFLAGS ?=
LDLIBS ?= -pthread

APP := build/producer-consumer
TEST_APP := build/unit-tests
TEST_LOG_DIR := build/test-logs
APP_SOURCES := $(wildcard src/*.c src/*/*.c src/*/*/*.c)
APP_HEADERS := $(wildcard src/*.h src/*/*.h src/*/*/*.h)
LIB_SOURCES := $(filter-out src/main.c,$(APP_SOURCES))
UNIT_SOURCES := $(wildcard tests/unit/test_*.c)

.PHONY: build run test clean

build: $(APP)

$(APP): $(APP_SOURCES) $(APP_HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(APP_SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)

run: $(APP)
	./$(APP) $(ARGS)

$(TEST_APP): $(UNIT_SOURCES) $(LIB_SOURCES) $(APP_HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(UNIT_SOURCES) $(LIB_SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)

test: $(APP) $(TEST_APP)
	@mkdir -p $(TEST_LOG_DIR)
	@if ./$(TEST_APP) > $(TEST_LOG_DIR)/unit-tests.log 2>&1; then \
		printf 'Unit tests passed.\n'; \
	else \
		printf 'Unit tests failed (see $(TEST_LOG_DIR)/unit-tests.log).\n' >&2; \
		cat $(TEST_LOG_DIR)/unit-tests.log >&2; \
		exit 1; \
	fi
	@for script in tests/integration/test_*.sh; do \
		[ -f "$$script" ] || continue; \
		log="$(TEST_LOG_DIR)/$${script##*/}.log"; \
		if sh "$$script" ./$(APP) > "$$log" 2>&1; then \
			printf '%s passed.\n' "$$script"; \
		else \
			printf '%s failed (see %s).\n' "$$script" "$$log" >&2; \
			cat "$$log" >&2; \
			exit 1; \
		fi; \
	done

clean:
	$(RM) -r build
