#!/usr/bin/env python3
"""ler_pdb_tpi.py - TPI (stream 2) e IPI (stream 4) do PDB, e o tipo de cada S_REGREL32 (M6b P77, D-M6-9).

Versao: 0.1.0
Changelog: 0.1.0 - P77-0: cabecalho de 56 B (Version 20040203, HeaderSize 56, TypeIndexBegin 0x1000, TypeRecordBytes,
                   hash sem fluxo) e registros u16 tamanho (sem ele) + u16 folha + corpo, em multiplo de 4 com LF_PAD
                   F3 F2 F1 (zero = achado). Decodifica LF_POINTER, LF_FIELDLIST (LF_MEMBER, LF_ENUMERATE), LF_STRUCTURE,
                   LF_ARRAY e LF_ENUM; numeric leaf (< 0x8000 direto; 0x8000..0x800A). Checa: TypeRecordBytes e fim do
                   fluxo, indice a frente ou fora do TPI, tipo simples valido (0x0600|T = ponteiro 64 bits), ponteiro de
                   8 B com ptrtype 0x0C, array com indice 0x23, struct/enum com count == membros e fieldlist 0 so em forward
                   ref. IPI: cabecalho e fluxo; registro = achado (nao checado nesta versao). Tipo do S_REGREL32 tem de ser
                   simples valido ou existir no TPI. Oraculo e defeitos sinteticos (ConstruirTpi) para o --selftest.
Uso: importado por ler_pdb.py (nao roda sozinho).
Depende de: stdlib (struct); ler_pdb_dbi (LerCampos, LerCString).
"""
import struct

from ler_pdb_dbi import LerCampos, LerCString

TPI_FLUXO = 2
IPI_FLUXO = 4
TPI_VERSAO = 20040203
TPI_TAM_CABECALHO = 56
TIPO_INICIO = 0x1000
FLUXO_HASH_NENHUM = 0xFFFF
NUM_BALDES_HASH = 0x3FFFF
CAMPOS_CABECALHO = ("Version", "HeaderSize", "TypeIndexBegin", "TypeIndexEnd", "TypeRecordBytes", "HashStreamIndex",
                    "HashAuxStreamIndex", "HashKeySize", "NumHashBuckets", "HashValueBufferOffset",
                    "HashValueBufferLength", "IndexOffsetBufferOffset", "IndexOffsetBufferLength",
                    "HashAdjBufferOffset", "HashAdjBufferLength")
CABECALHO_TIPOS = struct.Struct("<IIIIIHHIIiIiIiI")
REGISTRO_TIPO = struct.Struct("<HH")
FOLHA = struct.Struct("<H")
DUAS_U32 = struct.Struct("<II")
PONTEIRO = struct.Struct("<II")
CLASSE_CABECALHO = struct.Struct("<HHIII")
ENUM_CABECALHO = struct.Struct("<HHII")
MEMBRO_CABECALHO = struct.Struct("<HI")
NUMERICO_CODIGOS = {0x8000: ("<b", 1), 0x8001: ("<h", 2), 0x8002: ("<H", 2), 0x8003: ("<i", 4),
                    0x8004: ("<I", 4), 0x8009: ("<q", 8), 0x800A: ("<Q", 8)}
LF_POINTER = 0x1002
LF_ARRAY = 0x1503
LF_STRUCTURE = 0x1505
LF_ENUM = 0x1507
LF_FIELDLIST = 0x1203
LF_MEMBER = 0x150D
LF_ENUMERATE = 0x1502
PROP_FWDREF = 0x0080
PROP_NOME_UNICO = 0x0200
PONTEIRO_TIPO_64 = 0x0C
PONTEIRO_TAMANHO = 8
PONTEIRO_SIMPLES = 0x0600
INDICE_ARRAY = 0x0023
SIMPLES_VALIDOS = frozenset((0x0023, 0x0030, 0x0040, 0x0041, 0x0068, 0x0069, 0x0072, 0x0073, 0x0074, 0x0075,
                             0x0076, 0x0077))
