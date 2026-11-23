#!/usr/bin/env python3
"""verificar_pdb.py - PDB/CodeView e minidump pelo dbghelp.dll do System32, por ctypes (M6b P74, D-M6-10).

Versao: 0.4.0
Changelog: 0.4.0 - P78: o modo minidump imprime a linha de cada quadro do exe (arquivo.fj:linha; quadros de cima com
                   endereco de retorno -1) e lista 'pilha .fj: proc arquivo:linha' so com os quadros que tem linha
                   (o runtime de entrada fica de fora); --esperado <arq> (opcional) compara
                   essa lista com o .esperado e falha se diferir. ReportarPilha/ModoMinidump ganham o keyword Esperado.
           0.3.0 - P77: o modo locais saiu para verificar_pdb_locais.py (limite de 800 linhas); --exigir/--gdb/--esperado
                   ficam aqui e o despacho importa o modulo novo. Corrige no modo locais: breakpoint na linha do .gdb
                   (nao no prologo) e SYMBOL_INFO.Address como deslocamento de RBP (nao endereco absoluto). --selftest
                   roda tambem o da formatacao de locais. Constantes e funcoes so do modo locais saem daqui.
           0.2.1 - P76a: --so-procs no modo linhas (SymFromAddr por proc; as linhas C13 entram no P76c).
           0.2.0 - P76-0: modo linhas guiado pelo JSON do forja (--debug-code linhas,json, registros tipo "linhas"):
                   para cada proc, SymFromAddr(base do .text + inicio) tem de dar o nome Forja (funcao); para cada linha,
                   SymGetLineFromAddr64(base do .text + off) tem de dar arquivo:linha iguais. Sai do SymEnumSymbols (o
                   PDB do P76 precisa so do modulo, sem GSI). JSON ausente ou sem registro = FALHOU com a causa.
                   --selftest offline da comparacao (sem dbghelp): bate, nome errado, linha errada e sem simbolo.
                   0.1.0 - P74: modos modulo (SymLoadModuleEx + SymGetModuleInfo64: SymType e PDB casado com --exigir-pdb),
                   linhas (procs por SymEnumSymbols), locais (processo depurado, breakpoint apos o prologo do proc,
                   SymSetContext + SymEnumSymbols), minidump (MiniDumpWriteDump de exe parado em Sleep dentro de
                   proc de 3 niveis; o .dmp e lido aqui e a pilha e andada com StackWalk64).
                   --controles (P74): modulo em gabarito.c (gcc, sem PDB) e em cmd.exe; minidump em gabarito_dormir.c.
Uso: python verificar_pdb.py --selftest
     python verificar_pdb.py --controles
     python verificar_pdb.py --modo modulo --exe <pe> [--base 0x10000000] [--exigir-pdb]
     python verificar_pdb.py --modo linhas --exe <pe> [--base 0x10000000] --json <arquivo.debug.jsonl>
     python verificar_pdb.py --modo locais --exe <pe> --proc <nome> [--gdb <.gdb>] [--esperado <arq>] [--json <arq>]
     python verificar_pdb.py --modo minidump --exe <pe> [--espera-ms 1500] [--dorme-ms 60000] [--dmp <arquivo>] [--esperado <arq>]
Depende de: dbghelp.dll (System32); gcc (MinGW) so no --controles.
Saida: 0 PASSOU; 1 FALHOU; 2 ferramenta ausente.
"""
import argparse
import ctypes
import json
import os
import struct
import subprocess
import sys
import tempfile
import time
from ctypes import POINTER
from pathlib import Path

PASTA = Path(__file__).resolve().parent
GABARITO_C = PASTA.parent / "m6" / "gabarito" / "gabarito.c"
GABARITO_DORMIR_C = PASTA / "gabarito_dormir.c"
FIXTURAS_GDB = PASTA.parent / "m6" / "gdb"
SYSTEM32 = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32"
DBGHELP_DLL = SYSTEM32 / "dbghelp.dll"
CMD_EXE = SYSTEM32 / "cmd.exe"
BASE_PADRAO = 0x10000000
BASE_GCC = 0x10000000
BASE_CMD = 0x20000000
OPCOES_SIMBOLOS = 0x00000002 | 0x00000010 | 0x00080000  # SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_NO_PROMPTS
TIPOS_SIMBOLO = ("SymNone", "SymCoff", "SymCv", "SymPdb", "SymExport", "SymDeferred", "SymSym", "SymDia", "SymVirtual")
SYM_PDB = 3
TAM_NOME_SIMBOLO = 2048
MAQUINA_AMD64 = 0x8664
MODO_FLAT = 3
SEM_JANELA = 0x08000000
DUMP_NORMAL = 0
PROFUNDIDADE_MAX = 64
ASSINATURA_MDMP = 0x504D444D
FLUXO_THREADS = 3
FLUXO_MODULOS = 4
FLUXO_MEMORIA = 5
FLUXO_MEMORIA64 = 9
ALOCAR_COMMIT_RESERVA = 0x3000
MEM_LIBERAR = 0x8000
PAGINA_LEITURA_ESCRITA = 0x04
ARQUIVO_ESCRITA = 0x40000000
ARQUIVO_CRIAR_SEMPRE = 2
ARQUIVO_NORMAL = 0x80
ARQUIVO_INVALIDO = 0xFFFFFFFFFFFFFFFF
SUSPENSO_FALHOU = 0xFFFFFFFF

H = ctypes.c_void_p
U64 = ctypes.c_uint64
D = ctypes.c_uint32
B = ctypes.c_int32
TAM_PAGINA = 4096
CONTEXTO_ENDERECO = None


class ErroDbghelp(RuntimeError):
    """Chamada de dbghelp/kernel32 que devolveu falha; a mensagem traz a funcao e o GetLastError."""


