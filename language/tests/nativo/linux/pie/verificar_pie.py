#!/usr/bin/env python3
"""verificar_pie.py - M5b P41 (--pie): fixtures de prova e de libc construidas com --pie (ELF static-pie) rodam igual ao .esperado.

Objetivo: cada fixture .fj das pastas linux\\prova (sem foreign) e linux\\libc (foreign: rcrt1.o do musl) constroi em dev e release com
--target x86_64-linux --pie e roda no alpine-build; a saida tem de ser o .esperado byte a byte e o codigo de saida, o .codigo ou .exit (padrao 0).
O ASLR nao cabe aqui (a prova e manual: 2 execucoes do mesmo ELF com a base diferente em /proc/self/maps; ver o relato do P41).
Pre-requisito: language\\third\\musl\\x86_64 (rcrt1.o, crt1.o, libc.a) e o forja.exe ao lado de language\\compiler.
Uso: python -I verificar_pie.py --forja <copia do forja.exe>   (exit 0 = tudo verde; PULADA sem WSL)
Versao: 0.1.0
Changelog: 0.1.0 (15/11/2026) M5b P41 - primeira versao (prova e libc com --pie).
"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
PASTA_LINUX = AQUI.parent
sys.path.insert(0, str(PASTA_LINUX / "abi_sysv"))  # verificar_abi_sysv (MODOS)
sys.path.insert(0, str(PASTA_LINUX))               # wsl_lote
import verificar_abi_sysv as Chamada  # noqa: E402
import wsl_lote  # noqa: E402

SUITE = "pie"
ALVO = "x86_64-linux"
TEMPO_BUILD = 600
PASTAS = [PASTA_LINUX / "prova", PASTA_LINUX / "libc"]


# Fixtures que precisam de argumento ou ambiente do lote (arquivo: caminho; saida: FORJA_PROVA): so o verificar_linux as roda.
SO_VERIFICAR_LINUX = {"arquivo", "saida"}


def CodigoSoNumerico(Fonte: Path) -> bool:
    """False se o .codigo/.exit traz texto (ex.: R0003 do panico.fj) ou a fixture precisa de args/ambiente: nao e caso do pie."""
    if Fonte.stem in SO_VERIFICAR_LINUX:
        return False
    for Extensao in (".codigo", ".exit"):
        Arquivo = Fonte.with_suffix(Extensao)
        if Arquivo.is_file() and not Arquivo.read_text().strip().isdigit():
            return False
    return True


def FixturesDasPastas() -> list:
    return [Fonte for Pasta in PASTAS for Fonte in sorted(Pasta.glob("*.fj")) if CodigoSoNumerico(Fonte)]


def ComandoDeConstruir(Forja: Path, Fonte: Path, Modo: str, Saida: Path) -> list:
    Comando = [str(Forja), "build", "--target", ALVO, "--pie"]
    return Comando + (["--release"] if Modo == "release" else []) + [str(Fonte), "-o", str(Saida)]


def ConstruirPie(Forja: Path, Fonte: Path, Modo: str, Pasta: Path):
    """Constroi o fixture com --pie; devolve (elf, erro)."""
    Saida = Pasta / f"{Fonte.parent.name}_{Fonte.stem}_{Modo}.elf"
    Processo = subprocess.run(ComandoDeConstruir(Forja, Fonte, Modo, Saida), capture_output=True, timeout=TEMPO_BUILD, stdin=subprocess.DEVNULL)
    if Processo.returncode != 0 or not Saida.is_file():
        return None, f"build {Modo} exit {Processo.returncode}: " + (Processo.stdout + Processo.stderr).decode("utf-8", "replace")[:300]
    return Saida, None


def CodigoEsperado(Fonte: Path) -> int:
    for Extensao in (".codigo", ".exit"):
        Arquivo = Fonte.with_suffix(Extensao)
        if Arquivo.is_file():
            return int(Arquivo.read_text().strip())
    return 0


def Avaliar(Esperado: bytes, CodigoEsp: int, R) -> str | None:
    if R is None:
        return "sem resultado do lote"
    if R.Sinal is not None:
        return f"morreu por sinal {R.Sinal} (stderr: {R.Stderr[:200]!r})"
    if R.Exit != CodigoEsp:
        return f"exit {R.Exit} (esperado {CodigoEsp}): {R.Stderr[:200]!r}"
    if R.Stdout != Esperado:
        return f"stdout diverge: obtido {R.Stdout[:200]!r} esperado {Esperado[:200]!r}"
    return None


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Programas Forja com --pie (P41).")
    Analisador.add_argument("--forja", required=True, help="copia do forja.exe")
    Forja = Path(Analisador.parse_args().forja).resolve()
    if not wsl_lote.DistroDisponivel():
        print("pie: PULADA (WSL/distro ausente)")
        return 0
    Pasta = Path(tempfile.gettempdir()) / "forja_pie"
    Pasta.mkdir(parents=True, exist_ok=True)
    Falhas, Programas, Esperados = [], [], {}
    for Fonte in FixturesDasPastas():
        Rotulo = f"{Fonte.parent.name}/{Fonte.stem}"
        Esperados[Rotulo] = ((Fonte.with_suffix(".esperado")).read_bytes(), CodigoEsperado(Fonte))
        for Modo in Chamada.MODOS:
            Elf, Erro = ConstruirPie(Forja, Fonte, Modo, Pasta)
            if Erro:
                Falhas.append((f"{Rotulo}/{Modo}", f"build: {Erro}"))
            else:
                Programas.append(wsl_lote.Programa(f"{Rotulo}/{Modo}", Elf))
    Resultados = {R.Nome: R for R in wsl_lote.ExecutarLote(Programas, SUITE)}
    for Programa in Programas:
        Base = Programa.Nome.rsplit("/", 1)[0]
        Esperado, Codigo = Esperados[Base]
        Motivo = Avaliar(Esperado, Codigo, Resultados.get(Programa.Nome))
        if Motivo:
            Falhas.append((Programa.Nome, Motivo))
    for Rotulo, Motivo in Falhas:
        print(f"FALHOU {Rotulo}: {Motivo}")
    Total = len(FixturesDasPastas()) * len(Chamada.MODOS)
    print(f"pie: {Total - len(Falhas)}/{Total}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Principal())
