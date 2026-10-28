#!/bin/sh
set -eu

program=$1

expect_failure() {
    if "$program" "$@" >/dev/null 2>&1; then
        printf 'Expected failure for arguments: %s\n' "$*" >&2
        exit 1
    fi
}

printf 'shutdown\n' | "$program" 2 1 >/dev/null
printf 'shutdown\n' | "$program" 2 2 >/dev/null
printf 'produz 2 5 2 &\nshutdown\n' | "$program" +2 +1 >/dev/null
expect_failure
expect_failure 2
expect_failure 2 1 3
expect_failure 0 1
expect_failure -1 1
expect_failure abc 1
expect_failure 2x 1
expect_failure 2 abc
expect_failure 2 3
expect_failure 2147483648 1
expect_failure -2147483649 1
expect_failure 2 999999999999999999999999

printf 'CLI argument tests passed.\n'