class ImagehlpModulo(ctypes.Structure):
    _fields_ = [("SizeOfStruct", D), ("BaseOfImage", U64), ("ImageSize", D), ("TimeDateStamp", D), ("CheckSum", D),
                ("NumSyms", D), ("SymType", ctypes.c_int), ("ModuleName", ctypes.c_char * 32),
                ("ImageName", ctypes.c_char * 256), ("LoadedImageName", ctypes.c_char * 256),
                ("LoadedPdbName", ctypes.c_char * 256), ("CVSig", D), ("CVData", ctypes.c_char * 780),
                ("PdbSig", D), ("PdbSig70", ctypes.c_ubyte * 16), ("PdbAge", D), ("PdbUnmatched", B),
                ("DbgUnmatched", B), ("LineNumbers", B), ("GlobalSymbols", B), ("TypeInfo", B),
                ("SourceIndexed", B), ("Publics", B), ("MachineType", D), ("Reserved", D)]


class SimboloInfo(ctypes.Structure):
    _fields_ = [("SizeOfStruct", D), ("TypeIndex", D), ("Reserved", U64 * 2), ("Index", D), ("Size", D),
                ("ModBase", U64), ("Flags", D), ("Value", U64), ("Address", U64), ("Register", D), ("Scope", D),
                ("Tag", D), ("NameLen", D), ("MaxNameLen", D), ("Name", ctypes.c_char * TAM_NOME_SIMBOLO)]


class ImagehlpLinha(ctypes.Structure):
    _fields_ = [("SizeOfStruct", D), ("Key", H), ("LineNumber", D), ("FileName", ctypes.c_char_p), ("Address", U64)]


class ImagehlpQuadro(ctypes.Structure):
    _fields_ = [("InstructionOffset", U64), ("ReturnOffset", U64), ("FrameOffset", U64), ("StackOffset", U64),
                ("BackingStoreOffset", U64), ("FuncTableEntry", U64), ("Params", U64 * 4), ("Reserved", U64 * 5),
                ("Virtual", B), ("Reserved2", D)]


class EnderecoPilha(ctypes.Structure):
    _fields_ = [("Offset", U64), ("Segment", ctypes.c_uint16), ("Mode", ctypes.c_int)]


class AuxPilha(ctypes.Structure):
    _fields_ = [("Dados", U64 * 16)]


class QuadroPilha(ctypes.Structure):
    _fields_ = [("AddrPC", EnderecoPilha), ("AddrReturn", EnderecoPilha), ("AddrFrame", EnderecoPilha),
                ("AddrStack", EnderecoPilha), ("AddrBStore", EnderecoPilha), ("FuncTableEntry", H),
                ("Params", U64 * 4), ("Far", B), ("Virtual", B), ("Reserved", U64 * 3), ("KdHelp", AuxPilha)]


class ContextoX64(ctypes.Structure):
    _fields_ = [("P1Home", U64), ("P2Home", U64), ("P3Home", U64), ("P4Home", U64), ("P5Home", U64), ("P6Home", U64),
                ("ContextFlags", D), ("MxCsr", D), ("SegCs", ctypes.c_uint16), ("SegDs", ctypes.c_uint16),
                ("SegEs", ctypes.c_uint16), ("SegFs", ctypes.c_uint16), ("SegGs", ctypes.c_uint16),
                ("SegSs", ctypes.c_uint16), ("EFlags", D), ("Dr0", U64), ("Dr1", U64), ("Dr2", U64), ("Dr3", U64),
                ("Dr6", U64), ("Dr7", U64), ("Rax", U64), ("Rcx", U64), ("Rdx", U64), ("Rbx", U64), ("Rsp", U64),
                ("Rbp", U64), ("Rsi", U64), ("Rdi", U64), ("R8", U64), ("R9", U64), ("R10", U64), ("R11", U64),
                ("R12", U64), ("R13", U64), ("R14", U64), ("R15", U64), ("Rip", U64),
                ("FltSave", ctypes.c_ubyte * 512), ("VectorRegister", ctypes.c_ubyte * 416),
                ("VectorControl", U64), ("DebugControl", U64), ("LastBranchToRip", U64),
                ("LastBranchFromRip", U64), ("LastExceptionToRip", U64), ("LastExceptionFromRip", U64)]


class EventoDepuracao(ctypes.Structure):
    _fields_ = [("dwDebugEventCode", D), ("dwProcessId", D), ("dwThreadId", D), ("_alinhamento", D),
                ("u", ctypes.c_ubyte * 160)]


class InicioProcesso(ctypes.Structure):
    _fields_ = [("cb", D), ("lpReserved", ctypes.c_void_p), ("lpDesktop", ctypes.c_void_p),
                ("lpTitle", ctypes.c_void_p), ("dwX", D), ("dwY", D), ("dwXSize", D), ("dwYSize", D),
                ("dwXCountChars", D), ("dwYCountChars", D), ("dwFillAttribute", D), ("dwFlags", D),
                ("wShowWindow", ctypes.c_uint16), ("cbReserved2", ctypes.c_uint16),
                ("lpReserved2", ctypes.c_void_p), ("hStdInput", H), ("hStdOutput", H), ("hStdError", H)]


class InfoProcesso(ctypes.Structure):
    _fields_ = [("hProcess", H), ("hThread", H), ("dwProcessId", D), ("dwThreadId", D)]


CALLBACK_SIMBOLO = ctypes.WINFUNCTYPE(B, POINTER(SimboloInfo), D, H)
CALLBACK_LEITURA = ctypes.WINFUNCTYPE(B, H, U64, H, D, POINTER(D))


