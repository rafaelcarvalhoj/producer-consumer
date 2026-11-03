#!/usr/bin/env bash

set -uo pipefail

APP="${1:-./build/producer-consumer}"
OUTPUT_DIR="build/test-results/extensive"
TEST_SETTLE="${TEST_SETTLE:-1}"
TEST_TIMEOUT="${TEST_TIMEOUT:-20}"
PARADIGM_SHM=1

mkdir -p "$OUTPUT_DIR"

if [[ ! -x "$APP" ]]; then
    echo "ERROR: application not found or not executable: $APP" >&2
    exit 2
fi

passed=0
failed=0

report_result() {
    local name="$1"
    local ok="$2"
    if [[ "$ok" == "yes" ]]; then
        printf 'PASS: %s\n' "$name"
        ((passed += 1))
    else
        printf 'FAIL: %s (see %s/%s.log)\n' "$name" "$OUTPUT_DIR" "$name"
        ((failed += 1))
    fi
}

check_sequences() {
    local log="$1"
    local expected="$2"
    local sequences count valid

    sequences=$(grep -oE 'seq=[0-9]+' "$log" | cut -d= -f2 || true)
    count=$(printf '%s\n' "$sequences" | grep -cE '^[0-9]+$' || true)

    if [[ "$count" -ne "$expected" ]]; then
        echo "  sequence count: expected $expected, found $count"
        return 1
    fi

    valid=$(printf '%s\n' "$sequences" | sort -n | awk -v n="$expected" '
        BEGIN { expected = 1; valid = 1 }
        /^[0-9]+$/ {
            if ($1 != expected) valid = 0
            expected++
        }
        END { print (valid && expected == n + 1) ? 1 : 0 }
    ')

    if [[ "$valid" != "1" ]]; then
        echo "  sequences are duplicated, missing, or outside 1..$expected"
        return 1
    fi

    return 0
}

# Usage:
# run_case NAME PRODUCERS EXPECTED_SEQUENCES_OR_DASH [COMMAND ...]
# Commands are sent in order, then the script waits TEST_SETTLE seconds and
# sends shutdown. EXPECTED_SEQUENCES checks producer output in cases where
# only production is performed; use '-' for cases that also consume messages.
run_case() {
    local name="$1"
    local producers="$2"
    local expected_sequences="$3"
    shift 3

    local log="$OUTPUT_DIR/$name.log"
    local app_status
    local ok="yes"
    local command

    echo
    echo "======================================"
    echo "TEST: $name"
    echo "======================================"

    {
        for command in "$@"; do
            printf '%s\n' "$command"
        done
        sleep "$TEST_SETTLE"
        printf 'shutdown\n'
    } | timeout "${TEST_TIMEOUT}s" "$APP" "$producers" "$PARADIGM_SHM" >"$log" 2>&1
    app_status=${PIPESTATUS[1]}

    if [[ "$app_status" -ne 0 ]]; then
        echo "  application exit code: $app_status (expected 0)"
        ok="no"
    fi

    # These scenarios are expected to complete naturally before shutdown.
    # A SIGTERM here indicates that the settle interval was insufficient or
    # that a worker got stuck.
    if grep -Eq 'sinal=15|signal=15|SIGTERM' "$log"; then
        echo "  a worker appears to have been terminated with SIGTERM"
        ok="no"
    fi

    if grep -Eq 'Erro recolhendo processos filhos|Erro durante shutdown|Erro processando comando' "$log"; then
        echo "  application reported a worker/command/shutdown error"
        ok="no"
    fi

    if grep -Eq 'código de saída=[1-9][0-9]*|exit code=[1-9][0-9]*' "$log"; then
        echo "  at least one child process exited with a nonzero status"
        ok="no"
    fi

    if [[ "$expected_sequences" != "-" ]]; then
        if ! check_sequences "$log" "$expected_sequences"; then
            ok="no"
        fi
    fi

    report_result "$name" "$ok"
    echo "  log: $log"
}

# 1. Basic production, one channel and multiple channels.
run_case "production_one_message" 3 1 \
    'produz 1 1 2 &'

run_case "production_multiple_channels" 3 9 \
    'produz 1 3 2 &' \
    'produz 2 3 2 &' \
    'produz 3 3 2 &'

# p=0 distributes messages round-robin over all configured channels.
run_case "production_distributed" 3 90 \
    'produz 0 90 2 &'

# 2. Concurrency and buffer-capacity boundaries.
run_case "production_concurrent_same_channel" 3 80 \
    'produz 1 10 2 &' 'produz 1 10 2 &' \
    'produz 1 10 2 &' 'produz 1 10 2 &' \
    'produz 1 10 2 &' 'produz 1 10 2 &' \
    'produz 1 10 2 &' 'produz 1 10 2 &'

run_case "production_concurrent_all_channels" 3 75 \
    'produz 1 25 2 &' 'produz 2 25 2 &' 'produz 3 25 2 &'

# The SHM buffer has 85 message slots. Exactly 85 should fit without a consumer.
run_case "production_exact_buffer_capacity" 3 85 \
    'produz 1 85 2 &'

# 3. Consumers and ACK=1. Consumers are started first so they block waiting
# for messages; producers then publish and wait until their messages are read.
run_case "consumer_blocking_100_messages" 3 - \
    'consome 1 100 1 &' \
    'produz 1 100 1 &'

# More than two buffer capacities, exercising circular-buffer wraparound and
# producer backpressure. Each producer waits for its messages to be consumed.
run_case "buffer_wraparound_240_messages" 3 - \
    'consome 1 240 1 &' \
    'produz 1 80 1 &' \
    'produz 1 80 1 &' \
    'produz 1 80 1 &'

# Concurrent production and consumption on all three channels.
run_case "concurrent_all_channels_with_backpressure" 3 - \
    'consome 1 120 1 &' \
    'consome 2 120 1 &' \
    'consome 3 120 1 &' \
    'produz 1 120 1 &' \
    'produz 2 120 1 &' \
    'produz 3 120 1 &'

# Round-robin distribution across three channels while each consumer drains
# its corresponding channel.
run_case "distributed_with_three_consumers" 3 - \
    'consome 1 40 1 &' \
    'consome 2 40 1 &' \
    'consome 3 40 1 &' \
    'produz 0 120 1 &'

# 4. Nonblocking producer (ACK=2): it must finish without waiting for a reader.
run_case "producer_ack2_does_not_wait_for_consumer" 3 80 \
    'produz 1 10 2 &' 'produz 1 10 2 &' \
    'produz 1 10 2 &' 'produz 1 10 2 &' \
    'produz 1 10 2 &' 'produz 1 10 2 &' \
    'produz 1 10 2 &' 'produz 1 10 2 &'

# 5. Re-run startup/argument integration tests if the existing script is present.
if [[ -f "tests/integration/test_startup_args.sh" ]]; then
    echo
    echo "======================================"
    echo "TEST: startup_arguments"
    echo "======================================"

    if sh tests/integration/test_startup_args.sh "$APP" >"$OUTPUT_DIR/startup_arguments.log" 2>&1; then
        report_result "startup_arguments" "yes"
    else
        cat "$OUTPUT_DIR/startup_arguments.log"
        report_result "startup_arguments" "no"
    fi
fi

# Additional sanity check for the distributed-production log.
if ! grep -q 'seq=' "$OUTPUT_DIR/production_distributed.log"; then
    echo "FAIL: distributed production produced no visible sequence records"
    ((failed += 1))
fi

echo
echo "======================================"
echo "SUMMARY"
echo "======================================"
echo "Passed: $passed"
echo "Failed: $failed"
echo "Logs:   $OUTPUT_DIR"

if [[ "$failed" -gt 0 ]]; then
    exit 1
fi

exit 0
