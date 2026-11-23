#!/usr/bin/env python3
r"""shaping.py - oraculo independente do vendor:shaping (HarfBuzz 15.1.0) para o exemplo shaping_basico.fj.

Fonte independente: uharfbuzz (HarfBuzz embutido, versao 14.2.1 na 0.55.0; divergencia por versao e informacao, enumerada
no relatorio) para shaping e subset, e fontTools para metricas, cmap e hmtx lidos do proprio arquivo da fonte.

Uso:
  python -I shaping.py --verificar --forja <forja.exe absoluto>   roda shaping_basico.fj (fonte copiada para %TEMP%),
                                                                  compara a saida com shaping_basico.esperado e com o oraculo
  python -I shaping.py --dados <pasta>                            so confere a pasta (Amiri-Regular.ttf) contra o oraculo
Saida: 0 tudo bate; 1 divergencia; 2 uso ou arquivo ausente.
Versao: 0.1.0
Changelog: 0.1.0 - primeira versao (Amiri-Regular.ttf, SIL OFL 1.1; uharfbuzz 0.55.0 e fontTools 4.63.0).
"""
import argparse
import io
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import uharfbuzz as hb
from fontTools.ttLib import TTFont

PASTA_ESTE = Path(__file__).resolve().parent
EXEMPLO = PASTA_ESTE.parent / "shaping_basico.fj"
ESPERADO = PASTA_ESTE.parent / "shaping_basico.esperado"
FONTE_NOME = "Amiri-Regular.ttf"
PASTA_DADOS = PASTA_ESTE / "dados"

LATIM = "office fi"
ARABE = "العربية"  # العربية
AUSENTE = "a\U0001F600"  # a + emoji (fora do cmap)


def NomeErro(Codigo: str) -> str:
    return Codigo


def LinhaGlifos(Rotulo: str, Buffer: "hb.Buffer") -> list:
    """Cabecalho 'rotulo n=N' e uma linha por glifo, no mesmo formato do exemplo."""
    Infos = Buffer.glyph_infos
    Posicoes = Buffer.glyph_positions
    Linhas = [f"{Rotulo} n={len(Infos)}"]
    for Info, Pos in zip(Infos, Posicoes):
        Linhas.append(f"  g {Info.codepoint} {Info.cluster} {Pos.x_advance} {Pos.y_advance} {Pos.x_offset} {Pos.y_offset}")
    return Linhas


def Shapear(Fonte: "hb.Font", Rotulo: str, Texto: str, Opcoes: dict) -> list:
    """Shape com uharfbuzz. Opcoes: direction, script, language, features (dict tag->bool)."""
    Buffer = hb.Buffer()
    Buffer.add_utf8(Texto.encode("utf-8"))  # UTF-8 como a Forja (cluster = indice em bytes, como hb_buffer_add_utf8)
    if Opcoes.get("direction"):
        Buffer.direction = Opcoes["direction"]
    if Opcoes.get("script"):
        Buffer.script = Opcoes["script"]
    if Opcoes.get("language"):
        Buffer.language = Opcoes["language"]
    Buffer.guess_segment_properties()
    hb.shape(Fonte, Buffer, Opcoes.get("features", {}))
    return LinhaGlifos(Rotulo, Buffer)


def FonteHb(Dados: bytes) -> "hb.Font":
    Face = hb.Face(Dados)
    Fonte = hb.Font(Face)
    Upem = Face.upem
    Fonte.scale = (Upem, Upem)
    return Fonte


def ShapesEsperados(Dados: bytes) -> list:
    """Os 6 shapings do exemplo, na ordem e com os mesmos rotulos."""
    Fonte = FonteHb(Dados)
    Linhas = []
    Linhas += Shapear(Fonte, "latim_sem_liga", LATIM, {"direction": "ltr", "script": "Latn", "language": "pt-BR", "features": {"kern": True, "liga": False}})
    Linhas += Shapear(Fonte, "latim_com_liga", LATIM, {"direction": "ltr", "script": "Latn", "language": "pt-BR"})
    Linhas += Shapear(Fonte, "arabe_auto", ARABE, {})
    Linhas += Shapear(Fonte, "ausente", AUSENTE, {"direction": "ltr"})
    Linhas += ["script_curto invalid_input", "vazio n=0"]
    return Linhas


def MetricasEsperadas(Dados: bytes) -> list:
    """upem, metricas, glifos, gid de 'A', gid do emoji e avanco de A, lidos por fontTools (hhea, maxp, cmap, hmtx)."""
    Tt = TTFont(io.BytesIO(Dados))
    Upem = Tt["head"].unitsPerEm
    Hhea = Tt["hhea"]
    Cmap = Tt.getBestCmap()
    GidA = GidDoCodepoint(Tt, 65)
    NomeA = Tt.getGlyphName(GidA)
    Avanco = Tt["hmtx"][NomeA][0]
    return [
        f"upem {Upem}",
        f"metricas {Hhea.ascent} {Hhea.descent} {Hhea.lineGap}",
        f"glifos {Tt['maxp'].numGlyphs}",
        f"gid_A {GidA}",
        f"gid_emoji {GidDoCodepoint(Tt, 0x1F600)}",
        f"avanco_A {Avanco}",
        "avanco_fora invalid_glyph",
    ]


