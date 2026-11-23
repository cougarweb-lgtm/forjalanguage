# -*- coding: utf-8 -*-
"""std:proc (M5 P26b): args, env e exit no backend x64, dev e release, no Windows.

O que prova:
  1. Parser da linha de comando: args_eco.fj roda com 40 linhas de comando LANCADAS CRUAS (subprocess com str, que vai inteira ao CreateProcess) e a saida do x64
     tem de ser byte a byte a esperada, calculada AQUI pelo CommandLineToArgvW real (ctypes/shell32). O parser do x64 e o proprio (runtime\\nativo\\rt_linha.c): qualquer
     linha em que divergir do CommandLineToArgvW e FALHA (a linha e impressa).
  2. env: variavel cheia (UTF-8), vazia (existe, ""), ausente, mesmo nome com outra caixa, nome e valor nao ASCII e valor de 5000 bytes.
  3. exit: 0, 7, 255 (saem com o codigo), 256 e -1 (R0012, exit 101), com stdout pendente (300 linhas) e com bloco de heap vivo em dev (sem R0009).
  4. forja run <arq> -- a "b c" no x64.

Uso: python -I verificar_proc.py [--forja <forja.exe>] [--jobs N]
Saida: linhas de falha (se houver) e, no fim, "verificar_proc: <ok>/<total> casos de proc" (exit 0 so com ok == total).
Versao: 1.0.1
Changelog: 1.0.1 - 0.21.0 F4 residuo: sai o backend C (aposentado, X0003); a referencia e o CommandLineToArgvW; sai ConferirErroIgual (igualdade C x x64).
           1.0.0 - M5 P26b: primeira versao.
Verificacao: python -I verificar_proc.py --forja <abs do forja.exe>
"""

import argparse
import ctypes
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from ctypes import wintypes
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaLanguage = PastaAqui.parents[3]
BACKENDS = ("x64",)
MODOS = ("dev", "release")
TEMPO_LIMITE = 60

# Linhas de comando (depois do exe): as da documentacao da Microsoft e as de borda (barras, aspas, vazios, acentos, emoji).
LINHAS = [
    "",
    "a b c",
    '"a b c" d',
    'a "b c" d',
    'a""b',
    r"a\\b c\\d",
    '"a b"c "d e"',
    r"a\"b c",
    r'"a\\" b',
    r'"a\\\" b"',
    '"" x',
    'a "" b',
    '"a ""b"" c"',
    '"a""" b',
    "ação coração",
    "😀 x",
    "   a   b   ",
    "a\tb",
    '"sem fechar',
    r'\\\\"x y"',
    r'a\\\\"b',
    r'/flag --opt=1 -x "C:\Program Files\x y\" z',
    '"a b" "c d" "e f"',
    'x""',
    'x"""',
    'x""""',
    '"""a"""',
    '""""',
    "'aspas simples' x",
    r"caminho\sub\arq.txt",
    r'"C:\dir com espaco\" outro',
    '- "-" --',
    'um   "dois  tres"   quatro',
    '😀"é ç"ñ',
    'a"b c"d e',
    "\\ \\\\ \\\\\\",
    '"a" "b""c" d""',
    r"""\" \\" \\\" \\\\" x""",
    '"é""ã" 😀😀"" z',
    "a\t\t\tb   \tc",
]

Shell32 = ctypes.WinDLL("shell32")
Shell32.CommandLineToArgvW.restype = ctypes.POINTER(wintypes.LPWSTR)
Shell32.CommandLineToArgvW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_int)]


def DividirComOrganizador(Linha):
    """O CommandLineToArgvW do Windows sobre a linha inteira (programa + argumentos)."""
    Qtd = ctypes.c_int(0)
    Ptr = Shell32.CommandLineToArgvW(Linha, ctypes.byref(Qtd))
    return [Ptr[K] for K in range(Qtd.value)]


def SaidaEsperadaArgs(Linha):
    Partes = DividirComOrganizador(Linha)
    Texto = f"n={len(Partes)}\r\n"
    for Indice, Parte in enumerate(Partes):
        if Indice > 0:
            Texto += f"[{Indice}] ({len(Parte.encode('utf-8'))}) {Parte}\r\n"
    return Texto.encode("utf-8")


