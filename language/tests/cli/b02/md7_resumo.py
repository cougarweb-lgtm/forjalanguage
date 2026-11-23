# -*- coding: utf-8 -*-
"""M-D7 (IA1-b02): --resumo nos err_*.fj de language\\tests: so as chaves {codigo, arquivo, linha, coluna, causa, sugestao} (nessa ordem),
mesmo codigo/arquivo/linha/coluna/causa do modo completo em 100%, e linha media <= 250 caracteres.
Uso: python -I md7_resumo.py --forja <abs forja.exe>
Versao: 1.0.0 (b02)."""
import argparse
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

TESTES = Path(r"C:\Users\USUARIO\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language\tests")
CHAVES = ["codigo", "arquivo", "linha", "coluna", "causa", "sugestao"]
META_MEDIA = 250


def Linhas(Forja, Extra, Arquivo, Relativo):
    """Relativo: cwd = pasta da fixture e so o nome na linha de comando (como o agente chama); senao o caminho absoluto."""
    Alvo = Arquivo.name if Relativo else str(Arquivo)
    R = subprocess.run([Forja, "check", *Extra, Alvo], capture_output=True, timeout=120, cwd=str(Arquivo.parent))
    return R.returncode, [L for L in R.stdout.decode("utf-8").split("\n") if L.strip()]


def Conferir(Forja, Arquivo, Relativo):
    Rc1, Comp = Linhas(Forja, [], Arquivo, Relativo)
    Rc2, Res = Linhas(Forja, ["--resumo"], Arquivo, Relativo)
    Erros = []
    if Rc1 != Rc2 or len(Comp) != len(Res):
        Erros.append("rc/linhas diferem: %d/%d vs %d/%d" % (Rc1, len(Comp), Rc2, len(Res)))
        return Erros, [], 0
    Tamanhos = []
    for A, B in zip(Comp, Res):
        Ja, Jb = json.loads(A), json.loads(B)
        if list(Jb.keys()) != CHAVES:
            Erros.append("chaves " + str(list(Jb.keys())))
        for K in ("codigo", "arquivo", "linha", "coluna", "causa"):
            if Ja.get(K) != Jb.get(K):
                Erros.append("campo %s difere" % K)
        Tamanhos.append(len(B))
    return Erros, Tamanhos, len(Comp)


def Principal():
    Ap = argparse.ArgumentParser()
    Ap.add_argument("--forja", required=True)
    Ap.add_argument("--absoluto", action="store_true", help="passa o caminho absoluto do .fj (informativo: o arquivo sozinho gasta ~65 caracteres)")
    A = Ap.parse_args()
    Arquivos = sorted(P for P in TESTES.rglob("err_*.fj") if "__pycache__" not in str(P))
    with ThreadPoolExecutor(max_workers=8) as Pool:
        Res = list(Pool.map(lambda P: (P, Conferir(A.forja, P, not A.absoluto)), Arquivos))
    Todos, Falhas, SemSaida = [], 0, 0
    for P, (Erros, Tam, N) in Res:
        Todos += Tam
        if N == 0:
            SemSaida += 1
        if Erros:
            Falhas += 1
            print("FALHA", P.name, Erros[:2])
    Media = sum(Todos) / len(Todos) if Todos else 0
    Maior = max(Todos) if Todos else 0
    print("arquivos=%d sem_diagnostico_no_check=%d linhas=%d media=%.1f maior=%d falhas=%d" % (len(Arquivos), SemSaida, len(Todos), Media, Maior, Falhas))
    print("M-D7:", "verde" if Falhas == 0 and Media <= META_MEDIA else "VERMELHO")
    return 0 if Falhas == 0 and Media <= META_MEDIA else 1


sys.exit(Principal())
