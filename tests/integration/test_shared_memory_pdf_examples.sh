#!/usr/bin/env bash

# Valida os 3 exemplos da Seção 3 do PDF (trabalho_pratico_02_2026.pdf),
# erros de comando em tempo de execução (canal inexistente, p=0 em consome,
# limite de n_msgs > 1024) e os relatórios finais de consumo e tempo de operação.

set -uo pipefail

APP="${1:-./build/producer-consumer}"
PARADIGM="${PARADIGM:-1}"
TEST_TIMEOUT="${TEST_TIMEOUT:-12}"
SETTLE_TIME="${SETTLE_TIME:-0.3}"
OUTPUT_DIR="${OUTPUT_DIR:-build/test-results/pdf-examples}"

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
# 1. Exemplo 3.1 do PDF: Quatro Produtores (mpmc 4 1)
#    >> produz 3 2 1 &
#    >> consome 2 2 1 &
#    >> produz 2 5 1 &
#    >> shutdown
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
printf '\n======================================\n'
printf 'TEST 1: pdf_example_3_1_four_producers\n'
printf '======================================\n'
BEFORE="$TMP_DIR/ipc-ex31.txt"
ipc_snapshot > "$BEFORE"
LOG="$OUTPUT_DIR/pdf_example_3_1.log"

if run_scenario "pdf_example_3_1" 4 \
  'produz 3 2 1 &' \
  'consome 2 2 1 &' \
  'produz 2 5 1 &'; then
  pass "Exemplo 3.1 encerrou com status 0"
else
  fail "Exemplo 3.1 retornou erro (veja $LOG)"
fi

if grep -qE '\[CONSUMER [0-9]+\] Consumo concluído: 2 mensagens' "$LOG"; then
  pass "consumidor consumiu 2 mensagens da estrutura 2"
else
  fail "consumidor não concluiu consumo de 2 mensagens na estrutura 2 (veja $LOG)"
fi

if grep -qE '^Canal 1: 0 mensagens$' "$LOG" &&
   grep -qE '^Canal 2: 2 mensagens$' "$LOG" &&
   grep -qE '^Canal 3: 0 mensagens$' "$LOG" &&
   grep -qE '^Canal 4: 0 mensagens$' "$LOG" &&
   grep -qE '^TOTAL: 2 mensagens$' "$LOG"; then
  pass "relatório de mensagens consumidas por estrutura (4 canais) correto"
else
  fail "relatório de mensagens consumidas incorreto no Exemplo 3.1 (veja $LOG)"
fi

if grep -qE '\[MAIN\] Tempo total de execução: [0-9]+\.[0-9]+ segundos' "$LOG"; then
  pass "tempo de operação do mpmc impresso no encerramento"
else
  fail "tempo de operação do mpmc ausente no Exemplo 3.1 (veja $LOG)"
fi

assert_no_new_ipc "$BEFORE"

# ---------------------------------------------------------------------------
# 2. Exemplo 3.2 (a) do PDF: Dois Produtores (mpmc 2 <paradigm>)
#    >> produz 3 2 1 &   -> retorna erro (fila/canal 3 não existe), mpmc continua
#    >> consome 2 2 1 &
#    >> consome 2 2 1 &
#    >> produz 2 5 2 &
#    >> shutdown
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
printf '\n======================================\n'
printf 'TEST 2: pdf_example_3_2a_two_producers_with_invalid_channel\n'
printf '======================================\n'
BEFORE="$TMP_DIR/ipc-ex32a.txt"
ipc_snapshot > "$BEFORE"
LOG="$OUTPUT_DIR/pdf_example_3_2a.log"

if run_scenario "pdf_example_3_2a" 2 \
  'produz 3 2 1 &' \
  'consome 2 2 1 &' \
  'consome 2 2 1 &' \
  'produz 2 5 2 &'; then
  pass "Exemplo 3.2a encerrou com EXIT_SUCCESS (0) após shutdown mesmo com comando inválido intermediário"
else
  fail "Exemplo 3.2a não retornou EXIT_SUCCESS (veja $LOG)"
fi

if grep -qE '\[MAIN\] Erro processando comando' "$LOG"; then
  pass "erro reportado ao tentar produzir no canal 3 inexistente"
else
  fail "erro para canal 3 inexistente não foi reportado (veja $LOG)"
fi

completed_consumers="$(grep -cE '\[CONSUMER [0-9]+\] Consumo concluído: 2 mensagens' "$LOG" || true)"
if [[ "$completed_consumers" -eq 2 ]]; then
  pass "ambos os consumidores bloqueados no canal 2 consumiram 2 mensagens cada e terminaram"
else
  fail "esperados 2 consumidores concluídos com 2 mensagens; encontrados $completed_consumers (veja $LOG)"
fi

if grep -qE '^Canal 1: 0 mensagens$' "$LOG" &&
   grep -qE '^Canal 2: 4 mensagens$' "$LOG" &&
   grep -qE '^TOTAL: 4 mensagens$' "$LOG"; then
  pass "relatório final contabilizou 4 mensagens consumidas no Canal 2"
