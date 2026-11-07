CC ?= cc

LDFLAGS ?=
LDLIBS ?= -pthread

APP := build/producer-consumer
TEST_APP := build/unit-tests
TEST_LOG_DIR := build/test-logs


CPPFLAGS ?= -Isrc -std=c11 -Wall -Wextra -Wpedantic -g -pthread
HEADER_DIRS := $(shell find src -type d)
CPPFLAGS += $(addprefix -I,$(HEADER_DIRS))


APP_SOURCES := $(shell find src -name '*.c')
APP_HEADERS := $(shell find src -name '*.h')

MAIN_SOURCE := src/main.c

OTHER_MAINS := $(shell find src/ipc -name 'main.c' -o -name 'message_queue.c')

LIB_SOURCES := $(filter-out $(MAIN_SOURCE) $(OTHER_MAINS),$(APP_SOURCES))

UNIT_SOURCES := $(wildcard tests/unit/test_*.c)

# Executáveis adicionais
OTHER_APPS := $(patsubst src/ipc/%.c,build/ipc/%,$(OTHER_MAINS))

.PHONY: build run test clean

build: $(APP) $(OTHER_APPS)

$(APP): $(MAIN_SOURCE) $(LIB_SOURCES) $(APP_HEADERS)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MAIN_SOURCE) $(LIB_SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)

# Compila cada main separadamente
build/ipc/%: src/ipc/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

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