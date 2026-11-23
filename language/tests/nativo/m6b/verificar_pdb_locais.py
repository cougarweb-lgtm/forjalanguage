#!/usr/bin/env python3
"""verificar_pdb_locais.py - prova dos locais do PDB: processo parado na linha da fixture, nomes, tipos e valores (M6b P77, D-M6-10).

Versao: 0.1.2
Changelog: 0.1.2 - P77b: .gdb com 'print <var>.<campo>' (sem 'info locals'): o campo sai do tipo da variavel (TI_FINDCHILDREN: nome, offset, tipo), o valor da memoria em Rbp + off + campo,
                   formatado '$N = <valor>' como o .esperado; ConsultasDoGdb; o selftest cobre o caso offline.
           0.1.1 - P77a (main, causa raiz achada pelo Opus): SymEnumSymbols com BaseOfDll = 0 e mascara '*' (com a base do modulo o dbghelp lista so os globais e ignora o SymSetContext); EnumerarSimbolos perde o parametro Base; mensagem de 'nenhum local' deixa de culpar o PDB.
           0.1.0 - P77: modo locais saiu de verificar_pdb.py (limite de 800 linhas) e corrige 2 bugs da 0.2.x:
                   (a) o breakpoint saia em inicio do proc + prologo, antes de os locais serem atribuidos: agora cai no
                   endereco da linha do .gdb (break X.fj:N) resolvida pelo .exe.debug.jsonl (linhas, offset relativo
                   ao .text); (b) SYMBOL_INFO.Address era lido como absoluto: com SYMFLAG_REGREL (0x10) ou FRAMEREL (0x20,
                   DbgHelp.h) o valor fica em Rbp do contexto da thread + int32(Address). Tipo por SymGetTypeInfo e
                   SymFromIndex; valor formatado como o gdb (inteiro, bool true/false, f64 %g, struct {campo = valor});
                   comparado por regex (fullmatch) com o .esperado, sem a linha 'Breakpoint'. Robustez: WaitForDebugEvent
                   com teto de 60 s; LOAD_DLL fecha o hFile; excecao que nao e o breakpoint volta DBG_EXCEPTION_NOT_HANDLED;
                   search path = pasta de uma copia do exe (com o PDB ao lado); _NT_SYMBOL_PATH limpo; TerminateProcess e
                   WaitForSingleObject no finally. Variavel em registrador (M7a, R-M6-2) nao e RBP: quebra esta prova e
                   sai FALHOU com a causa (M7a tem de estender este verificador antes de mudar o gerador).
Uso: python verificar_pdb_locais.py --selftest      (offline: formato e comparacao contra um .esperado sintetico)
     via verificar_pdb.py --modo locais --exe <pe> --proc <nome> [--gdb <.gdb>] [--esperado <arq>] [--json <arq>]
     (padrao: <stem>.gdb e <stem>.esperado em tests\\nativo\\m6\\gdb; <exe>.debug.jsonl ao lado do exe)
Depende de: verificar_pdb.py (importado como V; dbghelp.dll e kernel32 por ctypes).
Saida: 0 PASSOU; 1 FALHOU; 2 ferramenta ausente (pelo verificar_pdb.py).
"""
import ctypes
import os
import re
import shutil
import struct
import tempfile
from pathlib import Path

import verificar_pdb as V

