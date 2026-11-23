#!/usr/bin/env python3
"""ler_pdb_dbi.py - DBI, modulos, contribuicoes de secao e stream C13 do PDB (M6b P76-0, D-M6-9).

Versao: 0.2.0
Changelog: 0.2.0 - P77-0: S_REGREL32 (deslocamento i32, tipo TPI u32, registro u16, nome) entre S_GPROC32 e S_END vira
                   'locais' do modulo, com o proc dono; registro que nao e RBP (334) e achado nesta versao (M7a). Tipo
                   do TPI e conferido em ler_pdb_tpi. ConstruirSimbolos/ConstruirModulo aceitam locais (selftest).
           0.1.1 - P76a: SymByteSize conta a assinatura (como no PDB da MS/LLVM) e o stream do modulo termina com GlobalRefsSize u32; antes o leitor tomava esses 4 bytes por
                   registro de simbolo. ConstruirModulo (selftest) acompanha.
           0.1.0 - P76-0: decodifica o DBI (stream 3: cabecalho de 64 B e os substreams ModInfo, SectionContribution,
                   SectionMap, FileInfo, TypeServer, EC, OptionalDbgHeader) e o stream de cada modulo (assinatura 4,
                   S_GPROC32/S_FRAMEPROC/S_END, DEBUG_S_LINES, DEBUG_S_FILECHKSMS, DEBUG_S_STRINGTABLE). Tolerante: DBI
                   de tamanho 0 = sem modulo, sem achado. Cada reprovacao vira texto em Achados; quem chama decide se e
                   falha (ler_pdb.py --exigir-modulo). Linha do DEBUG_S_LINES: 24 bits baixos, deltaLineEnd 7 bits e
                   fStatement no bit 31 (padrao MS; o contrato dizia 31 bits, igual quando delta = 0, que e o que o P76 grava).
                   Construir* monta os streams sinteticos que o --selftest do ler_pdb.py confere.
Uso: importado por ler_pdb.py (nao roda sozinho).
Depende de: stdlib (struct).
"""
import struct

DBI_STREAM = 3
FLUXO_NULO = 0xFFFF
DBI_ASSINATURA = -1
DBI_VERSAO = 19990903
DBI_CONSTRUCAO = 0x8E00
MAQUINA_AMD64 = 0x8664
SC_VERSAO = 0xF12EBA2D
SC_CARACTERISTICAS = 0x60000020
SM_ABSOLUTA_FLAGS = 0x0208
SM_ABSOLUTA_TAMANHO = 0xFFFFFFFF
ASSINATURA_C13 = 4
S_GPROC32 = 0x1110
S_FRAMEPROC = 0x1012
S_END = 0x0006
S_REGREL32 = 0x1111
CV_AMD64_RBP = 334
C13_LINHAS = 0xF2
C13_ARQUIVOS = 0xF4
FLUXO_CABECALHOS_SECAO = 7
NOME_ARQUIVO_OFFSET = 1
IDADE_SINTETICA = 1
ORDEM_SUBSTREAMS = (("ModInfo", "ModInfoSize"), ("SectionContribution", "SectionContributionSize"),
                    ("SectionMap", "SectionMapSize"), ("FileInfo", "SourceInfoSize"),
                    ("TypeServer", "TypeServerSize"), ("EC", "ECSubstreamSize"),
                    ("OptionalDbgHeader", "OptionalDbgHeaderSize"))
CAMPOS_DBI = ("VersionSignature", "VersionHeader", "Age", "GlobalStreamIndex", "BuildNumber", "PublicStreamIndex",
              "PdbDllVersion", "SymRecordStream", "PdbDllRbld", "ModInfoSize", "SectionContributionSize",
              "SectionMapSize", "SourceInfoSize", "TypeServerSize", "MFCTypeServerIndex", "OptionalDbgHeaderSize",
              "ECSubstreamSize", "Flags", "Machine", "Padding")
CAMPOS_MODINFO = ("Flags", "FluxoSimbolos", "TamanhoSimbolos", "TamanhoC11", "TamanhoC13", "ArquivosFonte",
                  "Pad", "Nao1", "NomeArquivoIndice", "CaminhoPdbIndice")