TIPO_INT = 0x0074
TIPO_F64 = 0x0041
ATRIBUTO_PUBLICO = 3
LOCAIS_SELFTEST = [("n", -8, TIPO_INT), ("p", -24, 0x1001)]
ORACULO_TIPOS = {
    0x1000: {"leaf": LF_FIELDLIST, "membros": [("px", TIPO_INT, 0), ("py", TIPO_INT, 4)]},
    0x1001: {"leaf": LF_STRUCTURE, "count": 2, "fieldlist": 0x1000, "tamanho": 8, "nome": "Ponto", "property": 0},
    0x1002: {"leaf": LF_POINTER, "tipo": 0x1001, "tamanho": 8},
    0x1003: {"leaf": LF_ARRAY, "elemento": TIPO_INT, "indice": INDICE_ARRAY, "tamanho": 16, "nome": ""},
    0x1004: {"leaf": LF_FIELDLIST, "enumeradores": [("A", 0), ("B", 1)]},
    0x1005: {"leaf": LF_ENUM, "count": 2, "base": TIPO_INT, "fieldlist": 0x1004, "nome": "Cor"},
}


# ---------- folhas: numeric leaf e padding ----------

def LerNumerico(Reg: bytes, Pos: int) -> tuple:
    """Numeric leaf: valor menor que 0x8000 vem direto em u16; senao o codigo diz o tamanho do valor."""
    (Folha,) = LerCampos(FOLHA, Reg, Pos)
    if Folha < 0x8000:
        return Folha, Pos + 2
    if Folha not in NUMERICO_CODIGOS:
        raise ValueError(f"numeric leaf 0x{Folha:04X} nao decodificada")
    Formato, Tamanho = NUMERICO_CODIGOS[Folha]
    (Valor,) = LerCampos(struct.Struct(Formato), Reg, Pos + 2)
    return Valor, Pos + 2 + Tamanho


def BytesDePad(Posicao: int) -> bytes:
    """LF_PAD ate o proximo multiplo de 4 (Posicao = deslocamento dentro do registro): F3 F2 F1, nunca zero."""
    return bytes(0xF0 | Restam for Restam in range((4 - Posicao % 4) % 4, 0, -1))


def ConsumirPad(Reg: bytes, Pos: int, Achados: list, Onde: str) -> int:
    """Confere o LF_PAD em Pos (zero ou outro valor = achado) e devolve a posicao depois dele."""
    Esperado = BytesDePad(Pos)
    if Reg[Pos:Pos + len(Esperado)] != Esperado:
        Achados.append(f"{Onde}: pad em {Pos} nao e {Esperado.hex() or 'vazio'} (LF_PAD F3 F2 F1, nunca zero)")
    return Pos + len(Esperado)


# ---------- decodificadores por folha (Pos e relativo ao inicio do registro) ----------

def DecodificarPonteiro(Reg: bytes, _Achados: list) -> tuple:
    Tipo, Atributos = LerCampos(PONTEIRO, Reg, 4)
    Campos = {"tipo": Tipo, "tamanho": (Atributos >> 13) & 0x3F, "ptrtype": Atributos & 0x1F}
    return Campos, [Tipo], 4 + PONTEIRO.size


def DecodificarClasse(Reg: bytes, _Achados: list) -> tuple:
    Contagem, Propriedade, Lista, _Derivada, _Virtual = LerCampos(CLASSE_CABECALHO, Reg, 4)
    Tamanho, Pos = LerNumerico(Reg, 4 + CLASSE_CABECALHO.size)
    Nome, Pos = LerCString(Reg, Pos, len(Reg))
    if Propriedade & PROP_NOME_UNICO:
        _Unico, Pos = LerCString(Reg, Pos, len(Reg))
    Campos = {"count": Contagem, "property": Propriedade, "fieldlist": Lista, "tamanho": Tamanho, "nome": Nome}
    return Campos, ([Lista] if Lista else []), Pos


