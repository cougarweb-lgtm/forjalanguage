#!/usr/bin/env python3
"""verificar_abi_sysv_cb.py - bateria ABI SysV de CALLBACK (M5b P38-0): 60 callbacks dev e release + fixture vivos_cb.

Objetivo: cada casos_cb/cbk_NN_<nome>.fj (proc Forja passado a cb_call_NN, lib libabi_sysv_cb.a) tem que imprimir EXATAMENTE a saida
de cbk_NN no oraculo C do alpine-build (casos_cb/oraculo_cb.c, um caso por indice). vivos_cb/vivos_cb.fj tem que bater com o oraculo
vivos_cb/oraculo_vivos_cb.c; se vivos_cb/vivos_cb.esperado existir, o oraculo tem que bater com ele tambem.
Fluxo: regenera os casos (gerar_abi_sysv_cb.Gerar), compila os dois oraculos no WSL, faz `forja build --target x86_64-linux` (dev e
--release), roda todos os ELFs e os oraculos num lote (wsl_lote, uma chamada do wsl.exe) e compara stdout com saida 0.
Modo --check: so o frontend (`forja check`) de cada .fj, sem build e sem WSL.
Pre-requisito: construir_abi_sysv_cb.sh (libs casos_cb/vendor/linux/libabi_sysv_cb.a e vivos_cb/vendor/linux/libvivos_cb.a).
Uso: python -I verificar_abi_sysv_cb.py --forja <copia do forja.exe> [--jobs N] [--so <trecho>] [--check] [--gravar-esperado]
Verificacao: python -I verificar_abi_sysv_cb.py --forja <copia do forja.exe>   (exit 0 = tudo verde)
Versao: 0.1.0
Changelog: 0.1.0 (15/11/2026) M5b P38-0 - primeira versao (callback 60 x dev/release contra o oraculo C do alpine; vivos_cb; --check).
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
import gerar_abi_sysv_cb  # noqa: E402
import verificar_abi_sysv as Chamada  # noqa: E402  (ComandoDeConstruir, ConstruirTodos, ALVO, MODOS)
import wsl_lote  # noqa: E402

VersaoVerificador = "0.1.0"
MODOS = Chamada.MODOS
TEMPO_GCC = 120
TEMPO_CHECK = 60
PASTA_VIVOS = AQUI / "vivos_cb"
ORACULO_VIVOS = PASTA_VIVOS / "oraculo_vivos_cb.c"
ESPERADO_VIVOS = PASTA_VIVOS / "vivos_cb.esperado"
NOME_VIVOS = "vivos_cb"
SUITE = "abi_sysv_cb"


def Casos(Trecho: str | None) -> list:
    """Lista de (nome, fonte .fj, indice do oraculo ou None para a fixture vivos_cb)."""
    Saida = [(Nome, gerar_abi_sysv_cb.PASTA_CASOS / f"{Nome}.fj", Numero)
             for Numero, Nome in enumerate(gerar_abi_sysv_cb.NomesDosCasos())]
    Saida.append((NOME_VIVOS, PASTA_VIVOS / f"{NOME_VIVOS}.fj", None))
    return [C for C in Saida if not Trecho or Trecho in C[0]]


def CompilarOraculo(Fonte: Path, Saida: Path) -> Path:
    """gcc do alpine-build sobre um oraculo C (que inclui o C da lib): o C e o esperado."""
    Comando = ["gcc", "-std=gnu11", "-O0", "-o", wsl_lote.CaminhoWsl(Saida), wsl_lote.CaminhoWsl(Fonte)]
    Processo = wsl_lote.Wsl(Comando, TEMPO_GCC)
    if Processo.returncode != 0 or not Saida.is_file():
        raise RuntimeError(f"oraculo {Fonte.name} nao compilou: " + wsl_lote.TextoDoWsl(Processo.stderr)[:400])
    return Saida


def OraculoDe(Nome: str, Indice) -> str:
    return f"oraculo/{Indice}" if Indice is not None else f"oraculo/{NOME_VIVOS}"


def ProgramasDoLote(Oraculos: dict, Elfs: dict, Lista: list) -> list:
    """Um wsl.exe para tudo: os oraculos (um por indice + o de vivos_cb) e os ELFs da Forja."""
    Programas = [wsl_lote.Programa(f"oraculo/{Indice}", Oraculos["cb"], Args=[str(Indice)])
                 for _Nome, _Fonte, Indice in Lista if Indice is not None]
    Programas.append(wsl_lote.Programa(f"oraculo/{NOME_VIVOS}", Oraculos["vivos"]))
    Programas += [wsl_lote.Programa(f"{Nome}/{Modo}", Elf) for (Nome, Modo), Elf in Elfs.items()]
    return Programas


def AvaliarCaso(Nome: str, Modo: str, Indice, Resultados: dict) -> str | None:
    """None = passou; texto = motivo da falha. Compara o stdout da Forja com o do oraculo C do mesmo caso."""
    R = Resultados.get(f"{Nome}/{Modo}")
    if R is None:
        return "sem resultado do lote"
    if R.Exit != 0:
        return f"exit {R.Exit} (esperado 0): " + R.Stderr.decode("utf-8", "replace")[:200]
    Oraculo = Resultados.get(OraculoDe(Nome, Indice))
    if Oraculo is None or Oraculo.Exit != 0 or not Oraculo.Stdout:
        return "oraculo C nao rodou"
    if R.Stdout != Oraculo.Stdout:
        return f"stdout diverge: obtido {R.Stdout[:120]!r} esperado {Oraculo.Stdout[:120]!r}"
    return None


def Rodar(Forja: Path, Lista: list, Jobs: int, Modos: tuple) -> tuple:
    """Devolve (falhas [(nome, modo, motivo)], resultados {chave: Resultado})."""
    Pasta = Path(tempfile.gettempdir()) / "forja_abi_sysv_cb"
    Pasta.mkdir(parents=True, exist_ok=True)
    Oraculos = {"cb": CompilarOraculo(gerar_abi_sysv_cb.ORACULO_C, Pasta / "oraculo_cb.elf"),
                "vivos": CompilarOraculo(ORACULO_VIVOS, Pasta / "oraculo_vivos_cb.elf")}
    Elfs, Erros = Chamada.ConstruirTodos(Forja, Lista, Pasta, Jobs, Modos)
    Resultados = {R.Nome: R for R in wsl_lote.ExecutarLote(ProgramasDoLote(Oraculos, Elfs, Lista), SUITE)}
    Falhas = []
    for Nome, _Fonte, Indice in Lista:
        for Modo in Modos:
            Motivo = Erros.get((Nome, Modo)) or AvaliarCaso(Nome, Modo, Indice, Resultados)
            if Motivo:
                Falhas.append((Nome, Modo, Motivo))
    return Falhas, Resultados


def ChecarFrontend(Forja: Path, Lista: list, Jobs: int) -> list:
    """forja check de cada fonte: exit 0 e saida vazia = 0 erros de frontend. Devolve [(nome, exit, texto)]."""
    def Checar(Item):
        Nome, Fonte, _Indice = Item
        Processo = subprocess.run([str(Forja), "check", str(Fonte)], capture_output=True, timeout=TEMPO_CHECK,
                                  stdin=subprocess.DEVNULL)
        return Nome, Processo.returncode, (Processo.stdout + Processo.stderr).decode("utf-8", "replace").strip()

    with ThreadPoolExecutor(max_workers=Jobs) as Piscina:
        return list(Piscina.map(Checar, Lista))


def ImprimirCheck(Resultados: list) -> int:
    Limpos = [R for R in Resultados if R[1] == 0 and not R[2]]
    for Nome, Codigo, Texto in Resultados:
        if Codigo != 0 or Texto:
            print(f"FALHOU check {Nome} (exit {Codigo}): {Texto[:300]}")
    print(f"forja check: {len(Limpos)}/{len(Resultados)} sem erro")
    return 0 if len(Limpos) == len(Resultados) else 1


def ImprimirResultados(Falhas: list, Lista: list) -> int:
    for Nome, Modo, Motivo in Falhas:
        print(f"FALHOU {Nome} {Modo}: {Motivo}")
    Callbacks = [C for C in Lista if C[2] is not None]
    for Modo in MODOS:
        Boas = sum(1 for Nome, _F, _I in Callbacks if not any(F[0] == Nome and F[1] == Modo for F in Falhas))
        print(f"callback {Modo}: {Boas}/{len(Callbacks)}")
    if any(C[0] == NOME_VIVOS for C in Lista):
        VivosOk = all(not any(F[0] == NOME_VIVOS and F[1] == Modo for F in Falhas) for Modo in MODOS)
        print(f"vivos_cb dev+release: {'verde' if VivosOk else 'FALHOU'}")
    return 0 if not Falhas else 1


def ConferirEsperadoVivos(Resultados: dict, Gravar: bool) -> None:
    """O oraculo de vivos_cb e o esperado: grava vivos_cb.esperado (--gravar-esperado) ou compara com o arquivo ja existente."""
    Oraculo = Resultados.get(f"oraculo/{NOME_VIVOS}")
    if Oraculo is None or Oraculo.Exit != 0 or not Oraculo.Stdout:
        print("vivos_cb oraculo: NAO RODOU")
        return
    if Gravar:
        ESPERADO_VIVOS.write_bytes(Oraculo.Stdout)
        print(f"vivos_cb esperado: gravado ({len(Oraculo.Stdout)} bytes)")
    elif not ESPERADO_VIVOS.is_file():
        print("vivos_cb esperado: AUSENTE (rode com --gravar-esperado)")
    else:
        Igual = ESPERADO_VIVOS.read_bytes() == Oraculo.Stdout
        print(f"vivos_cb esperado: {'confere' if Igual else 'DIVERGE do oraculo C'}")


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Bateria ABI SysV de callback 60 x dev/release + vivos_cb, contra o oraculo C do alpine-build.")
    Analisador.add_argument("--forja", required=True, help="copia do forja.exe")
    Analisador.add_argument("--jobs", type=int, default=4, help="builds em paralelo")
    Analisador.add_argument("--so", help="so os casos cujo nome contem este trecho")
    Analisador.add_argument("--check", action="store_true", help="so forja check (frontend), sem build nem WSL")
    Analisador.add_argument("--gravar-esperado", action="store_true", help="grava vivos_cb.esperado com a saida do oraculo C")
    Argumentos = Analisador.parse_args()
    Forja = Path(Argumentos.forja).resolve()
    gerar_abi_sysv_cb.Gerar()
    Lista = Casos(Argumentos.so)
    if Argumentos.check:
        return ImprimirCheck(ChecarFrontend(Forja, Lista, Argumentos.jobs))
    if not wsl_lote.DistroDisponivel():
        print("abi_sysv_cb: PULADA (WSL/distro ausente)")
        return 0
    Falhas, Resultados = Rodar(Forja, Lista, Argumentos.jobs, MODOS)
    ConferirEsperadoVivos(Resultados, Argumentos.gravar_esperado)
    return ImprimirResultados(Falhas, Lista)


if __name__ == "__main__":
    sys.exit(Principal())