def TabelaDbghelp() -> list:
    """Assinaturas do dbghelp (ANSI). Sem argtypes o HANDLE de 64 bits (pseudo-handle -1) seria truncado."""
    return [
        ("SymSetOptions", D, [D]),
        ("SymInitialize", B, [H, ctypes.c_char_p, B]),
        ("SymCleanup", B, [H]),
        ("SymLoadModuleEx", U64, [H, H, ctypes.c_char_p, ctypes.c_char_p, U64, D, H, D]),
        ("SymGetModuleInfo64", B, [H, U64, POINTER(ImagehlpModulo)]),
        ("SymFromAddr", B, [H, U64, POINTER(U64), POINTER(SimboloInfo)]),
        ("SymFromName", B, [H, ctypes.c_char_p, POINTER(SimboloInfo)]),
        ("SymFromIndex", B, [H, U64, D, POINTER(SimboloInfo)]),
        ("SymGetTypeInfo", B, [H, U64, D, D, H]),
        ("SymGetLineFromAddr64", B, [H, U64, POINTER(D), POINTER(ImagehlpLinha)]),
        ("SymEnumSymbols", B, [H, U64, ctypes.c_char_p, CALLBACK_SIMBOLO, H]),
        ("SymSetContext", B, [H, POINTER(ImagehlpQuadro), H]),
        ("SymFunctionTableAccess64", H, [H, U64]),
        ("SymGetModuleBase64", U64, [H, U64]),
        ("StackWalk64", B, [D, H, H, POINTER(QuadroPilha), H, CALLBACK_LEITURA, H, H, H]),
        ("MiniDumpWriteDump", B, [H, D, H, D, H, H, H]),
    ]


def TabelaKernel() -> list:
    return [
        ("CreateProcessW", B, [ctypes.c_wchar_p, H, H, H, B, D, H, ctypes.c_wchar_p,
                               POINTER(InicioProcesso), POINTER(InfoProcesso)]),
        ("WaitForDebugEvent", B, [POINTER(EventoDepuracao), D]),
        ("ContinueDebugEvent", B, [D, D, D]),
        ("WaitForSingleObject", D, [H, D]),
        ("ReadProcessMemory", B, [H, U64, H, ctypes.c_size_t, POINTER(ctypes.c_size_t)]),
        ("WriteProcessMemory", B, [H, U64, H, ctypes.c_size_t, POINTER(ctypes.c_size_t)]),
        ("FlushInstructionCache", B, [H, U64, ctypes.c_size_t]),
        ("GetThreadContext", B, [H, H]),
        ("SuspendThread", D, [H]),
        ("TerminateProcess", B, [H, D]),
        ("CloseHandle", B, [H]),
        ("CreateFileW", H, [ctypes.c_wchar_p, D, D, H, D, D, H]),
        ("VirtualAlloc", H, [H, ctypes.c_size_t, D, D]),
        ("VirtualFree", B, [H, ctypes.c_size_t, D]),
        ("GetCurrentProcess", H, []),
    ]


def DeclararApi(Dbghelp, Kernel) -> None:
    for Nome, Tipo, Argumentos in TabelaDbghelp():
        Funcao = getattr(Dbghelp, Nome)  # lint-ok: iaf-simbolo-dinamico ligacao ctypes; nomes literais na TabelaDbghelp deste arquivo
        Funcao.restype, Funcao.argtypes = Tipo, Argumentos
    for Nome, Tipo, Argumentos in TabelaKernel():
        Funcao = getattr(Kernel, Nome)  # lint-ok: iaf-simbolo-dinamico ligacao ctypes; nomes literais na TabelaKernel deste arquivo
        Funcao.restype, Funcao.argtypes = Tipo, Argumentos


DBGHELP = ctypes.WinDLL(str(DBGHELP_DLL), use_last_error=True)
KERNEL = ctypes.WinDLL("kernel32", use_last_error=True)
DeclararApi(DBGHELP, KERNEL)


def Conferir(Resultado, Acao: str) -> None:
    if not Resultado:
        raise ErroDbghelp(f"{Acao} falhou (GetLastError={ctypes.get_last_error()})")


def ContextoAlinhado():
    """Contexto x64 em memoria de pagina (16 bytes de alinhamento exigidos pelo dbghelp e GetThreadContext)."""
    global CONTEXTO_ENDERECO
    if CONTEXTO_ENDERECO is None:
        CONTEXTO_ENDERECO = KERNEL.VirtualAlloc(None, TAM_PAGINA, ALOCAR_COMMIT_RESERVA, PAGINA_LEITURA_ESCRITA)
        Conferir(CONTEXTO_ENDERECO, "VirtualAlloc")
    return ContextoX64.from_address(CONTEXTO_ENDERECO)


def IniciarSimbolos(HProc) -> None:
    DBGHELP.SymSetOptions(OPCOES_SIMBOLOS)
    Conferir(DBGHELP.SymInitialize(HProc, None, False), "SymInitialize")


def EncerrarSimbolos(HProc) -> None:
    DBGHELP.SymCleanup(HProc)


def CarregarModulo(HProc, Caminho: str, Base: int, Tamanho: int = 0) -> int:
    Carregado = DBGHELP.SymLoadModuleEx(HProc, None, Caminho.encode("mbcs"), None, Base, Tamanho, None, 0)
    Conferir(Carregado, f"SymLoadModuleEx({Path(Caminho).name})")
    return Carregado


def CarregarSePossivel(HProc, Caminho: str, Base: int, Tamanho: int) -> bool:
    """Modulos do dump que nao existem mais no disco nao impedem a pilha: ficam sem tabela de funcoes."""
    return bool(DBGHELP.SymLoadModuleEx(HProc, None, Caminho.encode("mbcs"), None, Base, Tamanho, None, 0))


