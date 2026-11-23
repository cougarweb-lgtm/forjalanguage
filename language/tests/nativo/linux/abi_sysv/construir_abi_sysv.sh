#!/bin/sh
# construir_abi_sysv.sh - gera as libs de teste SysV no alpine-build (WSL): casos/vendor/linux/libabi_sysv.a (60 assinaturas) e
# vivos/vendor/linux/libvivos.a (funcoes que destroem rsi/rdi/xmm6-15 com asm). Uso: sh construir_abi_sysv.sh (na pasta deste script).
# Versao: 0.1.1
# Changelog: 0.1.1 - M5b P37c: -fno-stack-protector (a lib com vetor local pedia __stack_chk_fail, simbolo da libc que o ligador ELF ainda nao traz; P39 da musl).
#            0.1.0 - M5b P37c: primeira versao (-O2 -fno-asynchronous-unwind-tables -fno-pic -fno-merge-constants, como o libsmoke do P37).
set -e
cd "$(dirname "$0")"
mkdir -p casos/vendor/linux vivos/vendor/linux
gcc -std=gnu11 -O2 -fno-asynchronous-unwind-tables -fno-pic -fno-merge-constants -fno-stack-protector -c assinaturas.c -o assinaturas_sysv.o
ar rcs casos/vendor/linux/libabi_sysv.a assinaturas_sysv.o
gcc -std=gnu11 -O2 -fno-asynchronous-unwind-tables -fno-pic -fno-merge-constants -fno-stack-protector -c vivos/vivos.c -o vivos_sysv.o
ar rcs vivos/vendor/linux/libvivos.a vivos_sysv.o
rm -f assinaturas_sysv.o vivos_sysv.o
sha256sum casos/vendor/linux/libabi_sysv.a vivos/vendor/linux/libvivos.a
