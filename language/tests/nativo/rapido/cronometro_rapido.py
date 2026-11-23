# -*- coding: utf-8 -*-
"""Cronometro pareado do medir_rapido: afinidade de 1 nucleo, copias de mesmo comprimento e pares A,B,B,A.

Objetivo: medir a razao novo/base de UM bench com o minimo de ruido. A e B viram copias com nomes de MESMO comprimento
na mesma pasta; o processo fica preso a 1 nucleo (so durante a medicao, para nao travar compilacao); a ordem dos pares
alterna (A,B / B,A / A,B ...) e 1 par de aquecimento e descartado. Cada execucao = tempo de parede do processo filho.
Decide em 12/24/36 pares (estat_rapido.AvaliarPares); bimodal ou pedido de 36 pula direto para 36.

Uso: importado por medir_rapido.py (chamar DENTRO de TravaBancada exclusiva).
Verificacao: python -I -B cronometro_rapido.py --selftest
Versao: 1.0.0
Changelog: 1.0.0 - primeira versao (decisao D0696).
"""

import contextlib
import ctypes
import os
import shutil
import subprocess
import sys
import tempfile
import time
from ctypes import wintypes
from pathlib import Path
from typing import NamedTuple

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

import estat_rapido as Est  # noqa: E402

VERSAO = "1.0.0"
TIMEOUT_EXECUCAO_S = 300
NUCLEO_MINIMO_PARA_PULAR_O_0 = 4
POSICAO_NUCLEO_ESCOLHIDO = 2  # nucleo 0 recebe as interrupcoes do SO: fica de fora quando ha 4+ nucleos


class ErroCronometro(Exception):
    """Falha ao executar um exe cronometrado (exit diferente de 0, timeout) ou afinidade impossivel."""


class CopiasPar(NamedTuple):
    ExeA: Path
    ExeB: Path
    Pasta: Path


def _Kernel():
    K = ctypes.WinDLL("kernel32", use_last_error=True)
    K.GetCurrentProcess.restype = wintypes.HANDLE
    K.GetProcessAffinityMask.argtypes = [wintypes.HANDLE, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t)]
    K.SetProcessAffinityMask.argtypes = [wintypes.HANDLE, ctypes.c_size_t]
    return K


def MascaraAtual():
    K = _Kernel()
    Processo, Sistema = ctypes.c_size_t(), ctypes.c_size_t()
    if not K.GetProcessAffinityMask(K.GetCurrentProcess(), ctypes.byref(Processo), ctypes.byref(Sistema)):
        raise ErroCronometro(f"GetProcessAffinityMask falhou (erro {ctypes.get_last_error()})")
    return Processo.value


def DefinirMascara(Mascara):
    K = _Kernel()
    if not K.SetProcessAffinityMask(K.GetCurrentProcess(), Mascara):
        raise ErroCronometro(f"SetProcessAffinityMask({Mascara:#x}) falhou (erro {ctypes.get_last_error()})")


def EscolherNucleo(Mascara):
    """Indice do nucleo fixo: o 3o permitido quando ha 4+, senao o ultimo."""
    Permitidos = [I for I in range(Mascara.bit_length()) if Mascara >> I & 1]
    if not Permitidos:
        raise ErroCronometro(f"mascara de afinidade vazia: {Mascara:#x}")
    if len(Permitidos) >= NUCLEO_MINIMO_PARA_PULAR_O_0:
        return Permitidos[POSICAO_NUCLEO_ESCOLHIDO]
    return Permitidos[-1]


@contextlib.contextmanager
def AfinidadeFixa(Nucleo=None):
    """Prende este processo (e os filhos que ele criar) a 1 nucleo; restaura a mascara original ao sair."""
    Original = MascaraAtual()
    Escolhido = EscolherNucleo(Original) if Nucleo is None else Nucleo
    if not (Original >> Escolhido) & 1:
        raise ErroCronometro(f"nucleo {Escolhido} fora da mascara permitida {Original:#x}")
    DefinirMascara(1 << Escolhido)
    try:
        yield Escolhido
    finally:
        DefinirMascara(Original)


def PrepararCopias(ExeA, ExeB, Pasta):
    """Copia A e B para <pasta>\\<nome>_A.exe e _B.exe: mesma pasta, nomes de mesmo comprimento."""
    Pasta.mkdir(parents=True, exist_ok=True)
    Base = Path(ExeA).stem
    CopiaA, CopiaB = Pasta / f"{Base}_A.exe", Pasta / f"{Base}_B.exe"
    shutil.copyfile(ExeA, CopiaA)
    shutil.copyfile(ExeB, CopiaB)
    return CopiasPar(CopiaA, CopiaB, Pasta)


