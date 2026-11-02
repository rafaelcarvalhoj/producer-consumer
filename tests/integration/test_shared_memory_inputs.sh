#!/usr/bin/env bash

set -u

APP="${1:-./build/producer-consumer}"
OUTPUT_DIR="build/test-results"

mkdir -p "$OUTPUT_DIR"

run_test() {
    local name="$1"
    local producers="$2"
    local paradigm="$3"
    local input="$4"

    echo "======================================"
    echo "TEST: $name"
    echo "======================================"

    printf "%s\n" "$input" |
        timeout 5s "$APP" "$producers" "$paradigm" \
        > "$OUTPUT_DIR/$name.log" 2>&1

    local status=$?

    echo "Exit code: $status"
    echo "Log: $OUTPUT_DIR/$name.log"
    echo
}

check_sequences() {
    local log="$1"
    local expected="$2"

    local sequences
    sequences=$(grep -oE 'seq=[0-9]+' "$log" |
        cut -d= -f2 |
        sort -n)

    local count
    count=$(printf "%s\n" "$sequences" | grep -cE '^[0-9]+$')

    if [ "$count" -ne "$expected" ]; then
        echo "FAIL: esperadas $expected mensagens, encontradas $count"
        return 1
    fi

    local actual
    actual=$(printf "%s\n" "$sequences" |
        awk -v n="$expected" '
            BEGIN { expected = 1; valid = 1 }
            {
                if ($1 != expected) valid = 0
                expected++
            }
            END { print valid && expected == n + 1 }
        ')

    if [ "$actual" -eq 1 ]; then
        echo "PASS: $expected sequenciais únicos e consecutivos"
    else
        echo "FAIL: sequenciais duplicados, ausentes ou fora do intervalo"
        return 1
    fi
}

run_test "production_simple" 3 1 \
    $'produz 1 1 2 &\nshutdown'

run_test "production_multiple" 3 1 \
    $'produz 1 3 2 &\nproduz 2 3 2 &\nproduz 3 3 2 &\nshutdown'

run_test "production_distributed" 3 1 \
    $'produz 0 9 2 &\nshutdown'

run_test "production_concurrent" 3 1 \
    $'produz 1 10 2 &\nproduz 1 10 2 &\nproduz 1 10 2 &\nshutdown'

run_test "production_stress_same_channel" 3 1 \
    $'produz 1 10 2 &\nproduz 1 10 2 &\nproduz 1 10 2 &\nproduz 1 10 2 &\nproduz 1 10 2 &\nproduz 1 10 2 &\nproduz 1 10 2 &\nproduz 1 10 2 &\nshutdown'

run_test "production_stress_all_channels" 3 1 \
    $'produz 1 25 2 &\nproduz 2 25 2 &\nproduz 3 25 2 &\nshutdown'

check_sequences "$OUTPUT_DIR/production_stress_same_channel.log" 80
check_sequences "$OUTPUT_DIR/production_stress_all_channels.log" 75
