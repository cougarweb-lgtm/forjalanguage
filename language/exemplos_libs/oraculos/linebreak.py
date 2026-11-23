#!/usr/bin/env python3
r"""linebreak.py - oraculo independente do vendor:linebreak (libunibreak 8.0.1, tabelas Unicode 17.0.0).

Le os arquivos OFICIAIS da Unicode 17.0.0 (LineBreakTest.txt, GraphemeBreakTest.txt e LineBreak.txt, baixados de unicode.org) e:
  1) grava o arquivo de casos: uma linha por caso, "L <hex UTF-8> <letras>" (quebra de linha) ou "G <hex UTF-8> <letras>" (grafema). linebreak_basico.fj <casos> confere com ele;
  2) com --saida-exemplo, compara a saida do exemplo sem argumento com o que os arquivos oficiais dizem para os casos embutidos.

Letras, UMA POR BYTE do texto. No ULTIMO byte de cada caractere UTF-8 fica o tipo da quebra depois dele; os outros bytes do caractere sao I:
  linha:   M = must_break, A = allow_break, N = no_break, I = inside_char
  grafema: B = boundary, N = inside_grapheme, I = inside_char
Regra da linha: o marcador ÷ do arquivo vira M se o caractere anterior tem a classe Line_Break BK, CR, LF ou NL (LB4/LB5) ou se e o fim do texto (LB3); senao A.
  O marcador × vira N. Grafema: ÷ vira B, × vira N (o fim do texto e ÷ no arquivo).

Uso: python -I linebreak.py <pasta_oficial> [--casos <arquivo>] [--saida-exemplo <saida.txt>]
  <pasta_oficial>: pasta com os 3 arquivos oficiais (fora do repo, em %TEMP%).
  --casos: padrao %TEMP%\linebreak_casos.txt.
Saida: 0 tudo bate; 1 algum caso do exemplo diverge; 2 uso ou arquivo oficial inesperado.
Versao: 0.1.0
Changelog: 0.1.0 - primeira versao (LineBreakTest, GraphemeBreakTest e LineBreak da Unicode 17.0.0).
"""
import os
import re
import sys
from pathlib import Path

VERSAO_UNICODE = "17.0.0"
CLASSES_OBRIGATORIAS = {"BK", "CR", "LF", "NL"}
RE_CLASSE = re.compile(r"^([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))?\s*;\s*([A-Z]{2})")
ARQUIVO_LB = "LineBreakTest.txt"
ARQUIVO_GB = "GraphemeBreakTest.txt"
ARQUIVO_CLASSES = "LineBreak.txt"

# Casos embutidos do exemplo (linebreak_basico.fj), na mesma ordem e com os mesmos codepoints do .fj.
EMBUTIDOS_LINHA = [
    ("L1", [0x0020, 0x0020, 0x0022]),
    ("L2", [0x000A, 0x0020, 0x0022]),
    ("L3", [0x0009, 0x0020, 0x0009]),
    ("L4", [0x000A, 0x0020, 0x000A]),
    ("L5", [0x4E00, 0x002E, 0x4E00, 0x002E]),
    ("L6", [0x0009, 0x0020, 0x0029]),
    ("L7", [0x0020, 0x0020, 0x0020]),
]
EMBUTIDOS_GRAFEMA = [
    ("G1", [0x000A, 0x000A]),
    ("G2", [0x000A, 0x0020]),
]


def VerificarVersao(Caminho, Esperado):
    """Confere no cabecalho do arquivo oficial a versao Unicode que o oraculo supoe."""
    Primeira = Caminho.read_text(encoding="utf-8").splitlines()[0]
    if Esperado not in Primeira:
        raise SystemExit(f"arquivo oficial inesperado: {Caminho.name} diz '{Primeira}', esperado {Esperado}")


def LerClassesObrigatorias(Caminho):
    """Codepoints com classe Line_Break BK, CR, LF ou NL (quebra obrigatoria depois deles)."""
    Cps = set()
    for Linha in Caminho.read_text(encoding="utf-8").splitlines():
        Casou = RE_CLASSE.match(Linha)
        if Casou and Casou.group(3) in CLASSES_OBRIGATORIAS:
            Ini = int(Casou.group(1), 16)
            Fim = int(Casou.group(2) or Casou.group(1), 16)
            Cps.update(range(Ini, Fim + 1))
    return Cps


def LerSequencias(Caminho):
    """Cada linha de dados vira (codepoints, marcadores): marcadores tem um a mais que os codepoints (o ultimo e o do fim)."""
    Saida = []
    for Linha in Caminho.read_text(encoding="utf-8").splitlines():
        Dados = Linha.split("#", 1)[0].strip()
        if not Dados:
            continue
        Tokens = Dados.split()
        Marcas = Tokens[0::2]
        Cps = [int(T, 16) for T in Tokens[1::2]]
        if len(Marcas) != len(Cps) + 1 or any(M not in ("×", "÷") for M in Marcas):
            raise SystemExit(f"linha oficial fora do formato: {Linha[:80]}")
        Saida.append((Cps, Marcas))
    return Saida


