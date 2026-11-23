"""Gera a tabela de bits das constantes P0..P4 do cbrt do musl (third\\musl_math\\cbrt.c) para o rt_wasm_math.fj.

Uso:
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\tabelas\\gerar_tabelas_math.py
Saida: uma linha `const CBRT_P_BITS: [5]u64 = {...}` para colar no rt_wasm_math.fj (Edit, nao reescrita por script).
Os decimais sao conferidos no proprio cbrt.c antes de sair; float() do Python arredonda como o gcc (mais proximo).
Versao: 0.1.0
"""
import os
import struct
import sys

PASTA_AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ_LANGUAGE = os.path.normpath(os.path.join(PASTA_AQUI, "..", "..", "..", "..", ".."))
CAMINHO_CBRT = os.path.join(RAIZ_LANGUAGE, "third", "musl_math", "cbrt.c")

# Decimais exatamente como estao no cbrt.c do musl 1.2.6
CONSTANTES_CBRT = [
    ("P0", "1.87595182427177009643"),
    ("P1", "-1.88497979543377169875"),
    ("P2", "1.621429720105354466140"),
    ("P3", "-0.758397934778766047437"),
    ("P4", "0.145996192886612446982"),
]


def BitsDoDouble(Valor):
    """Bits crus (u64) de um f64 IEEE-754 em little-endian."""
    return struct.unpack("<Q", struct.pack("<d", Valor))[0]


def ConfereNoFonte(Decimais):
    """Cada decimal tem de aparecer no cbrt.c: senao a tabela sairia de outra fonte."""
    with open(CAMINHO_CBRT, encoding="utf-8") as Arq:
        Texto = Arq.read()
    for Nome, Decimal in Decimais:
        if Decimal not in Texto:
            raise SystemExit(f"{Nome} = {Decimal} nao esta em {CAMINHO_CBRT}")


def Principal():
    ConfereNoFonte(CONSTANTES_CBRT)
    Bits = [f"0x{BitsDoDouble(float(Decimal)):016x}" for _, Decimal in CONSTANTES_CBRT]
    print(f"const CBRT_P_BITS: [{len(Bits)}]u64 = {{{', '.join(Bits)}}}")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
