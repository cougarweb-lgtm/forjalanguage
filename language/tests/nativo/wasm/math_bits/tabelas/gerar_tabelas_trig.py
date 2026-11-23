"""Gera as constantes e tabelas do grupo trig (sin, cos, tan em f64 e f32) do porte Forja da libm do musl 1.2.6.

Uso:
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\tabelas\\gerar_tabelas_trig.py [--saida <arquivo.txt>]
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\tabelas\\gerar_tabelas_trig.py --selftest

Le as constantes direto dos .c do musl (third\\musl_math, so leitura). Cada valor decimal e conferido contra o
hex do comentario do proprio C (/* 0xHI, 0xLO */), entao o que vai pro .fj e o double que o musl usa.
Saida: bloco de `const` para colar em runtime\\wasm\\rt_wasm_trig.fj. f64 de escalar vai em decimal de notacao fixa
(sem `e`, a Forja nao aceita); tabela de f64 vai em bits u64 (leitura com transmute).
Depende de: nada alem da stdlib. Versao: 0.1.0
"""
import argparse
import os
import re
import struct
import sys
from decimal import Decimal

PASTA_AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ_LANGUAGE = os.path.normpath(os.path.join(PASTA_AQUI, "..", "..", "..", "..", ".."))
PASTA_MUSL = os.path.join(RAIZ_LANGUAGE, "third", "musl_math")
TAMANHO_IPIO2_ESPERADO = 690  # 396 hex do 2/pi + ramo long double (LDBL_MAX_EXP > 1024, como no x64)
NUM_HEX_P = r"[-+]?0x[0-9a-fA-F.]+p[-+]?\d+"
NUM_DEC = r"[-+]?\d+\.\d+(?:e[-+]?\d+)?"
NUM_QUALQUER = rf"{NUM_HEX_P}|{NUM_DEC}"


def LerC(NomeArquivo):
    """Texto de um .c do musl (so leitura)."""
    with open(os.path.join(PASTA_MUSL, NomeArquivo), encoding="utf-8") as Arq:
        return Arq.read()


def SemComentarios(Texto):
    """Tira /* */ e // para que os numeros de comentario nao entrem nas tabelas."""
    Texto = re.sub(r"/\*.*?\*/", " ", Texto, flags=re.S)
    return re.sub(r"//[^\n]*", " ", Texto)


def ParseDouble(Token):
    """Hex-float (0x1.8p0) ou decimal do C, sempre para double."""
    if "p" in Token.lower():
        return float.fromhex(Token)
    return float(Token)


def ValorDe(Texto, Nome):
    """Valor da constante `Nome = valor` do C (\\b evita casar S1 dentro de S10 ou pio2_1 dentro de pio2_1t)."""
    Achado = re.search(rf"\b{Nome}\s*=\s*({NUM_QUALQUER})", Texto)
    if not Achado:
        raise ValueError(f"constante {Nome} nao encontrada")
    return ParseDouble(Achado.group(1))


def BlocoApos(Texto, Cabecalho):
    """Texto entre `Cabecalho` (ex.: 'T[] = {') e a proxima chave de fechamento."""
    Inicio = Texto.index(Cabecalho) + len(Cabecalho)
    return Texto[Inicio:Texto.index("}", Inicio)]


def NumerosDoBloco(Bloco):
    return re.findall(rf"[-+]?0x[0-9a-fA-F.]+(?:p[-+]?\d+)?|{NUM_DEC}", Bloco)


def BitsDouble(Valor):
    return struct.unpack("<Q", struct.pack("<d", Valor))[0]


def FixoDe(Valor):
    """Decimal de notacao fixa que volta ao mesmo double (repr do Python e o menor texto que faz isso)."""
    Texto = format(Decimal(repr(Valor)), "f")
    assert float(Texto) == Valor, (Valor, Texto)
    return Texto