CAMPOS_CONTRIBUICAO = ("ISect", "Pad1", "Offset", "Tamanho", "Caracteristicas", "Imod", "Pad2", "DataCrc", "RelocCrc")
CAMPOS_SECAO = ("Flags", "Ovl", "Group", "Frame", "SecName", "ClassName", "Offset", "TamanhoByte")
CAMPOS_OPCIONAL = ("FPO", "Exception", "Fixup", "OmapToSrc", "OmapFromSrc", "SectionHdr", "TokenRidMap",
                   "Xdata", "Pdata", "NewFpo", "SectionHdrOrig")
CAMPOS_GPROC = ("Pai", "Fim", "Proximo", "TamanhoCodigo", "DbgInicio", "DbgFim", "TipoFuncao", "Inicio",
                "Segmento", "Flags")

U32 = struct.Struct("<I")
DBI_CABECALHO = struct.Struct("<iIIHHHHHHIIIIIIIIHHI")
MODINFO_CAMPOS = struct.Struct("<HHIIIHHIII")
CONTRIBUICAO = struct.Struct("<HHiiIHHII")
MAPA_CABECALHO = struct.Struct("<HH")
MAPA_SECAO = struct.Struct("<6HII")
OPCIONAL = struct.Struct("<11H")
REGISTRO = struct.Struct("<HH")
GPROC = struct.Struct("<IIIIIIIIHB")
QUADRO = struct.Struct("<IIIIIHHI")
QUADRO_CURTO = struct.Struct("<IIIIIHI")
C13_CABECALHO = struct.Struct("<II")
LINHAS_CABECALHO = struct.Struct("<IHHI")
LINHAS_BLOCO = struct.Struct("<III")
LINHA = struct.Struct("<II")
ARQUIVO = struct.Struct("<IBBH")
REGREL = struct.Struct("<iIH")


def Alinhar4(Valor: int) -> int:
    return (Valor + 3) & ~3


def LerCampos(Estrutura: struct.Struct, Dados: bytes, Pos: int) -> tuple:
    """Campos de Estrutura em Pos; ValueError com a posicao quando o stream acaba antes."""
    if Pos < 0 or Pos + Estrutura.size > len(Dados):
        raise ValueError(f"campo de {Estrutura.size} B em {Pos} fora do stream de {len(Dados)} B")
    return Estrutura.unpack_from(Dados, Pos)


def LerU32s(Dados: bytes, Pos: int, Quantidade: int) -> list:
    if Quantidade < 0 or Pos + 4 * Quantidade > len(Dados):
        raise ValueError(f"{Quantidade} u32 em {Pos} passam do stream de {len(Dados)} B")
    return list(struct.unpack_from(f"<{Quantidade}I", Dados, Pos))


def LerCString(Dados: bytes, Pos: int, Fim: int) -> tuple:
    """Texto ate o NUL, a partir de Pos e sem passar de Fim; devolve (texto, posicao depois do NUL)."""
    Nul = Dados.find(b"\0", Pos, min(Fim, len(Dados))) if 0 <= Pos else -1
    if Nul < 0:
        raise ValueError(f"cadeia C sem NUL entre {Pos} e {Fim}")
    return Dados[Pos:Nul].decode("utf-8", "replace"), Nul + 1


# ---------- DBI: cabecalho e substreams ----------

def LerDbiCabecalho(Dados: bytes) -> dict:
    return dict(zip(CAMPOS_DBI, LerCampos(DBI_CABECALHO, Dados, 0)))


def ChecarCabecalhoDbi(Cab: dict, Achados: list) -> None:
    if Cab["VersionSignature"] != DBI_ASSINATURA or Cab["VersionHeader"] != DBI_VERSAO:
        Achados.append(f"DBI: VersionSignature {Cab['VersionSignature']} e VersionHeader {Cab['VersionHeader']} "
                       f"fora do esperado (-1 e 19990903)")


def SepararSubstreams(Dados: bytes, Cab: dict, Achados: list) -> dict:
    """Corta o DBI na ordem ModInfo, SectionContribution, SectionMap, FileInfo, TypeServer, EC, OptionalDbgHeader."""
    Partes = {}
    Pos = DBI_CABECALHO.size
    for Nome, Campo in ORDEM_SUBSTREAMS:
        Partes[Nome] = Dados[Pos:Pos + Cab[Campo]]
        Pos += Cab[Campo]
    if Pos != len(Dados):
        Achados.append(f"DBI: cabecalho 64 + substreams = {Pos} B, o stream tem {len(Dados)} B")
    return Partes


