# -*- coding: utf-8 -*-
"""verificar_saida.py - prova do D-G3-L2a (bloco A): os.write_stdout escreve bytes CRUS no stdout, na ordem do print, no backend x64 (o C foi aposentado na 0.21.0), dev e release, no Windows.

O que prova (a saida esperada e calculada AQUI, independente do compilador; cada variante tem de bater com ela byte a byte):
  1. misto.fj: print("a"), write_stdout("b" LF "c"), print("d") -> "a" CRLF "b" LF "c" "d" CRLF (o print fecha a linha com CRLF; o write_stdout NAO traduz o LF e nao acrescenta nada).
  2. cru.fj: write_stdout dos 256 valores de byte (inclusive 0, 10, 13 e 26) 300 vezes (76800 bytes, passa de qualquer buffer de 64 KiB) e print("fim"): sem traducao nem perda.
O stdout vai por cano (capture_output); o console nao e automatizado.

Uso: python -I verificar_saida.py [--forja <forja.exe>] [--jobs N]
Saida: linhas de falha (se houver) e "RESULTADO OK" (exit 0) ou "RESULTADO FALHOU" (exit 1).
Versao: 0.1.0
Changelog: 0.1.0 - D-G3-L2a: primeira versao (2 programas x dev e release, so x64). Nao registrada em bateria: roda a mao, como os outros verificadores de tests\\nativo\\s3.
Verificacao: python -I tests\\nativo\\s3\\saida\\verificar_saida.py
"""

import argparse
import os
import shutil
import stat
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PASTA_AQUI = Path(__file__).resolve().parent
PASTA_LANGUAGE = PASTA_AQUI.parents[3]
FORJA_PADRAO = PASTA_LANGUAGE / "compiler" / "forja.exe"
BACKENDS = ("x64",)
MODOS = ("dev", "release")
TEMPO_LIMITE = 120
REPETICOES = 300

PROGRAMA_MISTO = r'''
import "std:os" as os

proc main() {
    print("a")
    var meio: [3]u8 = {98, 10, 99}
    os.write_stdout(meio[..])
    print("d")
}
'''

PROGRAMA_CRU = r'''
import "std:os" as os

proc main() {
    var bloco: [256]u8 = {}
    for i in 0..<256 {
        bloco[i] = i as u8
    }
    for r in 0..<300 {
        os.write_stdout(bloco[..])
    }
    print("fim")
}
'''

PROGRAMAS = {
    "misto": (PROGRAMA_MISTO, b"a\r\nb\ncd\r\n"),
    "cru": (PROGRAMA_CRU, bytes(range(256)) * REPETICOES + b"fim\r\n"),
}


def Tarefas(Pasta):
    Itens = []
    for Nome, (Texto, Esperado) in PROGRAMAS.items():
        for Backend in BACKENDS:
            for Modo in MODOS:
                Fonte = Pasta / f"{Nome}_{Backend}_{Modo}.fj"
                Fonte.write_text(Texto, encoding="utf-8")
                Itens.append((Nome, Backend, Modo, Fonte, Esperado))
    return Itens


def Construir(Forja, Fonte, Backend, Modo, Saida):
    Comando = [str(Forja), "build", "--backend", Backend]
    if Modo == "release":
        Comando.append("--release")
    Comando += [str(Fonte), "-o", str(Saida)]
    Proc = subprocess.run(Comando, capture_output=True, stdin=subprocess.DEVNULL, timeout=TEMPO_LIMITE)
    if Proc.returncode != 0 or not Saida.exists():
        return Proc.stdout.decode("utf-8", "replace")[:600] + Proc.stderr.decode("utf-8", "replace")[:600]
    return None


def Executar(Forja, Pasta, Item):
    """Compila uma variante, roda e compara os bytes. Devolve (rotulo, falha ou None)."""
    Nome, Backend, Modo, Fonte, Esperado = Item
    Rotulo = f"{Nome} {Backend}/{Modo}"
    Exe = Pasta / f"{Nome}_{Backend}_{Modo}.exe"
    Erro = Construir(Forja, Fonte, Backend, Modo, Exe)
    if Erro is not None:
        return Rotulo, f"FALHA {Rotulo}: build: {Erro}"[:800]
    Proc = subprocess.run([str(Exe)], capture_output=True, stdin=subprocess.DEVNULL, timeout=TEMPO_LIMITE, cwd=str(Pasta))
    if Proc.returncode != 0:
        return Rotulo, f"FALHA {Rotulo}: rc={Proc.returncode} stderr={Proc.stderr[:300]!r}"[:800]
    if Proc.stdout != Esperado:
        Indice = next((I for I, (A, B) in enumerate(zip(Proc.stdout, Esperado)) if A != B), min(len(Proc.stdout), len(Esperado)))
        return Rotulo, f"FALHA {Rotulo}: stdout difere no byte {Indice} (obtido {len(Proc.stdout)} bytes, esperado {len(Esperado)}): {Proc.stdout[:60]!r}"[:800]
    return Rotulo, None


def LimparPasta(Pasta):
    def Forcar(Funcao, Caminho, _Info):
        os.chmod(Caminho, stat.S_IWRITE)
        Funcao(Caminho)

    shutil.rmtree(Pasta, onerror=Forcar)


def Principal():
    Parser = argparse.ArgumentParser()
    Parser.add_argument("--forja", default=str(FORJA_PADRAO))
    Parser.add_argument("--jobs", type=int, default=4)
    Args = Parser.parse_args()
    Forja = Path(Args.forja).resolve()
    Pasta = Path(tempfile.mkdtemp(prefix="verif_saida_"))
    Falhas = []
    Total = 0
    try:
        Itens = Tarefas(Pasta)
        with ThreadPoolExecutor(max_workers=Args.jobs) as Piscina:
            for _Rotulo, Falha in Piscina.map(lambda I: Executar(Forja, Pasta, I), Itens):
                Total += 1
                if Falha is not None:
                    Falhas.append(Falha)
    finally:
        LimparPasta(Pasta)
    for Linha in Falhas:
        print(Linha)
    print(f"verificar_saida: {Total - len(Falhas)}/{Total} variantes iguais ao esperado (x64, dev e release)")
    if Falhas:
        print("RESULTADO FALHOU")
        return 1
    print("RESULTADO OK")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
