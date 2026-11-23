#!/bin/sh
# construir.sh - gera vendor/linux/libsmoke.a no alpine-build (WSL). Uso: sh construir.sh (na pasta deste script).
# Versao: 0.1.1
# Changelog: 0.1.1 - M5b P37: -fno-merge-constants (achado: .rodata.cst16 do gcc fora do ligador ELF).
#            0.1.0 - M5b P37: primeira versao (prova smoke da ABI SysV).
set -e
cd "$(dirname "$0")"
mkdir -p vendor/linux
# -fno-merge-constants: sem isso o gcc põe as constantes reais em .rodata.cst8/.rodata.cst16 (secoes mesclaveis) que o ligador ELF ainda nao aceita (achado P37).
gcc -std=c11 -O2 -fno-merge-constants -fno-asynchronous-unwind-tables -c smoke.c -o smoke.o
ar rcs vendor/linux/libsmoke.a smoke.o
sha256sum vendor/linux/libsmoke.a