SIMBOLO_PARAMETRO_OU_LOCAL = 0x00000040 | 0x00000080  # SYMFLAG_PARAMETER | SYMFLAG_LOCAL (DbgHelp.h)
SYMFLAG_REGREL = 0x00000010  # DbgHelp.h: REGREL, Address e deslocamento a partir de Register
SYMFLAG_FRAMEREL = 0x00000020  # DbgHelp.h: FRAMEREL, Address e deslocamento a partir do quadro (RBP)
REGISTRO_RBP = 334  # CV_AMD64_RBP (DIA cvconst.h)
TI_GET_SYMTAG = 0
TI_GET_LENGTH = 2
TI_GET_TYPEID = 4
TI_GET_BASETYPE = 5
TI_FINDCHILDREN = 7
TI_GET_OFFSET = 10
TI_GET_CHILDRENCOUNT = 13
SYMTAG_UDT = 11
SYMTAG_ENUM = 12
SYMTAG_PONTEIRO = 14
SYMTAG_BASE = 16
BT_INT = 6
BT_FLOAT = 8
BT_BOOL = 10
CONTEXTO_COMPLETO = 0x0010000B
DEPURAR_SO_ESTE = 0x00000002
CONTINUAR = 0x00010002  # DBG_CONTINUE
EVENTO_EXCECAO = 1
EVENTO_CRIAR_PROCESSO = 3
EVENTO_SAIDA_PROCESSO = 5
EVENTO_CARREGAR_DLL = 6
EXCECAO_BREAKPOINT = 0x80000003
NAO_TRATADA = 0x80010001
TEMPO_EVENTO_MS = 60000
TEMPO_FIM_MS = 5000
PROFUNDIDADE_TIPO = 8
MASCARA_64 = (1 << 64) - 1
GDB_BREAK = re.compile(r"^\s*break\s+\S+:(\d+)\s*$", re.M)
GDB_PRINT_CAMPO = re.compile(r"^\s*print\s+(\w+)\.(\w+)\s*$", re.M)
KERNEL = V.KERNEL
DBGHELP = V.DBGHELP

TIPO_INT_SINTETICO = {"tag": SYMTAG_BASE, "base": BT_INT, "tamanho": 4}
TIPO_F64_SINTETICO = {"tag": SYMTAG_BASE, "base": BT_FLOAT, "tamanho": 8}
TIPO_BOOL_SINTETICO = {"tag": SYMTAG_BASE, "base": BT_BOOL, "tamanho": 1}
TIPO_PONTO_SINTETICO = {"tag": SYMTAG_UDT, "tamanho": 8,
                        "membros": [("x", 0, TIPO_INT_SINTETICO), ("y", 4, TIPO_INT_SINTETICO)]}
ESPERADO_SINTETICO = "Breakpoint 1, .*main\nn = 7\np = \\{x = 4, y = 9\\}\nr = 2\\.5\nok = true\n"


# ---------- formato do valor, como o gdb (tambem no --selftest, sem dbghelp) ----------

def FormatarBase(Base: int, Dados: bytes) -> str:
    if Base == BT_BOOL:
        return "true" if any(Dados) else "false"
    if Base == BT_FLOAT and len(Dados) in (4, 8):
        return f"{struct.unpack('<d' if len(Dados) == 8 else '<f', Dados)[0]:g}"
    if Base == BT_FLOAT:
        return f"<ponto flutuante de {len(Dados)} B>"
    return str(int.from_bytes(Dados, "little", signed=Base == BT_INT))


def FormatarValor(Tipo: dict, Dados: bytes) -> str:
    """Escalar direto; struct como {campo = valor, ...}; ponteiro em hex; enum como numero; resto explicito."""
    Tag = Tipo.get("tag")
    if Tag == SYMTAG_BASE:
        return FormatarBase(Tipo["base"], Dados)
    if Tag == SYMTAG_UDT:
        Campos = [f"{Nome} = {FormatarValor(Sub, Dados[Pos:Pos + Sub.get('tamanho', 0)])}"
                  for Nome, Pos, Sub in Tipo.get("membros", [])]
        return "{" + ", ".join(Campos) + "}"
    if Tag == SYMTAG_PONTEIRO:
        return "0x" + format(int.from_bytes(Dados, "little"), "x")
    if Tag == SYMTAG_ENUM:
        return str(int.from_bytes(Dados, "little"))
    return f"<tipo {Tag} nao suportado>"


def ConfereSaida(Produzidas: list, Esperado: str) -> list:
    """Cada linha do .esperado (regex, sem a linha 'Breakpoint') tem de casar inteira com alguma linha produzida."""
    Faltando = []
    for Linha in Esperado.splitlines():
        if not Linha.strip() or Linha.startswith("Breakpoint"):
            continue
        if not any(re.fullmatch(Linha, Produzida) for Produzida in Produzidas):
            Faltando.append(Linha)
    return Faltando