def LetrasDaLinha(Cps, Marcas, Obrigatorias):
    """Uma letra por byte do texto, para quebra de linha (ver o cabecalho)."""
    Saida = []
    for Indice, Cp in enumerate(Cps):
        Fim = Indice == len(Cps) - 1
        Marca = Marcas[Indice + 1]
        if Marca == "÷":
            Letra = "M" if (Fim or Cp in Obrigatorias) else "A"
        else:
            Letra = "N"
        Bytes = chr(Cp).encode("utf-8")
        Saida.append("I" * (len(Bytes) - 1) + Letra)
    return "".join(Saida)


def LetrasDoGrafema(Cps, Marcas):
    """Uma letra por byte do texto, para quebra de grafema (ver o cabecalho)."""
    Saida = []
    for Indice, Cp in enumerate(Cps):
        Letra = "B" if Marcas[Indice + 1] == "÷" else "N"
        Bytes = chr(Cp).encode("utf-8")
        Saida.append("I" * (len(Bytes) - 1) + Letra)
    return "".join(Saida)


def HexDoTexto(Cps):
    return "".join(chr(Cp) for Cp in Cps).encode("utf-8").hex()


def GravarCasos(Caminho, LinhasLB, LinhasGB):
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    with Caminho.open("w", encoding="utf-8", newline="\n") as Arq:
        for Linha in LinhasLB:
            Arq.write(f"L {Linha}\n")
        for Linha in LinhasGB:
            Arq.write(f"G {Linha}\n")


def ConferirExemplo(Caminho, Esperados):
    """Compara a saida do exemplo (linhas 'unicode' e 'Lx:'/'Gx:') com o que o oficial diz."""
    Obtidas = Caminho.read_text(encoding="utf-8").splitlines()
    Divergencias = 0
    if not Obtidas or Obtidas[0] != f"unicode {VERSAO_UNICODE}":
        print(f"exemplo: primeira linha deveria ser 'unicode {VERSAO_UNICODE}', veio {Obtidas[:1]}")
        Divergencias += 1
    Dados = Obtidas[1:]
    if len(Dados) != len(Esperados):
        print(f"exemplo: {len(Dados)} linhas de caso, esperado {len(Esperados)}")
        Divergencias += 1
    Acertos = 0
    for Obtida, Esperada in zip(Dados, Esperados):
        if Obtida == Esperada:
            Acertos += 1
        else:
            print(f"exemplo diverge: obtido '{Obtida}' esperado '{Esperada}'")
            Divergencias += 1
    print(f"oraculo exemplo: {Acertos}/{len(Esperados)} casos batem")
    return Divergencias


def Principal(Argumentos):
    if not Argumentos or Argumentos[0].startswith("--"):
        print(__doc__)
        return 2
    Pasta = Path(Argumentos[0])
    Casos = Path(os.environ["TEMP"]) / "linebreak_casos.txt"
    SaidaExemplo = None
    Resto = Argumentos[1:]
    while Resto:
        if len(Resto) < 2 or Resto[0] not in ("--casos", "--saida-exemplo"):
            print("uso: python -I linebreak.py <pasta_oficial> [--casos <arquivo>] [--saida-exemplo <saida.txt>]")
            return 2
        if Resto[0] == "--casos":
            Casos = Path(Resto[1])
        else:
            SaidaExemplo = Path(Resto[1])
        Resto = Resto[2:]

    VerificarVersao(Pasta / ARQUIVO_LB, VERSAO_UNICODE)
    VerificarVersao(Pasta / ARQUIVO_GB, VERSAO_UNICODE)
    VerificarVersao(Pasta / ARQUIVO_CLASSES, VERSAO_UNICODE)
    Obrigatorias = LerClassesObrigatorias(Pasta / ARQUIVO_CLASSES)

    SequenciasLB = LerSequencias(Pasta / ARQUIVO_LB)
    SequenciasGB = LerSequencias(Pasta / ARQUIVO_GB)

    LinhasLB = [HexDoTexto(Cps) + " " + LetrasDaLinha(Cps, Marcas, Obrigatorias) for Cps, Marcas in SequenciasLB]
    LinhasGB = [HexDoTexto(Cps) + " " + LetrasDoGrafema(Cps, Marcas) for Cps, Marcas in SequenciasGB]
    GravarCasos(Casos, LinhasLB, LinhasGB)
    print(f"casos gravados: {len(LinhasLB)} de quebra de linha, {len(LinhasGB)} de grafema -> {Casos}")

    if SaidaExemplo is None:
        return 0

    # Casos embutidos: procura a sequencia exata no arquivo oficial e deriva a letra esperada.
    IndiceLB = {tuple(Cps): LetrasDaLinha(Cps, Marcas, Obrigatorias) for Cps, Marcas in SequenciasLB}
    IndiceGB = {tuple(Cps): LetrasDoGrafema(Cps, Marcas) for Cps, Marcas in SequenciasGB}
    Esperados = []
    for Rotulo, Cps in EMBUTIDOS_LINHA:
        if tuple(Cps) not in IndiceLB:
            raise SystemExit(f"caso embutido {Rotulo} nao esta em {ARQUIVO_LB}")
        Esperados.append(f"{Rotulo}: {IndiceLB[tuple(Cps)]}")
    for Rotulo, Cps in EMBUTIDOS_GRAFEMA:
        if tuple(Cps) not in IndiceGB:
            raise SystemExit(f"caso embutido {Rotulo} nao esta em {ARQUIVO_GB}")
        Esperados.append(f"{Rotulo}: {IndiceGB[tuple(Cps)]}")
    return 1 if ConferirExemplo(SaidaExemplo, Esperados) else 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
