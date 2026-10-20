CC ?= cc
CPPFLAGS ?= -Isrc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -g -pthread
LDFLAGS ?=
LDLIBS ?= -pthread

APP := build/producer-consumer
TEST_APP := build/unit-tests
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
	./$(TEST_APP)
	@for script in tests/integration/test_*.sh; do \
		[ -f "$$script" ] || continue; \
		sh "$$script" ./$(APP) || exit $$?; \
	done

clean:
	$(RM) -r build