def Construir(Forja, Fonte, Backend, Modo, Saida):
    Comando = [str(Forja), "build", "--backend", Backend]
    if Modo == "release":
        Comando.append("--release")
    Comando += [str(Fonte), "-o", str(Saida)]
    Proc = subprocess.run(Comando, capture_output=True, stdin=subprocess.DEVNULL, timeout=TEMPO_LIMITE * 4)
    if Proc.returncode != 0 or not Saida.exists():
        return Proc.stdout.decode("utf-8", "replace")[:400] + Proc.stderr.decode("utf-8", "replace")[:400]
    return None


def Rodar(Exe, Argumentos, Ambiente=None):
    """Lanca o exe com a linha de comando CRUA: str inteira vai ao CreateProcess."""
    Linha = f'"{Exe}"' + (" " + Argumentos if Argumentos else "")
    Proc = subprocess.run(Linha, capture_output=True, stdin=subprocess.DEVNULL, env=Ambiente, timeout=TEMPO_LIMITE)
    return Proc.returncode, Proc.stdout, Proc.stderr


def AmbienteDeTeste():
    Amb = dict(os.environ)
    Amb["FJ_CHEIA"] = "héllo"
    Amb["FJ_VAZIA"] = ""
    Amb["FJ_ÇÃO"] = "ação ✓"
    Amb["FJ_LONGA"] = "x" * 5000
    Amb.pop("FJ_AUSENTE", None)
    return Amb


def EsperadoEnv():
    Linhas = [
        "FJ_CHEIA=<héllo> (6)",
        "FJ_VAZIA=<> (0)",
        "FJ_AUSENTE=none",
        "fj_cheia=<héllo> (6)",
        "FJ_ÇÃO=<ação ✓> (" + str(len("ação ✓".encode("utf-8"))) + ")",
        "FJ_LONGA=<" + "x" * 5000 + "> (5000)",
    ]
    return ("\r\n".join(Linhas) + "\r\n").encode("utf-8")


ARGS_ENV = 'FJ_CHEIA FJ_VAZIA FJ_AUSENTE fj_cheia FJ_ÇÃO FJ_LONGA'


def CasosDeSaida():
    """(fonte, rc, stdout esperado, trecho de stderr ou None)."""
    Esperado100 = "".join(f"{K}\r\n" for K in range(300)).encode()
    return [
        ("sair_0", 0, b"antes\r\n", None),
        ("sair_7", 7, b"antes\r\n", None),
        ("sair_255", 255, b"antes\r\n", None),
        ("sair_256", 101, b"antes\r\n", "R0012"),
        ("sair_menos1", 101, b"antes\r\n", "R0012"),
        ("sair_pendente", 2, Esperado100, None),
        ("sair_heap", 4, b"5\r\n", None),
    ]


def Falhar(Falhas, Rotulo, Detalhe):
    Falhas.append(f"FALHA {Rotulo}: {Detalhe}")


def ConferirArgs(Exes, Falhas):
    Total = 0
    Ok = 0
    for Backend in BACKENDS:
        for Modo in MODOS:
            Exe = Exes[("args_eco", Backend, Modo)]
            for Linha in LINHAS:
                Total += 1
                Rc, Saida, Erro = Rodar(Exe, Linha)
                Esperado = SaidaEsperadaArgs(f'"{Exe}"' + (" " + Linha if Linha else ""))
                if Rc == 0 and Saida == Esperado and Erro == b"":
                    Ok += 1
                else:
                    Falhar(Falhas, f"args {Backend}/{Modo} linha={Linha!r}", f"rc={Rc} saida={Saida!r} esperado={Esperado!r} stderr={Erro[:120]!r}")
    return Ok, Total


