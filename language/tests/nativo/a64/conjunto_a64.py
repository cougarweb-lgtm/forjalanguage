#!/usr/bin/env python3
"""conjunto_a64.py - lista de casos do harness verificar_a64.py (fatia A2.1).

Objetivo: so ENUMERA os casos; nao compila nem roda nada. Conjunto padrao = corpus da m1 (tests\\nativo\\m1\\*.fj e modulo\\principal.fj)
e examples\\01_ola.fj se existir (oraculo = x86_64-linux no WSL). Conjunto --minimo = fixtures de tests\\nativo\\a64\\fixtures\\*.fj que
tem .esperado ao lado (oraculo = .esperado; sem runtime Forja). Fixture sem .esperado nao entra e o selftest acusa.
Uso: importado por verificar_a64.py (sem CLI proprio).
Verificacao: python -I conjunto_a64.py --selftest
Versao: 1.0.0
Changelog: 1.0.0 (22/11/2026) A2.1 - criacao: conjunto padrao (m1 + 01_ola) e conjunto --minimo (fixtures do A1.3 com .esperado).
"""
import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LANGUAGE = AQUI.parents[2]
PASTA_FIXTURES = AQUI / "fixtures"
PASTA_M1 = LANGUAGE / "tests" / "nativo" / "m1"
EXEMPLO_OLA = LANGUAGE.parent / "examples" / "01_ola.fj"
EXTENSAO_ESPERADO = ".esperado"
GRUPO_FIXTURE = "fixture"
GRUPO_M1 = "m1"
GRUPO_EXEMPLO = "exemplo"


@dataclass
class Caso:
    Nome: str  # caminho a partir da pasta do projeto FORJA, em barra normal (e o que o --filtro procura)
    Fonte: Path
    Grupo: str
    Esperado: Path | None = None  # so as fixtures: o .esperado ao lado


def NomeDoCaso(Arquivo: Path) -> str:
    return Arquivo.relative_to(LANGUAGE.parent).as_posix()


def CasosFixtures() -> list:
    """Fixtures do A1.3: cada .fj com .esperado ao lado (sem .esperado, o .fj nao entra)."""
    Casos = []
    for Fonte in sorted(PASTA_FIXTURES.glob("*.fj")):
        Esperado = Fonte.with_suffix(EXTENSAO_ESPERADO)
        if Esperado.is_file():
            Casos.append(Caso(NomeDoCaso(Fonte), Fonte, GRUPO_FIXTURE, Esperado))
    return Casos


def CasosPadrao() -> list:
    """Corpus da m1 (os .fj soltos e modulo\\principal.fj) e examples\\01_ola.fj se existir: oraculo = x86_64-linux."""
    Casos = [Caso(NomeDoCaso(Fonte), Fonte, GRUPO_M1) for Fonte in sorted(PASTA_M1.glob("*.fj"))]
    Principal = PASTA_M1 / "modulo" / "principal.fj"
    if Principal.is_file():
        Casos.append(Caso(NomeDoCaso(Principal), Principal, GRUPO_M1))
    if EXEMPLO_OLA.is_file():
        Casos.append(Caso(NomeDoCaso(EXEMPLO_OLA), EXEMPLO_OLA, GRUPO_EXEMPLO))
    return Casos


def ConjuntoDo(Minimo: bool) -> list:
    return CasosFixtures() if Minimo else CasosPadrao()


def FixturesSemEsperado() -> list:
    return [Fonte.name for Fonte in sorted(PASTA_FIXTURES.glob("*.fj")) if not Fonte.with_suffix(EXTENSAO_ESPERADO).is_file()]


def Selftest() -> int:
    Casos = CasosPadrao() + CasosFixtures()
    Falhas = [f"fixture sem {EXTENSAO_ESPERADO}: {Nome}" for Nome in FixturesSemEsperado()]
    Falhas += [f"caso sem fonte: {Caso_.Nome}" for Caso_ in Casos if not Caso_.Fonte.is_file()]
    Nomes = [Caso_.Nome for Caso_ in Casos]
    Falhas += [f"caso duplicado: {Nome}" for Nome in sorted({N for N in Nomes if Nomes.count(N) > 1})]
    for Falha in Falhas:
        print("FALHA", Falha)
    print("conjunto_a64 selftest:", "VERDE" if not Falhas else f"{len(Falhas)} falha(s)")
    return 1 if Falhas else 0


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Lista de casos do verificar_a64 (so enumera; nao compila).")
    Analisador.add_argument("--selftest", action="store_true")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        return Selftest()
    Analisador.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(Principal())