def DecodificarEnum(Reg: bytes, _Achados: list) -> tuple:
    Contagem, Propriedade, Base, Lista = LerCampos(ENUM_CABECALHO, Reg, 4)
    Nome, Pos = LerCString(Reg, 4 + ENUM_CABECALHO.size, len(Reg))
    if Propriedade & PROP_NOME_UNICO:
        _Unico, Pos = LerCString(Reg, Pos, len(Reg))
    Campos = {"count": Contagem, "property": Propriedade, "base": Base, "fieldlist": Lista, "nome": Nome}
    return Campos, [Base, Lista], Pos


def DecodificarArray(Reg: bytes, _Achados: list) -> tuple:
    Elemento, Indice = LerCampos(DUAS_U32, Reg, 4)
    Tamanho, Pos = LerNumerico(Reg, 4 + DUAS_U32.size)
    Nome, Pos = LerCString(Reg, Pos, len(Reg))
    return {"elemento": Elemento, "indice": Indice, "tamanho": Tamanho, "nome": Nome}, [Elemento, Indice], Pos


def DecodificarMembro(Reg: bytes, Pos: int) -> tuple:
    """LF_MEMBER a partir do attr (Pos): devolve ((nome, tipo, offset), tipo, posicao depois do nome)."""
    _Atributo, Tipo = LerCampos(MEMBRO_CABECALHO, Reg, Pos)
    Deslocamento, Depois = LerNumerico(Reg, Pos + MEMBRO_CABECALHO.size)
    Nome, Depois = LerCString(Reg, Depois, len(Reg))
    return (Nome, Tipo, Deslocamento), Tipo, Depois


def DecodificarEnumerador(Reg: bytes, Pos: int) -> tuple:
    """LF_ENUMERATE a partir do attr (Pos): devolve ((nome, valor), None, posicao depois do nome)."""
    Valor, Depois = LerNumerico(Reg, Pos + 2)
    Nome, Depois = LerCString(Reg, Depois, len(Reg))
    return (Nome, Valor), None, Depois


def DecodificarSubcampo(Reg: bytes, Pos: int) -> tuple:
    """Subregistro de LF_FIELDLIST na folha Pos: (chave da lista, item, tipo citado ou None, posicao depois)."""
    (Folha,) = LerCampos(FOLHA, Reg, Pos)
    if Folha == LF_MEMBER:
        Item, Tipo, Depois = DecodificarMembro(Reg, Pos + 2)
        return "membros", Item, Tipo, Depois
    if Folha == LF_ENUMERATE:
        Item, _Nada, Depois = DecodificarEnumerador(Reg, Pos + 2)
        return "enumeradores", Item, None, Depois
    raise ValueError(f"subregistro de LF_FIELDLIST 0x{Folha:04X} nao decodificado")


def DecodificarListaCampos(Reg: bytes, Achados: list) -> tuple:
    """LF_FIELDLIST: cada subregistro fecha em 4 com LF_PAD; so LF_MEMBER e LF_ENUMERATE nesta versao."""
    Campos, Refs, Pos = {"membros": [], "enumeradores": []}, [], 4
    while Pos < len(Reg):
        Chave, Item, Tipo, Depois = DecodificarSubcampo(Reg, Pos)
        Campos[Chave].append(Item)
        if Tipo is not None:
            Refs.append(Tipo)
        Pos = ConsumirPad(Reg, Depois, Achados, "LF_FIELDLIST")
    return Campos, Refs, Pos


DECODIFICADORES = {LF_POINTER: DecodificarPonteiro, LF_STRUCTURE: DecodificarClasse, LF_ENUM: DecodificarEnum,
                   LF_ARRAY: DecodificarArray, LF_FIELDLIST: DecodificarListaCampos}


