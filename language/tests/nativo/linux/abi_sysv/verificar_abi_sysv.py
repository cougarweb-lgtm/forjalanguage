#!/usr/bin/env python3
"""verificar_abi_sysv.py - bateria ABI SysV no call site do foreign (M5b P37c): 60 assinaturas dev e release + fixture de valores vivos.

Objetivo: cada casos/sysv_NN_<nome>.fj (chamada pelo foreign na lib libabi_sysv.a) tem que imprimir EXATAMENTE a saida da mesma chamada
em C compilada no alpine-build (casos/oraculo_sysv.c, um caso por indice). vivos/vivos.fj tem que bater com vivos/vivos.esperado.
Fluxo: regenera os casos (gerar_abi_sysv.Gerar), compila o oraculo no WSL, faz `forja build --target x86_64-linux` (dev e --release),
roda todos os ELFs e o oraculo num lote (wsl_lote, uma chamada do wsl.exe) e compara stdout com saida 0.
Pre-requisito: construir_abi_sysv.sh (as libs casos/vendor/linux/libabi_sysv.a e vivos/vendor/linux/libvivos.a).
Uso: python -I verificar_abi_sysv.py --forja <copia do forja.exe> [--jobs N] [--so <trecho do nome>]
Verificacao: python -I verificar_abi_sysv.py --forja <copia do forja.exe>   (exit 0 = tudo verde)
Versao: 0.1.0
Changelog: 0.1.0 (15/11/2026) M5b P37c - primeira versao (60 assinaturas x dev/release contra o oraculo C do alpine; vivos).
"""
import argparse
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
sys.path.insert(0, str(AQUI.parent))  # tests\nativo\linux: wsl_lote
import gerar_abi_sysv  # noqa: E402
import wsl_lote  # noqa: E402

VersaoVerificador = "0.1.0"
ALVO = "x86_64-linux"
MODOS = ("dev", "release")
TEMPO_BUILD = 120
TEMPO_GCC = 120
PASTA_VIVOS = AQUI / "vivos"
NOME_VIVOS = "vivos"


def ComandoDeConstruir(Forja: Path, Fonte: Path, Modo: str, Saida: Path) -> list:
    return [str(Forja), "build", "--target", ALVO] + (["--release"] if Modo == "release" else []) + [str(Fonte), "-o", str(Saida)]


def Construir(Forja: Path, Fonte: Path, Modo: str, Saida: Path) -> str | None:
    Processo = subprocess.run(ComandoDeConstruir(Forja, Fonte, Modo, Saida), capture_output=True, timeout=TEMPO_BUILD, stdin=subprocess.DEVNULL)
    if Processo.returncode != 0 or not Saida.is_file():
        return f"build {Modo} exit {Processo.returncode}: " + (Processo.stdout + Processo.stderr).decode("utf-8", "replace")[:300]
    return None


def CompilarOraculo(Pasta: Path) -> Path:
    """gcc do alpine-build sobre casos/oraculo_sysv.c (que inclui assinaturas.c): o C e o esperado."""
    Elf = Pasta / "oraculo_sysv.elf"
    Comando = ["gcc", "-std=gnu11", "-O0", "-o", wsl_lote.CaminhoWsl(Elf), wsl_lote.CaminhoWsl(gerar_abi_sysv.ORACULO_C)]
    Processo = wsl_lote.Wsl(Comando, TEMPO_GCC)
    if Processo.returncode != 0 or not Elf.is_file():
        raise RuntimeError("oraculo C nao compilou: " + wsl_lote.TextoDoWsl(Processo.stderr)[:400])
    return Elf


def Casos(Trecho: str | None) -> list:
    """Lista de (nome, fonte .fj, indice do oraculo ou None para o .esperado de vivos)."""
    Saida = [(Nome, gerar_abi_sysv.PASTA_CASOS / f"{Nome}.fj", Numero) for Numero, Nome in enumerate(gerar_abi_sysv.NomesDosCasos())]
    Saida.append((NOME_VIVOS, PASTA_VIVOS / "vivos.fj", None))
    return [C for C in Saida if not Trecho or Trecho in C[0]]


def ConstruirTodos(Forja: Path, Lista: list, Pasta: Path, Jobs: int, Modos: tuple) -> tuple:
    """Devolve ({(nome, modo): Elf}, {(nome, modo): erro})."""
    Elfs, Erros = {}, {}
    Tarefas = [(Nome, Fonte, Modo) for Nome, Fonte, _Indice in Lista for Modo in Modos]

    def Fazer(Tarefa):
        Nome, Fonte, Modo = Tarefa
        Saida = Pasta / f"{Nome}_{Modo}.elf"
        return Tarefa, Saida, Construir(Forja, Fonte, Modo, Saida)

    with ThreadPoolExecutor(max_workers=Jobs) as Piscina:
        for Tarefa, Saida, Erro in Piscina.map(Fazer, Tarefas):
            if Erro:
                Erros[(Tarefa[0], Tarefa[2])] = Erro
            else:
                Elfs[(Tarefa[0], Tarefa[2])] = Saida
    return Elfs, Erros


