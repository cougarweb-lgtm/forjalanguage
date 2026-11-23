"""Confere o L14b da Linguagem v1.11 (A05): q * r, q * [3]f32 e abs(q) da Forja contra o raymath.h da raylib 6.2, bit a bit.

Etapas:
  1. gcc -O0 -ffp-contract=off compila tests\\v111\\quat_oraculo.c (inclui o raymath.h de interop_lab) e roda: 200 linhas de 8 palavras f32 em hexa.
  2. 'forja build --release' compila tests\\lowering\\189_quat_raymath.fj e roda: as mesmas 200 linhas.
  3. Compara linha a linha (CRLF do exe MinGW vira LF) e imprime "quat_oraculo N/200" (N = linhas iguais). Saida 0 so com 200/200 e o mesmo numero de linhas.

Versao: 0.1.0
Changelog: 0.1.0 - Linguagem v1.11 L14b: primeira versao (oraculo C x fixture lowering, --selftest).
Verificacao: python conferir_quat_oraculo.py --selftest   |   python conferir_quat_oraculo.py [--forja <forja.exe>]
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
RaizLinguagem = PastaAqui.parent.parent
ForjaPadrao = RaizLinguagem / "compiler" / "forja.exe"
OraculoC = PastaAqui / "quat_oraculo.c"
FixtureForja = RaizLinguagem / "tests" / "lowering" / "189_quat_raymath.fj"
PastaRaymath = RaizLinguagem / "interop_lab" / "raylib" / "vendor" / "include"
CasosEsperados = 200
TimeoutSegundos = 120


def Normalizar(Texto: str) -> list:
    """Linhas do stdout, com o CRLF do exe MinGW trocado por LF e sem a linha vazia final."""
    return Texto.replace("\r\n", "\n").rstrip("\n").split("\n")


def CompararLinhas(Oraculo: list, Forja: list) -> int:
    """Quantas linhas da Forja batem, na mesma posicao, com as do oraculo (linha a mais ou a menos nao conta)."""
    return sum(1 for A, B in zip(Oraculo, Forja) if A == B)


def Executar(Comando: list) -> str:
    Processo = subprocess.run(Comando, capture_output=True, timeout=TimeoutSegundos, cwd=RaizLinguagem)
    if Processo.returncode != 0:
        raise RuntimeError(f"{Comando[0]} saiu com {Processo.returncode}: {Processo.stderr.decode('utf-8', 'replace')[:300]}")
    return Processo.stdout.decode("utf-8")


def RodarOraculo(Pasta: Path) -> list:
    Gcc = shutil.which("gcc")
    if Gcc is None:
        raise RuntimeError("gcc nao encontrado no PATH")
    Exe = Pasta / "quat_oraculo.exe"
    Executar([Gcc, "-O0", "-ffp-contract=off", "-I", str(PastaRaymath), str(OraculoC), "-o", str(Exe), "-lm"])
    return Normalizar(Executar([str(Exe)]))


def RodarForja(Forja: Path, Pasta: Path) -> list:
    Exe = Pasta / "quat_raymath.exe"
    Executar([str(Forja), "build", "--release", str(FixtureForja), "-o", str(Exe)])
    return Normalizar(Executar([str(Exe)]))


def Conferir(Forja: Path) -> int:
    with tempfile.TemporaryDirectory() as Temporaria:
        Pasta = Path(Temporaria)
        Oraculo = RodarOraculo(Pasta)
        Saida = RodarForja(Forja, Pasta)
    Iguais = CompararLinhas(Oraculo, Saida)
    print(f"quat_oraculo {Iguais}/{CasosEsperados}")
    return 0 if Iguais == CasosEsperados and len(Oraculo) == CasosEsperados and len(Saida) == CasosEsperados else 1


def AutoTeste() -> int:
    Normal = Normalizar("a b\r\nc d\r\n")
    assert Normal == ["a b", "c d"], Normal
    assert CompararLinhas(["a", "b", "c"], ["a", "x", "c"]) == 2
    assert CompararLinhas(["a", "b"], ["a", "b", "c"]) == 2
    print("selftest ok")
    return 0


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Analisador.add_argument("--forja", default=str(ForjaPadrao))
    Analisador.add_argument("--selftest", action="store_true")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        return AutoTeste()
    try:
        return Conferir(Path(Argumentos.forja))
    except (RuntimeError, subprocess.TimeoutExpired) as Erro:
        print(f"quat_oraculo 0/{CasosEsperados} ({Erro})")
        return 1


if __name__ == "__main__":
    sys.exit(Principal())
