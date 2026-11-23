# -*- coding: utf-8 -*-
"""Confere o console UTF-8 do runtime nativo da Forja (passo extra do M1).

Gera o examples\\01_ola.fj com `forja build --backend x64`, compila o oraculo conferir_console.c
com gcc e roda. O oraculo cria um console proprio na pagina 850 e confere:
  - ola nativo no console: tela "Olá, Forja!", "42", "21" e pagina de saida de volta a 850 depois do exit;
  - ola nativo com stdout/stderr em pipe: bytes UTF-8 de sempre (0xC3 0xA1) e pagina intacta.
Sem console utilizavel na sessao o oraculo sai com 2 e este driver devolve o diagnostico dele.

Uso:
    python -I conferir_console.py [--forja <exe>]

Versao: 1.1.0
Changelog: 1.1.0 - 0.21.0 F4 passo 3: sai o controle negativo com o backend C (aposentado); o oraculo recebe um exe so.
           1.0.1 - M4 P25b: o backend C leva --backend c explicito (o padrao do dev passou a ser x64).
           1.0.0 - primeira versao: driver Python do oraculo conferir_console.c.
Verificacao: python -I conferir_console.py --forja ..\\..\\..\\compiler\\forja.exe
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaLanguage = PastaAqui.parents[2]
Ola = PastaLanguage.parent / "examples" / "01_ola.fj"
ForjaPadrao = PastaLanguage / "compiler" / "forja.exe"
RegexResumo = re.compile(r"checagens=(\d+) ok=(\d+) falhas=(\d+)")
TempoSegundos = 120


def CompilarOraculo(Pasta):
    Saida = Pasta / "conferir_console.exe"
    Resultado = subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                                str(PastaAqui / "conferir_console.c"), "-o", str(Saida)],
                               capture_output=True, text=True)
    if Resultado.returncode != 0:
        raise SystemExit(f"X0001 gcc recusou conferir_console.c:\n{Resultado.stderr}")
    return Saida


def Construir(Forja, Exe):
    # --backend explicito: o unico backend e o x64 desde a 0.21.0 (o C foi aposentado).
    Comando = [str(Forja), "build", "--backend", "x64", str(Ola), "-o", str(Exe)]
    Resultado = subprocess.run(Comando, capture_output=True, text=True)
    if Resultado.returncode != 0:
        raise SystemExit(f"X0001 build --backend x64 do 01_ola falhou ({Resultado.returncode}):\n"
                         f"{Resultado.stdout}{Resultado.stderr}")


def Principal():
    Parser = argparse.ArgumentParser(description="Confere o console UTF-8 do runtime nativo da Forja.")
    Parser.add_argument("--forja", default=str(ForjaPadrao), help="exe da Forja (padrao: compiler\\forja.exe)")
    Args = Parser.parse_args()

    Pasta = Path(tempfile.mkdtemp(prefix="conferir_console_"))
    try:
        Oraculo = CompilarOraculo(Pasta)
        ExeNativo = Pasta / "ola_x64.exe"
        Construir(Args.forja, ExeNativo)
        Resultado = subprocess.run([str(Oraculo), str(ExeNativo)], capture_output=True, text=True,
                                   timeout=TempoSegundos)
        print(Resultado.stdout, end="")
        print(Resultado.stderr, end="")
        Resumo = RegexResumo.search(Resultado.stdout)
        if Resultado.returncode == 2 or not Resumo:
            print(f"conferir_console: sem resultado (oraculo saiu {Resultado.returncode})")
            return 1
        return 0 if Resultado.returncode == 0 and int(Resumo.group(3)) == 0 else 1
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(Principal())