def ConferirEnv(Exes, Falhas):
    Total = 0
    Ok = 0
    for Backend in BACKENDS:
        for Modo in MODOS:
            Total += 1
            Rc, Saida, Erro = Rodar(Exes[("env_eco", Backend, Modo)], ARGS_ENV, AmbienteDeTeste())
            if Rc == 0 and Saida == EsperadoEnv() and Erro == b"":
                Ok += 1
            else:
                Falhar(Falhas, f"env {Backend}/{Modo}", f"rc={Rc} saida={Saida[:300]!r} stderr={Erro[:120]!r}")
    return Ok, Total


def ConferirSaidas(Exes, Falhas):
    Total = 0
    Ok = 0
    for Nome, RcEsperado, Esperado, TrechoErro in CasosDeSaida():
        for Backend in BACKENDS:
            for Modo in MODOS:
                Total += 1
                Rc, Saida, Erro = Rodar(Exes[(Nome, Backend, Modo)], "")
                ErroOk = (TrechoErro in Erro.decode("utf-8", "replace")) if TrechoErro else (Erro == b"")
                if Rc == RcEsperado and Saida == Esperado and ErroOk:
                    Ok += 1
                else:
                    Falhar(Falhas, f"{Nome} {Backend}/{Modo}", f"rc={Rc} (esperado {RcEsperado}) saida={Saida[:80]!r} stderr={Erro[:200]!r}")
    return Ok, Total


def ConferirRun(Forja, Pasta, Falhas):
    """forja run <arq> -- a "b c": os argumentos chegam ao programa nos dois backends."""
    Total = 0
    Ok = 0
    Esperado = b"n=3\r\n[1] (1) a\r\n[2] (3) b c\r\n"
    for Backend in BACKENDS:
        Total += 1
        Proc = subprocess.run([str(Forja), "run", "--backend", Backend, str(PastaAqui / "args_eco.fj"), "--", "a", "b c"], capture_output=True,
                              stdin=subprocess.DEVNULL, timeout=TEMPO_LIMITE * 4, cwd=str(Pasta))
        if Proc.returncode == 0 and Proc.stdout == Esperado:
            Ok += 1
        else:
            Falhar(Falhas, f"run {Backend}", f"rc={Proc.returncode} saida={Proc.stdout[:120]!r} stderr={Proc.stderr[:200]!r}")
    return Ok, Total


def ConstruirTudo(Forja, Pasta, Jobs, Falhas):
    Fontes = ["args_eco", "env_eco"] + [Caso[0] for Caso in CasosDeSaida()]
    Pedidos = [(F, B, M) for F in Fontes for B in BACKENDS for M in MODOS]
    Exes = {}

    def Um(Pedido):
        Fonte, Backend, Modo = Pedido
        Saida = Pasta / f"{Fonte}_{Backend}_{Modo}.exe"
        return Pedido, Saida, Construir(Forja, PastaAqui / f"{Fonte}.fj", Backend, Modo, Saida)

    with ThreadPoolExecutor(max_workers=Jobs) as Piscina:
        for Pedido, Saida, Erro in Piscina.map(Um, Pedidos):
            if Erro is not None:
                Falhar(Falhas, f"build {Pedido}", Erro)
            else:
                Exes[Pedido] = Saida
    return Exes


def Principal():
    Analisador = argparse.ArgumentParser()
    Analisador.add_argument("--forja", default=str(PastaLanguage / "compiler" / "forja.exe"))
    Analisador.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 4))
    Pedido = Analisador.parse_args()
    Pasta = Path(tempfile.mkdtemp(prefix="forja_m5_proc_"))
    Falhas = []
    try:
        Exes = ConstruirTudo(Path(Pedido.forja), Pasta, Pedido.jobs, Falhas)
        Ok = Total = 0
        if not Falhas:
            for Etapa in (ConferirArgs, ConferirEnv, ConferirSaidas):
                O, T = Etapa(Exes, Falhas)
                Ok += O
                Total += T
            O, T = ConferirRun(Path(Pedido.forja), Pasta, Falhas)
            Ok += O
            Total += T
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    for Linha in Falhas[:40]:
        print(Linha)
    print(f"verificar_proc: {Ok}/{Total} casos de proc (linhas de comando={len(LINHAS)})")
    return 0 if not Falhas and Ok == Total else 1


if __name__ == "__main__":
    sys.exit(Principal())
