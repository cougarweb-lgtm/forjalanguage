#!/bin/sh
# construir_libc.sh - lib do fixture ctor_init_array_libc (M5b P39-B3, d) no alpine-build (WSL): vendor/linux/libctor_libc.a com o construtor de .init_array.
# Uso: sh construir_libc.sh (na pasta deste script). Mesmas flags de callback_agregado/construir_callback_agregado.sh.
# Versao: 0.1.0
# Changelog: 0.1.0 - M5b P39-B3: primeira versao.
set -e
cd "$(dirname "$0")"
mkdir -p vendor/linux
gcc -std=gnu11 -O2 -fno-asynchronous-unwind-tables -fno-pic -fno-merge-constants -fno-stack-protector -c ctor_libc.c -o ctor_libc_obj.o
ar rcsD vendor/linux/libctor_libc.a ctor_libc_obj.o
rm -f ctor_libc_obj.o
sha256sum vendor/linux/libctor_libc.a