def DecodificarRegistro(Reg: bytes, Indice: int, Achados: list) -> dict:
    """Um registro de tipo: folha, campos decodificados e indices citados. Folha sem decodificador = achado."""
    Onde = f"TPI 0x{Indice:04X}"
    (_Tamanho, Folha) = REGISTRO_TIPO.unpack_from(Reg, 0)
    Vazio = {"indice": Indice, "leaf": Folha, "campos": {}, "refs": []}
    if Folha not in DECODIFICADORES:
        Achados.append(f"{Onde}: folha 0x{Folha:04X} nao decodificada nesta versao")
        return Vazio
    try:
        Campos, Refs, Pos = DECODIFICADORES[Folha](Reg, Achados)
        Pos = ConsumirPad(Reg, Pos, Achados, Onde)
    except ValueError as Erro:
        Achados.append(f"{Onde}: {Erro}")
        return Vazio
    if Pos != len(Reg):
        Achados.append(f"{Onde}: {len(Reg) - Pos} B sobram depois do corpo")
    return {**Vazio, "campos": Campos, "refs": Refs}


# ---------- checagens por registro e do fluxo ----------

def TipoSimplesValido(Tipo: int) -> bool:
    """Tipo simples do CodeView (abaixo de 0x1000) da lista do D-M6-3, ou ponteiro 64 bits dele (0x0600|T)."""
    if Tipo >= TIPO_INICIO:
        return False
    if Tipo in SIMPLES_VALIDOS:
        return True
    return PONTEIRO_SIMPLES <= Tipo < PONTEIRO_SIMPLES + 0x100 and (Tipo & 0xFF) in SIMPLES_VALIDOS


def ProblemaDeReferencia(Ref: int, Indice: int, Fim: int) -> str:
    """Texto do problema com o indice citado, ou '' quando o indice e valido (simples, ou anterior e dentro do TPI)."""
    if Ref < TIPO_INICIO:
        return "" if TipoSimplesValido(Ref) else f"tipo simples 0x{Ref:04X} invalido"
    if Ref >= Indice:
        return f"cita 0x{Ref:04X} a frente (indice nao anterior)"
    if Ref >= Fim:
        return f"cita 0x{Ref:04X} fora do TPI (fim 0x{Fim:04X})"
    return ""


def ConferirReferencias(Registro: dict, Fim: int, Achados: list) -> None:
    """Todo indice citado tem de ser simples valido ou anterior ao proprio registro e dentro do TPI."""
    for Ref in Registro["refs"]:
        Problema = ProblemaDeReferencia(Ref, Registro["indice"], Fim)
        if Problema:
            Achados.append(f"TPI 0x{Registro['indice']:04X}: {Problema}")


def ConferirContagem(Registro: dict, Tipos: dict, Achados: list) -> None:
    """struct/enum: fieldlist 0 so em forward ref; senao aponta para LF_FIELDLIST e count == membros."""
    Campos = Registro["campos"]
    if Campos["property"] & PROP_FWDREF:
        return
    Onde = f"TPI 0x{Registro['indice']:04X}"
    Lista = Campos["fieldlist"]
    if Lista == 0:
        Achados.append(f"{Onde}: fieldlist 0 com count {Campos['count']} (use LF_FIELDLIST vazio)")
        return
    Alvo = Tipos.get(Lista)
    if Alvo is None or Alvo["leaf"] != LF_FIELDLIST:
        Achados.append(f"{Onde}: fieldlist 0x{Lista:04X} nao e LF_FIELDLIST")
        return
    Chave = "enumeradores" if Registro["leaf"] == LF_ENUM else "membros"
    Quantos = len(Alvo["campos"].get(Chave, []))
    if Campos["count"] != Quantos:
        Achados.append(f"{Onde}: count {Campos['count']} != {Quantos} membro(s) do fieldlist")


def ConferirPonteiro(Registro: dict, _Tipos: dict, Achados: list) -> None:
    Campos, Onde = Registro["campos"], f"TPI 0x{Registro['indice']:04X}"
    if Campos["tamanho"] != PONTEIRO_TAMANHO or Campos["ptrtype"] != PONTEIRO_TIPO_64:
        Achados.append(f"{Onde}: ponteiro {Campos['tamanho']} B ptrtype 0x{Campos['ptrtype']:02X} (x64: 8 B, 0x0C)")