def LocaisDeReferencia() -> list:
    """Quatro locais do info_locals com os bytes que o processo teria: int 7, struct {4, 9}, f64 2.5 e bool true."""
    return [("n", TIPO_INT_SINTETICO, struct.pack("<i", 7)),
            ("p", TIPO_PONTO_SINTETICO, struct.pack("<ii", 4, 9)),
            ("r", TIPO_F64_SINTETICO, struct.pack("<d", 2.5)),
            ("ok", TIPO_BOOL_SINTETICO, b"\x01")]


def ProduzirLinhas(Locais: list) -> list:
    return [f"{Nome} = {FormatarValor(Tipo, Dados)}" for Nome, Tipo, Dados in Locais]


def AutotesteLocais() -> int:
    """Formato e comparacao offline: bate; valor, tipo e campo errados tem de ser acusados."""
    Base = LocaisDeReferencia()
    Casos = [("bate", Base, 0),
             ("valor errado", [("n", TIPO_INT_SINTETICO, struct.pack("<i", 8))] + Base[1:], 1),
             ("tipo errado", [("n", TIPO_F64_SINTETICO, struct.pack("<i", 7))] + Base[1:], 1),
             ("campo errado", Base[:1] + [("p", TIPO_PONTO_SINTETICO, struct.pack("<ii", 4, 10))] + Base[2:], 1)]
    Falhas = 0
    for Nome, Locais, Esperado in Casos:
        Faltando = ConfereSaida(ProduzirLinhas(Locais), ESPERADO_SINTETICO)
        if (len(Faltando) > 0) != (Esperado > 0):
            print(f"FALHOU selftest locais: caso '{Nome}' deu {len(Faltando)} linha(s) sem casar")
            Falhas += 1
    Falhas += AutotesteCampo()
    if Falhas:
        return 1
    print("PASSOU selftest locais: int, f64, bool e campo de struct batem com o .esperado sintetico; "
          "valor, tipo e campo errados acusados; 'print p.x' = $1 = 4 (offline)")
    return 0


def AutotesteCampo() -> int:
    """'print p.x' do .gdb vira a consulta (p, x); o campo x do Ponto sintetico {4, 9} formata como $1 = 4 e o y nao casa."""
    Consultas = ConsultasDoGdb("break a.fj:8\nrun\nprint p.x\n")
    Falhas = 0
    if Consultas != [("p", "x")] or ConsultasDoGdb("break a.fj:8\ninfo locals\n"):
        print(f"FALHOU selftest locais: ConsultasDoGdb devolveu {Consultas}")
        Falhas += 1
    Dados = struct.pack("<ii", 4, 9)
    Linhas = [f"${Numero} = {FormatarValor(Sub, Dados[Pos:Pos + Sub['tamanho']])}"
              for Numero, (Nome, Pos, Sub) in enumerate([M for M in TIPO_PONTO_SINTETICO["membros"] if M[0] == "x"], 1)]
    if ConfereSaida(Linhas, "Breakpoint 1, .*main\n\\$1 = 4\n") or not ConfereSaida(Linhas, "\\$1 = 9\n"):
        print(f"FALHOU selftest locais: print p.x produziu {Linhas}")
        Falhas += 1
    return Falhas


# ---------- fixture: linha do .gdb e offset no .debug.jsonl (bug a) ----------

def LinhaDoGdb(Texto: str) -> int:
    Achado = GDB_BREAK.search(Texto)
    if Achado is None:
        raise V.ErroDbghelp("--gdb sem 'break <arquivo.fj>:<linha>' (a linha de quebra da fixture)")
    return int(Achado.group(1))


def ConsultasDoGdb(Texto: str) -> list:
    """Pares (variavel, campo) dos 'print <var>.<campo>' do .gdb; vazio = o .gdb usa 'info locals'."""
    return GDB_PRINT_CAMPO.findall(Texto)


def OffsetDaLinha(Registros: list, Proc: str, Linha: int) -> int:
    """Offset relativo ao .text da linha da fixture no proc (menor offset se a linha tem mais de um)."""
    for Registro in Registros:
        if Registro.get("funcao") != Proc:
            continue
        Offsets = [Entrada["off"] for Entrada in Registro["linhas"] if Entrada["linha"] == Linha]
        if not Offsets:
            raise V.ErroDbghelp(f"proc {Proc} sem a linha {Linha} no .debug.jsonl")
        return min(Offsets)
    raise V.ErroDbghelp(f"proc {Proc} ausente no .debug.jsonl (use --debug-code linhas,json)")


