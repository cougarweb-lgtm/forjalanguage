# -*- coding: utf-8 -*-
"""Preparo do medir_rapido: compila SO o x64 release (1x por lado), classifica por SHA e confere a saida com o .esperado.

Objetivo: decidir quais benches mudaram de exe (so esses vao ao cronometro) gastando o minimo: nada de O0, O2 nem
backend C. Reaproveita do medir_bench: ListarBenchs, CompilarX64 (--backend x64 --release, checagem padrao), ShaDoArquivo.
Tambem prepara os kernels (otim\\kernels\\gerados) com o mesmo criterio.

Uso: importado por medir_rapido.py (rodar sob trava COMPARTILHADA: compila e roda 1x, sem cronometrar).
Verificacao: python -I -B preparo_rapido.py --selftest
Versao: 1.0.0
Changelog: 1.0.0 - primeira versao (decisao D0696).
"""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import NamedTuple

sys.dont_write_bytecode = True
PastaAqui = Path(__file__).resolve().parent
PastaNativo = PastaAqui.parent
PastaKernels = PastaNativo / "otim" / "kernels"
for Pasta in (PastaAqui, PastaNativo, PastaKernels):
    if str(Pasta) not in sys.path:
        sys.path.insert(0, str(Pasta))

import medir_bench as MB  # noqa: E402  (modulo irmao, nao alterado)

VERSAO = "1.0.0"
PASTA_KERNELS_GERADOS = PastaKernels / "gerados"
TIMEOUT_CORRETUDE_S = 300


class LadosCompilados(NamedTuple):
    Nome: str
    ExeBase: Path
    ExeNovo: Path
    Mudou: bool


def NormalizarSaida(Bytes):
    return Bytes.replace(b"\r\n", b"\n")


def ConferirSaida(Exe, Fonte, Esperado):
    """Roda o exe 1x e exige exit 0 e stdout igual ao .esperado (CRLF -> LF)."""
    Resultado = subprocess.run([str(Exe)], capture_output=True, cwd=str(Exe.parent), timeout=TIMEOUT_CORRETUDE_S)
    if Resultado.returncode != 0:
        raise MB.ErroBench(f"{Fonte.stem}: {Exe.name} saiu com {Resultado.returncode}")
    if NormalizarSaida(Resultado.stdout) != NormalizarSaida(Esperado):
        raise MB.ErroBench(f"{Fonte.stem}: saida de {Exe.name} difere do .esperado: {Resultado.stdout[:200]!r}")


def CompilarLado(Forja, Fonte, Pasta):
    """x64 release do bench em Pasta/<nome-do-forja>; devolve o caminho do exe."""
    Pasta.mkdir(parents=True, exist_ok=True)
    return MB.CompilarX64(Forja, Fonte, Pasta)


def CompilarLados(Fonte, Forjas, Pasta):
    """Compila base e novo (1x cada), classifica por SHA e confere a saida (so 1x quando o exe e identico)."""
    Base, Novo = Forjas
    ExeBase = CompilarLado(Base, Fonte, Pasta / "base")
    ExeNovo = CompilarLado(Novo, Fonte, Pasta / "novo")
    Mudou = MB.ShaDoArquivo(ExeBase) != MB.ShaDoArquivo(ExeNovo)
    Esperado = Fonte.with_suffix(".esperado.txt").read_bytes()
    ConferirSaida(ExeBase, Fonte, Esperado)
    if Mudou:
        ConferirSaida(ExeNovo, Fonte, Esperado)
    return LadosCompilados(Fonte.stem, ExeBase, ExeNovo, Mudou)


def CompilarBenchs(Nomes, Forjas, Pasta):
    """Todos os benches pedidos (None = os 14). Devolve ([LadosCompilados], {nome: erro}); falha de um nao derruba os outros."""
    Lados, Erros = [], {}
    for Fonte in MB.ListarBenchs(Nomes):
        try:
            Lados.append(CompilarLados(Fonte, Forjas, Pasta / Fonte.stem))
        except (MB.ErroBench, subprocess.TimeoutExpired) as Erro:
            Erros[Fonte.stem] = str(Erro)
    return Lados, Erros


def ListarKernels():
    Fontes = sorted(PASTA_KERNELS_GERADOS.glob("*.fj"))
    if not Fontes:
        raise MB.ErroBench(f"nenhum kernel em {PASTA_KERNELS_GERADOS}")
    return Fontes


def CompilarKernels(Forjas, Pasta):
    """Os kernels gerados, x64 release com base e novo; a saida tem de bater com o .esperado (zero gcc)."""
    Lados, Erros = [], {}
    for Fonte in ListarKernels():
        try:
            Lados.append(CompilarLados(Fonte, Forjas, Pasta / Fonte.stem))
        except (MB.ErroBench, subprocess.TimeoutExpired) as Erro:
            Erros[Fonte.stem] = str(Erro)
    return Lados, Erros


def Afirmar(Condicao, Mensagem):
    if not Condicao:
        raise AssertionError(Mensagem)


def TesteNormalizar():
    Afirmar(NormalizarSaida(b"a\r\nb\r\n") == b"a\nb\n", "CRLF vira LF")


def TesteConferirSaida():
    Pasta = Path(tempfile.mkdtemp(prefix="preparo_rapido_selftest_"))
    try:
        Fonte = Pasta / "x.fj"
        Programa = Pasta / "oi.bat"
        Programa.write_bytes(b"@echo off\r\necho oi\r\n")
        ConferirSaida(Programa, Fonte, b"oi\n")
        try:
            ConferirSaida(Programa, Fonte, b"diferente")
        except MB.ErroBench:  # lint-ok: iaf-erro-engolido a divergencia levantar ErroBench e o resultado esperado
            return
        Afirmar(False, "saida diferente deveria falhar")
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def TesteListagem():
    Afirmar(len(MB.ListarBenchs(None)) == 14, "14 benches em tests\\nativo\\bench")
    Afirmar(len(ListarKernels()) == 15, "15 kernels gerados")


def Selftest():
    for Teste in (TesteNormalizar, TesteConferirSaida, TesteListagem):
        Teste()
    print(f"preparo_rapido {VERSAO}: selftest OK")
    return True


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(0 if Selftest() else 1)
    print(__doc__)
