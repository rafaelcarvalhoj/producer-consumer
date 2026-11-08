#!/usr/bin/env bash

set -uo pipefail

APP="${1:-./build/producer-consumer}"
PRODUCERS="${PRODUCERS:-3}"
TEST_TIMEOUT="${TEST_TIMEOUT:-10}"
OUTPUT_DIR="${OUTPUT_DIR:-build/test-results/lifecycle}"

if [[ ! -x "$APP" ]]; then
  echo "ERROR: executável não encontrado ou não executável: $APP" >&2
  exit 2
fi
APP="$(realpath "$APP")"
if command -v stdbuf >/dev/null 2>&1; then
  APP_CMD=(stdbuf -oL -eL "$APP")
else
  APP_CMD=("$APP")
fi
mkdir -p "$OUTPUT_DIR"
TMP_DIR="$(mktemp -d)"

APP_PID=""
APP_IN=""
CURRENT_FIFO=""
CURRENT_LOG=""
FAILURES=0
TESTS=0

cleanup() {
  if [[ -n "$APP_PID" ]] && kill -0 "$APP_PID" 2>/dev/null; then
    kill -TERM "$APP_PID" 2>/dev/null || true
    if [[ -n "$APP_IN" ]]; then
      eval "exec ${APP_IN}>&-" 2>/dev/null || true
    fi
    sleep 0.2
    kill -KILL "$APP_PID" 2>/dev/null || true
    wait "$APP_PID" 2>/dev/null || true
  fi
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT INT TERM

pass() { printf 'PASS: %s\n' "$1"; }
fail() {
  printf 'FAIL: %s\n' "$1" >&2
  FAILURES=$((FAILURES + 1))
}

count_matches() {
  local pattern="$1"
  local n
  n="$(grep -cE "$pattern" "$CURRENT_LOG" 2>/dev/null || true)"
  printf '%s' "${n:-0}"
}

wait_for_pattern() {
  local pattern="$1"
  local tries=$((TEST_TIMEOUT * 10))
  local i
  for ((i = 0; i < tries; i++)); do
    if grep -qE "$pattern" "$CURRENT_LOG" 2>/dev/null; then
      return 0
    fi
    sleep 0.1
  done
  return 1
}

wait_for_count() {
  local pattern="$1"
  local expected="$2"
  local tries=$((TEST_TIMEOUT * 10))
  local i n
  for ((i = 0; i < tries; i++)); do
    n="$(count_matches "$pattern")"
    if (( n >= expected )); then
      return 0
    fi
    sleep 0.1
  done
  return 1
}

last_worker_pid() {
  sed -nE 's/.*Processo filho criado: PID=([0-9]+),.*/\1/p' "$CURRENT_LOG" | tail -n 1
}

worker_is_running() {
  local pid="$1"
  local state
  state="$(ps -o stat= -p "$pid" 2>/dev/null | awk '{print $1}')"
  [[ -n "$state" && "$state" != Z* && "$state" != X* ]]
}

wait_for_workers_to_exit() {
  local tries=$((TEST_TIMEOUT * 10))
  local i pid state active
  for ((i = 0; i < tries; i++)); do
    active=0
    while read -r pid; do
      [[ -z "$pid" ]] && continue
      state="$(ps -o stat= -p "$pid" 2>/dev/null | awk '{print $1}')"
      # Z/X significa que o processo já não executa; falta apenas o pai recolhê-lo.
      if [[ -n "$state" && "$state" != Z* && "$state" != X* ]]; then
        active=1
        break
      fi
    done < <(sed -nE 's/.*Processo filho criado: PID=([0-9]+),.*/\1/p' "$CURRENT_LOG")

    if (( active == 0 )); then
      return 0
    fi
    sleep 0.1
  done
  return 1
}

start_app() {
  local name="$1"
  CURRENT_LOG="$OUTPUT_DIR/$name.log"
  CURRENT_FIFO="$TMP_DIR/$name.fifo"
  mkfifo "$CURRENT_FIFO"

  "${APP_CMD[@]}" "$PRODUCERS" 1 < "$CURRENT_FIFO" > "$CURRENT_LOG" 2>&1 &
  APP_PID=$!
  exec {APP_IN}>"$CURRENT_FIFO"

  if ! wait_for_pattern 'Paradigm: SHARED_MEMORY'; then
    fail "$name: aplicação não inicializou (veja $CURRENT_LOG)"
    return 1
  fi
}

send_command() {
  printf '%s\n' "$1" >&"$APP_IN"
}

wait_app_exit() {
  local tries=$((TEST_TIMEOUT * 10))
  local i
  for ((i = 0; i < tries; i++)); do
    if ! kill -0 "$APP_PID" 2>/dev/null; then
      break
    fi
    sleep 0.1
  done

  if kill -0 "$APP_PID" 2>/dev/null; then
    fail "aplicação PID=$APP_PID não encerrou dentro de ${TEST_TIMEOUT}s"
    kill -KILL "$APP_PID" 2>/dev/null || true
  fi

  wait "$APP_PID"
  local status=$?
  APP_PID=""
  eval "exec ${APP_IN}>&-" 2>/dev/null || true
  APP_IN=""
  return "$status"
}

finish_normally() {
  send_command 'shutdown'
  if wait_app_exit; then
    :
  else
    fail "aplicação terminou com erro (veja $CURRENT_LOG)"
  fi
}

ipc_snapshot() {
  {
    ipcs -m 2>/dev/null | awk '$1 ~ /^0x/ { print "shm:" $2 }'
    ipcs -s 2>/dev/null | awk '$1 ~ /^0x/ { print "sem:" $2 }'
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
    pass "recursos SHM/SEM removidos"
  fi
}

interrupt_app() {
  local pid="$APP_PID"
  kill -INT "$APP_PID" 2>/dev/null || {
    fail "não foi possível enviar SIGINT para a aplicação"
    return 1
  }
  if wait_app_exit; then
    :
  else
    fail "aplicação retornou erro após SIGINT (veja $CURRENT_LOG)"
  fi
  if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
    fail "processo principal ainda existe após SIGINT"
  fi
}

# ---------------------------------------------------------------------------
# 1. Produtor bloqueia na 86ª mensagem (capacidade = 85), depois é liberado.
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
echo "[TEST 1] Backpressure: produtor bloqueia no buffer cheio e retoma"
start_app "producer_blocks_at_capacity" || exit 1
send_command 'produz 1 86 2 &'
PRODUCER_PATTERN='\[PRODUCER [0-9]+\] Mensagem seq='
if ! wait_for_count "$PRODUCER_PATTERN" 85; then
  fail "produtor não publicou 85 mensagens dentro do prazo"
else
  sleep 0.2
  produced="$(count_matches "$PRODUCER_PATTERN")"
  child="$(last_worker_pid)"
  if [[ "$produced" -ne 85 ]]; then
    fail "esperadas exatamente 85 mensagens antes do desbloqueio; encontradas $produced"
  elif [[ -z "$child" ]] || ! worker_is_running "$child"; then
    fail "produtor não permaneceu bloqueado após preencher o buffer"
  else
    pass "85 mensagens publicadas; produtor continua bloqueado (PID=$child)"
  fi
fi
send_command 'consome 1 86 1 &'
if wait_for_pattern 'Consumo concluído: 86 mensagens'; then
  pass "consumidor liberou espaço e recebeu 86 mensagens"
else
  fail "consumidor não concluiu 86 mensagens (veja $CURRENT_LOG)"
fi
if ! wait_for_workers_to_exit; then
  fail "filhos não terminaram naturalmente antes do shutdown"
fi
finish_normally
produced="$(count_matches "$PRODUCER_PATTERN")"
consumed="$(count_matches '\[CONSUMER [0-9]+\] Mensagem seq=')"
[[ "$produced" -eq 86 ]] && pass "produtor publicou 86 mensagens" || fail "esperadas 86 mensagens produzidas; encontradas $produced"
[[ "$consumed" -eq 86 ]] && pass "consumidor recebeu 86 mensagens" || fail "esperadas 86 mensagens consumidas; encontradas $consumed"

# ---------------------------------------------------------------------------
# 2. Consumidor bloqueia em buffer vazio até o produtor publicar.
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
echo "[TEST 2] Consumidor bloqueia em buffer vazio e retoma"
start_app "consumer_blocks_on_empty" || exit 1
send_command 'consome 1 5 1 &'
if wait_for_pattern 'Solicitadas 5 mensagens'; then
  sleep 0.2
  consumed="$(count_matches '\[CONSUMER [0-9]+\] Mensagem seq=')"
  child="$(last_worker_pid)"
  if [[ "$consumed" -ne 0 ]] && [[ -n "$child" ]] && worker_is_running "$child"; then
    fail "consumidor deveria estar bloqueado sem mensagens"
  elif [[ -z "$child" ]] || ! worker_is_running "$child"; then
    fail "consumidor não permaneceu ativo aguardando mensagens"
  else
    pass "consumidor permaneceu bloqueado no buffer vazio"
  fi
else
  fail "consumidor não iniciou dentro do prazo"
fi
send_command 'produz 1 5 2 &'
if wait_for_pattern 'Consumo concluído: 5 mensagens'; then
  pass "consumidor retomou após a produção"
else
  fail "consumidor não retomou após a produção (veja $CURRENT_LOG)"
fi
if ! wait_for_workers_to_exit; then
  fail "filhos não terminaram naturalmente antes do shutdown"
fi
finish_normally

# ---------------------------------------------------------------------------
# 3. ACK=1: produtor publica e bloqueia até todas as mensagens serem lidas.
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
echo "[TEST 3] ACK: produtor aguarda confirmação de consumo"
start_app "producer_waits_for_ack" || exit 1
send_command 'produz 1 5 1 &'
if wait_for_pattern 'Aguardando confirmação de 5 mensagens'; then
  sleep 0.2
  child="$(last_worker_pid)"
  if [[ -n "$child" ]] && worker_is_running "$child"; then
    pass "produtor permaneceu aguardando ACK"
  else
    fail "produtor não permaneceu ativo aguardando ACK"
  fi
else
  fail "produtor não chegou à espera de ACK"
fi
send_command 'consome 1 5 1 &'
if wait_for_pattern 'Todas as mensagens foram consumidas'; then
  pass "produtor recebeu as cinco confirmações"
else
  fail "produtor não recebeu todas as confirmações (veja $CURRENT_LOG)"
fi
if ! wait_for_workers_to_exit; then
  fail "filhos não terminaram naturalmente antes do shutdown"
fi
finish_normally

# ---------------------------------------------------------------------------
# 4. SIGINT com produtor bloqueado no buffer cheio + verificação de IPC.
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
echo "[TEST 4] SIGINT: interrompe produtor bloqueado e limpa IPC"
BEFORE="$TMP_DIR/ipc-before-producer-sigint.txt"
ipc_snapshot > "$BEFORE"
start_app "sigint_with_blocked_producer" || exit 1
send_command 'produz 1 86 2 &'
if wait_for_count "$PRODUCER_PATTERN" 85; then
  child="$(last_worker_pid)"
  if [[ -n "$child" ]] && worker_is_running "$child"; then
    pass "produtor bloqueado antes do SIGINT"
  else
    fail "não foi possível confirmar produtor bloqueado"
  fi
else
  fail "produtor não preencheu o buffer antes do SIGINT"
fi
interrupt_app
assert_no_new_ipc "$BEFORE"

# ---------------------------------------------------------------------------
# 5. SIGINT com consumidor bloqueado no buffer vazio + verificação de IPC.
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
echo "[TEST 5] SIGINT: interrompe consumidor bloqueado e limpa IPC"
BEFORE="$TMP_DIR/ipc-before-consumer-sigint.txt"
ipc_snapshot > "$BEFORE"
start_app "sigint_with_blocked_consumer" || exit 1
send_command 'consome 1 5 1 &'
if wait_for_pattern 'Solicitadas 5 mensagens'; then
  child="$(last_worker_pid)"
  if [[ -n "$child" ]] && worker_is_running "$child"; then
    pass "consumidor bloqueado antes do SIGINT"
  else
    fail "não foi possível confirmar consumidor bloqueado"
  fi
else
  fail "consumidor não iniciou antes do SIGINT"
fi
interrupt_app
assert_no_new_ipc "$BEFORE"

# ---------------------------------------------------------------------------
# 6. SIGINT enquanto o produtor espera ACK.
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
echo "[TEST 6] SIGINT: interrompe produtor aguardando ACK e limpa IPC"
BEFORE="$TMP_DIR/ipc-before-ack-sigint.txt"
ipc_snapshot > "$BEFORE"
start_app "sigint_with_producer_waiting_ack" || exit 1
send_command 'produz 1 5 1 &'
if wait_for_pattern 'Aguardando confirmação de 5 mensagens'; then
  child="$(last_worker_pid)"
  if [[ -n "$child" ]] && worker_is_running "$child"; then
    pass "produtor aguardando ACK antes do SIGINT"
  else
    fail "produtor não permaneceu aguardando ACK"
  fi
else
  fail "produtor não chegou à espera de ACK"
fi
interrupt_app
assert_no_new_ipc "$BEFORE"

# ---------------------------------------------------------------------------
# 7. Recuperação: nova instância após as interrupções deve funcionar.
# ---------------------------------------------------------------------------
TESTS=$((TESTS + 1))
echo "[TEST 7] Recovery: nova instância funciona após SIGINTs"
BEFORE="$TMP_DIR/ipc-before-recovery.txt"
ipc_snapshot > "$BEFORE"
start_app "recovery_after_sigint" || exit 1
send_command 'consome 1 3 1 &'
send_command 'produz 1 3 2 &'
if wait_for_pattern 'Consumo concluído: 3 mensagens'; then
  pass "nova instância consumiu três mensagens após recuperação"
else
  fail "nova instância não conseguiu produzir/consumir após SIGINT"
fi
if ! wait_for_workers_to_exit; then
  fail "filhos não terminaram naturalmente antes do shutdown"
fi
finish_normally
assert_no_new_ipc "$BEFORE"

printf '\n======================================\n'
printf 'Lifecycle tests: %d cenário(s), %d falha(s)\n' "$TESTS" "$FAILURES"
printf 'Logs: %s\n' "$OUTPUT_DIR"
printf '======================================\n'

if (( FAILURES > 0 )); then
  exit 1
fi
exit 0
