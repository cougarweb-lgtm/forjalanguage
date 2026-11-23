# -*- coding: utf-8 -*-
"""Runner dos kernels de micro-benchmark do M7b (F0c): confere a saida e mede a razao Forja/gcc-O2.

Objetivo: para cada kernel de gerados\\ (.fj + .esperado.txt, feitos por gerar_kernels.py), compila o x64 release
(`forja build --backend x64 --release`), exige que a saida seja igual ao .esperado.txt e mede a mediana de 5 execucoes
(1 aquecimento). O denominador e o binario gcc -O2 congelado (BinarioDeReferencia, BancoRef, D0730): o backend C saiu na
0.21.0. A razao e x64 / gcc-O2: quanto acima de 1, mais falta para a fatia indicada fechar o kernel.

Uso:
    python -I rodar_kernels.py [--pasta PASTA] [--forja CAMINHO] [--kernel NOME ...] [--repeticoes 5]

Saida: 0 se todos os kernels conferem; 1 se algum difere ou falha; 2 em erro de uso, de ambiente ou de oraculo (ErroOraculo).

Versao: 1.1.0
Changelog: 1.1.0 - 0.21.0 F3: o gcc -O2 vem do binario congelado (BinarioDeReferencia); CompilarC e conf_c saem; ErroOraculo vira exit 2.
           1.0.0 - M7b F0c: runner dos 15 kernels, conferencia dos dois backends contra o .esperado, mediana de 5,
           razao Forja/gcc-O2 e fatia que fecha o kernel (vem do catalogo de gerar_kernels.py).
Verificacao: python -I rodar_kernels.py --repeticoes 1 --kernel f1_div_u32_const
"""

import argparse
import math
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.dont_write_bytecode = True
PastaAqui = Path(__file__).resolve().parent
sys.path.insert(0, str(PastaAqui))
sys.path.insert(0, str(PastaAqui.parents[1] / "rapido"))

from gerar_kernels import (  # noqa: E402  (import depois do sys.path: modulo irmao)
    CATALOGO,
    CompilarX64,
    ErroKernel,
    FORJA_PADRAO,
    ResolverForja,
    RodarPrograma,
)
import oraculo_congelado as Oraculo  # noqa: E402  (pasta rapido\: binario gcc de referencia)

REPETICOES_PADRAO = 5
PASTA_PADRAO = PastaAqui / "gerados"
VERSAO = "1.1.0"


def DividirValor(Dividendo, Divisor):
    # Divisor vem de medicao: zero vira 0.0 em vez de ZeroDivisionError (CROSS-R02).
    if Divisor == 0:
        return 0.0
    return Dividendo / Divisor


def Normalizar(Texto):
    return Texto.replace("\r\n", "\n").strip()


def MedirPrograma(Exe, Pasta, Repeticoes):
    # Aquecimento (1 rodada, confere a saida) e depois Repeticoes cronometradas; devolve (saida, mediana em ms).
    Saida = RodarPrograma(Exe, Pasta)
    Tempos = []
    for _ in range(Repeticoes):
        Inicio = time.perf_counter()
        RodarPrograma(Exe, Pasta)
        Tempos.append((time.perf_counter() - Inicio) * 1000.0)
    return Saida, statistics.median(Tempos)


def LerEsperado(Pasta, Nome):
    Caminho = Pasta / f"{Nome}.esperado.txt"
    if not Caminho.is_file():
        raise ErroKernel(f"{Nome}: .esperado.txt ausente em {Pasta} (o --gerar foi aposentado na 0.21.0 F3; kernel novo precisa de .esperado de fonte independente)")
    return Normalizar(Caminho.read_text(encoding="utf-8"))