# ---------- ModInfo ----------

def LerModuloEm(Dados: bytes, Pos: int) -> tuple:
    """Entrada do ModInfo: 64 B fixos (SectionContribution embutida de 28 B) e ModuleName/ObjFileName em C string."""
    Mod = dict(zip(CAMPOS_MODINFO, LerCampos(MODINFO_CAMPOS, Dados, Pos + 32)))
    Contribuicao = dict(zip(CAMPOS_CONTRIBUICAO, LerCampos(CONTRIBUICAO, Dados, Pos + 4)))
    Nome, Depois = LerCString(Dados, Pos + 64, len(Dados))
    Obj, Depois = LerCString(Dados, Depois, len(Dados))
    Mod.update(contribuicao=Contribuicao, nome=Nome, obj=Obj)
    return Mod, Depois


def LerModulos(Dados: bytes, Achados: list) -> list:
    if len(Dados) % 4:
        Achados.append(f"ModInfo: {len(Dados)} B nao e multiplo de 4")
    Modulos, Pos = [], 0
    while Pos < len(Dados):
        Mod, Fim = LerModuloEm(Dados, Pos)
        Modulos.append(Mod)
        Pos = Alinhar4(Fim)
    return Modulos


# ---------- SectionContribution, SectionMap, OptionalDbgHeader ----------

def ChecarOrdemContribuicoes(Entradas: list, Achados: list) -> None:
    for Antes, Depois in zip(Entradas, Entradas[1:]):
        if (Antes["ISect"], Antes["Offset"]) > (Depois["ISect"], Depois["Offset"]):
            Achados.append(f"SectionContribution: fora de ordem (ISect, Offset) em {Depois['ISect']}:{Depois['Offset']}")
        if Antes["ISect"] == Depois["ISect"] and Antes["Offset"] + Antes["Tamanho"] > Depois["Offset"]:
            Achados.append(f"SectionContribution: sobreposicao na secao {Depois['ISect']} em {Depois['Offset']}")


