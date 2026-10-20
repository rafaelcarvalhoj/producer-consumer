# producer-consumer

Projeto em C para simular produtor e consumidor no terminal.

## Estrutura

```text
Makefile
README.md
src/
  main.c
  cli/
    startup_args.c/.h
    command_parser.c/.h
    parsing/
      str_to_int.c/.h
  structures/
    program_arguments.h
    program_command.h
  unions/
    paradigm.c/.h
    command.h
  application/
  domain/
  infrastructure/unix/
tests/
  unit/
  integration/
build/                    Gerado pelo Make
```

## Comandos

```sh
make build
make run ARGS="2 1"
make test
make clean
```
