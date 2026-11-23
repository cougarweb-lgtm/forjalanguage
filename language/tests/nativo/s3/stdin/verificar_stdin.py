# -*- coding: utf-8 -*-
"""verificar_stdin.py - prova do S3 P5b-2: entrada padrao do std:os (read_line e read_all_stdin) nos 2 backends (C e x64), dev e release, no Windows.

O que prova (a saida esperada e calculada AQUI pela regra do D7, independente do compilador; cada variante tem de bater com ela, logo C == x64 byte a byte):
  1. linhas.fj: entrada por cano (bytes): LF; CRLF; sem LF final; vazio (fim, zero linhas); so LF (linha vazia nao e fim); CR sozinho no fim;
     NUL no meio; UTF-8 com emoji (bytes crus); mil linhas seguidas; linha de 1 MiB em partes; CRLF na borda do buffer de 4096 (4095 e 4096 bytes).
  2. blocos.fj: read_all_stdin com 200 KB (padrao i % 251: LF e NUL inclusos, sem separar linhas) e com 0 bytes (lista vazia, nao e erro).
  3. prompt.fj (interativo): o print "Nome:" (com LF) tem de chegar ao stdout ANTES de o programa bloquear no read. O verificador so escreve
     no stdin depois de ler o prompt; sem o flush do stdout o programa trava e o vigia (TEMPO_LIMITE_INTERATIVO) o mata: falha.

O console (ReadConsoleW) NAO e automatizado: conferido a mao pelo humano.

Uso: python -I verificar_stdin.py [--forja <forja.exe>] [--jobs N]
Saida: linhas de falha (se houver) e "RESULTADO OK" (exit 0) ou "RESULTADO FALHOU" (exit 1).
Versao: 0.1.1
Changelog: 0.1.1 - S3 P5b-2: o stdout do print sai com CRLF no Windows (esperado passa por Crlf; PROMPT = "Nome:" + CRLF). 0.1.0 - S3 P5b-2: primeira versao (3 programas x 4 pares backend x modo; prompt interativo por Popen com vigia).
Verificacao: python -I tests\\nativo\\s3\\stdin\\verificar_stdin.py
"""

import argparse
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PASTA_AQUI = Path(__file__).resolve().parent
PASTA_LANGUAGE = PASTA_AQUI.parents[3]
FORJA_PADRAO = PASTA_LANGUAGE / "compiler" / "forja.exe"
BACKENDS = ("c", "x64")
MODOS = ("dev", "release")
TEMPO_LIMITE = 120
TEMPO_LIMITE_INTERATIVO = 60
MIB = 1024 * 1024
TAMANHO_PADRAO = 200000
LIMITE_IMPRESSO = 200
PROMPT = b"Nome:\r\n"  # print grava o LF como CRLF no stdout do Windows (rt_saida / CRT)

PROGRAMA_LINHAS = r'''
import "std:os" as os

proc main() {
    var total = 0
    var fim = false
    while not fim {
        switch linha in os.read_line(alloc = heap) {
        case str:
            total += 1
            print(len(linha))
            if len(linha) <= 200 {
                print(linha)
            }
            delete(linha)
        case os.InputError:
            fim = true
        }
    }
    print(f"linhas={total}")
}
'''

PROGRAMA_BLOCOS = r'''
import "std:os" as os

proc main() {
    switch dados in os.read_all_stdin(alloc = heap) {
    case [dynamic]u8:
        var ok = true
        for i in 0..<len(dados) {
            if dados[i] != (i % 251) as u8 {
                ok = false
            }
        }
        print(len(dados))
        print(ok)
        delete(dados)
    case os.InputError:
        print("erro")
    }
}
'''

PROGRAMA_PROMPT = r'''
import "std:os" as os

proc main() {
    print("Nome:")
    switch nome in os.read_line(alloc = heap) {
    case str:
        print(f"ola {nome}")
        delete(nome)
    case os.InputError:
        print("sem nome")
    }
}
'''

