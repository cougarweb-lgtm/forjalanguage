#!/bin/sh
# construir_abi_sysv_cb.sh - libs da bateria de callback SysV no alpine-build (WSL): casos_cb/vendor/linux/libabi_sysv_cb.a (60 cb_call_NN)
# e vivos_cb/vendor/linux/libvivos_cb.a (vvc_chama_cb + vvc_alinhamento). Uso: sh construir_abi_sysv_cb.sh (na pasta deste script).
# Versao: 0.2.0
# Changelog: 0.2.0 - M5b P38b: callback_misto/vendor/linux/libcb_misto.a (cb_misto.c: valor de proc chamado pela Forja e pelo C, 8 argumentos, panic em callback).
#            0.1.0 - M5b P38-0: primeira versao (-O2 -fno-asynchronous-unwind-tables -fno-pic -fno-merge-constants -fno-stack-protector,
#            como construir_abi_sysv.sh 0.1.1).
set -e
cd "$(dirname "$0")"
mkdir -p casos_cb/vendor/linux vivos_cb/vendor/linux
gcc -std=gnu11 -O2 -fno-asynchronous-unwind-tables -fno-pic -fno-merge-constants -fno-stack-protector -c casos_cb/cb_sysv.c -o cb_sysv_obj.o
ar rcs casos_cb/vendor/linux/libabi_sysv_cb.a cb_sysv_obj.o
gcc -std=gnu11 -O2 -fno-asynchronous-unwind-tables -fno-pic -fno-merge-constants -fno-stack-protector -c vivos_cb/vivos_cb.c -o vivos_cb_obj.o
ar rcs vivos_cb/vendor/linux/libvivos_cb.a vivos_cb_obj.o
mkdir -p callback_misto/vendor/linux
gcc -std=gnu11 -O2 -fno-asynchronous-unwind-tables -fno-pic -fno-merge-constants -fno-stack-protector -c callback_misto/cb_misto.c -o cb_misto_obj.o
ar rcs callback_misto/vendor/linux/libcb_misto.a cb_misto_obj.o
rm -f cb_sysv_obj.o vivos_cb_obj.o cb_misto_obj.o
sha256sum casos_cb/vendor/linux/libabi_sysv_cb.a vivos_cb/vendor/linux/libvivos_cb.a callback_misto/vendor/linux/libcb_misto.a