def ConferirArray(Registro: dict, _Tipos: dict, Achados: list) -> None:
    if Registro["campos"]["indice"] != INDICE_ARRAY:
        Achados.append(f"TPI 0x{Registro['indice']:04X}: indice do array 0x{Registro['campos']['indice']:04X} "
                       f"(esperado 0x0023)")


def ConferirRegistro(Registro: dict, Tipos: dict, Fim: int, Achados: list) -> None:
    ConferirReferencias(Registro, Fim, Achados)
    if Registro["campos"] and Registro["leaf"] in CHECAGENS_FOLHA:
        CHECAGENS_FOLHA[Registro["leaf"]](Registro, Tipos, Achados)


CHECAGENS_FOLHA = {LF_POINTER: ConferirPonteiro, LF_ARRAY: ConferirArray, LF_STRUCTURE: ConferirContagem,
                   LF_ENUM: ConferirContagem}


def LerCabecalhoTipos(Dados: bytes, Nome: str, Achados: list) -> dict:
    Cab = dict(zip(CAMPOS_CABECALHO, LerCampos(CABECALHO_TIPOS, Dados, 0)))
    if Cab["Version"] != TPI_VERSAO:
        Achados.append(f"{Nome}: Version {Cab['Version']} (esperado {TPI_VERSAO})")
    if Cab["HeaderSize"] != TPI_TAM_CABECALHO:
        Achados.append(f"{Nome}: HeaderSize {Cab['HeaderSize']} (esperado {TPI_TAM_CABECALHO})")
    if Cab["TypeIndexBegin"] != TIPO_INICIO:
        Achados.append(f"{Nome}: TypeIndexBegin 0x{Cab['TypeIndexBegin']:04X} (esperado 0x1000)")
    return Cab


def SepararRegistrosTipo(Dados: bytes, Cab: dict, Nome: str, Achados: list) -> list:
    """Registros do fim do cabecalho ate TypeRecordBytes; cada um com tamanho total multiplo de 4."""
    Fim = TPI_TAM_CABECALHO + Cab["TypeRecordBytes"]
    if Fim != len(Dados):
        Achados.append(f"{Nome}: cabecalho 56 + TypeRecordBytes {Cab['TypeRecordBytes']} = {Fim} B, o fluxo tem {len(Dados)} B")
    Regs, Pos, Limite = [], TPI_TAM_CABECALHO, min(Fim, len(Dados))
    while Pos < Limite:
        (Tamanho, _Folha) = LerCampos(REGISTRO_TIPO, Dados, Pos)
        Total = Tamanho + 2
        if Total < 4 or Total % 4 or Pos + Total > Limite:
            Achados.append(f"{Nome}: registro em {Pos} com {Total} B (nao alinhado a 4 ou passa do fim)")
            break
        Regs.append(Dados[Pos:Pos + Total])
        Pos += Total
    if Pos != Fim:
        Achados.append(f"{Nome}: registros somam {Pos - TPI_TAM_CABECALHO} B, TypeRecordBytes diz {Cab['TypeRecordBytes']}")
    return Regs


def AnalisarTpi(Dados: bytes, Achados: list) -> dict:
    if not Dados:
        return {"tipos": {}, "vazio": True, "cabecalho": {}}
    Cab = LerCabecalhoTipos(Dados, "TPI", Achados)
    Regs = SepararRegistrosTipo(Dados, Cab, "TPI", Achados)
    Tipos = {TIPO_INICIO + K: DecodificarRegistro(Reg, TIPO_INICIO + K, Achados) for K, Reg in enumerate(Regs)}
    Fim = TIPO_INICIO + len(Regs)
    if Cab["TypeIndexEnd"] != Fim:
        Achados.append(f"TPI: TypeIndexEnd 0x{Cab['TypeIndexEnd']:04X} != 0x{Fim:04X} (registros lidos)")
    for Registro in Tipos.values():
        ConferirRegistro(Registro, Tipos, Fim, Achados)
    return {"tipos": Tipos, "vazio": not Tipos, "cabecalho": Cab}


