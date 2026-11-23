"""Gera as fixtures math_g1..math_g5 do corpus do wasm (std:math: libm do musl portada para Forja pura, 0.31.0).

Cada fixture chama as funcoes de um grupo do std:math com entradas de um LCG (bits crus, [-20,20], [-1000,1000], [-2,2]) mais uma lista de
valores especiais, e imprime os BITS de cada resultado (NaN vira um padrao so: a prova e por classe). O oraculo e o x64 (musl em C):
o verificar_wasm compara a saida do wasm com a do x64 byte a byte, em dev e em release (0 ULP de diferenca).

Uso:
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\gerar_math_bits.py            grava as 5 fixtures em tests\\nativo\\wasm\\corpus
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\gerar_math_bits.py --selftest  confere a geracao (sem gravar)
Versao: 0.1.0
"""
import os
import sys

PASSOS = 1500
PASTA_AQUI = os.path.dirname(os.path.abspath(__file__))
PASTA_CORPUS = os.path.normpath(os.path.join(PASTA_AQUI, "..", "corpus"))

# grupo -> (funcoes de 1 argumento, funcoes de 2 argumentos); cada uma vale em f64 e em f32
GRUPOS = {
    "math_g1": ("floor ceil trunc round cbrt".split(), "copysign fmod hypot".split()),
    "math_g2": ("sin cos tan".split(), []),
    "math_g3": ("atan asin acos".split(), ["atan2"]),
    "math_g4": ("exp exp2 log log2 log10".split(), []),
    "math_g5": ([], ["pow"]),
}

ESPECIAIS_64 = [
    "0x0000000000000000", "0x8000000000000000", "0x7ff0000000000000", "0xfff0000000000000", "0x7ff8000000000000",
    "0x3ff0000000000000", "0xbff0000000000000", "0x3fe0000000000000", "0x4000000000000000", "0x4008000000000000",
    "0x4024000000000000", "0x7fefffffffffffff", "0x0000000000000001", "0x0010000000000000", "0x3ff921fb54442d18",
    "0x400921fb54442d18", "0x4415af1d78b58c40", "0x41e0000000000000", "0x43e0000000000000", "0xc3e0000000000000",
    "0x3c90000000000000", "0x4059000000000000", "0x40862e42fefa39ef", "0x4086232bdd7abcd2", "0xc0874910d52d3051",
    "0x3ff8000000000000", "0x3fd3333333333333", "0x000fffffffffffff", "0x3feffffffffffffe", "0x3ff0000000000001",
]
ESPECIAIS_32 = [
    "0x00000000", "0x80000000", "0x7f800000", "0xff800000", "0x7fc00000", "0x3f800000", "0xbf800000", "0x3f000000",
    "0x40000000", "0x40400000", "0x41200000", "0x7f7fffff", "0x00000001", "0x00800000", "0x3fc90fdb", "0x40490fdb",
    "0x5f000000", "0xdf000000", "0x42b17218", "0x42b00000", "0xc2cff1b4", "0x42c80000", "0x3f7ffffe", "0x3f800001",
    "0x007fffff", "0x3e99999a", "0x4b800000", "0x33800000",
]

CABECALHO = """// std:math no alvo wasm ({nome}, 0.31.0): {lista}, em f64 e em f32, {passos} entradas de um LCG por funcao mais os valores especiais;
// imprime os BITS de cada resultado (NaN = um padrao so). wasm = x64 (musl em C) byte a byte, em dev e em release.
// GERADO por tests\\nativo\\wasm\\math_bits\\gerar_math_bits.py: nao editar a mao.
// Verificacao: python tests\\nativo\\wasm\\verificar_wasm.py --modos dev,release --filtro {nome}
import "std:math" as math
const PASSOS: int = {passos}
const ESPECIAIS64: [{n64}]u64 = {{{esp64}}}
const ESPECIAIS32: [{n32}]u32 = {{{esp32}}}

proc proximo(estado: u64) -> u64 {{
    return wrapping_add(wrapping_mul(estado, 6364136223846793005), 1442695040888963407)
}}

proc entrada64(estado: u64, k: int) -> f64 {{
    let sel: int = k % 4
    if sel == 0 {{
        return transmute(estado, to = f64)
    }}
    let u: f64 = ((estado >> 11) as f64) / 9007199254740992.0
    if sel == 1 {{
        return (u - 0.5) * 40.0
    }}
    if sel == 2 {{
        return (u - 0.5) * 2000.0
    }}
    return (u - 0.5) * 4.0
}}

proc entrada32(estado: u64, k: int) -> f32 {{
    let sel: int = k % 4
    if sel == 0 {{
        return transmute((estado >> 32) as u32, to = f32)
    }}
    return entrada64(estado, k) as f32
}}

proc bits64(r: f64) -> u64 {{
    if r != r {{
        return 0x7ff8000000000000
    }}
    return transmute(r, to = u64)
}}

proc bits32(r: f32) -> u32 {{
    if r != r {{
        return 0x7fc00000
    }}
    return transmute(r, to = u32)
}}
"""