def EsperadoDoVivos() -> bytes:
    return (PASTA_VIVOS / "vivos.esperado").read_bytes()


def RodarLote(Elfs: dict, Oraculo: Path, Lista: list, Pasta: Path) -> dict:
    """Um wsl.exe para tudo: os ELFs de forja e os oraculos (um por indice). Devolve {chave: Resultado}."""
    Programas = [wsl_lote.Programa(f"oraculo/{Indice}", Oraculo, Args=[str(Indice)]) for _Nome, _Fonte, Indice in Lista if Indice is not None]
    Programas += [wsl_lote.Programa(f"{Nome}/{Modo}", Elf) for (Nome, Modo), Elf in Elfs.items()]
    Resultados = wsl_lote.ExecutarLote(Programas, "abi_sysv")
    return {R.Nome: R for R in Resultados}


def AvaliarCaso(Nome: str, Modo: str, Indice, Resultados: dict) -> str | None:
    """None = passou; texto = motivo da falha. Oraculo (sysv) ou .esperado (vivos)."""
    R = Resultados.get(f"{Nome}/{Modo}")
    if R is None:
        return "sem resultado do lote"
    if R.Exit != 0:
        return f"exit {R.Exit} (esperado 0): " + R.Stderr.decode("utf-8", "replace")[:200]
    if Indice is None:
        Esperado = EsperadoDoVivos()
    else:
        Oraculo = Resultados.get(f"oraculo/{Indice}")
        if Oraculo is None or Oraculo.Exit != 0 or not Oraculo.Stdout:
            return "oraculo C nao rodou"
        Esperado = Oraculo.Stdout
    if R.Stdout != Esperado:
        return f"stdout diverge: obtido {R.Stdout[:120]!r} esperado {Esperado[:120]!r}"
    return None


def Rodar(Forja: Path, Trecho: str | None, Jobs: int, Modos: tuple) -> tuple:
    gerar_abi_sysv.Gerar()
    Lista = Casos(Trecho)
    Pasta = Path(tempfile.gettempdir()) / "forja_abi_sysv"
    Pasta.mkdir(parents=True, exist_ok=True)
    Oraculo = CompilarOraculo(Pasta)
    Elfs, Erros = ConstruirTodos(Forja, Lista, Pasta, Jobs, Modos)
    Resultados = RodarLote(Elfs, Oraculo, Lista, Pasta)
    Falhas = []
    for Nome, _Fonte, Indice in Lista:
        for Modo in Modos:
            Motivo = Erros.get((Nome, Modo)) or AvaliarCaso(Nome, Modo, Indice, Resultados)
            if Motivo:
                Falhas.append((Nome, Modo, Motivo))
    return Falhas, Lista


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Bateria ABI SysV 60/60 (dev e release) + vivos, contra o oraculo C do alpine-build.")
    Analisador.add_argument("--forja", required=True, help="copia do forja.exe")
    Analisador.add_argument("--jobs", type=int, default=4, help="builds em paralelo")
    Analisador.add_argument("--so", help="so os casos cujo nome contem este trecho")
    Argumentos = Analisador.parse_args()
    if not wsl_lote.DistroDisponivel():
        print("abi_sysv: PULADA (WSL/distro ausente)")
        return 0
    Falhas, Lista = Rodar(Path(Argumentos.forja).resolve(), Argumentos.so, Argumentos.jobs, MODOS)
    for Nome, Modo, Motivo in Falhas:
        print(f"FALHOU {Nome} {Modo}: {Motivo}")
    Sysv = [C for C in Lista if C[2] is not None]
    for Modo in MODOS:
        Boas = sum(1 for Nome, _F, _I in Sysv if not any(F[0] == Nome and F[1] == Modo for F in Falhas))
        print(f"abi_sysv {Modo}: {Boas}/{len(Sysv)}")
    if any(C[0] == NOME_VIVOS for C in Lista):
        VivosOk = all(not any(F[0] == NOME_VIVOS and F[1] == Modo for F in Falhas) for Modo in MODOS)
        print(f"vivos dev+release: {'verde' if VivosOk else 'FALHOU'}")
    return 0 if not Falhas else 1


if __name__ == "__main__":
    sys.exit(Principal())