def InfoModulo(HProc, Base: int) -> dict:
    Modulo = ImagehlpModulo()
    Modulo.SizeOfStruct = ctypes.sizeof(ImagehlpModulo)
    Conferir(DBGHELP.SymGetModuleInfo64(HProc, Base, ctypes.byref(Modulo)), "SymGetModuleInfo64")
    Tipo = TIPOS_SIMBOLO[Modulo.SymType] if Modulo.SymType < len(TIPOS_SIMBOLO) else str(Modulo.SymType)
    Casado = Modulo.SymType == SYM_PDB and not Modulo.PdbUnmatched
    return {"tipo": Tipo, "pdb": Modulo.LoadedPdbName.decode("mbcs", "replace"), "casado": bool(Casado)}


def ModoModulo(Exe: Path, Base: int, ExigirPdb: bool) -> int:
    HProc = KERNEL.GetCurrentProcess()
    IniciarSimbolos(HProc)
    try:
        CarregarModulo(HProc, str(Exe), Base)
        Info = InfoModulo(HProc, Base)
    finally:
        EncerrarSimbolos(HProc)
    Casado = "sim" if Info["casado"] else "nao"
    print(f"modulo {Exe.name}: carregado base=0x{Base:x} SymType={Info['tipo']} "
          f"PDB={Info['pdb'] or '-'} PDB casado: {Casado}")
    if ExigirPdb and not Info["casado"]:
        print("FALHOU modulo: PDB nao casado (GUID/idade) ou ausente")
        return 1
    print("PASSOU modulo: dbghelp carregou o modulo")
    return 0


def SecoesPe(Dados: bytes) -> list:
    """(nome, VirtualAddress, VirtualSize, PointerToRawData, SizeOfRawData) de cada secao do cabecalho PE."""
    (Pe,) = struct.unpack_from("<I", Dados, 0x3C)
    (NumSecoes,) = struct.unpack_from("<H", Dados, Pe + 6)
    (TamOpcional,) = struct.unpack_from("<H", Dados, Pe + 20)
    Inicio = Pe + 24 + TamOpcional
    Secoes = []
    for Indice in range(NumSecoes):
        Entrada = Dados[Inicio + 40 * Indice:Inicio + 40 * Indice + 40]
        Nome = Entrada[:8].rstrip(b"\0").decode("ascii", "replace")
        VTamanho, Va, RawTamanho, Raw = struct.unpack_from("<IIII", Entrada, 8)
        Secoes.append((Nome, Va, VTamanho, Raw, RawTamanho))
    return Secoes


def RvaDaSecao(Exe: Path, Nome: str) -> int:
    """RVA de uma secao do arquivo (linhas do D-M6-2 sao relativas ao .text)."""
    for NomeSecao, Va, _VTamanho, _Raw, _RawTamanho in SecoesPe(Exe.read_bytes()):
        if NomeSecao == Nome:
            return Va
    raise ErroDbghelp(f"secao {Nome} nao achada em {Exe.name}")


def LinhaDoEndereco(HProc, Endereco: int):
    Linha = ImagehlpLinha()
    Linha.SizeOfStruct = ctypes.sizeof(ImagehlpLinha)
    Deslocamento = D(0)
    if not DBGHELP.SymGetLineFromAddr64(HProc, Endereco, ctypes.byref(Deslocamento), ctypes.byref(Linha)):
        return None
    return Path(Linha.FileName.decode("mbcs", "replace")).name, Linha.LineNumber


def RegistrosDeTexto(Texto: str) -> list:
    """Registros tipo "linhas" do JSONL do --debug-code linhas,json (um JSON por linha; outros tipos sao ignorados)."""
    Registros = [json.loads(Linha) for Linha in Texto.splitlines() if Linha.strip()]
    return [Registro for Registro in Registros if Registro.get("tipo") == "linhas"]


def ConfereRegistros(Registros: list, BaseTexto: int, NomeNoEndereco, LinhaNoEndereco) -> tuple:
    """Proc: SymFromAddr(BaseTexto + inicio) = funcao. Linha: SymGetLineFromAddr64(BaseTexto + off) = (arquivo, linha).
    NomeNoEndereco(end) -> str e LinhaNoEndereco(end) -> (arquivo, linha) | None sao as consultas ao dbghelp.
    Devolve (procs conferidos, linhas conferidas, divergencias)."""
    Divergencias = []
    Linhas = 0
    for Registro in Registros:
        Nome = NomeNoEndereco(BaseTexto + Registro["inicio"])
        if Nome != Registro["funcao"]:
            Divergencias.append(f"proc {Registro['funcao']} inicio={Registro['inicio']}: dbghelp {Nome!r}")
        Linhas += len(Registro["linhas"])
        Divergencias += ConfereLinhasDoProc(Registro, BaseTexto, LinhaNoEndereco)
    return len(Registros), Linhas, Divergencias


def ConfereLinhasDoProc(Registro: dict, BaseTexto: int, LinhaNoEndereco) -> list:
    Divergencias = []
    for Entrada in Registro["linhas"]:
        Esperado = (Path(Entrada["arquivo"]).name, Entrada["linha"])
        Achado = LinhaNoEndereco(BaseTexto + Entrada["off"])
        if Achado != Esperado:
            Divergencias.append(f"{Registro['funcao']} off={Entrada['off']}: dbghelp {Achado} json {Esperado}")
    return Divergencias


