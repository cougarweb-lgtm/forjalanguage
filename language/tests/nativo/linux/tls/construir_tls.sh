#!/bin/sh
# construir_tls.sh - lib do fixture tls_threads (M5c-b P61) no alpine-build (WSL): vendor/linux/libtls.a com __thread (initial-exec e local-exec) e um alinhado a 64.
# Uso: sh construir_tls.sh (na pasta deste script). Flags de ../libc/construir_libc.sh, com -fPIC: o initial-exec de __thread so gera GOTTPOFF com PIC.
# Versao: 0.1.0
# Changelog: 0.1.0 - M5c-b P61: primeira versao.
set -e
cd "$(dirname "$0")"
mkdir -p vendor/linux
gcc -std=gnu11 -O2 -fPIC -fno-asynchronous-unwind-tables -fno-stack-protector -c tls_helper.c -o tls_helper_obj.o
ar rcsD vendor/linux/libtls.a tls_helper_obj.o
rm -f tls_helper_obj.o
sha256sum vendor/linux/libtls.a