# ---------- processo depurado (bug b: Address e offset, nao endereco absoluto) ----------

def CopiarParaPasta(Exe: Path, Pasta: Path) -> Path:
    """Copia do exe e do PDB ao lado numa pasta propria: o search path (pasta do exe) acha o PDB."""
    Copia = Pasta / Exe.name
    shutil.copy2(Exe, Copia)
    Pdb = Exe.with_suffix(".pdb")
    if Pdb.exists():
        shutil.copy2(Pdb, Pasta / Pdb.name)
    return Copia


def EscreverBreakpoint(HProc, Endereco: int) -> None:
    """int3 no endereco da linha; o byte original nao volta (o processo e morto logo depois)."""
    Buffer = ctypes.create_string_buffer(b"\xcc", 1)
    Escritos = ctypes.c_size_t(0)
    V.Conferir(KERNEL.WriteProcessMemory(HProc, Endereco, ctypes.addressof(Buffer), 1, ctypes.byref(Escritos)),
               "WriteProcessMemory")
    KERNEL.FlushInstructionCache(HProc, Endereco, 1)


def LerMemoriaDebug(HProc, Endereco: int, Tamanho: int) -> bytes:
    Buffer = ctypes.create_string_buffer(Tamanho)
    Lidos = ctypes.c_size_t(0)
    V.Conferir(KERNEL.ReadProcessMemory(HProc, Endereco, ctypes.addressof(Buffer), Tamanho, ctypes.byref(Lidos)),
               "ReadProcessMemory")
    return Buffer.raw[:Lidos.value]


def PrepararBreakpoint(HProc, Exe: Path, BaseReal: int, RvaAlvo: int) -> int:
    """Carrega o PDB com search path = pasta da copia (Exe.parent) e poe o int3 em BaseReal + RVA(.text) + offset."""
    V.DBGHELP.SymSetOptions(V.OPCOES_SIMBOLOS)
    V.Conferir(V.DBGHELP.SymInitialize(HProc, str(Exe.parent).encode("mbcs"), False), "SymInitialize (pasta do exe)")
    V.CarregarModulo(HProc, str(Exe), BaseReal)
    Alvo = BaseReal + RvaAlvo
    EscreverBreakpoint(HProc, Alvo)
    return Alvo


def PegarTipo(HProc, Base: int, Indice: int, Campo: int):
    """Um valor de SymGetTypeInfo: TI_GET_LENGTH e ULONG64; os demais campos usados aqui sao DWORD."""
    Formato = V.U64 if Campo == TI_GET_LENGTH else V.D
    Valor = Formato()
    V.Conferir(DBGHELP.SymGetTypeInfo(HProc, Base, Indice, Campo, ctypes.addressof(Valor)),
               f"SymGetTypeInfo(campo {Campo}, tipo 0x{Indice:X})")
    return Valor.value


def NomeDoIndice(HProc, Base: int, Indice: int) -> str:
    Simbolo = V.SimboloInfo()
    Simbolo.SizeOfStruct = ctypes.sizeof(V.SimboloInfo) - V.TAM_NOME_SIMBOLO
    Simbolo.MaxNameLen = V.TAM_NOME_SIMBOLO
    V.Conferir(DBGHELP.SymFromIndex(HProc, Base, Indice, ctypes.byref(Simbolo)), f"SymFromIndex(0x{Indice:X})")
    return Simbolo.Name.decode("mbcs", "replace")


def MembrosDoTipo(HProc, Base: int, Indice: int, Profundidade: int) -> list:
    """Filhos do tipo struct (TI_FINDCHILDREN): (nome, offset, tipo) de cada membro."""
    Quantos = PegarTipo(HProc, Base, Indice, TI_GET_CHILDRENCOUNT)
    Buffer = (V.D * (2 + Quantos))()
    Buffer[0] = Quantos
    V.Conferir(DBGHELP.SymGetTypeInfo(HProc, Base, Indice, TI_FINDCHILDREN, ctypes.addressof(Buffer)),
               "SymGetTypeInfo(TI_FINDCHILDREN)")
    Membros = []
    for Filho in Buffer[2:]:
        Deslocamento = PegarTipo(HProc, Base, Filho, TI_GET_OFFSET)
        TipoMembro = PegarTipo(HProc, Base, Filho, TI_GET_TYPEID)
        Membros.append((NomeDoIndice(HProc, Base, Filho), Deslocamento,
                        TipoDeIndice(HProc, Base, TipoMembro, Profundidade + 1)))
    return Membros


