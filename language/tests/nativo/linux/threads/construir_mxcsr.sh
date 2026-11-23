#!/bin/sh
# construir_mxcsr.sh - lib da gemea Linux de mxcsr_ftz.fj (M5c-b P58) no alpine-build (WSL): vendor/linux/libmxcsr_linux.a com forja_t_mxcsr_ler e forja_t_mxcsr_ligar_ftz.
# Uso: sh construir_mxcsr.sh (na pasta deste script). Mesmas flags de ../libc/construir_libc.sh.
# Versao: 0.1.0
# Changelog: 0.1.0 - M5c-b P58: primeira versao.
set -e
cd "$(dirname "$0")"
mkdir -p vendor/linux
gcc -std=gnu11 -O2 -msse -fno-asynchronous-unwind-tables -fno-pic -fno-stack-protector -c mxcsr_helper_linux.c -o mxcsr_helper_obj.o
ar rcsD vendor/linux/libmxcsr_linux.a mxcsr_helper_obj.o
rm -f mxcsr_helper_obj.o
sha256sum vendor/linux/libmxcsr_linux.a
