#!/usr/bin/env bash

# Testa o comportamento não-bloqueante do consumidor (ack=2) exigido pela
# especificação (trabalho_pratico_02_2026.pdf, seção 2, pág. 2):
# "No caso de haver menos mensagens do que n_msgs, o consumidor fica bloqueado
# se ack=1 ou um erro é retornado, se ack=2, informando o número de mensagens
# que foram consumidas."

set -uo pipefail

APP="${1:-./build/producer-consumer}"
PARADIGM="${PARADIGM:-1}"
TEST_TIMEOUT="${TEST_TIMEOUT:-10}"
SETTLE_TIME="${SETTLE_TIME:-0.3}"
OUTPUT_DIR="${OUTPUT_DIR:-build/test-results/consumer-ack2}"

mkdir -p "$OUTPUT_DIR"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT INT TERM

if [[ ! -x "$APP" ]]; then
  printf 'ERRO: executável não encontrado ou sem permissão: %s\n' "$APP" >&2
  exit 2
fi

FAILURES=0
TESTS=0

pass() { printf 'PASS: %s\n' "$1"; }
fail() {
  printf 'FAIL: %s\n' "$1" >&2
  FAILURES=$((FAILURES + 1))
}

ipc_snapshot() {
  {
    ipcs -m 2>/dev/null | awk '$1 ~ /^0x/ { print "shm:" $2 }'
    ipcs -s 2>/dev/null | awk '$1 ~ /^0x/ { print "sem:" $2 }'
    ipcs -q 2>/dev/null | awk '$1 ~ /^0x/ { print "msg:" $2 }'
  } | sort -u
}

assert_no_new_ipc() {
  local before="$1"
  local after="$TMP_DIR/ipc-after.txt"
  ipc_snapshot > "$after"
  local leaked
  leaked="$(comm -13 "$before" "$after")"
  if [[ -n "$leaked" ]]; then
    printf 'Novos recursos IPC ainda presentes:\n%s\n' "$leaked" >&2
    fail "recursos IPC não foram removidos"
  else
    pass "recursos IPC removidos sem vazamento"
  fi
}

run_scenario() {
  local name="$1"
  local producers="$2"
  shift 2

  local log="$OUTPUT_DIR/$name.log"
  local command

  {
    for command in "$@"; do
      printf '%s\n' "$command"
      sleep "$SETTLE_TIME"
    done
    printf 'shutdown\n'
  } | timeout "${TEST_TIMEOUT}s" "$APP" "$producers" "$PARADIGM" > "$log" 2>&1

  return "${PIPESTATUS[1]}"
}

# ---------------------------------------------------------------------------
# 1. Buffer vazio: consome p n_msgs 2 & deve falhar imediatamente (0 consumidas)
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
printf '\n======================================\n'
printf 'TEST 1: consumer_ack2_empty_buffer\n'
printf '======================================\n'
BEFORE="$TMP_DIR/ipc-1.txt"
ipc_snapshot > "$BEFORE"
LOG="$OUTPUT_DIR/consumer_ack2_empty_buffer.log"

if run_scenario "consumer_ack2_empty_buffer" 2 'consome 1 4 2 &'; then
  pass "aplicação encerrou com status 0 após shutdown"
else
  fail "aplicação falhou em consumer_ack2_empty_buffer (veja $LOG)"
fi

if grep -qE 'Erro: solicitadas 4 mensagens, mas apenas 0 foram consumidas\.' "$LOG"; then
  pass "consumidor reportou erro informando 0 de 4 mensagens consumidas"
else
  fail "mensagem de erro de consumo parcial/vazio ausente (veja $LOG)"
fi

if grep -qE '\[MAIN\] CONSUMIDOR PID=[0-9]+: .*código de saída=1' "$LOG"; then
  pass "consumidor encerrou com código de saída=1 sem precisar de SIGTERM"
else
  fail "relatório de saída=1 do consumidor ausente (veja $LOG)"
fi

if grep -qE '^Canal 1: 0 mensagens$' "$LOG" && grep -qE '^TOTAL: 0 mensagens$' "$LOG"; then
  pass "estatísticas finais indicam 0 mensagens consumidas"
else
  fail "estatísticas finais incorretas para buffer vazio (veja $LOG)"
fi

assert_no_new_ipc "$BEFORE"