def AnalisarIpi(Dados: bytes, Achados: list) -> dict:
    """IPI: cabecalho e fluxo sao conferidos; registro de simbolo ainda nao e checado (o escritor grava IPI vazio)."""
    if not Dados:
        return {"vazio": True, "registros": 0}
    Cab = LerCabecalhoTipos(Dados, "IPI", Achados)
    Regs = SepararRegistrosTipo(Dados, Cab, "IPI", Achados)
    if Regs:
        Achados.append(f"IPI: {len(Regs)} registro(s) nao checados nesta versao (o escritor grava IPI vazio)")
    return {"vazio": not Regs, "registros": len(Regs)}


def AnalisarTipos(Fluxos: dict) -> dict:
    """TPI e IPI do PDB. Erro de cabecalho levanta ValueError dentro daqui e vira achado."""
    Achados = []
    try:
        Tpi = AnalisarTpi(Fluxos.get(TPI_FLUXO, b""), Achados)
        Ipi = AnalisarIpi(Fluxos.get(IPI_FLUXO, b""), Achados)
    except ValueError as Erro:
        Achados.append(f"TPI/IPI: {Erro}")
        Tpi, Ipi = {"tipos": {}, "vazio": True, "cabecalho": {}}, {"vazio": True, "registros": 0}
    return {"tpi": Tpi, "ipi": Ipi, "achados": Achados}


def TipoConhecido(Tipo: int, Tpi: dict) -> bool:
    return TipoSimplesValido(Tipo) or Tipo in Tpi["tipos"]


def ConferirTiposDosLocais(Modulos: list, Tpi: dict, Achados: list) -> None:
    """Tipo de cada S_REGREL32 tem de ser simples valido ou um registro do TPI."""
    Locais = [Local for Modulo in Modulos for Local in Modulo.get("locais", [])]
    for Local in Locais:
        if not TipoConhecido(Local["tipo_tpi"], Tpi):
            Achados.append(f"S_REGREL32 {Local['nome']}: tipo 0x{Local['tipo_tpi']:04X} nem simples valido nem no TPI")


# ---------- construtores sinteticos (--selftest) ----------

def ConstruirRegistroTipo(Folha: int, Corpo: bytes) -> bytes:
    """Registro de tipo: u16 tamanho (sem ele), u16 folha, corpo e LF_PAD ate 4."""
    Reg = bytearray(REGISTRO_TIPO.pack(0, Folha) + Corpo)
    Reg += BytesDePad(len(Reg))
    Reg[0:2] = struct.pack("<H", len(Reg) - 2)
    return bytes(Reg)


def ConstruirMembro(Nome: str, Tipo: int, Deslocamento: int) -> bytes:
    return (FOLHA.pack(LF_MEMBER) + MEMBRO_CABECALHO.pack(ATRIBUTO_PUBLICO, Tipo)
            + struct.pack("<H", Deslocamento) + Nome.encode() + b"\0")


def ConstruirEnumerador(Nome: str, Valor: int) -> bytes:
    return FOLHA.pack(LF_ENUMERATE) + FOLHA.pack(ATRIBUTO_PUBLICO) + struct.pack("<H", Valor) + Nome.encode() + b"\0"


def ConstruirListaCampos(Subs: list, Zerar: bool) -> bytes:
    """LF_FIELDLIST: cada subregistro fecha em 4 com LF_PAD; Zerar troca o pad do 1o por zeros (defeito de teste)."""
    Corpo = bytearray()
    for Indice, Sub in enumerate(Subs):
        Corpo += Sub
        Pad = BytesDePad(4 + len(Corpo))
        if Zerar and Indice == 0:
            Pad = bytes(len(Pad))
        Corpo += Pad
    return ConstruirRegistroTipo(LF_FIELDLIST, bytes(Corpo))