def ModoLinhas(Exe: Path, Base: int, Json, SoProcs: bool = False) -> int:
    if Json is None:
        print("FALHOU linhas: informe --json <arquivo.debug.jsonl> (forja build ... --debug-code linhas,json)")
        return 1
    Registros = RegistrosDeTexto(Json.read_text(encoding="utf-8"))
    if not Registros:
        print(f"FALHOU linhas: {Json.name} sem registro tipo linhas (use --debug-code linhas,json)")
        return 1
    if SoProcs:
        Registros = [{**Registro, "linhas": []} for Registro in Registros]
    BaseTexto = Base + RvaDaSecao(Exe, ".text")
    HProc = KERNEL.GetCurrentProcess()
    IniciarSimbolos(HProc)
    try:
        CarregarModulo(HProc, str(Exe), Base)
        Procs, Linhas, Divergencias = ConfereRegistros(
            Registros, BaseTexto,
            lambda Endereco: NomeDoEndereco(HProc, Endereco),
            lambda Endereco: LinhaDoEndereco(HProc, Endereco))
    finally:
        EncerrarSimbolos(HProc)
    for Linha in Divergencias:
        print(f"FALHOU linhas: {Linha}")
    if Divergencias:
        return 1
    print(f"PASSOU linhas: {Procs} procs e {Linhas} linhas batem com o JSON (dbghelp SymFromAddr/SymGetLineFromAddr64)")
    return 0


def AutotesteLinhas() -> int:
    """Prova offline da comparacao (sem dbghelp): bate; nome errado, linha errada e endereco sem simbolo tem de acusar."""
    Texto = json.dumps({"tipo": "linhas", "funcao": "main", "simbolo": "fj_main", "inicio": 0x10,
                        "linhas": [{"off": 0x12, "arquivo": "C:/x/bt3.fj", "linha": 15},
                                   {"off": 0x20, "arquivo": "C:/x/bt3.fj", "linha": 16}]}) + "\n"
    Texto += json.dumps({"tipo": "cabecalho"}) + "\n"
    Registros = RegistrosDeTexto(Texto)
    TabelaSimbolos = {0x1010: "main"}
    TabelaLinhas = {0x1012: ("bt3.fj", 15), 0x1020: ("bt3.fj", 16)}
    Casos = [("bate", TabelaSimbolos, TabelaLinhas, 0),
             ("nome errado", {0x1010: "outro"}, TabelaLinhas, 1),
             ("linha errada", TabelaSimbolos, {0x1012: ("bt3.fj", 15), 0x1020: ("bt3.fj", 17)}, 1),
             ("sem simbolo", {}, TabelaLinhas, 1)]
    Falhas = 0
    for Nome, TabelaS, TabelaL, Esperado in Casos:
        _Procs, _Quantos, Divergencias = ConfereRegistros(
            Registros, 0x1000, lambda Endereco: TabelaS.get(Endereco, "-"),
            lambda Endereco: TabelaL.get(Endereco))
        if (len(Divergencias) > 0) != (Esperado > 0):
            print(f"FALHOU selftest linhas: caso '{Nome}' deu {len(Divergencias)} divergencia(s)")
            Falhas += 1
    if Falhas:
        return 1
    print("PASSOU selftest linhas: bate; nome errado, linha errada e sem simbolo acusados (offline)")
    return 0


def LancarProcesso(Exe: Path, Argumentos: list, Flags: int, Info: InfoProcesso) -> None:
    Linha = ctypes.create_unicode_buffer(" ".join([f'"{Exe}"'] + Argumentos))
    Inicio = InicioProcesso()
    Inicio.cb = ctypes.sizeof(InicioProcesso)
    Conferir(KERNEL.CreateProcessW(None, ctypes.addressof(Linha), None, None, False, Flags | SEM_JANELA, None,
                                   None, ctypes.byref(Inicio), ctypes.byref(Info)), "CreateProcessW")


def EscreverDump(Info: InfoProcesso, Caminho: Path) -> None:
    Arquivo = KERNEL.CreateFileW(str(Caminho), ARQUIVO_ESCRITA, 0, None, ARQUIVO_CRIAR_SEMPRE, ARQUIVO_NORMAL, None)
    if Arquivo in (None, ARQUIVO_INVALIDO):
        raise ErroDbghelp(f"CreateFileW({Caminho.name}) falhou")
    try:
        Conferir(DBGHELP.MiniDumpWriteDump(Info.hProcess, Info.dwProcessId, Arquivo, DUMP_NORMAL, None, None, None),
                 "MiniDumpWriteDump")
    finally:
        KERNEL.CloseHandle(Arquivo)


def LerModulosDump(Dados: bytes, Fluxos: dict) -> list:
    if FLUXO_MODULOS not in Fluxos:
        return []
    Rva = Fluxos[FLUXO_MODULOS][0]
    (Total,) = struct.unpack_from("<I", Dados, Rva)
    Modulos = []
    for Indice in range(Total):
        Pos = Rva + 4 + 108 * Indice
        Base, Tamanho = struct.unpack_from("<QI", Dados, Pos)
        (NomeRva,) = struct.unpack_from("<I", Dados, Pos + 20)
        (TamNome,) = struct.unpack_from("<I", Dados, NomeRva)
        Nome = Dados[NomeRva + 4:NomeRva + 4 + TamNome].decode("utf-16-le")
        Modulos.append((Base, Tamanho, Nome))
    return Modulos


def LerThreadsDump(Dados: bytes, Fluxos: dict) -> dict:
    if FLUXO_THREADS not in Fluxos:
        return {}
    Rva = Fluxos[FLUXO_THREADS][0]
    (Total,) = struct.unpack_from("<I", Dados, Rva)
    Threads = {}
    for Indice in range(Total):
        Pos = Rva + 4 + 48 * Indice
        (Tid,) = struct.unpack_from("<I", Dados, Pos)
        CtxTamanho, CtxRva = struct.unpack_from("<II", Dados, Pos + 40)
        Threads[Tid] = (CtxRva, CtxTamanho)
    return Threads


def MemoriaDeLista(Dados: bytes, Rva: int) -> list:
    (Total,) = struct.unpack_from("<I", Dados, Rva)
    Blocos = []
    for Indice in range(Total):
        Inicio, Tamanho, Posicao = struct.unpack_from("<QII", Dados, Rva + 4 + 16 * Indice)
        Blocos.append((Inicio, Dados[Posicao:Posicao + Tamanho]))
    return Blocos


