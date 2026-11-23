#!/usr/bin/env python3
"""verificar_nomes.py - nome, tipo e casa dos locais nomeados na BIR (M6 P65a, D-M6-13).

Versao: 0.1.0
Changelog: 0.1.0 - P65a: --debug-code locais das 6 fixtures de gdb\\*.fj (+ mods\\geo.fj) contra um oraculo fixo escrito aqui (o que o fonte declara).
Uso: python verificar_nomes.py --forja <forja.exe> [--alvo windows|linux|ambos] [--corpus <arquivo.fj>]
  --corpus: so conta "sem_casa" do programa (sem oraculo de nomes) e imprime o total de locais nomeados.
Saida: 0 tudo PASSOU; 1 alguma conferencia FALHOU.
Conferencia por proc do usuario: o conjunto {nome: tipo} e igual ao do fonte (logo nenhum temporario, id >= Decl.nlocais, entra), os ids sao 0..n-1 sem buraco,
sem_casa = 0, e param agregado tem indireto=1 (nenhum nas fixtures: o controle fica no CAMPO indireto de cada linha, que tem de ser 0 nelas).
"""
import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent
FIXTURES = RAIZ / "gdb"
# fixture -> {sufixo do proc: {nome: nome do tipo}}; o nome do tipo e o de BirNomeDoTipo (int, f64, bool, struct, str...).
ORACULO = {
    "break_arquivo": {"soma": {"a": "int", "b": "int", "r": "int"}, "main": {}},
    "break_modulo": {"area": {"lado": "int"}, "main": {}},
    "bt3": {"nivel_c": {"x": "int", "y": "int"}, "nivel_b": {"x": "int"}, "nivel_a": {"x": "int"}, "main": {}},
    "info_locals": {"main": {"n": "int", "r": "f64", "ok": "bool"}},
    "print_campo": {"main": {"p": "struct"}},
    "print_str": {"main": {"texto": "str"}},
}
REGEX_LOCAL = re.compile(r"^  local (\d+) (\S+) tipo=(\S+) tid=(\d+) casa=(slot|vreg)(\d+) indireto=([01])$")
REGEX_SEM_CASA = re.compile(r"^  sem_casa=(\d+)$")


def LerLocais(Forja: Path, Fonte: Path, Alvo: str, Tmp: Path) -> dict:
    """Roda o build com --debug-code locais e devolve {funcao: ([(id, nome, tipo, casa, indireto)], sem_casa)}."""
    Exe = Tmp / (Fonte.stem + "_" + Alvo + ".exe")
    Comando = [Forja.as_posix(), "build"]
    if Alvo == "linux":
        Comando += ["--target", "x86_64-linux"]
    Comando += [Fonte.as_posix(), "-o", Exe.as_posix(), "--debug-code", "locais"]
    Proc = subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
    Txt = Path(str(Exe) + ".debug.txt")
    if not Txt.exists():
        raise RuntimeError(f"sem .debug.txt: {(Proc.stdout + Proc.stderr)[-300:]}")
    return AnalisarTexto(Txt.read_text(encoding="utf-8"))


def AnalisarTexto(Texto: str) -> dict:
    Funcoes = {}
    Atual = None
    for Linha in Texto.splitlines():
        if Linha.startswith("funcao "):
            Atual = Linha[7:].strip()
            Funcoes[Atual] = ([], 0)
            continue
        if Atual is None:
            continue
        L = REGEX_LOCAL.match(Linha)
        if L:
            Funcoes[Atual][0].append((int(L[1]), L[2], L[3], f"{L[5]}{L[6]}", L[7] == "1"))
            continue
        S = REGEX_SEM_CASA.match(Linha)
        if S:
            Funcoes[Atual] = (Funcoes[Atual][0], int(S[1]))
    return Funcoes


def AcharProc(Funcoes: dict, Sufixo: str):
    Achados = [N for N in Funcoes if N == "fj_" + Sufixo or N.endswith("_" + Sufixo)]
    return Funcoes[Achados[0]] if len(Achados) == 1 else None


def ConferirProc(Sufixo: str, Esperado: dict, Dados) -> list:
    if Dados is None:
        return [f"proc '{Sufixo}' ausente ou ambiguo no dump"]
    Locais, SemCasa = Dados
    Erros = []
    Nomes = {N: T for (_, N, T, _, _) in Locais}
    if set(Nomes) != set(Esperado):
        Erros.append(f"{Sufixo}: nomes {sorted(Nomes)} != fonte {sorted(Esperado)}")
    for N, TipoEsperado in Esperado.items():
        if N in Nomes and Nomes[N] != TipoEsperado:
            Erros.append(f"{Sufixo}.{N}: tipo {Nomes[N]} != {TipoEsperado}")
    if sorted(I for (I, _, _, _, _) in Locais) != sorted(set(I for (I, _, _, _, _) in Locais)):
        Erros.append(f"{Sufixo}: id repetido")
    if SemCasa != 0:
        Erros.append(f"{Sufixo}: sem_casa={SemCasa}")
    if any(Ind for (_, _, _, _, Ind) in Locais):
        Erros.append(f"{Sufixo}: indireto inesperado (nenhum parametro agregado nas fixtures)")
    return Erros


def VerificarFixture(Forja: Path, Nome: str, Alvo: str, Tmp: Path) -> list:
    Funcoes = LerLocais(Forja, FIXTURES / f"{Nome}.fj", Alvo, Tmp)
    Erros = []
    for Sufixo, Esperado in ORACULO[Nome].items():
        Erros += ConferirProc(Sufixo, Esperado, AcharProc(Funcoes, Sufixo))
    return Erros


def ContarSemCasa(Forja: Path, Corpus: Path, Alvo: str, Tmp: Path) -> tuple:
    Funcoes = LerLocais(Forja, Corpus, Alvo, Tmp)
    return sum(len(L) for (L, _) in Funcoes.values()), sum(S for (_, S) in Funcoes.values())


def Principal() -> int:
    Pa = argparse.ArgumentParser()
    Pa.add_argument("--forja", required=True)
    Pa.add_argument("--alvo", default="windows", choices=["windows", "linux", "ambos"])
    Pa.add_argument("--corpus")
    Args = Pa.parse_args()
    Forja = Path(Args.forja).resolve()
    Alvos = ["windows", "linux"] if Args.alvo == "ambos" else [Args.alvo]
    Falhou = False
    with tempfile.TemporaryDirectory() as PastaTemporaria:
        Tmp = Path(PastaTemporaria)  # lint-ok: iaf-nome-generico mesmo nome do verificar_gdb.py (pasta de saida do teste)
        for Alvo in Alvos:
            if Args.corpus:
                Locais, Sem = ContarSemCasa(Forja, Path(Args.corpus).resolve(), Alvo, Tmp)
                print(f"{Alvo} corpus: locais nomeados={Locais} sem_casa={Sem}")
                Falhou = Falhou or Sem != 0
                continue
            for Nome in ORACULO:
                Erros = VerificarFixture(Forja, Nome, Alvo, Tmp)
                print(f"{'FALHOU' if Erros else 'PASSOU'} {Alvo} {Nome} {'; '.join(Erros)}")
                Falhou = Falhou or bool(Erros)
    return 1 if Falhou else 0


if __name__ == "__main__":
    sys.exit(Principal())
