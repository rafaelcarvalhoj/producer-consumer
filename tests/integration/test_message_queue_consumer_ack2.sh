#!/usr/bin/env bash

# Testa o consumidor não-bloqueante (ack=2) no paradigma de troca de mensagens
# (paradigma=2), reaproveitando o script do paradigma de memória compartilhada.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

PARADIGM=2 \
OUTPUT_DIR="${OUTPUT_DIR:-build/test-results/msg-consumer-ack2}" \
  exec bash "$SCRIPT_DIR/test_shared_memory_consumer_ack2.sh" "$@"
