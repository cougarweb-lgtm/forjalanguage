#!/bin/sh
# construir_musl.sh - compila a semente musl 1.2.6 para o alvo linux x86_64.
# Objetivo: P32(a) do contrato_m5_resto (D-R4). Gera libc.a, crt1.o, rcrt1.o, crti.o, crtn.o
#   FORA do forja.exe (a semente vai para third\musl\x86_64\). Nao toca compiler\ nem runtime\.
# Uso (dentro do WSL alpine-build, por harness):
#   sh /mnt/c/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/tests/nativo/linux/musl/construir_musl.sh <musl-1.2.6.tar.gz> <pasta_saida>
#   sh .../construir_musl.sh --selftest   (prova que a conferencia de sha256 aceita o certo e recusa o errado)
# Versao: 0.1.0
# Changelog:
#   0.1.0 (2026-11-14) primeira versao: sha256 conferido contra d585fd3b..., configure --disable-shared,
#                      make, copia dos 5 arquivos + SHA256SUMS na pasta de saida.
# Depende de: gcc, make, tar, sha256sum, nproc (Alpine). Licenca do musl: MIT.

MUSL_SHA256=d585fd3b613c66151fc3249e8ed44f77020cb5e6c1e635a616d3f9f82460512a

conferir_sha() {
    # $1 = arquivo, $2 = sha256 esperado. Retorna 0 se bater.
    atual=$(sha256sum "$1" | cut -d' ' -f1)
    [ "$atual" = "$2" ]
}

selftest() {
    d=$(mktemp -d) || return 1
    printf 'x' > "$d/arq"
    if conferir_sha "$d/arq" 0000; then
        echo "FALHOU selftest: aceitou hash errado"
        return 1
    fi
    h=$(sha256sum "$d/arq" | cut -d' ' -f1)
    if ! conferir_sha "$d/arq" "$h"; then
        echo "FALHOU selftest: recusou hash certo"
        return 1
    fi
    echo "PASSOU selftest sha256"
}

construir() {
    tgz=$1
    saida=$2
    [ -f "$tgz" ] || { echo "FALHOU: tarball nao existe: $tgz"; return 1; }
    conferir_sha "$tgz" "$MUSL_SHA256" || { echo "FALHOU: sha256 diferente de $MUSL_SHA256"; return 1; }
    echo "PASSOU sha256 do tarball"
    obra=$(mktemp -d) || return 1
    tar -xzf "$tgz" -C "$obra" || return 1
    src="$obra/musl-1.2.6"
    cd "$src" || return 1
    ./configure --disable-shared > "$obra/configure.log" 2>&1 \
        || { echo "FALHOU configure, log: $obra/configure.log"; return 1; }
    make -j"$(nproc)" > "$obra/make.log" 2>&1 \
        || { echo "FALHOU make, log: $obra/make.log"; return 1; }
    mkdir -p "$saida" || return 1
    for f in libc.a crt1.o rcrt1.o crti.o crtn.o; do
        cp "$src/lib/$f" "$saida/$f" || return 1
    done
    (cd "$saida" && sha256sum libc.a crt1.o rcrt1.o crti.o crtn.o > SHA256SUMS) || return 1
    echo "OK build em $obra ; saida em $saida"
}

if [ "$1" = "--selftest" ]; then
    selftest
else
    construir "$1" "$2"
fi