def TabelaTrigonometria():
    """Coleta todas as constantes e tabelas, na ordem em que o .fj as usa."""
    Sen = SemComentarios(LerC("__sin.c"))
    Cos = SemComentarios(LerC("__cos.c"))
    Tan = SemComentarios(LerC("__tan.c"))
    Rem = SemComentarios(LerC("__rem_pio2.c"))
    RemGrande = SemComentarios(LerC("__rem_pio2_large.c"))
    RemF = SemComentarios(LerC("__rem_pio2f.c"))
    SinDf = SemComentarios(LerC("__sindf.c"))
    CosDf = SemComentarios(LerC("__cosdf.c"))
    TanDf = SemComentarios(LerC("__tandf.c"))
    Escalares = [
        ("SIN_S1", ValorDe(Sen, "S1")), ("SIN_S2", ValorDe(Sen, "S2")), ("SIN_S3", ValorDe(Sen, "S3")),
        ("SIN_S4", ValorDe(Sen, "S4")), ("SIN_S5", ValorDe(Sen, "S5")), ("SIN_S6", ValorDe(Sen, "S6")),
        ("COS_C1", ValorDe(Cos, "C1")), ("COS_C2", ValorDe(Cos, "C2")), ("COS_C3", ValorDe(Cos, "C3")),
        ("COS_C4", ValorDe(Cos, "C4")), ("COS_C5", ValorDe(Cos, "C5")), ("COS_C6", ValorDe(Cos, "C6")),
        ("TAN_PIO4", ValorDe(Tan, "pio4")), ("TAN_PIO4LO", ValorDe(Tan, "pio4lo")),
        ("TRIG_TOINT", 1.5 * 2.0 ** 52),
        ("TRIG_INVPIO2", ValorDe(Rem, "invpio2")), ("TRIG_PIO4", ValorDe(Rem, "pio4")),
        ("TRIG_PIO2_1", ValorDe(Rem, "pio2_1")), ("TRIG_PIO2_1T", ValorDe(Rem, "pio2_1t")),
        ("TRIG_PIO2_2", ValorDe(Rem, "pio2_2")), ("TRIG_PIO2_2T", ValorDe(Rem, "pio2_2t")),
        ("TRIG_PIO2_3", ValorDe(Rem, "pio2_3")), ("TRIG_PIO2_3T", ValorDe(Rem, "pio2_3t")),
        ("TRIG_PIO2F_1", ValorDe(RemF, "pio2_1")), ("TRIG_PIO2F_1T", ValorDe(RemF, "pio2_1t")),
        ("TRIG_PIO4F", ValorDe(RemF, "pio4")),
        ("TRIG_MPIO2_1", 1 * float.fromhex("0x1.921fb54442d18p0")),
        ("TRIG_MPIO2_2", 2 * float.fromhex("0x1.921fb54442d18p0")),
        ("TRIG_MPIO2_3", 3 * float.fromhex("0x1.921fb54442d18p0")),
        ("TRIG_MPIO2_4", 4 * float.fromhex("0x1.921fb54442d18p0")),
        ("SINDF_S1", ValorDe(SinDf, "S1")), ("SINDF_S2", ValorDe(SinDf, "S2")),
        ("SINDF_S3", ValorDe(SinDf, "S3")), ("SINDF_S4", ValorDe(SinDf, "S4")),
        ("COSDF_C0", ValorDe(CosDf, "C0")), ("COSDF_C1", ValorDe(CosDf, "C1")),
        ("COSDF_C2", ValorDe(CosDf, "C2")), ("COSDF_C3", ValorDe(CosDf, "C3")),
    ]
    Tabelas = [
        ("TRIG_IPIO2", "i32", [int(Numero, 16) for Numero in NumerosDoBloco(BlocoApos(RemGrande, "ipio2[] = {"))]),
        ("TRIG_PIO2_BITS", "u64", [BitsDouble(ParseDouble(Numero)) for Numero in NumerosDoBloco(BlocoApos(RemGrande, "PIo2[] = {"))]),
        ("TRIG_TAN_T_BITS", "u64", [BitsDouble(ParseDouble(Numero)) for Numero in NumerosDoBloco(BlocoApos(Tan, "T[] = {"))]),
        ("TRIG_TANDF_T_BITS", "u64", [BitsDouble(ParseDouble(Numero)) for Numero in NumerosDoBloco(BlocoApos(TanDf, "T[] = {"))]),
    ]
    if len(Tabelas[0][2]) != TAMANHO_IPIO2_ESPERADO:
        raise ValueError(f"ipio2 com {len(Tabelas[0][2])} entradas, esperado {TAMANHO_IPIO2_ESPERADO}")
    return Escalares, Tabelas


