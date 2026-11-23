#!/bin/sh
# premissa_c.sh - prova a premissa (c) do D-R2/D-R3: ld liga crt1.o + main.o (SysV, chama ms_abi) + pthread_create x6 + libc.a.
# Objetivo: o musl e o unico dono do processo (TLS, errno por thread); roda no WSL alpine-build e sai 0 so com PASSOU.
# Uso (dentro do WSL alpine-build):
#   sh /mnt/c/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/tests/nativo/linux/musl/premissa_c.sh
#   --selftest: mesma prova (o proprio teste e a verificacao).
# Versao: 0.1.0
# Changelog:
#   0.1.0 (2026-11-14) primeira versao: gcc -c do premissa_c.c, ld -static com crt1/crti/crtn + libc.a da semente, 3 execucoes.
# Depende de: gcc, ld (binutils), musl libc.a da pasta third\musl\x86_64 (P32a).

BASE=/mnt/c/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language
FONTE=$BASE/tests/nativo/linux/musl/premissa_c.c
SEMENTE=$BASE/third/musl/x86_64

premissa() {
    obra=$(mktemp -d) || return 1
    gcc -O1 -Wall -fno-stack-protector -fno-pic -c "$FONTE" -o "$obra/premissa_c.o" || { echo "FALHOU gcc -c"; return 1; }
    ld -static -o "$obra/premissa_c" "$SEMENTE/crt1.o" "$SEMENTE/crti.o" "$obra/premissa_c.o" "$SEMENTE/libc.a" "$SEMENTE/crtn.o" \
        || { echo "FALHOU ld"; return 1; }
    for execucao in 1 2 3; do
        "$obra/premissa_c" || { echo "FALHOU execucao $execucao, codigo $?"; return 1; }
    done
    echo "ELF: $(readelf -hW "$obra/premissa_c" | grep Type)"
}

premissa
