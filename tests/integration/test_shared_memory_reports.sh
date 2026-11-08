#!/usr/bin/env bash

# Verifica se todo PID registrado como filho recebe exatamente um relatório
# individual de encerramento, incluindo duração e resultado (exit code/sinal).
#
# Formato esperado pelo teste:
# [MAIN] PRODUTOR PID=1234: 0.123456 segundos; código de saída=0
# [MAIN] CONSUMIDOR PID=1235: 0.234567 segundos; interrompido pelo sinal 15

set -uo pipefail

APP="${1:-./build/producer-consumer}"
CHANNELS="${CHANNELS:-3}"
TEST_TIMEOUT="${TEST_TIMEOUT:-15}"
SETTLE_TIME="${SETTLE_TIME:-0.25}"
OUTPUT_DIR="${OUTPUT_DIR:-build/test-results/worker-reports}"

mkdir -p "$OUTPUT_DIR"

for command in awk grep mkfifo; do
    if ! command -v "$command" >/dev/null 2>&1; then
        printf 'ERRO: comando necessário não encontrado: %s\n' "$command" >&2
        exit 2
    fi
done

if [[ ! -x "$APP" ]]; then
    printf 'ERRO: executável não encontrado ou sem permissão: %s\n' "$APP" >&2
    exit 2
fi

if ! [[ "$CHANNELS" =~ ^[1-9][0-9]*$ ]]; then
    printf 'ERRO: CHANNELS deve ser um inteiro positivo.\n' >&2
    exit 2
fi

if ! [[ "$TEST_TIMEOUT" =~ ^[1-9][0-9]*$ ]]; then
    printf 'ERRO: TEST_TIMEOUT deve ser um inteiro positivo.\n' >&2
    exit 2
fi