def MemoriaDe64(Dados: bytes, Rva: int) -> list:
    Total, BaseRva = struct.unpack_from("<QQ", Dados, Rva)
    Blocos, Posicao = [], BaseRva
    for Indice in range(Total):
        Inicio, Tamanho = struct.unpack_from("<QQ", Dados, Rva + 16 + 16 * Indice)
        Blocos.append((Inicio, Dados[Posicao:Posicao + Tamanho]))
        Posicao += Tamanho
    return Blocos


def LerMemoriaDump(Dados: bytes, Fluxos: dict) -> list:
    if FLUXO_MEMORIA64 in Fluxos:
        return MemoriaDe64(Dados, Fluxos[FLUXO_MEMORIA64][0])
    if FLUXO_MEMORIA in Fluxos:
        return MemoriaDeLista(Dados, Fluxos[FLUXO_MEMORIA][0])
    return []


def LerDump(Caminho: Path) -> dict:
    Dados = Caminho.read_bytes()
    Assinatura, _Versao, NumFluxos, RvaDiretorio = struct.unpack_from("<IIII", Dados, 0)
    if Assinatura != ASSINATURA_MDMP:
        raise ErroDbghelp(f"{Caminho.name} nao e minidump (MDMP ausente)")
    Fluxos = {}
    for Indice in range(NumFluxos):
        Tipo, Tamanho, Rva = struct.unpack_from("<III", Dados, RvaDiretorio + 12 * Indice)
        Fluxos.setdefault(Tipo, (Rva, Tamanho))
    return {"dados": Dados, "modulos": LerModulosDump(Dados, Fluxos),
            "threads": LerThreadsDump(Dados, Fluxos), "memoria": LerMemoriaDump(Dados, Fluxos)}


def ImagemPe(Caminho: Path, Tamanho: int) -> bytes:
    """Imagem do modulo em memoria (secoes no RVA), do arquivo em disco: o dump nao tem .pdata/.xdata."""
    if not Caminho.exists():
        return b""
    Dados = Caminho.read_bytes()
    Imagem = bytearray(Tamanho)
    Cabecalho = min(0x400, len(Dados), Tamanho)
    Imagem[0:Cabecalho] = Dados[0:Cabecalho]
    for _Nome, Va, _VTamanho, Raw, RawTamanho in SecoesPe(Dados):
        Quantidade = min(RawTamanho, Tamanho - Va, len(Dados) - Raw)
        if Quantidade > 0:
            Imagem[Va:Va + Quantidade] = Dados[Raw:Raw + Quantidade]
    return bytes(Imagem)


def ImagemDoEndereco(Modulos: list, Endereco: int, Cache: dict):
    """(inicio, bytes) da imagem do modulo que contem o endereco, ou None."""
    for Base, Tamanho, Nome in Modulos:
        if Base <= Endereco < Base + Tamanho:
            if Base not in Cache:
                Cache[Base] = ImagemPe(Path(Nome), Tamanho)
            return Base, Cache[Base]
    return None


def FabricaLeitura(Memoria: list, Modulos: list):
    """Callback do StackWalk64. Le a pilha dos blocos do dump; o dbghelp tambem le o .xdata do modulo
    (UNWIND_INFO), que o MiniDumpNormal nao grava: esses enderecos vem da imagem do arquivo do modulo."""
    Cache = {}

    def Ler(_HProc, Endereco, Buffer, Tamanho, Lidos) -> bool:
        Bloco = next(((Inicio, Bytes) for Inicio, Bytes in Memoria
                      if Inicio <= Endereco < Inicio + len(Bytes)), None)
        Bloco = Bloco or ImagemDoEndereco(Modulos, Endereco, Cache)
        if Bloco is None:
            return False
        Inicio, Bytes = Bloco
        Pedaco = Bytes[Endereco - Inicio:Endereco - Inicio + Tamanho]
        if not Pedaco:
            return False
        ctypes.memmove(Buffer, Pedaco, len(Pedaco))
        if Lidos:
            Lidos[0] = len(Pedaco)
        return True

    return CALLBACK_LEITURA(Ler)


def AndarPilha(HProc, Dump: dict, Tid: int) -> list:
    CtxRva, CtxTamanho = Dump["threads"][Tid]
    Ctx = ContextoAlinhado()
    Bytes = Dump["dados"][CtxRva:CtxRva + CtxTamanho]
    ctypes.memmove(ctypes.addressof(Ctx), Bytes, min(len(Bytes), ctypes.sizeof(ContextoX64)))
    Quadro = QuadroPilha()
    Quadro.AddrPC.Offset, Quadro.AddrPC.Mode = Ctx.Rip, MODO_FLAT
    Quadro.AddrFrame.Offset, Quadro.AddrFrame.Mode = Ctx.Rbp, MODO_FLAT
    Quadro.AddrStack.Offset, Quadro.AddrStack.Mode = Ctx.Rsp, MODO_FLAT
    Leitor = FabricaLeitura(Dump["memoria"], Dump["modulos"])
    TabelaFuncoes = ctypes.cast(DBGHELP.SymFunctionTableAccess64, H)
    BaseModulo = ctypes.cast(DBGHELP.SymGetModuleBase64, H)
    Pcs = []
    for _Passo in range(PROFUNDIDADE_MAX):
        Ok = DBGHELP.StackWalk64(MAQUINA_AMD64, HProc, None, ctypes.byref(Quadro), ctypes.addressof(Ctx),
                                 Leitor, TabelaFuncoes, BaseModulo, None)
        if not Ok or Quadro.AddrPC.Offset == 0:
            break
        Pcs.append(Quadro.AddrPC.Offset)
    return Pcs