def TiposSelftest(Defeito=None) -> list:
    """Registros 0x1000..0x1005 (oraculo em ORACULO_TIPOS). Defeito: indice (ponteiro a frente), pad, count."""
    Membros = [ConstruirMembro("px", TIPO_INT, 0), ConstruirMembro("py", TIPO_INT, 4)]
    Contagem = 3 if Defeito == "count" else 2
    Alvo = 0x1010 if Defeito == "indice" else 0x1001
    Enumeradores = [ConstruirEnumerador("A", 0), ConstruirEnumerador("B", 1)]
    return [
        ConstruirListaCampos(Membros, Defeito == "pad"),
        ConstruirRegistroTipo(LF_STRUCTURE, CLASSE_CABECALHO.pack(Contagem, 0, 0x1000, 0, 0)
                              + struct.pack("<H", 8) + b"Ponto\0"),
        ConstruirRegistroTipo(LF_POINTER, PONTEIRO.pack(Alvo, PONTEIRO_TIPO_64 | (PONTEIRO_TAMANHO << 13))),
        ConstruirRegistroTipo(LF_ARRAY, DUAS_U32.pack(TIPO_INT, INDICE_ARRAY) + struct.pack("<H", 16) + b"\0"),
        ConstruirListaCampos(Enumeradores, False),
        ConstruirRegistroTipo(LF_ENUM, ENUM_CABECALHO.pack(2, 0, TIPO_INT, 0x1004) + b"Cor\0"),
    ]


def ConstruirTpi(Defeito=None) -> bytes:
    """TPI sintetico com o oraculo; Defeito 'bytes' soma 4 ao TypeRecordBytes (cabecalho errado)."""
    Lista = TiposSelftest(Defeito)
    Corpo = b"".join(Lista)
    Total = len(Corpo) + (4 if Defeito == "bytes" else 0)
    Cabecalho = CABECALHO_TIPOS.pack(TPI_VERSAO, TPI_TAM_CABECALHO, TIPO_INICIO, TIPO_INICIO + len(Lista), Total,
                                     FLUXO_HASH_NENHUM, FLUXO_HASH_NENHUM, 4, NUM_BALDES_HASH, 0, 0, 0, 0, 0, 0)
    return Cabecalho + Corpo


def DivergenciasDoRegistro(Indice: int, Esperado: dict, Reg) -> list:
    """Diferencas entre um registro lido (ou None, se ausente) e o oraculo dele."""
    if Reg is None:
        return [f"TPI 0x{Indice:04X}: ausente"]
    Erros = []
    if Reg["leaf"] != Esperado["leaf"]:
        Erros.append(f"TPI 0x{Indice:04X}: folha 0x{Reg['leaf']:04X} esperada 0x{Esperado['leaf']:04X}")
    for Chave, Valor in Esperado.items():
        if Chave != "leaf" and Reg["campos"].get(Chave) != Valor:
            Erros.append(f"TPI 0x{Indice:04X} {Chave}: lido {Reg['campos'].get(Chave)!r} esperado {Valor!r}")
    return Erros


def ConferirOraculoTipos(Tipos: dict, Modulos: list) -> list:
    """Compara o TPI e os locais lidos com ORACULO_TIPOS e LOCAIS_SELFTEST; devolve divergencias (vazia = PASSOU)."""
    Lidos = Tipos["tpi"]["tipos"]
    Erros = []
    for Indice, Esperado in ORACULO_TIPOS.items():
        Erros += DivergenciasDoRegistro(Indice, Esperado, Lidos.get(Indice))
    Locais = [(L["nome"], L["deslocamento"], L["tipo_tpi"]) for M in Modulos for L in M["locais"]]
    if Locais != LOCAIS_SELFTEST:
        Erros.append(f"S_REGREL32: lido {Locais} esperado {LOCAIS_SELFTEST}")
    return Erros
