# -*- coding: utf-8 -*-
"""lint_casa.py - B-W5 do IA1: acha foreign que reimplementa o que a std da Forja ja traz (P-IA1-a07).

Versao 0.2.0
Changelog:
  0.2.0 (13/11/2026) S3 P4 (L11b-2, D10): simbolos de libm (sin, sinf, pow, sqrt...) em foreign apontam para o std:math (math.<fn>; sqrt e o embutido).
        Mesma saida; selftest com caso sujo e limpo de libm.
  0.1.0 (11/11/2026) primeira versao: procura, dentro de blocos `foreign { ... }`, procs cujo simbolo C esta na lista
        exit, fopen, strlen, atoi, _write, Sleep, memcpy (o nome do proc, ou o alias entre aspas depois do "=").

Uso:
  python lint_casa.py <arquivo.fj> [<arquivo.fj> ...] [--json]     exit 0 limpo, 1 achou, 2 uso
  python lint_casa.py --selftest
Saida: uma linha por achado "arquivo:linha: foreign <simbolo> (use <alternativa>)", ou uma linha JSON por arquivo com --json.
Reversao: apagar este arquivo. Depende de: Python 3.9+ (biblioteca padrao).
"""
import argparse
import json
import re
import sys
from pathlib import Path

SIMBOLOS_PROIBIDOS = {
    "exit": "std:os exit", "fopen": "std:arquivo", "strlen": "len(c as str)", "atoi": "parse_int da std:texto",
    "_write": "print / eprint", "Sleep": "sleep_ms da std:os", "memcpy": "append(&l, items = s[..])",
}
# S3 P4 (D10): funcoes de libm que o std:math cobre. Sufixo f = f32 (a gemea do std:math escolhe pelo tipo).
FUNCOES_LIBM = ("sin", "cos", "tan", "asin", "acos", "atan", "atan2", "exp", "exp2", "log", "log2", "log10", "pow", "cbrt",
                "hypot", "fmod", "floor", "ceil", "round", "trunc", "copysign")
LIBM_PROIBIDOS = {}
for _Funcao in FUNCOES_LIBM:
    for _Simbolo in (_Funcao, _Funcao + "f"):
        LIBM_PROIBIDOS[_Simbolo] = "math.%s (std:math)" % _Funcao
LIBM_PROIBIDOS["sqrt"] = "sqrt (embutido, sem math.)"
LIBM_PROIBIDOS["sqrtf"] = "sqrt (embutido, sem math.)"
PADRAO_PROC = re.compile(r'\bproc\s+(\w+)\s*(?:=\s*"([^"]+)")?\s*\(')
PADRAO_FOREIGN = re.compile(r"\bforeign\s*\{")


def TirarComentarios(Texto):
    """Troca o comentario // por espacos (mantem colunas e linhas); `//` dentro de aspas e preservado."""
    Linhas = []
    for Linha in Texto.split("\n"):
        Dentro, Corte = False, len(Linha)
        for Indice, Letra in enumerate(Linha):
            if Letra == '"':
                Dentro = not Dentro
            elif not Dentro and Linha.startswith("//", Indice):
                Corte = Indice
                break
        Linhas.append(Linha[:Corte] + " " * (len(Linha) - Corte))
    return "\n".join(Linhas)


def BlocosForeign(Texto):
    """Pares (inicio, fim) de cada corpo `foreign { ... }`, por contagem de chaves."""
    Blocos = []
    for Achado in PADRAO_FOREIGN.finditer(Texto):
        Nivel, Posicao = 1, Achado.end()
        while Nivel and Posicao < len(Texto):
            Nivel += {"{": 1, "}": -1}.get(Texto[Posicao], 0)
            Posicao += 1
        Blocos.append((Achado.end(), Posicao))
    return Blocos


def LinhaDe(Texto, Posicao):
    return Texto.count("\n", 0, Posicao) + 1


def Achados(Texto):
    """Lista de {linha, simbolo, use} para cada proc foreign cujo simbolo C esta na lista proibida."""
    Limpo = TirarComentarios(Texto)
    Lista = []
    for Inicio, Fim in BlocosForeign(Limpo):
        for Proc in PADRAO_PROC.finditer(Limpo, Inicio, Fim):
            Simbolo = Proc.group(2) or Proc.group(1)
            Uso = SIMBOLOS_PROIBIDOS.get(Simbolo) or LIBM_PROIBIDOS.get(Simbolo)
            if Uso is not None:
                Lista.append({"linha": LinhaDe(Limpo, Proc.start()), "simbolo": Simbolo, "use": Uso})
    return Lista


def AchadosDoArquivo(Caminho):
    return Achados(Path(Caminho).read_text(encoding="utf-8", errors="replace"))


def Selftest():
    Sujo = ('foreign {\n    link "msvcrt"\n    proc exit(c: i32)\n    proc c_len = "strlen"(p: cstr) -> u64\n    proc puts(t: cstr) -> i32\n'
            '    proc mat_sinf = "sinf"(x: f32) -> f32\n    proc pow(x: f64, y: f64) -> f64\n}\n')
    Limpo = ('// proc exit(c: i32) e proc pow(x: f64, y: f64) em comentario\nforeign {\n    link "m"\n    proc minha_funcao(x: f64) -> f64\n}\n'
             'proc exit_depois() {}\n')
    Esperado = [(3, "exit"), (4, "strlen"), (6, "sinf"), (7, "pow")]
    Obtido = [(A["linha"], A["simbolo"]) for A in Achados(Sujo)]
    Ok = Obtido == Esperado and Achados(Limpo) == []
    print(json.dumps({"selftest": {"ok": Ok, "achados_sujo": Obtido, "achados_limpo": Achados(Limpo)}}))
    return 0 if Ok else 1


def Principal():
    Analisador = argparse.ArgumentParser(description="B-W5: foreign que a std da Forja ja cobre")
    Analisador.add_argument("arquivos", nargs="*")
    Analisador.add_argument("--json", action="store_true")
    Analisador.add_argument("--selftest", action="store_true")
    Args = Analisador.parse_args()
    if Args.selftest:
        return Selftest()
    if not Args.arquivos:
        print("uso: lint_casa.py <arquivo.fj>... [--json] | --selftest", file=sys.stderr)
        return 2
    Total = 0
    for Arquivo in Args.arquivos:
        Lista = AchadosDoArquivo(Arquivo)
        Total += len(Lista)
        if Args.json:
            print(json.dumps({"arquivo": Arquivo, "achados": Lista}, ensure_ascii=False))
        for Item in Lista if not Args.json else []:
            print("%s:%d: foreign %s (use %s)" % (Arquivo, Item["linha"], Item["simbolo"], Item["use"]))
    return 1 if Total else 0


if __name__ == "__main__":
    sys.exit(Principal())
