#!/usr/bin/env bash

# Roda os exemplos da Seção 3 do PDF no paradigma de troca de mensagens
# (paradigma=2), reaproveitando o script do paradigma de memória compartilhada.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

PARADIGM=2 \
OUTPUT_DIR="${OUTPUT_DIR:-build/test-results/msg-pdf-examples}" \
  exec bash "$SCRIPT_DIR/test_shared_memory_pdf_examples.sh" "$@"