def TipoDeIndice(HProc, Base: int, Indice: int, Profundidade: int = 0) -> dict:
    """Tipo do TPI do modulo: escalar (base), struct (membros), ponteiro ou enum; o resto fica sem detalhe."""
    Tag = PegarTipo(HProc, Base, Indice, TI_GET_SYMTAG)
    Tipo = {"tag": Tag, "tamanho": 0}
    if Tag in (SYMTAG_BASE, SYMTAG_UDT, SYMTAG_PONTEIRO, SYMTAG_ENUM):
        Tipo["tamanho"] = int(PegarTipo(HProc, Base, Indice, TI_GET_LENGTH))
    if Tag == SYMTAG_BASE:
        Tipo["base"] = PegarTipo(HProc, Base, Indice, TI_GET_BASETYPE)
    if Tag == SYMTAG_UDT and Profundidade < PROFUNDIDADE_TIPO:
        Tipo["membros"] = MembrosDoTipo(HProc, Base, Indice, Profundidade)
    return Tipo


def EnumerarSimbolos(HProc) -> list:
    """Parametros e locais do quadro ja definido por SymSetContext (copiados dentro do callback).
    BaseOfDll = 0 com mascara '*': e o unico jeito de o dbghelp enumerar o ESCOPO do contexto; com a base do modulo ele
    lista so os globais (este PDB nao tem nenhum) e ignora o SymSetContext."""
    Achados = []

    def Coletar(Simbolo, _Tamanho, _Contexto) -> bool:
        Conteudo = Simbolo.contents
        if Conteudo.Flags & SIMBOLO_PARAMETRO_OU_LOCAL:
            Achados.append({"nome": Conteudo.Name.decode("mbcs", "replace"), "flags": Conteudo.Flags,
                            "endereco": Conteudo.Address, "registro": Conteudo.Register,
                            "tipo": Conteudo.TypeIndex, "base": Conteudo.ModBase})
        return True

    Cb = V.CALLBACK_SIMBOLO(Coletar)
    V.Conferir(DBGHELP.SymEnumSymbols(HProc, 0, b"*", Cb, None), "SymEnumSymbols")
    return Achados


def EnderecoDoLocal(Simbolo: dict, RbpDoContexto: int) -> int:
    """Local em RBP: Address e o deslocamento (i32) a partir do quadro; registro que nao e RBP quebra a prova."""
    Flags = Simbolo["flags"]
    if not Flags & (SYMFLAG_REGREL | SYMFLAG_FRAMEREL):
        raise V.ErroDbghelp(f"{Simbolo['nome']}: sem REGREL/FRAMEREL (flags 0x{Flags:X})")
    if not Flags & SYMFLAG_FRAMEREL and Simbolo["registro"] != REGISTRO_RBP:
        raise V.ErroDbghelp(f"{Simbolo['nome']}: variavel no registro {Simbolo['registro']}, nao RBP ({REGISTRO_RBP}); "
                            "variavel em registrador e M7a (R-M6-2) e exige estender esta prova")
    Deslocamento = ctypes.c_int32(Simbolo["endereco"] & 0xFFFFFFFF).value
    return (RbpDoContexto + Deslocamento) & MASCARA_64


def ConverterLocais(HProc, Simbolos: list, RbpDoContexto: int) -> list:
    """Nome = valor no formato do gdb, lendo a memoria do processo parado."""
    Linhas = []
    for Simbolo in Simbolos:
        Tipo = TipoDeIndice(HProc, Simbolo["base"], Simbolo["tipo"])
        Tamanho = Tipo["tamanho"]
        Dados = LerMemoriaDebug(HProc, EnderecoDoLocal(Simbolo, RbpDoContexto), Tamanho) if 0 < Tamanho <= 64 else b""
        Linhas.append(f"{Simbolo['nome']} = {FormatarValor(Tipo, Dados)}")
    return Linhas