def FormataTabela(Nome, Tipo, Valores):
    """Tabela em linhas de 6, como o musl; i32 e u64 em hex."""
    Linhas = [f"const {Nome}: [{len(Valores)}]{Tipo} = {{"]
    for Inicio in range(0, len(Valores), 6):
        Trecho = Valores[Inicio:Inicio + 6]
        Linhas.append("    " + ", ".join(f"0x{Valor:X}" for Valor in Trecho) + ",")
    Linhas.append("}")
    return "\n".join(Linhas)


def BlocoGerado():
    Escalares, Tabelas = TabelaTrigonometria()
    Linhas = ["// ---- constantes escalares (decimal de notacao fixa; mesmo double que o musl) ----"]
    for Nome, Valor in Escalares:
        Linhas.append(f"const {Nome}: f64 = {FixoDe(Valor)}")
    Linhas.append("// ---- tabelas (bits: o f64 e lido com transmute) ----")
    for Nome, Tipo, Valores in Tabelas:
        Linhas.append(FormataTabela(Nome, Tipo, Valores))
    return "\n".join(Linhas) + "\n"


def Selftest():
    """Confere cada constante decimal contra o hex do comentario do proprio C; e a contagem das tabelas."""
    Confere = 0
    for NomeArquivo in ["__sin.c", "__cos.c", "__tan.c", "__rem_pio2.c", "__rem_pio2_large.c", "__rem_pio2f.c",
                        "__sindf.c", "__cosdf.c", "__tandf.c"]:
        Texto = LerC(NomeArquivo)
        Padrao = rf"({NUM_QUALQUER})\s*[,;]?\s*/\*\s*(?:0x)?([0-9A-Fa-f]{{8}}),\s*(?:0x)?([0-9A-Fa-f]{{8}})\s*\*/"
        for Achado in re.finditer(Padrao, Texto):
            Esperado = (int(Achado.group(2), 16) << 32) | int(Achado.group(3), 16)
            Obtido = BitsDouble(ParseDouble(Achado.group(1)))
            assert Obtido == Esperado, (NomeArquivo, Achado.group(1), hex(Obtido), hex(Esperado))
            Confere += 1
    assert Confere >= 40, Confere
    Escalares, Tabelas = TabelaTrigonometria()
    assert len(Tabelas[0][2]) == TAMANHO_IPIO2_ESPERADO
    assert Tabelas[0][2][0] == 0xA2F983, hex(Tabelas[0][2][0])
    assert Tabelas[1][2][0] == 0x3FF921FB40000000, hex(Tabelas[1][2][0])
    assert len(Tabelas[2][2]) == 13 and len(Tabelas[3][2]) == 6
    print(f"selftest OK: {Confere} constantes do C conferidas contra o hex do comentario; {len(Escalares)} escalares; "
          f"tabelas {[(Nome, len(Valores)) for Nome, _, Valores in Tabelas]}")


def Principal():
    Analisador = argparse.ArgumentParser(description="gera consts e tabelas do grupo trig (musl -> Forja)")
    Analisador.add_argument("--saida", default="")
    Analisador.add_argument("--selftest", action="store_true")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        Selftest()
        return 0
    Texto = BlocoGerado()
    if Argumentos.saida:
        with open(Argumentos.saida, "w", encoding="utf-8", newline="\n") as Arq:
            Arq.write(Texto)
        print(f"gravado: {Argumentos.saida} ({Texto.count(chr(10))} linhas)")
    else:
        sys.stdout.write(Texto)
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
