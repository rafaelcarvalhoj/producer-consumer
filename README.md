# producer-consumer

Projeto em C para simular produtor e consumidor no terminal.

## Estrutura

```text
src/
  main.c                  Entrada do programa
  cli/
    startup_args.c/.h     Argumentos iniciais
    command_loop.c/.h    Leitura de comandos até shutdown
    command_parser.c/.h  Converte texto em uma estrutura de comando
    command_dispatch.c/.h Chama a operação correspondente na application
    output.c/.h          Formata e imprime mensagens
  application/            Operações de produção e consumo
  structures/             Estruturas de dados
  domain/                 Regras, como a capacidade do buffer
  infrastructure/unix/    Funções para chamadas de sistema Unix
tests/
  unit/                   Testes de unidades
  integration/            Testes do programa completo
build/                    Arquivos gerados pelo Make
```

## Comandos

```sh
make build
make run
make test
make clean
```

O `main.c` é apenas o ponto de entrada inicial. A leitura dos comandos do terminal ainda será implementada em `cli/`.
