# -*- coding: utf-8 -*-
"""Agregador do amostrador de RIP (amostrador.c, P24b do M4): self time, dono e etapa do pipeline.

Cada amostra e (RIP absoluto, chamador RVA ou 0). RIP dentro do forja.exe: a funcao do forja.exe (nm -n, menos o ImageBase
preferido) e o dono. RIP fora (ntdll, ucrtbase...): o nome e a exportacao mais proxima da DLL (exportacoes_pe.py; 'interno'
se longe de qualquer exportacao) e o dono e a funcao do forja.exe que chamou (chamador RVA); sem chamador = sem_chamador.
Etapa = arquivo fonte que define a funcao (MapearFuncoes sobre o compiler\\ original; third\\fadec = codificar).
Uso: importado por perfil_forja.py. Verificacao: python -I tests\\nativo\\perfil\\agregar_amostras.py --selftest
Versao: 0.2.0
Changelog: 0.2.0 - M4 P24b: dono por varredura de pilha (chamador RVA do amostrador 0.2.0), nome das exportacoes de DLL,
           categorias do sistema (alocacao, memset/memcpy, Nt*), tabela por dono e por etapa com a referencia (a).
           0.1.0 - M4 P24b: primeira versao (nm -n, RVA -> funcao, etapa por arquivo, tabela de self time).
"""

import bisect
import collections
import ctypes
import re
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import exportacoes_pe as Exportacoes  # noqa: E402

PASTAS_FONTE_C = ("common", "frontend", "semantic", "ir", "backend", "nativo", "layout", "cache", "driver", "third")
# Ordem importa: a 1a regra que casa com o caminho relativo (dentro de compiler\ ou third\) vence.
REGRAS_ETAPA = (
    ("frontend/lexer", "lex"), ("frontend/", "parse"), ("semantic/", "check"), ("ir/", "ir"), ("nativo/bir/", "bir"),
    ("third/fadec/", "codificar"),
    ("nativo/x64/x64_isel", "isel"), ("nativo/x64/x64_gerar", "isel"), ("nativo/x64/x64_alocar", "regs"), ("nativo/x64/x64_abi", "regs"),
    ("nativo/x64/x64_listar", "outros"), ("nativo/x64/x64_formatar", "outros"), ("nativo/x64/", "codificar"),
    ("nativo/obj/", "ligar"), ("nativo/fmt/", "escrever"), ("common/", "comum"),
)
DLLS_DO_SISTEMA = ("ntdll.dll", "KERNELBASE.dll", "kernel32.dll", "msvcrt.dll", "ucrtbase.dll")
NOMES_ALOCACAO = frozenset({"RtlAllocateHeap", "RtlFreeHeap", "RtlReAllocateHeap", "HeapAlloc", "HeapFree", "HeapReAlloc",
                            "malloc", "calloc", "realloc", "free", "_malloc_base", "_calloc_base", "_realloc_base", "_free_base",
                            "_aligned_malloc", "_aligned_free", "_aligned_realloc", "_recalloc", "LocalAlloc", "LocalFree",
                            "GlobalAlloc", "GlobalFree"})
NOMES_COPIA_ZERA = frozenset({"memset", "memcpy", "memmove", "memcmp", "RtlFillMemory", "RtlZeroMemory", "RtlCopyMemory", "RtlMoveMemory"})
PadraoDefinicao = re.compile(r"^[A-Za-z_][\w\s\*]*?\b([A-Za-z_]\w*)\s*\(")
PadraoAtributo = re.compile(r"__attribute__\s*\(\(.*?\)\)\)*\s*")
PadraoSimbolo = re.compile(r"^([0-9a-fA-F]+)\s+[TtWw]\s+(\S+)$")
ESISTEMA = "sistema"
SEM_FONTE = "sem_fonte"
SEM_CHAMADOR = "sem_chamador"


def MapearFuncoes(PastaCompilador):
    """nome -> caminho relativo (dentro de compiler\\) do .c que define a funcao; a 1a definicao vence."""
    Mapa = {}
    for Pasta in PASTAS_FONTE_C:
        for Arquivo in sorted((PastaCompilador / Pasta).rglob("*.c")):
            Relativo = Arquivo.relative_to(PastaCompilador).as_posix()
            for Linha in Arquivo.read_text(encoding="utf-8", errors="replace").splitlines():
                Casa = PadraoDefinicao.match(PadraoAtributo.sub("", Linha))
                if Casa and not Linha.rstrip().endswith(";"):
                    Mapa.setdefault(Casa.group(1), Relativo)
    return Mapa


def EtapaDoArquivo(Relativo):
    """Etapa do pipeline (lex..escrever, comum ou outros) pelo caminho do arquivo que define a funcao."""
    for Prefixo, Etapa in REGRAS_ETAPA:
        if Relativo.startswith(Prefixo):
            return Etapa
    return "outros"


def EtapaDaFuncao(Nome, Mapa):
    if Nome in (SEM_CHAMADOR, ESISTEMA):
        return "sistema"
    Relativo = Mapa.get(Nome)
    return EtapaDoArquivo(Relativo) if Relativo else SEM_FONTE