else
  fail "relatório final incorreto no Exemplo 3.2a (veja $LOG)"
fi

assert_no_new_ipc "$BEFORE"

# ---------------------------------------------------------------------------
# 3. Exemplo 3.2 (b) do PDF: Cinco Produtores com padrão striped (p=0)
#    >> produz 0 15 2 &
#    >> consome 2 3 1 &
#    >> consome 1 2 1 &
#    >> produz 3 2 1 &
#    >> shutdown
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
printf '\n======================================\n'
printf 'TEST 3: pdf_example_3_2b_five_producers_striped\n'
printf '======================================\n'
BEFORE="$TMP_DIR/ipc-ex32b.txt"
ipc_snapshot > "$BEFORE"
LOG="$OUTPUT_DIR/pdf_example_3_2b.log"

if run_scenario "pdf_example_3_2b" 5 \
  'produz 0 15 2 &' \
  'consome 2 3 1 &' \
  'consome 1 2 1 &' \
  'produz 3 2 1 &'; then
  pass "Exemplo 3.2b encerrou com status 0"
else
  fail "Exemplo 3.2b retornou erro (veja $LOG)"
fi

# Verifica que o modo striped (p=0) colocou 3 mensagens em cada um dos 5 canais
# (mais 2 mensagens no canal 3 do segundo produtor = 5 no canal 3).
c1_prod="$(grep -cE -- '-> canal=1$' "$LOG" || true)"
c2_prod="$(grep -cE -- '-> canal=2$' "$LOG" || true)"
c3_prod="$(grep -cE -- '-> canal=3$' "$LOG" || true)"
c4_prod="$(grep -cE -- '-> canal=4$' "$LOG" || true)"
c5_prod="$(grep -cE -- '-> canal=5$' "$LOG" || true)"

if [[ "$c1_prod" -eq 3 && "$c2_prod" -eq 3 && "$c3_prod" -eq 5 && "$c4_prod" -eq 3 && "$c5_prod" -eq 3 ]]; then
  pass "distribuição striped (3 por canal) + 2 no canal 3 verificada"
else
  fail "distribuição por canal inesperada: c1=$c1_prod c2=$c2_prod c3=$c3_prod c4=$c4_prod c5=$c5_prod (veja $LOG)"
fi

if grep -qE '^Canal 1: 2 mensagens$' "$LOG" &&
   grep -qE '^Canal 2: 3 mensagens$' "$LOG" &&
   grep -qE '^Canal 3: 0 mensagens$' "$LOG" &&
   grep -qE '^Canal 4: 0 mensagens$' "$LOG" &&
   grep -qE '^Canal 5: 0 mensagens$' "$LOG" &&
   grep -qE '^TOTAL: 5 mensagens$' "$LOG"; then
  pass "relatório final de consumo nos 5 canais correto (Canal 1=2, Canal 2=3, TOTAL=5)"
else
  fail "relatório final incorreto no Exemplo 3.2b (veja $LOG)"
fi

assert_no_new_ipc "$BEFORE"

# ---------------------------------------------------------------------------
# 4. Validação de consome p=0 (inválido) e limite n_msgs > 1024
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
printf '\n======================================\n'
printf 'TEST 4: runtime_invalid_consume_p0_and_nmsgs_over_1024\n'
printf '======================================\n'
BEFORE="$TMP_DIR/ipc-validation.txt"
ipc_snapshot > "$BEFORE"
LOG="$OUTPUT_DIR/runtime_invalid_consume_p0.log"

if run_scenario "runtime_invalid_consume_p0" 2 \
  'consome 0 1 1 &' \
  'produz 1 1 2 &' \
  'consome 1 1 1 &'; then
  pass "consome p=0 rejeitado sem abortar o mpmc; comandos seguintes executados com sucesso"
else
  fail "aplicação falhou após consome p=0 (veja $LOG)"
fi

if grep -qE '\[MAIN\] Erro processando comando' "$LOG" &&
   grep -qE '^Canal 1: 1 mensagens$' "$LOG"; then
  pass "erro registrado para consome p=0 e 1 mensagem consumida no Canal 1"
else
  fail "comportamento inesperado em runtime_invalid_consume_p0 (veja $LOG)"
fi

LOG_OVER="$OUTPUT_DIR/nmsgs_over_1024.log"
if printf 'produz 1 1025 2 &\nshutdown\n' | timeout "${TEST_TIMEOUT}s" "$APP" 2 "$PARADIGM" > "$LOG_OVER" 2>&1; then
  fail "n_msgs=1025 deveria ser rejeitado pelo parser com código de saída != 0"
else
  pass "n_msgs=1025 rejeitado conforme limite máximo de 1024 do PDF"
fi

assert_no_new_ipc "$BEFORE"

printf '\n======================================\n'
printf 'PDF Examples & Validation tests: %d cenário(s), %d falha(s)\n' "$TESTS" "$FAILURES"
printf 'Logs: %s\n' "$OUTPUT_DIR"
printf '======================================\n'

[[ $FAILURES -eq 0 ]]
