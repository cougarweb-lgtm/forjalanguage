#!/bin/sh
# censo_musl.sh - censo da semente musl: relocacoes, tipos de secao e simbolos externos do libc.a.
# Objetivo: P32(b) do contrato_m5_resto (D-R2/D-R6). Diz quais tipos de relocacao e de secao o
#   leitor ELF (P33) precisa aceitar e se o libc.a pede algo fora dele (libgcc, por exemplo).
# Uso (dentro do WSL alpine-build):
#   sh censo_musl.sh <libc.a> <saida.txt>
#   sh censo_musl.sh --selftest   (roda o censo em crt1.o e confere que a saida tem relocacoes)
# Versao: 0.1.0
# Changelog:
#   0.1.0 (2026-11-14) primeira versao: readelf -rW (tipo x contagem), readelf -SW (secoes),
#                      nm -u contra nm de definidos (simbolos externos).
# Depende de: binutils (readelf, nm), awk, sort, comm (Alpine).

censo() {
    lib=$1
    saida=$2
    {
        echo "== MEMBROS: $(readelf -SW "$lib" | grep -c '^File:')"
        echo "== RELOCACOES (contagem  tipo)"
        readelf -rW "$lib" | awk '$3 ~ /^R_X86_64_/ {c[$3]++} END {for (t in c) printf "%8d %s\n", c[t], t}' | sort -rn
        echo "== SECOES (contagem  nome; so o nome da secao)"
        readelf -SW "$lib" | awk '/\] \./ {sub(/.*\] /, ""); split($0, p, " "); c[p[1]]++} END {for (s in c) printf "%8d %s\n", c[s], s}' | sort -rn
        echo "== SIMBOLOS EXTERNOS (nm -u sem o que o proprio archive define)"
        nm -g --defined-only "$lib" 2>/dev/null | awk 'NF >= 3 {print $NF}' | sort -u > "$saida.defs"
        nm -u "$lib" 2>/dev/null | awk 'NF == 2 {print $2}' | sort -u > "$saida.undefs"
        comm -23 "$saida.undefs" "$saida.defs"
        echo "== fim"
    } > "$saida"
    rm -f "$saida.defs" "$saida.undefs"
}

selftest() {
    d=$(mktemp -d) || return 1
    censo /mnt/c/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/third/musl/x86_64/crt1.o "$d/s.txt" || return 1
    if grep -q '^ *[0-9][0-9]* R_X86_64_' "$d/s.txt" && grep -q '== fim' "$d/s.txt"; then
        echo "PASSOU selftest censo"
    else
        echo "FALHOU selftest censo: saida sem relocacoes"
        return 1
    fi
}

if [ "$1" = "--selftest" ]; then
    selftest
else
    censo "$1" "$2"
fi