def ConsultarCampos(HProc, Simbolos: list, RbpDoContexto: int, Consultas: list) -> list:
    """'$N = valor' de cada 'print <var>.<campo>': variavel pelo nome, campo pelo nome entre os membros do tipo, memoria em Rbp + off + campo."""
    Linhas = []
    for Numero, (Variavel, Campo) in enumerate(Consultas, 1):
        Simbolo = next((S for S in Simbolos if S["nome"] == Variavel), None)
        if Simbolo is None:
            raise V.ErroDbghelp(f"print {Variavel}.{Campo}: local '{Variavel}' ausente no escopo")
        Tipo = TipoDeIndice(HProc, Simbolo["base"], Simbolo["tipo"])
        Membro = next((M for M in Tipo.get("membros", []) if M[0] == Campo), None)
        if Membro is None:
            raise V.ErroDbghelp(f"print {Variavel}.{Campo}: o tipo de '{Variavel}' (tag {Tipo['tag']}) nao tem o campo '{Campo}'")
        _, Pos, Sub = Membro
        Dados = LerMemoriaDebug(HProc, EnderecoDoLocal(Simbolo, RbpDoContexto) + Pos, Sub["tamanho"])
        Linhas.append(f"${Numero} = {FormatarValor(Sub, Dados)}")
    return Linhas


def ColetarLocais(Info, Alvo: int, BaseReal: int, Consultas: list) -> list:
    """Contexto do breakpoint: quadro (Rsp, Rbp, Rip) para SymSetContext, depois simbolos e valores (ou os campos de 'print <var>.<campo>')."""
    Ctx = V.ContextoAlinhado()
    Ctx.ContextFlags = CONTEXTO_COMPLETO
    V.Conferir(KERNEL.GetThreadContext(Info.hThread, ctypes.addressof(Ctx)), "GetThreadContext")
    Quadro = V.ImagehlpQuadro()
    Quadro.InstructionOffset = Alvo
    Quadro.StackOffset = Ctx.Rsp
    Quadro.FrameOffset = Ctx.Rbp
    V.Conferir(DBGHELP.SymSetContext(Info.hProcess, ctypes.byref(Quadro), None), "SymSetContext")
    Simbolos = EnumerarSimbolos(Info.hProcess)
    if Consultas:
        return ConsultarCampos(Info.hProcess, Simbolos, Ctx.Rbp, Consultas)
    return ConverterLocais(Info.hProcess, Simbolos, Ctx.Rbp)


def EhBreakpointDoAlvo(Cru: bytes, Alvo) -> bool:
    """A excecao do evento e o int3 da linha da fixture (ExceptionAddress = alvo)."""
    Excecao = struct.unpack_from("<I", Cru, 0)[0]
    EnderecoExcecao = struct.unpack_from("<Q", Cru, 16)[0]
    return Alvo is not None and Excecao == EXCECAO_BREAKPOINT and EnderecoExcecao == Alvo["alvo"]


def ContinuacaoDaExcecao(Cru: bytes) -> int:
    """DBG_CONTINUE para o breakpoint do sistema (loader); excecao alheia volta DBG_EXCEPTION_NOT_HANDLED."""
    return CONTINUAR if struct.unpack_from("<I", Cru, 0)[0] == EXCECAO_BREAKPOINT else NAO_TRATADA


def AbrirAlvo(Info, Exe: Path, Cru: bytes, RvaAlvo: int) -> dict:
    """Evento CREATE_PROCESS: a base real vem do evento; o int3 entra antes de qualquer codigo do proc rodar."""
    (BaseReal,) = struct.unpack_from("<Q", Cru, 24)
    return {"alvo": PrepararBreakpoint(Info.hProcess, Exe, BaseReal, RvaAlvo), "base": BaseReal}


def FecharArquivoDll(Cru: bytes) -> None:
    """LOAD_DLL_DEBUG_EVENT entrega um hFile aberto: sem fechar, o processo de verificacao vaza handle."""
    (Arquivo,) = struct.unpack_from("<Q", Cru, 0)
    KERNEL.CloseHandle(Arquivo)