PROGRAMAS = {"linhas": PROGRAMA_LINHAS, "blocos": PROGRAMA_BLOCOS, "prompt": PROGRAMA_PROMPT}


def EntradasLinhas():
    """(caso, entrada) de cada linha de teste; a regra de cada uma esta em EsperadoLinhas."""
    return [
        ("lf", b"linha1\nlinha2\n"),
        ("crlf", b"a\r\nb\r\n"),
        ("sem_lf_final", b"x\ny"),
        ("vazio", b""),
        ("so_lf", b"\n\n"),
        ("cr_no_fim", b"a\r"),
        ("nul_no_meio", b"a\x00b\n"),
        ("utf8", "açúcar \U0001F600\n".encode("utf-8")),
        ("mil_linhas", b"".join(f"{I}\n".encode("ascii") for I in range(1000))),
        ("linha_1mib", b"A" * MIB + b"\n" + b"fim\n"),
        ("cr_na_borda_4095", b"C" * 4095 + b"\r\n"),
        ("cr_na_borda_4096", b"D" * 4096 + b"\r\n"),
    ]


def PadraoBytes(Tamanho):
    """Bytes 0..250 repetidos (i % 251): tem LF e NUL, sem separar nada para o read_all."""
    return bytes((I % 251) for I in range(Tamanho))


def Crlf(Dados):
    """O print do Forja grava cada LF como CRLF no stdout do Windows; o dado impresso nao tem LF (a linha ja foi quebrada)."""
    return Dados.replace(b"\n", b"\r\n")


def EsperadoLinhas(Entrada):
    """Regra do D7: LF fecha a linha; um CR logo antes sai; a ultima parte sem LF vale se nao for vazia."""
    Partes = Entrada.split(b"\n")
    Linhas = []
    for Indice, Parte in enumerate(Partes):
        if Indice < len(Partes) - 1:
            Linhas.append(Parte[:-1] if Parte.endswith(b"\r") else Parte)
        elif Parte:
            Linhas.append(Parte)
    Saida = b""
    for Linha in Linhas:
        Saida += f"{len(Linha)}\n".encode("ascii")
        if len(Linha) <= LIMITE_IMPRESSO:
            Saida += Linha + b"\n"
    return Crlf(Saida + f"linhas={len(Linhas)}\n".encode("ascii"))


def CasosDe(Nome):
    """Lista de (caso, entrada ou None para o interativo, saida esperada)."""
    if Nome == "linhas":
        return [(Caso, Entrada, EsperadoLinhas(Entrada)) for Caso, Entrada in EntradasLinhas()]
    if Nome == "blocos":
        return [
            ("padrao_200k", PadraoBytes(TAMANHO_PADRAO), Crlf(f"{TAMANHO_PADRAO}\ntrue\n".encode("ascii"))),
            ("vazio", b"", Crlf(b"0\ntrue\n")),
        ]
    return [("prompt", None, Crlf(b"Nome:\nola Ana\n"))]


def Construir(Forja, Fonte, Backend, Modo, Saida):
    Comando = [str(Forja), "build", "--backend", Backend]
    if Modo == "release":
        Comando.append("--release")
    Comando += [str(Fonte), "-o", str(Saida)]
    Proc = subprocess.run(Comando, capture_output=True, stdin=subprocess.DEVNULL, timeout=TEMPO_LIMITE)
    if Proc.returncode != 0 or not Saida.exists():
        return Proc.stdout.decode("utf-8", "replace")[:600] + Proc.stderr.decode("utf-8", "replace")[:600]
    return None


def RodarEntrada(Exe, Cwd, Entrada):
    """Roda o exe com a entrada por cano (stdin=bytes); o timeout pega o programa travado na leitura."""
    Proc = subprocess.run([str(Exe)], input=Entrada, capture_output=True, timeout=TEMPO_LIMITE, cwd=str(Cwd))
    return Proc.returncode, Proc.stdout, Proc.stderr.decode("utf-8", "replace")


