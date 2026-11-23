"""Prova W0 do par endereco_de/ponteiro_de (plano_backend_wasm.txt, item 13).

Versao: 1.0.0
Changelog: 1.0.0 - fixture ptr_par_declarado_no_app.fj: a sema aceita (check sai 0) e o build recusa com X0101,
           ou seja, um app que declara o par NAO o resolve. Quando o backend wasm existir, o mesmo build com
           --target wasm32-wasi deve manter o X0101 (so a pasta do runtime resolve o par).
Reversao: apagar este arquivo e a fixture.
Verificacao: python verificar_par_ponteiro.py [--forja <forja.exe>]
"""

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

PASTA = Path(__file__).resolve().parent
FIXTURE = PASTA / "ptr_par_declarado_no_app.fj"
FORJA_PADRAO = PASTA.parents[2] / "compiler" / "forja.exe"
PAR = ("endereco_de", "ponteiro_de")
TIMEOUT_SEG = 60


def ExecutarForja(Forja: Path, Argumentos: list[str]) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(Forja), *Argumentos], capture_output=True, text=True, timeout=TIMEOUT_SEG
    )


def LerDiagnostico(Saida: str) -> dict:
    for Linha in reversed(Saida.strip().splitlines()):
        if Linha.startswith("{"):
            return json.loads(Linha)
    return {}


def ConferirCheck(Forja: Path) -> list[str]:
    Resultado = ExecutarForja(Forja, ["check", str(FIXTURE)])
    if Resultado.returncode != 0:
        return [f"check deveria sair 0 e saiu {Resultado.returncode}: {Resultado.stdout.strip()[:200]}"]
    return []


def ConferirBuild(Forja: Path, Release: bool) -> list[str]:
    Rotulo = "release" if Release else "dev"
    with tempfile.TemporaryDirectory() as Pasta:
        Argumentos = ["build", "--backend", "x64", str(FIXTURE), "-o", str(Path(Pasta) / "x.exe")]
        if Release:
            Argumentos.insert(1, "--release")
        Resultado = ExecutarForja(Forja, Argumentos)
    Diagnostico = LerDiagnostico(Resultado.stdout)
    Erros = []
    if Resultado.returncode != 1:
        Erros.append(f"build {Rotulo}: exit esperado 1, veio {Resultado.returncode}")
    if Diagnostico.get("codigo") != "X0101":
        Erros.append(f"build {Rotulo}: codigo esperado X0101, veio {Diagnostico.get('codigo')}")
    if Diagnostico.get("simbolo") not in PAR:
        Erros.append(f"build {Rotulo}: simbolo fora do par: {Diagnostico.get('simbolo')}")
    return Erros


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Prova W0 do par de ponteiro")
    Analisador.add_argument("--forja", type=Path, default=FORJA_PADRAO)
    Opcoes = Analisador.parse_args()
    if not Opcoes.forja.exists() or not FIXTURE.exists():
        print(f"ERRO: faltam {Opcoes.forja} ou {FIXTURE}", file=sys.stderr)
        return 2
    Erros = ConferirCheck(Opcoes.forja) + ConferirBuild(Opcoes.forja, False) + ConferirBuild(Opcoes.forja, True)
    for Erro in Erros:
        print(f"FALHOU: {Erro}")
    print("OK: check=0, build dev e release = X0101 (par fora do runtime e recusado)" if not Erros else "REPROVADO")
    return 1 if Erros else 0


if __name__ == "__main__":
    sys.exit(Principal())