def ModuloDoEndereco(Modulos: list, Endereco: int) -> str:
    for Base, Tamanho, Nome in Modulos:
        if Base <= Endereco < Base + Tamanho:
            return Path(Nome).name.lower()
    return "-"


def NomeDoEndereco(HProc, Endereco: int) -> str:
    Simbolo = SimboloInfo()
    Simbolo.SizeOfStruct = ctypes.sizeof(SimboloInfo) - TAM_NOME_SIMBOLO
    Simbolo.MaxNameLen = TAM_NOME_SIMBOLO
    Deslocamento = U64(0)
    if not DBGHELP.SymFromAddr(HProc, Endereco, ctypes.byref(Deslocamento), ctypes.byref(Simbolo)):
        return "-"
    return Simbolo.Name.decode("mbcs", "replace")


def LinhaDoQuadro(HProc, Indice: int, Endereco: int):
    """Quadro 0 traz o PC exato; os de cima trazem o endereco de retorno, que cai na instrucao seguinte a chamada (-1)."""
    return LinhaDoEndereco(HProc, Endereco if Indice == 0 else Endereco - 1)


def ReportarPilha(HProc, Pcs: list, Modulos: list, Exe: Path, *, Esperado=None) -> int:
    NoExe = 0
    Achados = []
    for Indice, Endereco in enumerate(Pcs):
        Modulo = ModuloDoEndereco(Modulos, Endereco)
        Simbolo = NomeDoEndereco(HProc, Endereco)
        Linha = LinhaDoQuadro(HProc, Indice, Endereco) if Modulo == Exe.name.lower() else None
        Texto = f"{Linha[0]}:{Linha[1]}" if Linha else "-"
        if Modulo == Exe.name.lower():
            NoExe += 1
        if Modulo == Exe.name.lower() and Linha:
            Achados.append(f"{Simbolo} {Texto}")
        print(f"quadro {Indice}: pc=0x{Endereco:x} modulo={Modulo} simbolo={Simbolo} linha={Texto}")
    for Achado in Achados:
        print(f"pilha .fj: {Achado}")
    if NoExe < 3:
        print(f"FALHOU minidump: {NoExe} quadros no exe (esperado >= 3)")
        return 1
    if not ConfereQuadrosFj(Achados, Exe, Esperado):
        return 1
    print(f"PASSOU minidump: {len(Pcs)} quadros, {NoExe} no exe, {len(Achados)} com linha .fj")
    return 0


def ConfereQuadrosFj(Achados: list, Exe: Path, Esperado) -> bool:
    """Com --esperado, a pilha .fj tem de bater com ele. Sem ele, se o exe tem o .debug.jsonl do forja ao lado,
    tem de haver >= 3 quadros .fj com linha; exe sem JSON (controle do gcc) so conta quadros do modulo."""
    if Esperado is not None:
        return ConfereEsperado(Achados, Esperado)
    if Exe.with_name(Exe.name + ".debug.jsonl").exists() and len(Achados) < 3:
        print(f"FALHOU minidump: {len(Achados)} quadros .fj com linha (esperado >= 3; ha .debug.jsonl do forja)")
        return False
    return True


def ConfereEsperado(Achados: list, Esperado: Path) -> bool:
    """Compara a pilha .fj (uma linha por quadro: 'proc arquivo.fj:linha') com o .esperado; # e linha vazia nao contam."""
    Lista = [Linha.strip() for Linha in Esperado.read_text(encoding="utf-8").splitlines()
             if Linha.strip() and not Linha.lstrip().startswith("#")]
    if Lista == Achados:
        print(f"pilha = {Esperado.name} ({len(Lista)} quadros .fj)")
        return True
    print(f"FALHOU minidump: pilha != {Esperado.name}; esperado {Lista}; achado {Achados}")
    return False


def EspelharModulos(Modulos: list) -> list:
    """Espelha no verificador cada modulo do dump no mesmo endereco. O dbghelp le o .pdata pela memoria do
    processo, nao pelo arquivo: sem o espelho o exe do dump nao tem tabela de funcoes e a pilha para no quadro 3.
    Modulo que ja existe aqui no mesmo endereco (ntdll, kernel32...) nao e espelhado."""
    Espelhos = []
    for Base, Tamanho, Nome in Modulos:
        Espelho = KERNEL.VirtualAlloc(Base, Tamanho, ALOCAR_COMMIT_RESERVA, PAGINA_LEITURA_ESCRITA)
        if Espelho is None:
            continue
        Imagem = ImagemPe(Path(Nome), Tamanho)
        ctypes.memmove(Espelho, Imagem, len(Imagem))
        Espelhos.append(Espelho)
    return Espelhos


def LiberarEspelhos(Espelhos: list) -> None:
    for Espelho in Espelhos:
        KERNEL.VirtualFree(Espelho, 0, MEM_LIBERAR)


def AndarPilhaDump(Dmp: Path, Exe: Path, Tid: int, Esperado=None) -> int:
    Dump = LerDump(Dmp)
    HProc = KERNEL.GetCurrentProcess()
    IniciarSimbolos(HProc)
    Espelhos = EspelharModulos(Dump["modulos"])
    try:
        for Base, Tamanho, Nome in Dump["modulos"]:
            CarregarSePossivel(HProc, Nome, Base, Tamanho)
        Pcs = AndarPilha(HProc, Dump, Tid)
        return ReportarPilha(HProc, Pcs, Dump["modulos"], Exe, Esperado=Esperado)
    finally:
        EncerrarSimbolos(HProc)
        LiberarEspelhos(Espelhos)