def SubsetEsperado(Dados: bytes, Gids: list) -> hb.Face:
    """Subset pelo uharfbuzz (glyph set = Gids, sem unicodes), como o exemplo pede."""
    Face = hb.Face(Dados)
    Entrada = hb.SubsetInput()
    Entrada.glyph_set.update(Gids)
    return hb.subset(Face, Entrada)


def ConferirSubset(Caminho: Path, Dados: bytes, Gids: list) -> list:
    """fontTools abre o subset gravado pelo exemplo e confere glifos e cmap contra o uharfbuzz."""
    Divergencias = []
    if not Caminho.exists():
        return ["subset.ttf nao foi gravado"]
    Feito = TTFont(str(Caminho))
    Refeito = TTFont(io.BytesIO(bytes(SubsetEsperado(Dados, Gids).blob.data)))
    if Feito["maxp"].numGlyphs != Refeito["maxp"].numGlyphs:
        Divergencias.append(f"subset numGlyphs {Feito['maxp'].numGlyphs} != oraculo {Refeito['maxp'].numGlyphs}")
    if sorted(Feito.getBestCmap().keys()) != sorted(Refeito.getBestCmap().keys()):
        Divergencias.append("subset cmap (codepoints) diverge do oraculo")
    return Divergencias


def GidDoCodepoint(Tt: TTFont, Codepoint: int) -> int:
    """gid do codepoint no cmap; 0 se ausente. fontTools devolve o NOME do glifo, por isso getGlyphID."""
    Nome = Tt.getBestCmap().get(Codepoint)
    return 0 if Nome is None else Tt.getGlyphID(Nome)


def GidsDoSubset(Dados: bytes) -> list:
    """Mesmos gids que o exemplo usa: 0, 'o' e 'f'."""
    Tt = TTFont(io.BytesIO(Dados))
    return [0, GidDoCodepoint(Tt, ord("o")), GidDoCodepoint(Tt, ord("f"))]


def LinhasOraculo(Dados: bytes) -> list:
    """Saida esperada do exemplo (sem a linha de versao e sem o tamanho do subset, que dependem da versao do HarfBuzz)."""
    Linhas = MetricasEsperadas(Dados)
    Linhas += ShapesEsperados(Dados)
    Linhas += ["subset_fora invalid_glyph", "subset_vazio invalid_input"]
    return Linhas


def LinhasDoForja(Saida: str) -> list:
    return [L for L in Saida.replace("\r\n", "\n").split("\n") if L and not L.startswith("{") and not L.startswith("versao ")]


def CompararLinhas(Obtido: list, Esperado: list) -> list:
    """Diferencas linha a linha (a saida do exemplo tem de conter as linhas do oraculo, na ordem)."""
    Problemas = []
    Ignorar = ("subset bytes ", "aleatoria ", "vazia ", "sem fonte")
    Obtido = [L for L in Obtido if not L.startswith(Ignorar)]
    for Indice, Linha in enumerate(Esperado):
        if Indice >= len(Obtido):
            Problemas.append(f"faltou: {Linha!r}")
        elif Obtido[Indice] != Linha:
            Problemas.append(f"linha {Indice}: forja {Obtido[Indice]!r} != oraculo {Linha!r}")
    return Problemas


def Verificar(Forja: Path) -> int:
    Dados = (PASTA_DADOS / FONTE_NOME).read_bytes()
    PastaTemp = Path(tempfile.mkdtemp(prefix="shaping_oraculo_"))
    try:
        shutil.copy(PASTA_DADOS / FONTE_NOME, PastaTemp / FONTE_NOME)
        Proc = subprocess.run([str(Forja), "run", "--backend", "x64", str(EXEMPLO), "--", str(PastaTemp), str(PastaTemp / "subset.ttf")],
                              capture_output=True, timeout=600)
        Saida = Proc.stdout.decode("utf-8", errors="replace")
        Problemas = []
        Problemas += CompararLinhas(LinhasDoForja(Saida), LinhasOraculo(Dados))
        Problemas += ConferirSubset(PastaTemp / "subset.ttf", Dados, GidsDoSubset(Dados))
        if ESPERADO.exists():
            Esp = ESPERADO.read_bytes().decode("utf-8").replace("\r\n", "\n")
            if Saida.replace("\r\n", "\n") != Esp:
                Problemas.append("stdout diverge de shaping_basico.esperado")
    finally:
        shutil.rmtree(PastaTemp, ignore_errors=True)
    print(f"uharfbuzz {hb.__version__} (HarfBuzz embutido) | fontTools ok")
    print(f"linhas do oraculo: {len(LinhasOraculo(Dados))}")
    for P in Problemas:
        print("DIVERGE " + P)
    if not Problemas:
        print("shaping ok: oraculo e exemplo batem")
    return 1 if Problemas else 0


def Principal(Argumentos: list) -> int:
    Parser = argparse.ArgumentParser(description="oraculo do vendor:shaping")
    Parser.add_argument("--verificar", action="store_true")
    Parser.add_argument("--forja", type=Path)
    Parser.add_argument("--dados", type=Path, default=PASTA_DADOS)
    Ns = Parser.parse_args(Argumentos)
    if Ns.verificar:
        if Ns.forja is None or not Ns.forja.exists():
            print("uso: --verificar --forja <forja.exe absoluto>")
            return 2
        return Verificar(Ns.forja)
    Fonte = Ns.dados / FONTE_NOME
    if not Fonte.exists():
        print(f"fonte ausente: {Fonte}")
        return 2
    for Linha in LinhasOraculo(Fonte.read_bytes()):
        print(Linha)
    return 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