def BlocoUnaria(Nome, Tipo):
    """Laco de uma funcao de 1 argumento em f64 ou f32."""
    Bits = "bits64" if Tipo == "f64" else "bits32"
    Entrada = "entrada64" if Tipo == "f64" else "entrada32"
    Especiais = "ESPECIAIS64" if Tipo == "f64" else "ESPECIAIS32"
    Largo = "u64" if Tipo == "f64" else "u32"
    return f"""
proc roda_{Nome}_{Tipo}(estado0: u64) {{
    print("{Nome} {Tipo}")
    for e in 0..<len({Especiais}) {{
        let x: {Tipo} = transmute({Especiais}[e], to = {Tipo})
        print({Bits}(math.{Nome}(x)))
    }}
    var estado: u64 = estado0
    for i in 0..<PASSOS {{
        estado = proximo(estado)
        let x: {Tipo} = {Entrada}(estado, i)
        print({Bits}(math.{Nome}(x)))
    }}
}}
"""


def BlocoBinaria(Nome, Tipo):
    """Laco de uma funcao de 2 argumentos: especiais x especiais, depois o LCG."""
    Bits = "bits64" if Tipo == "f64" else "bits32"
    Entrada = "entrada64" if Tipo == "f64" else "entrada32"
    Especiais = "ESPECIAIS64" if Tipo == "f64" else "ESPECIAIS32"
    return f"""
proc roda_{Nome}_{Tipo}(estado0: u64) {{
    print("{Nome} {Tipo}")
    for e in 0..<len({Especiais}) {{
        for d in 0..<len({Especiais}) {{
            let x: {Tipo} = transmute({Especiais}[e], to = {Tipo})
            let y: {Tipo} = transmute({Especiais}[d], to = {Tipo})
            print({Bits}(math.{Nome}(x, y)))
        }}
    }}
    var estado: u64 = estado0
    for i in 0..<PASSOS {{
        estado = proximo(estado)
        let x: {Tipo} = {Entrada}(estado, i)
        estado = proximo(estado)
        let y: {Tipo} = {Entrada}(estado, (i / 4) + 1)
        print({Bits}(math.{Nome}(x, y)))
    }}
}}
"""


def GerarFixture(Nome, Unarias, Binarias):
    """Texto completo de uma fixture."""
    Lista = " ".join(Unarias + Binarias)
    Texto = CABECALHO.format(nome=Nome, lista=Lista, passos=PASSOS, n64=len(ESPECIAIS_64), n32=len(ESPECIAIS_32),
                             esp64=", ".join(ESPECIAIS_64), esp32=", ".join(ESPECIAIS_32))
    Chamadas = []
    for Tipo in ("f64", "f32"):
        for Nome1 in Unarias:
            Texto += BlocoUnaria(Nome1, Tipo)
            Chamadas.append(f"roda_{Nome1}_{Tipo}")
        for Nome2 in Binarias:
            Texto += BlocoBinaria(Nome2, Tipo)
            Chamadas.append(f"roda_{Nome2}_{Tipo}")
    Texto += "\nproc main() {\n"
    for Indice, Chamada in enumerate(Chamadas):
        Texto += f"    {Chamada}({88172645463325252 + Indice})\n"
    Texto += "}\n"
    return Texto


def Gerar(Gravar):
    """Gera (e grava, se pedido) as fixtures; devolve o dicionario nome -> texto."""
    Saida = {}
    for Nome, (Unarias, Binarias) in GRUPOS.items():
        Saida[Nome] = GerarFixture(Nome, Unarias, Binarias)
        if Gravar:
            Caminho = os.path.join(PASTA_CORPUS, Nome + ".fj")
            with open(Caminho, "w", encoding="utf-8", newline="\n") as Arq:
                Arq.write(Saida[Nome])
    return Saida


def Selftest():
    """5 fixtures, cada uma com main e com todas as funcoes do grupo nos 2 tipos."""
    Saida = Gerar(False)
    assert len(Saida) == 5, "esperava 5 fixtures"
    for Nome, (Unarias, Binarias) in GRUPOS.items():
        Texto = Saida[Nome]
        assert "proc main()" in Texto, Nome
        for Funcao in Unarias + Binarias:
            for Tipo in ("f64", "f32"):
                assert f"proc roda_{Funcao}_{Tipo}(" in Texto, (Nome, Funcao, Tipo)
        assert "ç" not in Texto, "so ASCII"
    print("selftest OK: 5 fixtures")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        Selftest()
    else:
        for NomeFixture, TextoFixture in Gerar(True).items():
            print("gravado", NomeFixture, len(TextoFixture.splitlines()), "linhas")