if ! [[ "$SETTLE_TIME" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
    printf 'ERRO: SETTLE_TIME deve ser um número positivo.\n' >&2
    exit 2
fi

TOTAL_FAILURES=0

validate_reports() {
    local log="$1"
    local scenario="$2"

    printf '\n--- Validando relatórios: %s ---\n' "$scenario"

    if [[ ! -s "$log" ]]; then
        printf 'FAIL: log vazio ou inexistente: %s\n' "$log"
        return 1
    fi

    # A validação compara os PID criados pelo processo principal com os PID
    # presentes nos relatórios que incluem tempo de execução.
    awk '
        function extract_pid(line, marker, rest) {
            marker = index(line, "PID=")
            if (marker == 0) return ""

            rest = substr(line, marker + 4)
            if (match(rest, /^[0-9]+/))
                return substr(rest, 1, RLENGTH)

            return ""
        }

        index($0, "[MAIN] Processo filho criado: PID=") {
            pid = extract_pid($0)
            if (pid != "") {
                created[pid]++
                created_total++
            }
        }

        index($0, " segundos;") && index($0, "PID=") &&
        (index($0, "PRODUTOR PID=") ||
         index($0, "CONSUMIDOR PID=") ||
         index($0, "WORKER PID=")) {
            pid = extract_pid($0)
            if (pid != "") {
                reported[pid]++
                reported_total++
            }
        }

        END {
            failed = 0

            if (created_total == 0) {
                print "FAIL: nenhum processo filho foi registrado no log."
                failed = 1
            }

            for (pid in created) {
                if (created[pid] != 1) {
                    printf "FAIL: PID %s foi anunciado %d vez(es) como criado.\n", \
                           pid, created[pid]
                    failed = 1
                }

                if (reported[pid] == 0) {
                    printf "FAIL: PID %s foi criado, mas não possui relatório de encerramento.\n", pid
                    failed = 1
                } else if (reported[pid] != 1) {
                    printf "FAIL: PID %s possui %d relatórios de encerramento (esperado: 1).\n", \
                           pid, reported[pid]
                    failed = 1
                } else {
                    printf "PASS: PID %s possui exatamente um relatório.\n", pid
                }
            }

            for (pid in reported) {
                if (!(pid in created)) {
                    printf "FAIL: existe relatório para PID %s que não consta como criado.\n", pid
                    failed = 1
                }
            }

            printf "Resumo: %d PID(s) criado(s); %d relatório(s) encontrado(s).\n", \
                   created_total, reported_total

            exit failed
        }
    ' "$log"
}

run_pipe_scenario() {
    local name="$1"
    local input="$2"
    local log="$OUTPUT_DIR/$name.log"
    local status

    printf '\n======================================\n'
    printf 'TEST: %s\n' "$name"
    printf '======================================\n'

    # SETTLE_TIME permite que os primeiros workers terminem enquanto o pai
    # continua recebendo comandos. Isso exercita tanto zombie_kill quanto
    # worker_manager_shutdown no mesmo cenário.
    {
        while IFS= read -r line; do
            printf '%s\n' "$line"
            sleep "$SETTLE_TIME"
        done <<< "$input"
    } | timeout --signal=TERM "${TEST_TIMEOUT}s" \
          "$APP" "$CHANNELS" 1 > "$log" 2>&1

    status=$?
    printf 'Exit code: %d\nLog: %s\n' "$status" "$log"

    if [[ $status -ne 0 ]]; then
        printf 'FAIL: aplicação terminou com status %d.\n' "$status"
        return 1
    fi

    validate_reports "$log" "$name"
}

run_sigint_scenario() {
    local name="sigint_with_multiple_workers"
    local log="$OUTPUT_DIR/$name.log"
    local tmpdir fifo app_pid watchdog status

    printf '\n======================================\n'
    printf 'TEST: %s\n' "$name"
    printf '======================================\n'

    tmpdir=$(mktemp -d) || return 1
    fifo="$tmpdir/input.fifo"

    if ! mkfifo "$fifo"; then
        rm -rf "$tmpdir"
        return 1
    fi

    # A aplicação lê da FIFO. Mantemos o descritor aberto para que ela fique
    # bloqueada aguardando o próximo comando até receber SIGINT.
    "$APP" "$CHANNELS" 1 < "$fifo" > "$log" 2>&1 &
    app_pid=$!

    # Abre o escritor; a abertura sincroniza com a leitura da aplicação.
    if ! exec 3>"$fifo"; then
        kill -TERM "$app_pid" 2>/dev/null || true
        wait "$app_pid" 2>/dev/null || true
        rm -rf "$tmpdir"
        return 1
    fi

    printf 'consome 1 1 1 &\n' >&3
    sleep "$SETTLE_TIME"
    printf 'consome 2 1 1 &\n' >&3
    sleep "$SETTLE_TIME"
    printf 'produz 3 10 2 &\n' >&3
    sleep "$SETTLE_TIME"

    # Watchdog evita deixar a suíte presa caso o shutdown falhe.
    (
        sleep "$TEST_TIMEOUT" &
        timer_pid=$!
        trap 'kill "$timer_pid" 2>/dev/null || true' TERM INT
        wait "$timer_pid"
        kill -TERM "$app_pid" 2>/dev/null || true
    ) &
    watchdog=$!

    kill -INT "$app_pid" 2>/dev/null || true
    exec 3>&-

    wait "$app_pid"
    status=$?

    kill -TERM "$watchdog" 2>/dev/null || true
    wait "$watchdog" 2>/dev/null || true
    rm -rf "$tmpdir"

    printf 'Exit code: %d\nLog: %s\n' "$status" "$log"

    # application_run pode retornar sucesso após tratar SIGINT, conforme a
    # política atual do programa. O que importa aqui é não ter timeout externo.
    if [[ $status -eq 143 ]]; then
        printf 'FAIL: watchdog precisou encerrar a aplicação (SIGTERM).\n'
        return 1
    fi

    validate_reports "$log" "$name"
}

# Dois workers concluem normalmente (um produtor com ACK e seu consumidor).
# Um consumidor adicional permanece bloqueado e deve ser encerrado no shutdown.
run_pipe_scenario "mixed_reap_and_shutdown" \
$'produz 1 5 1 &\nconsome 1 5 1 &\nconsome 2 1 1 &\nshutdown' || TOTAL_FAILURES=$((TOTAL_FAILURES + 1))

# Três workers são registrados; o SIGINT deve fazer o pai encerrar os
# consumidores bloqueados e recolher também o produtor, caso ainda esteja ativo.
run_sigint_scenario || TOTAL_FAILURES=$((TOTAL_FAILURES + 1))

printf '\n======================================\n'
if [[ $TOTAL_FAILURES -eq 0 ]]; then
    printf 'PASS: todos os PIDs criados receberam exatamente um relatório.\n'
else
    printf 'FAIL: %d cenário(s) falharam.\n' "$TOTAL_FAILURES"
fi
printf 'Logs: %s\n' "$OUTPUT_DIR"
printf '======================================\n'

[[ $TOTAL_FAILURES -eq 0 ]]