def RodarUm(Kernel, Pasta, Forja, PastaTemp, Repeticoes):
    Nome = Kernel.nome
    Fonte = Pasta / f"{Nome}.fj"
    if not Fonte.is_file():
        raise ErroKernel(f"{Nome}: {Fonte.name} ausente em {Pasta}")
    Esperado = LerEsperado(Pasta, Nome)
    ExeX64 = CompilarX64(Forja, Fonte, PastaTemp)
    ExeC = Oraculo.BinarioDeReferencia(Fonte, "win", "O2")
    SaidaX64, MsX64 = MedirPrograma(ExeX64, PastaTemp, Repeticoes)
    _, MsC = MedirPrograma(ExeC, PastaTemp, Repeticoes)
    return {
        "nome": Nome,
        "fatia": f"{Kernel.fatia} {Kernel.nome_fatia}",
        "conf_x64": Normalizar(SaidaX64) == Esperado,
        "ms_x64": MsX64,
        "ms_c": MsC,
        "razao": DividirValor(MsX64, MsC),
    }


def ImprimirTabela(Linhas):
    print("kernel | fatia que fecha | x64 ms | gcc-O2 ms | razao Forja/gcc-O2 | saida x64")
    for L in Linhas:
        print(f"{L['nome']} | {L['fatia']} | {L['ms_x64']:.1f} | {L['ms_c']:.1f} | {L['razao']:.2f} | "
              f"{'OK' if L['conf_x64'] else 'DIFERE'}")


def ImprimirResumo(Linhas):
    Validas = [L["razao"] for L in Linhas if L["razao"] > 0]
    Geomedia = 1.0 if not Validas else math.prod(Validas) ** (1.0 / len(Validas))
    TotalX64Certo = sum(1 for L in Linhas if L["conf_x64"])
    print(f"conferencia: x64 {TotalX64Certo}/{len(Linhas)} iguais ao .esperado")
    print(f"razao Forja/gcc-O2 geomedia: {Geomedia:.2f}")


def ResolverKernels(Nomes):
    if not Nomes:
        return list(CATALOGO)
    Conhecidos = {K.nome: K for K in CATALOGO}
    Faltando = [N for N in Nomes if N not in Conhecidos]
    if Faltando:
        raise ErroKernel(f"kernel(s) inexistente(s): {', '.join(Faltando)}")
    return [Conhecidos[N] for N in Nomes]


def RodarTodos(Kernels, Pasta, Forja, Repeticoes):
    PastaTemp = Path(tempfile.mkdtemp(prefix="kernels_rodar_"))
    Linhas, Falhas = [], []
    try:
        for K in Kernels:
            try:
                Linhas.append(RodarUm(K, Pasta, Forja, PastaTemp, Repeticoes))
            except ErroKernel as Erro:
                Falhas.append(str(Erro))
    finally:
        shutil.rmtree(PastaTemp, ignore_errors=True)
    return Linhas, Falhas


def MontarArgumentos():
    Ap = argparse.ArgumentParser(description="Roda os kernels do M7b: confere x64 e backend C e mede a razao.")
    Ap.add_argument("--pasta", default=str(PASTA_PADRAO), help="pasta com os .fj e .esperado.txt")
    Ap.add_argument("--forja", default=FORJA_PADRAO, help="caminho absoluto do forja.exe")
    Ap.add_argument("--kernel", nargs="+", default=None, help="so estes kernels")
    Ap.add_argument("--repeticoes", type=int, default=REPETICOES_PADRAO, help="medicoes cronometradas (padrao 5)")
    return Ap


def Principal(Argumentos=None):
    Args = MontarArgumentos().parse_args(Argumentos)
    if Args.repeticoes < 1:
        print("ERRO: --repeticoes precisa ser >= 1", file=sys.stderr)
        return 2
    try:
        Forja = ResolverForja(Args.forja)
        Kernels = ResolverKernels(Args.kernel)
        Pasta = Path(Args.pasta).resolve()
        Linhas, Falhas = RodarTodos(Kernels, Pasta, Forja, Args.repeticoes)
    except (ErroKernel, Oraculo.ErroOraculo) as Erro:
        print(f"ERRO: {Erro}", file=sys.stderr)
        return 2
    ImprimirTabela(Linhas)
    ImprimirResumo(Linhas)
    for Falha in Falhas:
        print(f"FALHOU: {Falha}")
    Divergentes = [L["nome"] for L in Linhas if not L["conf_x64"]]
    if Divergentes or Falhas:
        print(f"divergentes: {', '.join(Divergentes) or '-'}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