def CategoriaDoNome(Nome):
    """Categoria de uma exportacao do sistema: alocacao/liberacao, memset/memcpy, syscall Nt*, ou resto."""
    if Nome in NOMES_ALOCACAO:
        return "alocacao_liberacao"
    if Nome in NOMES_COPIA_ZERA:
        return "memset_memcpy"
    if Nome.startswith(("Nt", "Zw")):
        return "syscall_Nt"
    if Nome == "interno":
        return "dll_interno"
    return "outros_dll"


def LerPreferida(Exe):
    """ImageBase preferido do PE64: campo do Optional Header (e_lfanew + 4 + 20 + 24)."""
    Cabecalho = Path(Exe).read_bytes()[:4096]
    Nt = struct.unpack_from("<I", Cabecalho, 0x3C)[0]
    return struct.unpack_from("<Q", Cabecalho, Nt + 48)[0]


def LerSimbolos(Exe):
    """(RVA ordenado, nomes) das funcoes do forja.exe: nm -n (linha 'endereco T nome') menos o ImageBase preferido."""
    Feito = subprocess.run(["nm", "-n", str(Exe)], capture_output=True, text=True, encoding="utf-8", errors="replace")
    Preferida = LerPreferida(Exe)
    Pares = []
    for Linha in Feito.stdout.splitlines():
        Casa = PadraoSimbolo.match(Linha.strip())
        if Casa:
            Pares.append((int(Casa.group(1), 16) - Preferida, Casa.group(2)))
    if not Pares:
        raise SystemExit("X0003 nm sem simbolos em " + str(Exe))
    return [Rva for Rva, _ in Pares], [Nome for _, Nome in Pares]


def LerAmostras(Arquivo):
    """(base, tamanho, [(RIP absoluto, chamador RVA ou 0)]) do arquivo do amostrador 0.2.0."""
    Partes = Path(Arquivo).read_text(encoding="utf-8").split()
    Pares = [(int(Partes[Indice], 16), int(Partes[Indice + 1], 16)) for Indice in range(7, len(Partes) - 1, 2)]
    return int(Partes[1], 16), int(Partes[3], 16), Pares


def FuncaoDaRva(Rva, Tamanho, Enderecos, Nomes):
    """Funcao que contem a RVA; fora da imagem (ou antes do 1o simbolo) = sistema."""
    if Rva < 0 or Rva >= Tamanho:
        return ESISTEMA
    Indice = bisect.bisect_right(Enderecos, Rva) - 1
    return Nomes[Indice] if Indice >= 0 else ESISTEMA


def ModulosDoSistema():
    """(nome, base, tamanho) das DLLs do sistema nesta ferramenta: ntdll/kernel32 ficam na mesma base em todo processo da sessao."""
    Kernel = ctypes.WinDLL("kernel32")
    Kernel.GetModuleHandleW.restype = ctypes.c_void_p
    Kernel.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
    Modulos = []
    for Nome in DLLS_DO_SISTEMA:
        Base = Kernel.GetModuleHandleW(Nome)
        if Base:
            Nt = struct.unpack_from("<I", ctypes.string_at(Base, 0x40), 0x3C)[0]
            Tamanho = struct.unpack_from("<I", ctypes.string_at(Base + Nt + 24, 60), 56)[0]
            Modulos.append((Nome, Base, Tamanho))
    return Modulos


def NomeFora(Rip, Ctx):
    """(DLL, exportacao mais proxima ou 'interno') de um RIP fora do forja.exe; fora de todas = ('outro', 'interno')."""
    for Nome, Base, Tamanho in Ctx["modulos"]:
        if Base <= Rip < Base + Tamanho:
            if Nome not in Ctx["tabelas"]:
                Ctx["tabelas"][Nome] = Exportacoes.LerExportacoes(Exportacoes.DllDoSistema(Nome))
            return Nome, Exportacoes.NomeDaExportacao(Ctx["tabelas"][Nome], Rip - Base)
    return "outro", "interno"


def ContarAmostra(Acumulo, Rip, Chamador, Ctx):
    """Conta 1 amostra no dono: a funcao do forja.exe (dentro) ou o forja.exe que chamou o sistema (fora)."""
    if Ctx["base"] <= Rip < Ctx["base"] + Ctx["tamanho"]:
        Nome = FuncaoDaRva(Rip - Ctx["base"], Ctx["tamanho"], Ctx["enderecos"], Ctx["nomes"])
        Acumulo["self"][Nome] += 1
        Acumulo["dono"][Nome] += 1
        return
    Dll, Exportacao = NomeFora(Rip, Ctx)
    Acumulo["sistema"][Dll + "!" + Exportacao] += 1
    Acumulo["categoria"][CategoriaDoNome(Exportacao)] += 1
    Dono = FuncaoDaRva(Chamador, Ctx["tamanho"], Ctx["enderecos"], Ctx["nomes"]) if Chamador else SEM_CHAMADOR
    Acumulo["dono"][Dono] += 1
    Acumulo["chamador"][Dono] += 1