def LacoDepuracao(Info, Exe: Path, RvaAlvo: int, Consultas: list) -> list:
    """Eventos do processo ate o breakpoint da linha; teto de 60 s por evento."""
    Evento = V.EventoDepuracao()
    Alvo = None
    while True:
        V.Conferir(KERNEL.WaitForDebugEvent(ctypes.byref(Evento), TEMPO_EVENTO_MS), "WaitForDebugEvent (60 s)")
        Cru = bytes(Evento.u)
        Codigo, Continuacao = Evento.dwDebugEventCode, CONTINUAR
        if Codigo == EVENTO_EXCECAO and EhBreakpointDoAlvo(Cru, Alvo):
            return ColetarLocais(Info, Alvo["alvo"], Alvo["base"], Consultas)
        if Codigo == EVENTO_CRIAR_PROCESSO:
            Alvo = AbrirAlvo(Info, Exe, Cru, RvaAlvo)
        if Codigo == EVENTO_CARREGAR_DLL:
            FecharArquivoDll(Cru)
        if Codigo == EVENTO_EXCECAO:
            Continuacao = ContinuacaoDaExcecao(Cru)
        if Codigo == EVENTO_SAIDA_PROCESSO:
            raise V.ErroDbghelp("processo terminou antes do breakpoint (a linha da fixture nunca executou)")
        V.Conferir(KERNEL.ContinueDebugEvent(Evento.dwProcessId, Evento.dwThreadId, Continuacao), "ContinueDebugEvent")


def ExecutarEColetar(Exe: Path, RvaAlvo: int, Consultas: list) -> list:
    """Roda a copia do exe sob depuracao (so ele), para na linha e devolve as linhas 'nome = valor' do proc."""
    os.environ.pop("_NT_SYMBOL_PATH", None)
    os.environ.pop("_NT_ALT_SYMBOL_PATH", None)
    Info = V.InfoProcesso()
    V.LancarProcesso(Exe, [], DEPURAR_SO_ESTE, Info)
    try:
        return LacoDepuracao(Info, Exe, RvaAlvo, Consultas)
    finally:
        V.EncerrarSimbolos(Info.hProcess)
        KERNEL.TerminateProcess(Info.hProcess, 1)
        KERNEL.WaitForSingleObject(Info.hProcess, TEMPO_FIM_MS)
        KERNEL.CloseHandle(Info.hThread)
        KERNEL.CloseHandle(Info.hProcess)


def ModoLocais(Exe: Path, Proc: str, Arquivos: tuple) -> int:
    """Arquivos = (.gdb, .esperado, .exe.debug.jsonl): a fixture da linha de quebra e o que o proc tem de mostrar."""
    Gdb, Esperado, Json = Arquivos
    TextoGdb = Gdb.read_text(encoding="utf-8")
    Linha = LinhaDoGdb(TextoGdb)
    Consultas = ConsultasDoGdb(TextoGdb)
    Registros = V.RegistrosDeTexto(Json.read_text(encoding="utf-8"))
    RvaAlvo = V.RvaDaSecao(Exe, ".text") + OffsetDaLinha(Registros, Proc, Linha)
    Pasta = Path(tempfile.mkdtemp(prefix="p77_locais_"))
    try:
        Produzidas = ExecutarEColetar(CopiarParaPasta(Exe, Pasta), RvaAlvo, Consultas)
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    for Saida in Produzidas:
        print(Saida)
    if not Produzidas:
        print(f"FALHOU locais: nenhum parametro/local no proc {Proc} (nenhum local no escopo do SymSetContext: conferir com ler_pdb --exigir-locais se o PDB tem S_REGREL32)")
        return 1
    Faltando = ConfereSaida(Produzidas, Esperado.read_text(encoding="utf-8"))
    for Pendente in Faltando:
        print(f"FALHOU locais: linha do {Esperado.name} sem casar: {Pendente}")
    if Faltando:
        return 1
    print(f"PASSOU locais: {len(Produzidas)} local(is) de {Proc} batem com {Esperado.name} (linha {Linha} do {Gdb.name})")
    return 0
