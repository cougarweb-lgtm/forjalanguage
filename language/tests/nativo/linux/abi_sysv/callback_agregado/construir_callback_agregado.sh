#!/bin/sh
# construir_callback_agregado.sh - lib da fixture de callback com agregado (M5b P38c) no alpine-build (WSL): vendor/linux/libcb_agregado.a (cba_chama24/12/16/8, cba_reentra).
# Uso: sh construir_callback_agregado.sh (na pasta deste script). Mesmas flags de construir_abi_sysv_cb.sh.
# Versao: 0.1.0
# Changelog: 0.1.0 - M5b P38c: primeira versao.
set -e
cd "$(dirname "$0")"
mkdir -p vendor/linux
gcc -std=gnu11 -O2 -fno-asynchronous-unwind-tables -fno-pic -fno-merge-constants -fno-stack-protector -c cb_agregado.c -o cb_agregado_obj.o
ar rcs vendor/linux/libcb_agregado.a cb_agregado_obj.o
rm -f cb_agregado_obj.o
sha256sum vendor/linux/libcb_agregado.a