def RodarInterativo(Exe, Cwd):
    """So escreve no stdin depois de ver o prompt no stdout: se o prompt nao chegou, o programa trava e o vigia o mata."""
    Proc = subprocess.Popen([str(Exe)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, cwd=str(Cwd))
    Vigia = threading.Timer(TEMPO_LIMITE_INTERATIVO, Proc.kill)
    Vigia.start()
    try:
        Visto = b""
        while PROMPT not in Visto:
            Pedaco = os.read(Proc.stdout.fileno(), 4096)
            if not Pedaco:
                break
            Visto += Pedaco
        if PROMPT not in Visto:
            return None, Visto, "o prompt nao chegou antes da leitura (stdout sem flush?)"
        Proc.stdin.write(b"Ana\n")
        Proc.stdin.close()
        Resto = Proc.stdout.read()
        Proc.wait()
        return Proc.returncode, Visto + Resto, Proc.stderr.read().decode("utf-8", "replace")
    finally:
        Vigia.cancel()
        if Proc.poll() is None:
            Proc.kill()


def Tarefas(Pasta):
    Itens = []
    for Nome, Texto in PROGRAMAS.items():
        for Backend in BACKENDS:
            for Modo in MODOS:
                Fonte = Pasta / f"{Nome}_{Backend}_{Modo}.fj"
                Fonte.write_text(Texto, encoding="utf-8")
                Itens.append((Nome, Backend, Modo, Fonte))
    return Itens


def Executar(Forja, Pasta, Item):
    """Compila uma variante e roda todos os casos dela. Devolve (rotulo, casos ok, casos total, falhas)."""
    Nome, Backend, Modo, Fonte = Item
    Rotulo = f"{Nome} {Backend}/{Modo}"
    Casos = CasosDe(Nome)
    Exe = Pasta / f"{Nome}_{Backend}_{Modo}.exe"
    Erro = Construir(Forja, Fonte, Backend, Modo, Exe)
    if Erro is not None:
        return Rotulo, 0, len(Casos), [f"FALHA {Rotulo}: build: {Erro}"[:800]]
    Cwd = Pasta / f"run_{Nome}_{Backend}_{Modo}"
    Cwd.mkdir(exist_ok=True)
    Ok = 0
    Falhas = []
    for Caso, Entrada, Esperado in Casos:
        try:
            if Entrada is None:
                Rc, Saida, Err = RodarInterativo(Exe, Cwd)
            else:
                Rc, Saida, Err = RodarEntrada(Exe, Cwd, Entrada)
        except subprocess.TimeoutExpired:
            Falhas.append(f"FALHA {Rotulo} {Caso}: tempo esgotado (travou na leitura?)")
            continue
        if Rc is None or Rc != 0:
            Falhas.append(f"FALHA {Rotulo} {Caso}: rc={Rc} stderr={Err[:300]}"[:800])
            continue
        if Saida != Esperado:
            Falhas.append(f"FALHA {Rotulo} {Caso}: saida={Saida[:300]!r} esperado={Esperado[:300]!r}"[:800])
            continue
        Ok += 1
    return Rotulo, Ok, len(Casos), Falhas


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
    Pasta = Path(tempfile.mkdtemp(prefix="verif_stdin_"))
    Falhas = []
    Ok = 0
    Total = 0
    try:
        Itens = Tarefas(Pasta)
        with ThreadPoolExecutor(max_workers=Args.jobs) as Piscina:
            for Rotulo, OkVariante, TotalVariante, FalhasVariante in Piscina.map(lambda I: Executar(Forja, Pasta, I), Itens):
                Ok += OkVariante
                Total += TotalVariante
                Falhas.extend(FalhasVariante)
    finally:
        LimparPasta(Pasta)
    for Linha in Falhas:
        print(Linha)
    print(f"verificar_stdin: {Ok}/{Total} casos conferidos (C e x64, dev e release; cada um igual ao esperado)")
    if Falhas:
        print("RESULTADO FALHOU")
        return 1
    print("RESULTADO OK")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
