#!/usr/bin/env python3
"""verificar_run_test.py - verifica `forja run` e `forja test` no alvo wasm32-wasip1 (0.33.0, pelo Node) contra o x64 nativo.

Cada fixture de teste\\ (t_*.fj: tem `test` e main; r_*.fj: so main) roda duas vezes com o mesmo forja.exe:
(a) ORACULO: forja <comando> [opcoes] <fonte>                       (x64 nativo)
(b) CANDIDATO: forja <comando> --target wasm32-wasip1 [opcoes] <fonte>   (o executor Node embutido)
Casos: t_* em `test`, `test --json` e `test --filtro ok`; r_* em `run`; todos em dev e release.
Compara: stdout (CRLF -> LF), exit e, no stderr, as linhas "  expect falhou:" e a tupla (codigo, arquivo, linha, causa) de cada JSON de falha
(o JSON do wasm nao leva pilha nem explicacao, como o dos demais R-codes do alvo). Qualquer diferenca reprova.
Cobre ainda: filtro maior que 191 bytes e `forja test <modulo>` recusados com X0003 no wasm; `run -- args` (0.34.0) compara os argumentos do programa.

Uso:   python verificar_run_test.py [--forja <forja.exe>] [--modos dev,release] [--jobs N] [--timeout-seg N]
Saida: uma linha por caso; no fim o resumo. Exit 0 so com tudo ok.
Versao: 1.1.0
Changelog: 1.1.0 - run com argumentos do programa (fixture r_args, 0.34.0). 1.0.0 - primeira versao (0.33.0).
Verificacao: python language/tests/nativo/wasm/verificar_run_test.py --forja language/compiler/forja.exe
"""
import argparse
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LANGUAGE = AQUI.parents[2]
FIXTURES = AQUI / "teste"
FORJA_PADRAO = LANGUAGE / "compiler" / "forja.exe"
ALVO_WASM = "wasm32-wasip1"
TEMPO_PADRAO = 180
FILTRO_LONGO = "x" * 192


ARGUMENTOS_DO_PROGRAMA = ["um", "dois tres", "", "açaí", "--release", "a\\b\\"]


def NovoCaso(Fonte: Path, Comando: str, Extra: list, Modo: str, Programa: list = None) -> dict:
    return {"Fonte": Fonte, "Comando": Comando, "Extra": Extra, "Modo": Modo, "Programa": Programa or []}


def MontarCasos(Modos: list) -> list:
    Lista = []
    for Fonte in sorted(FIXTURES.glob("*.fj")):
        for Modo in Modos:
            if Fonte.name.startswith("t_"):
                Lista.append(NovoCaso(Fonte, "test", [], Modo))
                Lista.append(NovoCaso(Fonte, "test", ["--json"], Modo))
                Lista.append(NovoCaso(Fonte, "test", ["--filtro", "ok"], Modo))
            else:
                Lista.append(NovoCaso(Fonte, "run", [], Modo))
                if Fonte.name == "r_args.fj":  # 0.34.0: argumentos do programa (espaco, vazio, acento, --release, barra no fim)
                    Lista.append(NovoCaso(Fonte, "run", [], Modo, ARGUMENTOS_DO_PROGRAMA))
    return Lista


def MontarArgumentos(Forja: Path, Caso: dict, EhWasm: bool) -> list:
    Argumentos = [str(Forja), Caso["Comando"]]
    if EhWasm:
        Argumentos += ["--target", ALVO_WASM]
    if Caso["Modo"] == "release":
        Argumentos.append("--release")
    Argumentos = Argumentos + Caso["Extra"] + [Caso["Fonte"].name]
    if Caso["Programa"]:
        Argumentos += ["--"] + Caso["Programa"]
    return Argumentos


def Rodar(Argumentos: list, Pasta: Path, Tempo: int) -> tuple:
    Processo = subprocess.run(Argumentos, cwd=str(Pasta), capture_output=True, timeout=Tempo)
    return Processo.stdout.replace(b"\r\n", b"\n"), Processo.stderr.replace(b"\r\n", b"\n"), Processo.returncode


def ResumoStderr(Stderr: bytes) -> list:
    """Linhas de expect e (codigo, arquivo, linha, causa) de cada JSON de falha; o resto do stderr (pilha, explicacao) fica de fora."""
    Saida = []
    for Linha in Stderr.decode("utf-8", "replace").split("\n"):
        if Linha.startswith("  expect falhou:"):
            Saida.append(Linha)
        elif Linha.startswith("{\"codigo\""):
            try:
                Registro = json.loads(Linha)
            except ValueError:
                Saida.append("JSON invalido: " + Linha[:80])
                continue
            Saida.append(f"{Registro.get('codigo')}|{Registro.get('arquivo')}|{Registro.get('linha')}|{Registro.get('causa')}")
    return Saida