def CronometrarExe(Exe, Pasta):
    """Tempo de parede (ms) de uma execucao; exit != 0 e erro."""
    Inicio = time.perf_counter()
    Resultado = subprocess.run([str(Exe)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=str(Pasta),
                               timeout=TIMEOUT_EXECUCAO_S)
    Fim = time.perf_counter()
    if Resultado.returncode != 0:
        raise ErroCronometro(f"{Exe.name}: exit {Resultado.returncode} na cronometragem")
    return (Fim - Inicio) * 1000.0


def ExecutarPar(Copias, Indice):
    """Um par intercalado: indice par = A,B; impar = B,A. Devolve (tempo A, tempo B) em ms."""
    Tempos = {}
    Ordem = (("A", Copias.ExeA), ("B", Copias.ExeB))
    for Rotulo, Exe in (Ordem if Indice % 2 == 0 else tuple(reversed(Ordem))):
        Tempos[Rotulo] = CronometrarExe(Exe, Copias.Pasta)
    return Tempos["A"], Tempos["B"]


def MedianaAvulsa(Exe, Pasta, Repeticoes=3):
    """Mediana de poucas execucoes de um exe so (peso do tick quando o bench nao foi pareado)."""
    CronometrarExe(Exe, Pasta)
    return Est.Mediana([CronometrarExe(Exe, Pasta) for _ in range(Repeticoes)])


def CronometrarFixo(Copias, Pares=Est.PASSO_PARES):
    """Razao mediana novo/base em N pares fixos (kernels: sem veredito, so o numero). 1 par de aquecimento descartado."""
    ExecutarPar(Copias, 0)
    Razoes = []
    for Indice in range(Pares):
        A, B = ExecutarPar(Copias, Indice)
        Razoes.append(B / A)
    return Est.Mediana(Razoes)


def CronometrarBench(Copias, *, Sempre36=False):
    """Pares ate decidir (12/24/36). Devolve dict com razoes, tempos, IC, veredito, bimodal e numero de pares."""
    ExecutarPar(Copias, 0)  # aquecimento descartado
    TemposA, TemposB, Razoes = [], [], []
    Bimodais = Sempre36
    while True:
        for _ in range(Est.PASSO_PARES):
            A, B = ExecutarPar(Copias, len(Razoes))
            TemposA.append(A)
            TemposB.append(B)
            Razoes.append(B / A)
        if len(Razoes) == Est.PASSO_PARES and not Bimodais:
            Bimodais = Est.Bimodal(TemposA, TemposB, Razoes)
        Veredito, Ic = Est.AvaliarPares(Razoes, Bimodais)
        if Veredito is not None:
            return {"razoes": Razoes, "tempos_a": TemposA, "tempos_b": TemposB, "ic": Ic, "veredito": Veredito,
                    "bimodal": Bimodais, "pares": len(Razoes)}


def Afirmar(Condicao, Mensagem):
    if not Condicao:
        raise AssertionError(Mensagem)


def TesteAfinidade():
    Original = MascaraAtual()
    Nucleo = EscolherNucleo(Original)
    with AfinidadeFixa(Nucleo):
        Afirmar(MascaraAtual() == 1 << Nucleo, "dentro do contexto a mascara e de 1 nucleo")
        Filho = subprocess.run([sys.executable, "-I", "-B", "-c",
                                "import ctypes;K=ctypes.WinDLL('kernel32');K.GetCurrentProcess.restype=ctypes.c_void_p;"
                                "P=ctypes.c_size_t();S=ctypes.c_size_t();"
                                "K.GetProcessAffinityMask(ctypes.c_void_p(K.GetCurrentProcess()),ctypes.byref(P),ctypes.byref(S));"
                                "print(P.value)"], capture_output=True, text=True, timeout=60)
        Afirmar(Filho.stdout.strip() == str(1 << Nucleo), f"filho herda a afinidade: {Filho.stdout!r} {Filho.stderr!r}")
    Afirmar(MascaraAtual() == Original, "mascara restaurada ao sair")
    Afirmar(EscolherNucleo(0b1111) == 2 and EscolherNucleo(0b0110) == 2 and EscolherNucleo(0b1) == 0, "escolha do nucleo")


def TesteCopias():
    Pasta = Path(tempfile.mkdtemp(prefix="cron_rapido_selftest_"))
    try:
        Origem = Pasta / "bench_x64.exe"
        Origem.write_bytes(b"MZ")
        Copias = PrepararCopias(Origem, Origem, Pasta / "par")
        Afirmar(len(Copias.ExeA.name) == len(Copias.ExeB.name) and Copias.ExeA != Copias.ExeB, "nomes de mesmo comprimento")
        Afirmar(Copias.ExeA.parent == Copias.ExeB.parent, "mesma pasta")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def TesteCronometro():
    Pasta = Path(tempfile.mkdtemp(prefix="cron_rapido_selftest_"))
    try:
        Exe = Pasta / "nada.bat"  # python.exe sem argumentos ficaria esperando o stdin
        Exe.write_bytes(b"@echo off\r\nexit /b 0\r\n")
        Copias = CopiasPar(Exe, Exe, Pasta)
        Afirmar(ExecutarPar(Copias, 0)[0] > 0, "par cronometrado com tempo positivo")
        Afirmar(os.path.isdir(Pasta), "pasta de trabalho")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def Selftest():
    for Teste in (TesteAfinidade, TesteCopias, TesteCronometro):
        Teste()
    print(f"cronometro_rapido {VERSAO}: selftest OK")
    return True


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(0 if Selftest() else 1)
    print(__doc__)
