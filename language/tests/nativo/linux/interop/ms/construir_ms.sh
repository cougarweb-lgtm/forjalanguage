#!/bin/sh
# construir_ms.sh - gera libms.a (sem -fPIC) e libms_pic.a (com -fPIC) a partir de libms.c, no alpine-build (WSL).
# Uso (no WSL, na pasta deste script): sh construir_ms.sh
# Verificacao: nm -u libms.a deve listar so o que a libc nao traz; readelf -r libms.o mostra R_X86_64_64 (init_array) e PLT/GOT conforme o -fPIC.
# Versao: 0.2.0
# Changelog: 0.2.0 - M5b P36: as libs vao para vendor/linux (o link do alvo linux so procura <pasta>\vendor\linux\lib<x>.a).
#            0.1.0 - M5b P36: primeira versao (duas variantes: estatica e PIC).
set -e
cd "$(dirname "$0")"
mkdir -p vendor/linux
gcc -O2 -fno-asynchronous-unwind-tables -c libms.c -o libms.o
ar rcs vendor/linux/libms.a libms.o
gcc -O2 -fno-asynchronous-unwind-tables -fPIC -c libms.c -o libms_pic.o
ar rcs vendor/linux/libms_pic.a libms_pic.o
sha256sum vendor/linux/libms.a vendor/linux/libms_pic.a
