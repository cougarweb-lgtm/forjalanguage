"""medir_custo.py - custo de build e de run do forja: atual x forja_m6c (ABBA), mediana (M6d, P92a).

Mede corpus_200000 (benchmark\\corpus\\gerado, dev): build dev e run dev do forja atual contra a copia do fecho do M6c
(%TEMP%\\forja_m6c.exe), em pares ABBA (a ordem alterna a cada par, para o aquecimento nao favorecer um lado).
--simbolos: mesmo forja atual, --simbolos sim x nao. --lib: lt_bench(200000) (fixture lib\\bench_lib.fj), dev x release do
mesmo forja, so relato. Os gates (<= 1,05x dev, <= 1,15x simbolos) sao informados; quem decide e o main.

Versao: 0.1.0
Changelog:
  0.1.0 - M6d P92a: primeira versao (ABBA, mediana, --pares, --simbolos, --lib).
Uso: python tests\\nativo\\m6d\\medir_custo.py [--pares 9] [--simbolos] [--lib] [--forja-atual <exe>]
Depende de: stdlib do Python; forja atual (compiler\\forja.exe, copiado para %TEMP%) e forja_m6c.exe em %TEMP%.
"""

import argparse
import os
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

PastaM6d = Path(__file__).resolve().parent
PastaForja = PastaM6d.parents[2]
Corpus = PastaForja / "benchmark" / "corpus" / "gerado" / "corpus_200000.fj"
FixtureLib = PastaM6d / "lib" / "bench_lib.fj"
ForjaM6c = Path(os.environ.get("TEMP", tempfile.gettempdir())) / "forja_m6c.exe"
ForjaAtualFonte = PastaForja / "compiler" / "forja.exe"
TetoSegundos = 900
GateDev = 1.05
GateSimbolos = 1.15


def CronometrarComando(Comando, Cwd):
    Inicio = time.perf_counter()
    subprocess.run(Comando, check=True, capture_output=True, cwd=Cwd, timeout=TetoSegundos)
    return time.perf_counter() - Inicio


def MedirBuildRun(Forja, Fonte, Saida, Cwd, Opcoes):
    """Devolve (segundos de build, segundos de run) de Forja compilando Fonte com Opcoes."""
    Build = CronometrarComando([str(Forja), "build", *Opcoes, str(Fonte), "-o", str(Saida)], Cwd)
    Run = CronometrarComando([str(Saida)], Cwd)
    return Build, Run


def Pares(Quantidade):
    """ABBA: a ordem (A,B) / (B,A) alterna; devolve os indices das duas posicoes do par."""
    return [(0, 1) if K % 2 == 0 else (1, 0) for K in range(Quantidade)]


def MedirAbba(ForjaA, ForjaB, Quantidade, Fonte, Tmp, Opcoes):
    """Mede A e B em pares ABBA. Devolve dict com as medianas e a razao A/B de build e de run."""
    Coletas = {"A": {"build": [], "run": []}, "B": {"build": [], "run": []}}
    Forjas = {"A": ForjaA, "B": ForjaB}
    for Par in Pares(Quantidade):
        for Ordem in Par:
            Rotulo = "A" if Ordem == 0 else "B"
            Saida = Path(Tmp) / f"saida_{Rotulo}.exe"
            Build, Run = MedirBuildRun(Forjas[Rotulo], Fonte, Saida, Tmp, Opcoes)
            Coletas[Rotulo]["build"].append(Build)
            Coletas[Rotulo]["run"].append(Run)
    Resumo = {}
    for Metrica in ("build", "run"):
        Mediana_A = statistics.median(Coletas["A"][Metrica])
        Mediana_B = statistics.median(Coletas["B"][Metrica])
        Resumo[Metrica] = {"A": round(Mediana_A, 3), "B": round(Mediana_B, 3), "razao": round(Mediana_A / Mediana_B, 4)}
    return Resumo


def CopiarFontes(Tmp):
    """Copia o corpus, a copia do forja atual e a fixture de lib para a pasta de trabalho (o build fica fora do repo)."""
    shutil.copyfile(Corpus, Path(Tmp) / Corpus.name)
    ForjaAtual = Path(Tmp) / "forja_atual.exe"
    shutil.copyfile(ForjaAtualFonte, ForjaAtual)
    return ForjaAtual


def Principal(Argumentos):
    Parser = argparse.ArgumentParser(description="custo do forja: atual x m6c (P92a)")
    Parser.add_argument("--pares", type=int, default=9)
    Parser.add_argument("--simbolos", action="store_true")
    Parser.add_argument("--lib", action="store_true")
    Opcoes = Parser.parse_args(Argumentos)
    if not ForjaM6c.is_file():
        print("falta a copia do fecho M6c: " + str(ForjaM6c))
        return 2
    with tempfile.TemporaryDirectory() as Tmp:
        ForjaAtual = CopiarFontes(Tmp)
        Fonte = Path(Tmp) / Corpus.name
        if Opcoes.simbolos:
            Sim = MedirAbba(ForjaAtual, ForjaAtual, Opcoes.pares, Fonte, Tmp, ["--simbolos", "sim"])
            Nao = MedirAbba(ForjaAtual, ForjaAtual, Opcoes.pares, Fonte, Tmp, ["--simbolos", "nao"])
            Razao = Sim["build"]["A"] / Nao["build"]["A"]
            print(f"simbolos sim/nao (build): {Razao:.4f}x  gate <= {GateSimbolos}x: {'OK' if Razao <= GateSimbolos else 'ESTOUROU'}")
            return 0 if Razao <= GateSimbolos else 1
        if Opcoes.lib:
            shutil.copyfile(FixtureLib, Path(Tmp) / FixtureLib.name)
            shutil.copyfile(PastaM6d / "libteste" / "libteste_a.a", Path(Tmp) / "libteste_a.a")
            Dev = MedirAbba(ForjaAtual, ForjaAtual, Opcoes.pares, Path(Tmp) / FixtureLib.name, Tmp, [])
            Rel = MedirAbba(ForjaAtual, ForjaAtual, Opcoes.pares, Path(Tmp) / FixtureLib.name, Tmp, ["--release"])
            print(f"lib lt_bench(200000) run dev/release (so relato): {Dev['run']['A'] / Rel['run']['A']:.3f}x")
            return 0
        Resumo = MedirAbba(ForjaAtual, ForjaM6c, Opcoes.pares, Fonte, Tmp, [])
        for Metrica in ("build", "run"):
            Dados = Resumo[Metrica]
            Gate = "OK" if Dados["razao"] <= GateDev else "ESTOUROU"
            print(f"{Metrica} dev: atual {Dados['A']}s x m6c {Dados['B']}s = {Dados['razao']}x (gate <= {GateDev}x: {Gate})")
        return 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