def ModoMinidump(Exe: Path, EsperaMs: int, DormeMs: int, Dmp: Path, *, Esperado=None) -> int:
    Info = InfoProcesso()
    LancarProcesso(Exe, [str(DormeMs)], 0, Info)
    try:
        time.sleep(EsperaMs / 1000)
        Anterior = KERNEL.SuspendThread(Info.hThread)
        if Anterior == SUSPENSO_FALHOU:
            raise ErroDbghelp("SuspendThread")
        EscreverDump(Info, Dmp)
        return AndarPilhaDump(Dmp, Exe, Info.dwThreadId, Esperado)
    finally:
        KERNEL.TerminateProcess(Info.hProcess, 1)
        KERNEL.CloseHandle(Info.hThread)
        KERNEL.CloseHandle(Info.hProcess)


def CompilarGcc(Fonte: Path, Saida: Path) -> Path:
    Resultado = subprocess.run(["gcc", "-g", "-gdwarf-4", "-O0", "-fno-omit-frame-pointer", "-o", str(Saida),
                                str(Fonte)], capture_output=True, text=True, check=False)
    if Resultado.returncode != 0:
        raise ErroDbghelp(f"gcc falhou em {Fonte.name}: {Resultado.stderr[-300:]}")
    return Saida


def Controles() -> int:
    Pasta = Path(tempfile.mkdtemp(prefix="p74_"))
    Exe = CompilarGcc(GABARITO_C, Pasta / "gabarito.exe")
    Dormir = CompilarGcc(GABARITO_DORMIR_C, Pasta / "gabarito_dormir.exe")
    print("controle 1: modulo do exe do gcc (gcc nao gera PDB; so prova que carrega)")
    Falhas = ModoModulo(Exe, BASE_GCC, False)
    print("controle 2: modulo do cmd.exe do proprio Windows")
    Falhas += ModoModulo(CMD_EXE, BASE_CMD, False)
    print("controle 3: minidump de exe do gcc parado em Sleep, pilha andada com StackWalk64")
    Falhas += ModoMinidump(Dormir, 1500, 60000, Pasta / "gabarito_dormir.dmp")
    return 1 if Falhas else 0


def Principal() -> int:
    Parser = argparse.ArgumentParser(description="PDB/CodeView e minidump pelo dbghelp.dll (M6b P74).")
    Parser.add_argument("--selftest", action="store_true", help="prova offline da comparacao do modo linhas (sem dbghelp)")
    Parser.add_argument("--controles", action="store_true", help="controles positivos do P74 (sem LLVM)")
    Parser.add_argument("--modo", choices=["modulo", "linhas", "locais", "minidump"])
    Parser.add_argument("--exe", type=Path)
    Parser.add_argument("--base", type=lambda Texto: int(Texto, 0), default=BASE_PADRAO)
    Parser.add_argument("--json", type=Path, help="linhas do --debug-code linhas (D-M6-2) para o modo linhas")
    Parser.add_argument("--proc", help="nome do proc (simbolo) para o modo locais")
    Parser.add_argument("--gdb", type=Path, help="modo locais: .gdb da fixture (padrao: tests\\nativo\\m6\\gdb\\<exe>.gdb)")
    Parser.add_argument("--esperado", type=Path, help="modo locais: .esperado da fixture (padrao: <exe>.esperado na mesma pasta)")
    Parser.add_argument("--espera-ms", type=int, default=1500)
    Parser.add_argument("--dorme-ms", type=int, default=60000)
    Parser.add_argument("--dmp", type=Path, default=Path(tempfile.gettempdir()) / "verificar_pdb.dmp")
    Parser.add_argument("--exigir-pdb", action="store_true")
    Parser.add_argument("--so-procs", action="store_true", help="modo linhas: confere so SymFromAddr por proc (sem SymGetLineFromAddr64; linhas C13 = P76c)")
    Argumentos = Parser.parse_args()
    if Argumentos.selftest:
        import verificar_pdb_locais
        Falhas = AutotesteLinhas()
        Falhas += verificar_pdb_locais.AutotesteLocais()
        return 1 if Falhas else 0
    if Argumentos.controles:
        return Controles()
    if Argumentos.modo is None or Argumentos.exe is None:
        Parser.error("informe --controles ou --modo e --exe")
    return Despachar(Argumentos)


def ArquivosLocais(Argumentos) -> tuple:
    """Modo locais: .gdb e .esperado da fixture de mesmo nome do exe; o .exe.debug.jsonl fica ao lado do exe."""
    Nome = Argumentos.exe.stem
    Gdb = Argumentos.gdb or FIXTURAS_GDB / f"{Nome}.gdb"
    Esperado = Argumentos.esperado or FIXTURAS_GDB / f"{Nome}.esperado"
    Json = Argumentos.json or Argumentos.exe.with_name(Argumentos.exe.name + ".debug.jsonl")
    return Gdb, Esperado, Json


def Despachar(Argumentos) -> int:
    if Argumentos.modo == "modulo":
        return ModoModulo(Argumentos.exe, Argumentos.base, Argumentos.exigir_pdb)
    if Argumentos.modo == "linhas":
        return ModoLinhas(Argumentos.exe, Argumentos.base, Argumentos.json, Argumentos.so_procs)
    if Argumentos.modo == "locais":
        import verificar_pdb_locais
        return verificar_pdb_locais.ModoLocais(Argumentos.exe, Argumentos.proc, ArquivosLocais(Argumentos))
    return ModoMinidump(Argumentos.exe, Argumentos.espera_ms, Argumentos.dorme_ms, Argumentos.dmp,
                        Esperado=Argumentos.esperado)


if __name__ == "__main__":
    try:
        sys.exit(Principal())
    except FileNotFoundError as Erro:
        print(f"FERRAMENTA AUSENTE {Erro}")
        sys.exit(2)
    except ErroDbghelp as Erro:
        print(f"FALHOU {Erro}")
        sys.exit(1)
