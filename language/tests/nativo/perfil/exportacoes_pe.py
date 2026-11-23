# -*- coding: utf-8 -*-
"""Exportacoes nomeadas de um PE64 lido do disco: nome da funcao de uma DLL do sistema pelo RVA.

Usado pelo agregar_amostras.py para dar nome aos RIPs que caem em ntdll, ucrtbase e kernel32. RVA sem exportacao
a menos de LIMITE_INTERNO bytes = "interno" (funcao sem nome exportado).
Uso: importado. Verificacao: python -I tests\\nativo\\perfil\\exportacoes_pe.py --selftest
Versao: 0.1.0
Changelog: 0.1.0 - M4 P24b: primeira versao (diretorio de exportacao PE64, RVA -> exportacao mais proxima, limite de 4 KB).
"""

import bisect
import os
import struct
import sys
from pathlib import Path

LIMITE_INTERNO = 4096


def SecoesDoPe(Dados, Nt):
    """(RVA, tamanho, offset no arquivo) de cada secao do PE64."""
    NumSecoes = struct.unpack_from("<H", Dados, Nt + 6)[0]
    TamOpt = struct.unpack_from("<H", Dados, Nt + 20)[0]
    Saida = []
    for Indice in range(NumSecoes):
        Virtual, Rva, Bruto, Ptr = struct.unpack_from("<IIII", Dados, Nt + 24 + TamOpt + Indice * 40 + 8)
        Saida.append((Rva, max(Virtual, Bruto), Ptr))
    return Saida


def ParaOffset(Secoes, Rva):
    for SecaoRva, Tamanho, Ptr in Secoes:
        if SecaoRva <= Rva < SecaoRva + Tamanho:
            return Rva - SecaoRva + Ptr
    raise SystemExit("X0009 RVA " + hex(Rva) + " fora das secoes")


def LerExportacoes(Caminho):
    """(RVA ordenado, nomes) das exportacoes nomeadas do PE64 em Caminho."""
    Dados = Path(Caminho).read_bytes()
    Nt = struct.unpack_from("<I", Dados, 0x3C)[0]
    Secoes = SecoesDoPe(Dados, Nt)
    Pos = ParaOffset(Secoes, struct.unpack_from("<I", Dados, Nt + 24 + 112)[0])
    Quantidade = struct.unpack_from("<I", Dados, Pos + 24)[0]
    FuncRva, NomeRva, OrdRva = struct.unpack_from("<III", Dados, Pos + 28)
    Pares = []
    for Indice in range(Quantidade):
        PtrNome = struct.unpack_from("<I", Dados, ParaOffset(Secoes, NomeRva) + 4 * Indice)[0]
        Ordinal = struct.unpack_from("<H", Dados, ParaOffset(Secoes, OrdRva) + 2 * Indice)[0]
        Funcao = struct.unpack_from("<I", Dados, ParaOffset(Secoes, FuncRva) + 4 * Ordinal)[0]
        Ini = ParaOffset(Secoes, PtrNome)
        Pares.append((Funcao, Dados[Ini:Dados.index(b"\0", Ini)].decode("ascii", "replace")))
    Pares.sort()
    return [Rva for Rva, _ in Pares], [Nome for _, Nome in Pares]


def NomeDaExportacao(Tabela, Rva):
    """Nome da exportacao de maior RVA <= Rva; 'interno' se nao ha exportacao a menos de LIMITE_INTERNO."""
    Enderecos, Nomes = Tabela
    Indice = bisect.bisect_right(Enderecos, Rva) - 1
    if Indice < 0 or Rva - Enderecos[Indice] > LIMITE_INTERNO:
        return "interno"
    return Nomes[Indice]


def DllDoSistema(Nome):
    return Path(os.environ["SystemRoot"]) / "System32" / Nome


def AutoTeste():
    Tabela = LerExportacoes(DllDoSistema("ntdll.dll"))
    if len(Tabela[1]) < 1000 or "RtlAllocateHeap" not in Tabela[1]:
        raise SystemExit("X0008 selftest exportacoes: ntdll sem os nomes esperados")
    Rva = Tabela[0][Tabela[1].index("RtlAllocateHeap")]
    if NomeDaExportacao(Tabela, Rva + 1) != "RtlAllocateHeap" or NomeDaExportacao(Tabela, 0) != "interno":
        raise SystemExit("X0008 selftest exportacoes: busca de nome falhou")
    print("selftest OK")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        AutoTeste()