def LerContribuicoes(Dados: bytes, Achados: list) -> list:
    if not Dados:
        return []
    (Versao,) = LerCampos(U32, Dados, 0)
    if Versao != SC_VERSAO:
        Achados.append(f"SectionContribution: versao 0x{Versao:08X} (esperado 0xF12EBA2D)")
    Corpo = len(Dados) - U32.size
    if Corpo % CONTRIBUICAO.size:
        Achados.append(f"SectionContribution: {Corpo} B de entradas nao e multiplo de {CONTRIBUICAO.size}")
    Entradas = [dict(zip(CAMPOS_CONTRIBUICAO, LerCampos(CONTRIBUICAO, Dados, U32.size + CONTRIBUICAO.size * K)))
                for K in range(Corpo // CONTRIBUICAO.size)]
    ChecarOrdemContribuicoes(Entradas, Achados)
    return Entradas


def LerMapaSecoes(Dados: bytes, Achados: list) -> list:
    if not Dados:
        return []
    Quantidade, Logicos = LerCampos(MAPA_CABECALHO, Dados, 0)
    if Quantidade != Logicos:
        Achados.append(f"SectionMap: Count {Quantidade} != LogCount {Logicos}")
    if len(Dados) != MAPA_CABECALHO.size + MAPA_SECAO.size * Quantidade:
        Achados.append(f"SectionMap: {len(Dados)} B nao bate com Count={Quantidade}")
    Entradas = [dict(zip(CAMPOS_SECAO, LerCampos(MAPA_SECAO, Dados, MAPA_CABECALHO.size + MAPA_SECAO.size * K)))
                for K in range(Quantidade)]
    if Entradas and (Entradas[-1]["Flags"], Entradas[-1]["TamanhoByte"]) != (SM_ABSOLUTA_FLAGS, SM_ABSOLUTA_TAMANHO):
        Achados.append("SectionMap: ultima entrada nao e absoluta (flags 0x208, tamanho 0xFFFFFFFF)")
    return Entradas


def ChecarCabecalhosSecao(Indice: int, Fluxos: dict, Achados: list) -> None:
    if Indice == FLUXO_NULO:
        return
    if Indice not in Fluxos:
        Achados.append(f"OptionalDbgHeader: SectionHdr aponta para o stream {Indice}, que nao existe")
    elif len(Fluxos[Indice]) % 40:
        Achados.append(f"OptionalDbgHeader: stream SectionHdr {Indice} com {len(Fluxos[Indice])} B, nao e multiplo de 40")


def LerCabecalhosOpcionais(Dados: bytes, Fluxos: dict, Achados: list) -> dict:
    if not Dados:
        return {}
    if len(Dados) != OPCIONAL.size:
        Achados.append(f"OptionalDbgHeader: {len(Dados)} B (esperado {OPCIONAL.size})")
    Opcional = dict(zip(CAMPOS_OPCIONAL, LerCampos(OPCIONAL, Dados, 0)))
    ChecarCabecalhosSecao(Opcional["SectionHdr"], Fluxos, Achados)
    return Opcional


# ---------- stream do modulo: simbolos ----------

def LerGproc(Dados: bytes, Pos: int, Total: int) -> dict:
    Campos = dict(zip(CAMPOS_GPROC, LerCampos(GPROC, Dados, Pos + REGISTRO.size)))
    Nome, _Depois = LerCString(Dados, Pos + REGISTRO.size + GPROC.size, Pos + Total)
    return {"nome": Nome, "end": Campos["Fim"], "inicio": Campos["Inicio"], "tamanho": Campos["TamanhoCodigo"]}


def LerRegrel(Dados: bytes, Pos: int, Total: int) -> dict:
    """S_REGREL32: deslocamento (i32, a partir do registro em 'registro'), tipo TPI e nome."""
    Deslocamento, Tipo, Registro = LerCampos(REGREL, Dados, Pos + REGISTRO.size)
    Nome, _Depois = LerCString(Dados, Pos + REGISTRO.size + REGREL.size, Pos + Total)
    return {"deslocamento": Deslocamento, "tipo_tpi": Tipo, "registro": Registro, "nome": Nome}


DETALHES_SIMBOLO = {S_GPROC32: LerGproc, S_REGREL32: LerRegrel}


def LerRegistrosSimbolo(Dados: bytes, Inicio: int, Fim: int, Achados: list) -> list:
    """Registros de simbolo entre Inicio e Fim: u16 tamanho (sem ele mesmo), u16 tipo, total multiplo de 4."""
    Registros, Pos = [], Inicio
    while Pos < Fim:
        Tamanho, Tipo = LerCampos(REGISTRO, Dados, Pos)
        Total = Tamanho + 2
        if Total < 4 or Total % 4 or Pos + Total > Fim:
            Achados.append(f"simbolo em {Pos}: {Total} B nao alinhado a 4 ou passa do fim da regiao de simbolos")
            break
        Registro = {"off": Pos, "tipo": Tipo, "total": Total}
        if Tipo in DETALHES_SIMBOLO:
            Registro.update(DETALHES_SIMBOLO[Tipo](Dados, Pos, Total))
        Registros.append(Registro)
        Pos += Total
    return Registros


def ProcsDoModulo(Registros: list, Achados: list) -> list:
    """Cada S_GPROC32 tem de apontar, por End, para um S_END do mesmo stream."""
    Tipos = {Registro["off"]: Registro["tipo"] for Registro in Registros}
    Procs = [Registro for Registro in Registros if Registro["tipo"] == S_GPROC32]
    for Proc in Procs:
        if Tipos.get(Proc["end"]) != S_END:
            Achados.append(f"S_GPROC32 {Proc['nome']} em {Proc['off']}: End {Proc['end']} nao aponta para S_END "
                           f"(tipo la = {Tipos.get(Proc['end'])})")
    return Procs


def LocaisDoModulo(Registros: list, Procs: list, Achados: list) -> list:
    """S_REGREL32 tem de cair entre o S_GPROC32 do proc e o seu S_END. Registro que nao e RBP e achado: variavel em
    registrador (M7a, R-M6-2) pede outra prova; nesta versao so o RBP do quadro e aceito."""
    Locais = []
    for Registro in (R for R in Registros if R["tipo"] == S_REGREL32):
        Dono = next((Proc for Proc in Procs if Proc["off"] < Registro["off"] < Proc["end"]), None)
        if Dono is None:
            Achados.append(f"S_REGREL32 {Registro['nome']} em {Registro['off']}: fora de qualquer S_GPROC32..S_END")
            continue
        if Registro["registro"] != CV_AMD64_RBP:
            Achados.append(f"S_REGREL32 {Registro['nome']}: registro {Registro['registro']} (so RBP={CV_AMD64_RBP} nesta versao)")
        Locais.append({**Registro, "proc": Dono["nome"]})
    return Locais


# ---------- stream do modulo: substreams C13 ----------

def LerSubstreamsC13(Dados: bytes, Inicio: int, Fim: int, Achados: list) -> list:
    Subs, Pos = [], Inicio
    while Pos < Fim:
        Tipo, Tamanho = LerCampos(C13_CABECALHO, Dados, Pos)
        if Pos + C13_CABECALHO.size + Tamanho > Fim:
            Achados.append(f"C13 tipo 0x{Tipo:X} em {Pos}: {Tamanho} B passam do fim do stream")
            break
        if Tamanho % 4:
            Achados.append(f"C13 tipo 0x{Tipo:X} em {Pos}: {Tamanho} B nao alinhado a 4")
        Subs.append((Tipo, Dados[Pos + C13_CABECALHO.size:Pos + C13_CABECALHO.size + Tamanho]))
        Pos = Alinhar4(Pos + C13_CABECALHO.size + Tamanho)
    return Subs


def LerLinhaNo(Corpo: bytes, Pos: int, CbCon: int, Achados: list) -> dict:
    Off, Bruto = LerCampos(LINHA, Corpo, Pos)
    if Off >= CbCon:
        Achados.append(f"DEBUG_S_LINES: offset {Off} >= cbCon {CbCon}")
    return {"off": Off, "linha": Bruto & 0xFFFFFF, "statement": bool(Bruto >> 31)}


def LerLinhas(Corpo: bytes, Achados: list) -> dict:
    """DEBUG_S_LINES: cabecalho do contribution e blocos por arquivo; BlockSize = 12 + 8 * nLines."""
    OffCon, SegCon, _Flags, CbCon = LerCampos(LINHAS_CABECALHO, Corpo, 0)
    Resultado = {"offcon": OffCon, "segcon": SegCon, "cbcon": CbCon, "blocos": []}
    Pos = LINHAS_CABECALHO.size
    while Pos < len(Corpo):
        FileId, NLinhas, CbBloco = LerCampos(LINHAS_BLOCO, Corpo, Pos)
        if CbBloco != LINHAS_BLOCO.size + LINHA.size * NLinhas:
            Achados.append(f"DEBUG_S_LINES (offcon 0x{OffCon:X}): BlockSize {CbBloco} != 12 + 8 * {NLinhas}")
        Linhas = [LerLinhaNo(Corpo, Pos + LINHAS_BLOCO.size + LINHA.size * K, CbCon, Achados) for K in range(NLinhas)]
        Resultado["blocos"].append({"file_id": FileId, "linhas": Linhas})
        if CbBloco <= 0:
            break
        Pos += CbBloco
    return Resultado


def ResolverArquivo(NomeOff: int, Nomes, Achados: list) -> dict:
    """NameIndex tem de ser o offset de um nome do /names; Nomes = None quando o PDB nao tem /names."""
    if Nomes is None:
        Achados.append("DEBUG_S_FILECHKSMS: sem /names para resolver NameIndex")
        return {}
    if NomeOff not in Nomes["nomes"]:
        Achados.append(f"DEBUG_S_FILECHKSMS: NameIndex {NomeOff} nao cai num nome do /names")
        return {}
    return {"nome": Nomes["nomes"][NomeOff]}


def LerArquivos(Corpo: bytes, Nomes, Achados: list) -> list:
    """DEBUG_S_FILECHKSMS: entradas de 8 B (NameIndex, cbChecksum, tipo, pad); sem checksum nesta versao."""
    if len(Corpo) % ARQUIVO.size:
        Achados.append(f"DEBUG_S_FILECHKSMS: {len(Corpo)} B nao e multiplo de {ARQUIVO.size}")
    Arquivos = []
    for K in range(len(Corpo) // ARQUIVO.size):
        NomeOff, Checksum, _Tipo, _Pad = LerCampos(ARQUIVO, Corpo, ARQUIVO.size * K)
        if Checksum:
            Achados.append(f"DEBUG_S_FILECHKSMS: checksum de {Checksum} B nao suportado nesta versao")
        Arquivos.append({"off": ARQUIVO.size * K, "nome_off": NomeOff, **ResolverArquivo(NomeOff, Nomes, Achados)})
    return Arquivos


def ChecarIdsDeArquivo(Linhas: list, Arquivos: list, Achados: list) -> None:
    Validos = {Arquivo["off"] for Arquivo in Arquivos}
    for Bloco in (Bloco for Linha in Linhas for Bloco in Linha["blocos"]):
        if Bloco["file_id"] not in Validos:
            Achados.append(f"DEBUG_S_LINES: FileId {Bloco['file_id']} nao cai no inicio de entrada do FILECHKSMS")


def AnalisarC13(Subs: list, Nomes, Achados: list) -> dict:
    Linhas = [LerLinhas(Corpo, Achados) for Tipo, Corpo in Subs if Tipo == C13_LINHAS]
    Arquivos = [Arquivo for Tipo, Corpo in Subs if Tipo == C13_ARQUIVOS
                for Arquivo in LerArquivos(Corpo, Nomes, Achados)]
    ChecarIdsDeArquivo(Linhas, Arquivos, Achados)
    return {"linhas": Linhas, "arquivos": Arquivos}


def AnalisarModulo(Dados: bytes, Modulo: dict, Nomes, Achados: list) -> dict:
    """Stream do modulo: assinatura 4 + registros (TamanhoSimbolos conta a assinatura), C11, substreams C13 (TamanhoC13) e GlobalRefsSize u32."""
    (Assinatura,) = LerCampos(U32, Dados, 0)
    Resultado = {"procs": [], "linhas": [], "arquivos": [], "locais": []}
    if Assinatura != ASSINATURA_C13:
        Achados.append(f"modulo {Modulo['nome']}: assinatura {Assinatura} (esperado 4)")
        return Resultado
    FimSim = Modulo["TamanhoSimbolos"]
    IniC13 = FimSim + Modulo["TamanhoC11"]
    FimC13 = IniC13 + Modulo["TamanhoC13"]
    if FimC13 + U32.size != len(Dados):
        Achados.append(f"modulo {Modulo['nome']}: SymByteSize + C11 + C13 + GlobalRefsSize(4) = {FimC13 + U32.size}, o stream tem {len(Dados)} B")
    Registros = LerRegistrosSimbolo(Dados, U32.size, FimSim, Achados)
    Resultado["procs"] = ProcsDoModulo(Registros, Achados)
    Resultado["locais"] = LocaisDoModulo(Registros, Resultado["procs"], Achados)
    Subs = LerSubstreamsC13(Dados, IniC13, FimC13, Achados)
    Resultado.update(AnalisarC13(Subs, Nomes, Achados))
    return Resultado


def AnalisarModuloDoDbi(Modulo: dict, Fluxos: dict, Nomes, Achados: list) -> dict:
    Indice = Modulo["FluxoSimbolos"]
    if Indice not in Fluxos:
        Achados.append(f"modulo {Modulo['nome']}: stream de simbolos {Indice} nao existe")
        return {**Modulo, "procs": [], "linhas": [], "arquivos": [], "locais": []}
    return {**Modulo, **AnalisarModulo(Fluxos[Indice], Modulo, Nomes, Achados)}


def AnalisarDbiCompleto(Dbi: bytes, Fluxos: dict, Nomes, Achados: list) -> dict:
    Cab = LerDbiCabecalho(Dbi)
    ChecarCabecalhoDbi(Cab, Achados)
    Partes = SepararSubstreams(Dbi, Cab, Achados)
    Modulos = [AnalisarModuloDoDbi(Mod, Fluxos, Nomes, Achados) for Mod in LerModulos(Partes["ModInfo"], Achados)]
    return {"cabecalho": Cab,
            "modulos": Modulos,
            "contribuicoes": LerContribuicoes(Partes["SectionContribution"], Achados),
            "secoes": LerMapaSecoes(Partes["SectionMap"], Achados),
            "cabecalhos": LerCabecalhosOpcionais(Partes["OptionalDbgHeader"], Fluxos, Achados)}


def AnalisarDbi(Fluxos: dict, Nomes) -> dict:
    """Valida o DBI e os modulos. DBI ausente ou vazio = sem modulo, sem achado. Nomes: saida de LerNomes ou None."""
    Achados = []
    Resumo = {"tem_modulo": False, "modulos": [], "contribuicoes": [], "secoes": [], "cabecalhos": {},
              "achados": Achados}
    Dbi = Fluxos.get(DBI_STREAM, b"")
    if Dbi:
        try:
            Resumo.update(AnalisarDbiCompleto(Dbi, Fluxos, Nomes, Achados))
        except ValueError as Erro:
            Achados.append(f"DBI: {Erro}")
    Resumo["tem_modulo"] = bool(Resumo["modulos"])
    return Resumo


# ---------- construtores sinteticos (--selftest) ----------

def TamanhoGproc(Nome: str) -> int:
    return Alinhar4(REGISTRO.size + GPROC.size + len(Nome.encode()) + 1)


def FazerGproc(Nome: str, Inicio: int, Tamanho: int, Fim: int) -> bytes:
    Total = TamanhoGproc(Nome)
    Corpo = GPROC.pack(0, Fim, 0, Tamanho, 0, Tamanho, 0x1000, Inicio, 1, 0)
    Registro = REGISTRO.pack(Total - 2, S_GPROC32) + Corpo + Nome.encode() + b"\0"
    return Registro.ljust(Total, b"\0")


def FazerQuadro(Curto: bool) -> bytes:
    """S_FRAMEPROC de 32 B; Curto = 30 B (defeito de alinhamento de proposito)."""
    if Curto:
        return REGISTRO.pack(QUADRO_CURTO.size + 2, S_FRAMEPROC) + QUADRO_CURTO.pack(0, 0, 0, 0, 0, 0, 0)
    return REGISTRO.pack(QUADRO.size + 2, S_FRAMEPROC) + QUADRO.pack(0, 0, 0, 0, 0, 0, 0, 0)


def FazerRegrel(Nome: str, Deslocamento: int, Tipo: int) -> bytes:
    """S_REGREL32 em RBP (CV_AMD64_RBP): total multiplo de 4, preenchido com zeros como os demais simbolos."""
    Total = Alinhar4(REGISTRO.size + REGREL.size + len(Nome.encode()) + 1)
    Registro = REGISTRO.pack(Total - 2, S_REGREL32) + REGREL.pack(Deslocamento, Tipo, CV_AMD64_RBP)
    return (Registro + Nome.encode() + b"\0").ljust(Total, b"\0")


def ConstruirSimbolos(Procs: list, Defeito=None, Locais=None) -> bytes:
    """S_GPROC32 + S_FRAMEPROC + S_REGREL32 (so os Locais = [(nome, deslocamento, tipo)], no 1o proc) + S_END por proc,
    com posicoes contadas a partir do offset 4 do stream."""
    Corpo = bytearray()
    for Indice, (Nome, Inicio, Tamanho, _Linhas) in enumerate(Procs):
        Primeiro = Indice == 0
        Quadro = FazerQuadro(Defeito == "alinhamento" and Primeiro)
        Regrels = b"".join(FazerRegrel(*Local) for Local in Locais) if (Primeiro and Locais) else b""
        Pos = U32.size + len(Corpo)
        Fim = Pos + TamanhoGproc(Nome) + len(Quadro) + len(Regrels) + (8 if Defeito == "end" and Primeiro else 0)
        Corpo += FazerGproc(Nome, Inicio, Tamanho, Fim) + Quadro + Regrels + REGISTRO.pack(2, S_END)
    return bytes(Corpo)


def FazerLinhasProc(Inicio: int, Tamanho: int, Linhas: list, BlocoErrado: bool) -> bytes:
    CbBloco = LINHAS_BLOCO.size + LINHA.size * len(Linhas) + (2 if BlocoErrado else 0)
    Bloco = LINHAS_BLOCO.pack(0, len(Linhas), CbBloco) + b"".join(LINHA.pack(Off, Num) for Off, Num in Linhas)
    return LINHAS_CABECALHO.pack(Inicio, 1, 0, Tamanho) + Bloco


def ConstruirSubstreamC13(Tipo: int, Corpo: bytes) -> bytes:
    return C13_CABECALHO.pack(Tipo, len(Corpo)) + Corpo.ljust(Alinhar4(len(Corpo)), b"\0")


def ConstruirC13(Procs: list, Defeito=None) -> bytes:
    Partes = [ConstruirSubstreamC13(C13_LINHAS, FazerLinhasProc(Inicio, Tamanho, Linhas,
                                                                Defeito == "blocksize" and Indice == 0))
              for Indice, (_Nome, Inicio, Tamanho, Linhas) in enumerate(Procs)]
    Partes.append(ConstruirSubstreamC13(C13_ARQUIVOS, ARQUIVO.pack(NOME_ARQUIVO_OFFSET, 0, 0, 0)))
    return b"".join(Partes)


def ConstruirModulo(Procs: list, Defeito=None, Locais=None) -> tuple:
    """Stream do modulo (assinatura + simbolos + C13 + GlobalRefsSize 0); devolve (bytes, SymByteSize com a assinatura, TamanhoC13) para o ModInfo."""
    Sim = ConstruirSimbolos(Procs, Defeito, Locais)
    C13 = ConstruirC13(Procs, Defeito)
    return U32.pack(ASSINATURA_C13) + Sim + C13 + U32.pack(0), U32.size + len(Sim), len(C13)


def ConstruirModInfo(Fluxo: int, Tamanhos: tuple, Contribuicao: tuple, Nomes: tuple) -> bytes:
    """Entrada do ModInfo: Tamanhos = (TamanhoSimbolos, TamanhoC13); Contribuicao = (ISect, Offset, Tamanho);
    Nomes = (ModuleName, ObjFileName)."""
    TamSim, TamC13 = Tamanhos
    Isect, Offset, Tamanho = Contribuicao
    Nome, Obj = Nomes
    Corpo = U32.pack(0) + CONTRIBUICAO.pack(Isect, 0, Offset, Tamanho, SC_CARACTERISTICAS, 0, 0, 0, 0)
    Corpo += MODINFO_CAMPOS.pack(0, Fluxo, TamSim, 0, TamC13, 1, 0, 0, 0, 0)
    Corpo += Nome.encode() + b"\0" + Obj.encode() + b"\0"
    return Corpo.ljust(Alinhar4(len(Corpo)), b"\0")


def ConstruirContribuicoes(Entradas: list) -> bytes:
    return U32.pack(SC_VERSAO) + b"".join(CONTRIBUICAO.pack(Isect, 0, Offset, Tamanho, SC_CARACTERISTICAS, 0, 0, 0, 0)
                                          for Isect, Offset, Tamanho in Entradas)


SECOES_SELFTEST = ((0x0108, 1, 0x1000), (0x0108, 2, 0x1000), (0x0208, 0, 0xFFFFFFFF))


def ConstruirMapaSecoes() -> bytes:
    """Duas secoes e a entrada absoluta do fim (flags 0x208, tamanho 0xFFFFFFFF)."""
    Corpo = MAPA_CABECALHO.pack(len(SECOES_SELFTEST), len(SECOES_SELFTEST))
    for Flags, Frame, Tamanho in SECOES_SELFTEST:
        Corpo += MAPA_SECAO.pack(Flags, 0, 0, Frame, 0xFFFF, 0xFFFF, 0, Tamanho)
    return Corpo


def ConstruirCabecalhosOpcionais() -> bytes:
    Campos = [FLUXO_NULO] * len(CAMPOS_OPCIONAL)
    Campos[CAMPOS_OPCIONAL.index("SectionHdr")] = FLUXO_CABECALHOS_SECAO
    return OPCIONAL.pack(*Campos)


def ConstruirDbi(ModInfo: bytes, Contribuicoes: bytes, Secoes: bytes, Opcional: bytes) -> bytes:
    """Cabecalho de 64 B com os tamanhos reais (idade IDADE_SINTETICA); FileInfo, TypeServer e EC saem vazios."""
    Cab = DBI_CABECALHO.pack(DBI_ASSINATURA, DBI_VERSAO, IDADE_SINTETICA, FLUXO_NULO, DBI_CONSTRUCAO, FLUXO_NULO, 0,
                             FLUXO_NULO, 0, len(ModInfo), len(Contribuicoes), len(Secoes), 0, 0, 0, len(Opcional),
                             0, 0, MAQUINA_AMD64, 0)
    return Cab + ModInfo + Contribuicoes + Secoes + Opcional