# ---------------------------------------------------------------------------
# 2. Consumo parcial (3 disponíveis, 5 solicitadas com ack=2):
#    - consome as 3 disponíveis;
#    - sinaliza ACK para o produtor das 3 mensagens (desbloqueando-o);
#    - retorna erro informando que apenas 3 de 5 foram consumidas;
#    - contabiliza 3 mensagens no relatório final.
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
printf '\n======================================\n'
printf 'TEST 2: consumer_ack2_partial_with_producer_ack1\n'
printf '======================================\n'
BEFORE="$TMP_DIR/ipc-2.txt"
ipc_snapshot > "$BEFORE"
LOG="$OUTPUT_DIR/consumer_ack2_partial_with_producer_ack1.log"

if run_scenario "consumer_ack2_partial_with_producer_ack1" 2 \
  'produz 1 3 1 &' \
  'consome 1 5 2 &'; then
  pass "aplicação encerrou com status 0 após shutdown"
else
  fail "aplicação falhou em consumer_ack2_partial_with_producer_ack1 (veja $LOG)"
fi

if grep -qE '\[PRODUCER [0-9]+\] Todas as mensagens foram consumidas' "$LOG" &&
   grep -qE '\[MAIN\] PRODUTOR PID=[0-9]+: .*código de saída=0' "$LOG"; then
  pass "produtor (ack=1) recebeu confirmação das 3 mensagens e encerrou com código 0"
else
  fail "produtor não recebeu ACK das 3 mensagens consumidas parcialmente (veja $LOG)"
fi

if grep -qE 'Erro: solicitadas 5 mensagens, mas apenas 3 foram consumidas\.' "$LOG" &&
   grep -qE '\[MAIN\] CONSUMIDOR PID=[0-9]+: .*código de saída=1' "$LOG"; then
  pass "consumidor (ack=2) reportou erro informando 3 de 5 mensagens consumidas"
else
  fail "consumidor não reportou corretamente o consumo parcial de 3/5 (veja $LOG)"
fi

if grep -qE '^Canal 1: 3 mensagens$' "$LOG" &&
   grep -qE '^Canal 2: 0 mensagens$' "$LOG" &&
   grep -qE '^TOTAL: 3 mensagens$' "$LOG"; then
  pass "estatísticas finais contabilizaram exatamente as 3 mensagens consumidas"
else
  fail "estatísticas finais incorretas após consumo parcial (veja $LOG)"
fi

assert_no_new_ipc "$BEFORE"

# ---------------------------------------------------------------------------
# 3. Mensagens suficientes (>= n_msgs) com ack=2:
#    - consome exatamente n_msgs com sucesso (código de saída=0)
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
printf '\n======================================\n'
printf 'TEST 3: consumer_ack2_sufficient_messages\n'
printf '======================================\n'
BEFORE="$TMP_DIR/ipc-3.txt"
ipc_snapshot > "$BEFORE"
LOG="$OUTPUT_DIR/consumer_ack2_sufficient_messages.log"

if run_scenario "consumer_ack2_sufficient_messages" 2 \
  'produz 2 6 2 &' \
  'consome 2 4 2 &'; then
  pass "aplicação encerrou com status 0 após shutdown"
else
  fail "aplicação falhou em consumer_ack2_sufficient_messages (veja $LOG)"
fi

if grep -qE '\[CONSUMER [0-9]+\] Consumo concluído: 4 mensagens' "$LOG" &&
   grep -qE '\[MAIN\] CONSUMIDOR PID=[0-9]+: .*código de saída=0' "$LOG"; then
  pass "consumidor (ack=2) consumiu 4 de 6 mensagens disponíveis com código 0"
else
  fail "consumidor (ack=2) falhou quando havia mensagens suficientes (veja $LOG)"
fi

if grep -qE '^Canal 1: 0 mensagens$' "$LOG" &&
   grep -qE '^Canal 2: 4 mensagens$' "$LOG" &&
   grep -qE '^TOTAL: 4 mensagens$' "$LOG"; then
  pass "estatísticas finais indicam 4 mensagens consumidas no Canal 2"
else
  fail "estatísticas finais incorretas em consumer_ack2_sufficient_messages (veja $LOG)"
fi

assert_no_new_ipc "$BEFORE"

printf '\n======================================\n'
printf 'Consumer ACK=2 tests: %d cenário(s), %d falha(s)\n' "$TESTS" "$FAILURES"
printf 'Logs: %s\n' "$OUTPUT_DIR"
printf '======================================\n'

[[ $FAILURES -eq 0 ]]