def Verificar(Forja: Path, Caso: dict, Tempo: int) -> tuple:
    Nome = f"{Caso['Fonte'].name} {Caso['Comando']} {' '.join(Caso['Extra'])} [{Caso['Modo']}]{' com args' if Caso['Programa'] else ''}".replace("  ", " ")
    Pasta = Caso["Fonte"].parent
    try:
        Esperado = Rodar(MontarArgumentos(Forja, Caso, False), Pasta, Tempo)
        Obtido = Rodar(MontarArgumentos(Forja, Caso, True), Pasta, Tempo)
    except subprocess.TimeoutExpired:
        return Nome, False, "estourou o tempo"
    if Esperado[0] != Obtido[0]:
        return Nome, False, f"stdout diverge:\n  x64:  {Esperado[0][:400]!r}\n  wasm: {Obtido[0][:400]!r}"
    if Esperado[2] != Obtido[2]:
        return Nome, False, f"exit diverge: x64 {Esperado[2]} wasm {Obtido[2]} (stderr wasm: {Obtido[1][:300]!r})"
    if ResumoStderr(Esperado[1]) != ResumoStderr(Obtido[1]):
        return Nome, False, f"stderr diverge:\n  x64:  {ResumoStderr(Esperado[1])}\n  wasm: {ResumoStderr(Obtido[1])}"
    return Nome, True, ""


def VerificarRecusas(Forja: Path, Tempo: int) -> list:
    """O que o wasm recusa com X0003 (exit 2): filtro > 191 bytes e argumentos do programa."""
    Fonte = FIXTURES / "t_so_ok.fj"
    Casos = [
        ("filtro de 192 bytes", ["test", "--target", ALVO_WASM, "--filtro", FILTRO_LONGO, Fonte.name]),
        ("forja test de modulo", ["test", "--target", ALVO_WASM, "std:math"]),
    ]
    Resultados = []
    for Nome, Argumentos in Casos:
        Processo = subprocess.run([str(Forja)] + Argumentos, cwd=str(FIXTURES), capture_output=True, timeout=Tempo)
        Ok = (Processo.returncode == 2) and (b"X0003" in Processo.stdout + Processo.stderr)
        Motivo = "" if Ok else f"esperado X0003/exit 2, veio exit {Processo.returncode}: {(Processo.stdout + Processo.stderr)[:200]!r}"
        Resultados.append(("recusa: " + Nome, Ok, Motivo))
    return Resultados


def Principal() -> int:
    Analisador = argparse.ArgumentParser()
    Analisador.add_argument("--forja", default=str(FORJA_PADRAO))
    Analisador.add_argument("--modos", default="dev,release")
    Analisador.add_argument("--jobs", type=int, default=4)
    Analisador.add_argument("--timeout-seg", type=int, default=TEMPO_PADRAO)
    Opcoes = Analisador.parse_args()
    Forja = Path(Opcoes.forja).resolve()
    Casos = MontarCasos(Opcoes.modos.split(","))
    # O exe temporario do forja run/test e o mesmo para o mesmo fonte, comando e modo: os casos de um grupo rodam em fila, os grupos em paralelo
    Grupos = {}
    for Caso in Casos:
        Grupos.setdefault((Caso["Fonte"], Caso["Modo"]), []).append(Caso)
    with ThreadPoolExecutor(max_workers=Opcoes.jobs) as Pool:
        PorGrupo = Pool.map(lambda Grupo: [Verificar(Forja, Caso, Opcoes.timeout_seg) for Caso in Grupo], list(Grupos.values()))
        Resultados = [Resultado for Lista in PorGrupo for Resultado in Lista]
    Resultados += VerificarRecusas(Forja, Opcoes.timeout_seg)
    Falhas = 0
    for Nome, Ok, Motivo in Resultados:
        print(("ok      " if Ok else "DIVERGE ") + Nome)
        if not Ok:
            Falhas += 1
            print("   " + Motivo)
    print(f"{len(Resultados)} casos, {Falhas} divergem")
    return 0 if Falhas == 0 else 1


if __name__ == "__main__":
    sys.exit(Principal())
