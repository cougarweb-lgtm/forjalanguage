#!/bin/sh
# construir.sh - gera vendor/linux/libconst.a (sem -fPIC) e libconst_pic.a (com -fPIC) a partir de libconst.c, no alpine-build (WSL). Uso: sh construir.sh (na pasta deste script).
# Sem -fstack-protector (o canario nao e o assunto da prova) e sem -fasynchronous-unwind-tables (o unwind do ELF e descartado, D-R6).
# -fno-tree-loop-distribute-patterns: sem ele o gcc troca o laco de strlen por call strlen, e o strlen do runtime Linux usa ABI Win64 (rcx), nao SysV (achado P37d; o runtime nao muda).
# Versao: 0.1.1
# Changelog: 0.1.1 - M5b P37d: -fno-tree-loop-distribute-patterns (sem call strlen no .o).
#            0.1.0 - M5b P37d: primeira versao (duas variantes: estatica e PIC; determinismo conferido por md5 de dois builds no verificar).
set -e
cd "$(dirname "$0")"
mkdir -p vendor/linux
gcc -std=c11 -O2 -fno-asynchronous-unwind-tables -fno-stack-protector -fno-tree-loop-distribute-patterns -c libconst.c -o libconst.o
ar rcs vendor/linux/libconst.a libconst.o
gcc -std=c11 -O2 -fno-asynchronous-unwind-tables -fno-stack-protector -fno-tree-loop-distribute-patterns -fPIC -c libconst.c -o libconst_pic.o
ar rcs vendor/linux/libconst_pic.a libconst_pic.o
sha256sum vendor/linux/libconst.a vendor/linux/libconst_pic.a