def AgregarAmostras(Arquivos, Exe, Mapa):
    """Conta todas as amostras; devolve {'self', 'sistema', 'categoria', 'chamador', 'dono', 'etapas', 'total', 'mapa'}."""
    Enderecos, Nomes = LerSimbolos(Exe)
    Acumulo = {Chave: collections.Counter() for Chave in ("self", "sistema", "categoria", "chamador", "dono")}
    Ctx = {"enderecos": Enderecos, "nomes": Nomes, "modulos": ModulosDoSistema(), "tabelas": {}, "base": 0, "tamanho": 0}
    Total = 0
    for Arquivo in Arquivos:
        Ctx["base"], Ctx["tamanho"], Pares = LerAmostras(Arquivo)
        Total += len(Pares)
        for Rip, Chamador in Pares:
            ContarAmostra(Acumulo, Rip, Chamador, Ctx)
    Etapas = collections.Counter()
    for Nome, Quantidade in Acumulo["dono"].items():
        Etapas[EtapaDaFuncao(Nome, Mapa)] += Quantidade
    return {**Acumulo, "etapas": Etapas, "total": Total, "mapa": Mapa}


def TabelaDonos(Resultado, MsTotal, Top):
    """(b1) tempo por funcao do forja.exe (self + sistema chamado por ela), top N, com a etapa."""
    Total = max(Resultado["total"], 1)
    Mapa = Resultado["mapa"]
    Linhas = ["pos | ms/build | %total | self ms | de sistema ms | etapa | arquivo:funcao"]
    for Posicao, (Nome, Quantidade) in enumerate(Resultado["dono"].most_common(Top), 1):
        Local = Mapa.get(Nome) or "(sem fonte)"
        Linhas.append(f"{Posicao:>3} | {Quantidade / Total * MsTotal:8.2f} | {Quantidade / Total * 100:6.2f} | "
                      f"{Resultado['self'][Nome] / Total * MsTotal:7.2f} | {Resultado['chamador'][Nome] / Total * MsTotal:7.2f} | "
                      f"{EtapaDaFuncao(Nome, Mapa):<9} | {Local}:{Nome}")
    return "\n".join(Linhas)


def TabelaEtapasDonos(Resultado, MsTotal, Referencia):
    """(b2) por etapa, com a referencia (a) em ms e a % de cada um: a distribuicao deve se aproximar de (a)."""
    Total = max(Resultado["total"], 1)
    RefTotal = sum(Referencia.values()) or 1.0
    Linhas = ["etapa | ms amostrador | % amostrador | ms (a) | % (a)"]
    for Etapa in ("lex", "parse", "check", "ir", "bir", "isel", "regs", "codificar", "ligar", "escrever", "comum", "outros", "sistema", SEM_FONTE):
        Qtd = Resultado["etapas"].get(Etapa, 0)
        Ref = Referencia.get(Etapa, 0.0)
        Linhas.append(f"{Etapa:<9} | {Qtd / Total * MsTotal:8.1f} | {Qtd / Total * 100:6.1f} | {Ref:7.1f} | {Ref / RefTotal * 100:6.1f}")
    return "\n".join(Linhas)


def TabelaSistema(Resultado):
    """(b3) o que fica fora do forja.exe: categorias (% do total) e as exportacoes mais quentes."""
    Total = max(Resultado["total"], 1)
    Linhas = ["fora do forja.exe, por categoria (% do total): " + " ; ".join(
        f"{Nome} {Quantidade / Total * 100:.1f}%" for Nome, Quantidade in Resultado["categoria"].most_common())]
    Linhas.append("exportacoes mais quentes (% do total): " + " ; ".join(
        f"{Nome} {Quantidade / Total * 100:.1f}%" for Nome, Quantidade in Resultado["sistema"].most_common(12)))
    Linhas.append(f"sem_chamador (fora do forja.exe sem call achado): {Resultado['chamador'][SEM_CHAMADOR] / Total * 100:.1f}% do total")
    return "\n".join(Linhas)


def AutoTeste():
    """Prova sem compilar: RVA -> funcao, etapa por arquivo e categoria do sistema."""
    Enderecos, Nomes, Mapa = [0x1000, 0x2000], ["fa", "fb"], {"X64Gerar": "nativo/x64/x64_gerar.c"}
    Casos = (
        FuncaoDaRva(0x1500, 0x4000, Enderecos, Nomes) == "fa",
        FuncaoDaRva(0x2FFF, 0x4000, Enderecos, Nomes) == "fb",
        FuncaoDaRva(0x9000, 0x4000, Enderecos, Nomes) == ESISTEMA,
        EtapaDaFuncao("X64Gerar", Mapa) == "isel",
        EtapaDaFuncao(SEM_CHAMADOR, Mapa) == "sistema",
        EtapaDoArquivo("third/fadec/encode.c") == "codificar",
        CategoriaDoNome("RtlAllocateHeap") == "alocacao_liberacao" and CategoriaDoNome("memset") == "memset_memcpy",
        CategoriaDoNome("NtWriteFile") == "syscall_Nt",
    )
    if not all(Casos):
        raise SystemExit("X0008 selftest do agregador falhou: " + repr(Casos))
    print("selftest OK")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        AutoTeste()
