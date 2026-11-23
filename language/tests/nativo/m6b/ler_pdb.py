#!/usr/bin/env python3
"""ler_pdb.py - leitor do PDB: container MSF 7.0, info (stream 1), /names (stream 5), DBI e modulos (M6b P74/P76-0, D-M6-9).

Versao: 0.3.0
Changelog: 0.3.0 - P77-0: TPI (stream 2) e IPI (stream 4) decodificados via ler_pdb_tpi.py (cabecalho de 56 B, registros
                   com LF_PAD F3 F2 F1, LF_POINTER/FIELDLIST/STRUCTURE/ARRAY/ENUM, checagens de indice, count e pad) e os
                   S_REGREL32 do modulo (locais) com o tipo conferido contra o TPI. Novos flags: --exigir-tipos (sai 3 com
                   'TPI: ...' se vazio ou com achado) e --exigir-locais (sai 3 sem S_REGREL32). Sem as flags, PDB com
                   TPI vazio despeja 'TPI: vazio' e sai 0. --selftest ganha um PDB com TPI de 6 tipos e 2 locais (oraculo
                   fixo) e 6 controles negativos de tipo (indice a frente, pad zero, count, TypeRecordBytes, End, tipo
                   de local inexistente).
                   0.2.0 - P76-0: despeja tambem o info stream (versao, idade, GUID, mapa de nomes), a tabela /names (hash
                   PdbHashV1, cada nome acha o seu bucket) e o DBI com os modulos (procs S_GPROC32, linhas DEBUG_S_LINES,
                   FILECHKSMS), via ler_pdb_dbi.py. Tolerante: DBI vazio (o PDB do P75) = sem modulo, sai 0.
                   --exigir-modulo sai 3 com 'DBI: ...' quando nao ha modulo/proc ou ha achado. --selftest ganha um PDB
                   sintetico com 1 modulo, 2 procs, 2 linhas por proc, SectionMap, OptionalDbgHeader e /names, conferido
                   contra oraculo fixo, mais o PDB vazio e 4 controles negativos (End, alinhamento, BlockSize, SC).
                   0.1.0 - P74: superbloco (magic, tamanho de bloco, NumBlocos, diretorio, endereco do mapa de blocos),
                   diretorio de streams (tamanho, lista de blocos e primeiros 16 bytes de cada stream).
Uso: python ler_pdb.py <arquivo.pdb> [--exigir-modulo] [--exigir-tipos] [--exigir-locais]
                                                        (despeja o MSF, info, /names, DBI, TPI e locais)
     python ler_pdb.py --selftest                        (prova do leitor, sem ferramenta externa)
Saida: 0 OK; 1 FALHOU (selftest); 2 arquivo nao e MSF 7.00, ou info/names com formato invalido;
       3 --exigir-modulo / --exigir-tipos / --exigir-locais sem o que foi pedido, ou com achado.
"""
import argparse
import struct
import sys
from pathlib import Path

from ler_pdb_dbi import (
    AnalisarDbi, ConstruirCabecalhosOpcionais, ConstruirContribuicoes, ConstruirDbi, ConstruirMapaSecoes,
    ConstruirModInfo, ConstruirModulo, LerCampos, LerCString, LerU32s,
)
from ler_pdb_tpi import (
    AnalisarTipos, ConferirOraculoTipos, ConferirTiposDosLocais, ConstruirTpi, LOCAIS_SELFTEST,
)

MAGICA_MSF70 = b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\x00\x00\x00"
TAMANHOS_BLOCO_VALIDOS = (512, 1024, 2048, 4096)
STREAM_NULA = 0xFFFFFFFF
FLUXO_INFO = 1
FLUXO_NOMES = 5
FLUXO_MODULO = 6
NOME_FLUXO_NOMES = "/names"
INFO_VERSAO = 20000404
INFO_CARACTERISTICA = 20140508
CAPACIDADE_MAPA = 8
NOMES_ASSINATURA = 0xEFFEEFFE
NOMES_VERSAO = 1
GUID_SELFTEST = bytes(range(16))
INFO_CABECALHO = struct.Struct("<III")
NOMES_CABECALHO = struct.Struct("<III")
U32 = struct.Struct("<I")
PAR = struct.Struct("<II")
CONTAGEM = struct.Struct("<II")
GUID = struct.Struct("<16s")
ARQUIVO_SELFTEST = "fonte.fj"
PROCS_SELFTEST = [("fj_alfa", 0x10, 0x20, [(0, 10), (8, 11)]), ("fj_beta", 0x30, 0x18, [(0, 20), (0x10, 21)])]
CONTROLES_NEGATIVOS = (("End apontando errado", "end"), ("alinhamento quebrado", "alinhamento"),
                       ("BlockSize errado", "blocksize"), ("SC desordenada", "sc"))
CONTROLES_TIPOS = (("indice a frente no TPI", "indice"), ("LF_PAD zero no fieldlist", "pad"),
                   ("count de struct != membros", "count"), ("TypeRecordBytes errado", "bytes"),
                   ("End do S_GPROC32 errado (com locais)", "end"),
                   ("tipo de S_REGREL32 inexistente", "tiporegrel"))


# ---------- MSF 7.0 (P74) ----------

def Teto(Tamanho: int, TamanhoBloco: int) -> int:
    """Quantidade de blocos necessaria para guardar Tamanho bytes."""
    return (Tamanho + TamanhoBloco - 1) // TamanhoBloco


def LerSuperbloco(Dados: bytes) -> dict:
    """Superbloco MSF 7.0: magic de 32 bytes seguido de 6 u32 (56 bytes)."""
    if len(Dados) < 56 or Dados[:32] != MAGICA_MSF70:
        raise ValueError("arquivo nao e MSF 7.00 (magic ausente)")
    Campos = struct.unpack_from("<6I", Dados, 32)
    Sb = dict(zip(["TamanhoBloco", "BlocoMapaLivre", "NumBlocos", "NumBytesDiretorio", "NaoUsado", "EnderecoMapa"], Campos))
    if Sb["TamanhoBloco"] not in TAMANHOS_BLOCO_VALIDOS:
        raise ValueError(f"tamanho de bloco invalido: {Sb['TamanhoBloco']}")
    return Sb


def LerBlocos(Dados: bytes, Sb: dict, Indices: list, Tamanho: int) -> bytes:
    """Concatena os blocos pelos indices e corta no tamanho real do dado."""
    B = Sb["TamanhoBloco"]
    Partes = []
    for Indice in Indices:
        if Indice >= Sb["NumBlocos"]:
            raise ValueError(f"bloco {Indice} fora do arquivo ({Sb['NumBlocos']} blocos)")
        Partes.append(Dados[Indice * B:(Indice + 1) * B])
    return b"".join(Partes)[:Tamanho]


def LerDiretorio(Dados: bytes, Sb: dict) -> bytes:
    """Diretorio de streams: o mapa (lista de blocos do diretorio) fica no bloco EnderecoMapa."""
    B = Sb["TamanhoBloco"]
    NBlocosDir = Teto(Sb["NumBytesDiretorio"], B)
    if NBlocosDir * 4 > B:
        raise ValueError("mapa de diretorio em mais de um bloco: nao suportado nesta versao")
    Mapa = Dados[Sb["EnderecoMapa"] * B:Sb["EnderecoMapa"] * B + NBlocosDir * 4]
    Indices = list(struct.unpack_from(f"<{NBlocosDir}I", Mapa))
    return LerBlocos(Dados, Sb, Indices, Sb["NumBytesDiretorio"])


def LerStreams(Diretorio: bytes, TamanhoBloco: int) -> list:
    """Le NumStreams, os tamanhos e, para cada stream, a lista de blocos (STREAM_NULA = stream sem dado)."""
    (NumStreams,) = struct.unpack_from("<I", Diretorio, 0)
    Tamanhos = struct.unpack_from(f"<{NumStreams}I", Diretorio, 4)
    Posicao = 4 + 4 * NumStreams
    Streams = []
    for Id, Tamanho in enumerate(Tamanhos):
        Nula = Tamanho == STREAM_NULA
        NBlocos = 0 if Nula else Teto(Tamanho, TamanhoBloco)
        Indices = list(struct.unpack_from(f"<{NBlocos}I", Diretorio, Posicao))
        Posicao += 4 * NBlocos
        Streams.append({"id": Id, "tamanho": 0 if Nula else Tamanho, "nula": Nula, "blocos": Indices})
    return Streams


def LerTudo(Dados: bytes) -> tuple:
    """Superbloco, lista de blocos do diretorio, streams e o dado de cada stream."""
    Sb = LerSuperbloco(Dados)
    Diretorio = LerDiretorio(Dados, Sb)
    Streams = LerStreams(Diretorio, Sb["TamanhoBloco"])
    for Stream in Streams:
        Stream["dados"] = LerBlocos(Dados, Sb, Stream["blocos"], Stream["tamanho"])
    return Sb, Streams


def ConstruirMsf(Payloads: list, TamanhoBloco: int = 4096) -> bytes:
    """MSF 7.0 sintetico: blocos 0 (superbloco), 1 e 2 (mapa livre, zerados), depois dados, diretorio e mapa."""
    Blocos = [bytearray(TamanhoBloco) for _ in range(3)]
    IndicesStreams = []
    for Payload in Payloads:
        Lista = []
        for Parte in range(Teto(len(Payload), TamanhoBloco)):
            Trecho = Payload[Parte * TamanhoBloco:(Parte + 1) * TamanhoBloco]
            Blocos.append(bytearray(Trecho.ljust(TamanhoBloco, b"\0")))
            Lista.append(len(Blocos) - 1)
        IndicesStreams.append(Lista)
    Diretorio = struct.pack("<I", len(Payloads))
    Diretorio += struct.pack(f"<{len(Payloads)}I", *[len(P) for P in Payloads])
    for Lista in IndicesStreams:
        Diretorio += struct.pack(f"<{len(Lista)}I", *Lista)
    IndicesDir = []
    for Parte in range(Teto(len(Diretorio), TamanhoBloco)):
        Trecho = Diretorio[Parte * TamanhoBloco:(Parte + 1) * TamanhoBloco]
        Blocos.append(bytearray(Trecho.ljust(TamanhoBloco, b"\0")))
        IndicesDir.append(len(Blocos) - 1)
    Blocos.append(bytearray(struct.pack(f"<{len(IndicesDir)}I", *IndicesDir).ljust(TamanhoBloco, b"\0")))
    EnderecoMapa = len(Blocos) - 1
    Superbloco = MAGICA_MSF70 + struct.pack("<6I", TamanhoBloco, 1, len(Blocos), len(Diretorio), 0, EnderecoMapa)
    Blocos[0][:len(Superbloco)] = Superbloco
    return b"".join(bytes(Bloco) for Bloco in Blocos)


def PayloadsDoSelftest() -> list:
    """Tres streams: 16 bytes (1 bloco), 5000 bytes (2 blocos, com troca de valor na fronteira), 100 bytes (1 bloco)."""
    return [bytes(range(16)), b"\x55" * 4096 + b"\xab" * 904, b"\xaa" * 100]


# Oraculo fixo: valores que o leitor tem de devolver para o MSF acima (calculados a mao, nao pelo leitor).
ORACULO_SELFTEST = {
    "NumBlocos": 9, "NumBytesDiretorio": 32, "EnderecoMapa": 8,
    "streams": [
        {"tamanho": 16, "blocos": [3], "cabecalho": "000102030405060708090a0b0c0d0e0f", "final": "0c0d0e0f"},
        {"tamanho": 5000, "blocos": [4, 5], "cabecalho": "55" * 16, "final": "abababab"},
        {"tamanho": 100, "blocos": [6], "cabecalho": "aa" * 16, "final": "aaaaaaaa"},
    ],
}


def ConferirSelftest(Sb: dict, Streams: list) -> list:
    """Compara o resultado do leitor com ORACULO_SELFTEST; devolve a lista de divergencias (vazia = PASSOU)."""
    Divergencias = []
    for Chave in ("NumBlocos", "NumBytesDiretorio", "EnderecoMapa"):
        if Sb[Chave] != ORACULO_SELFTEST[Chave]:
            Divergencias.append(f"{Chave}: lido {Sb[Chave]} esperado {ORACULO_SELFTEST[Chave]}")
    for Lido, Esperado in zip(Streams, ORACULO_SELFTEST["streams"]):
        Id = Lido["id"]
        for Campo, Valor in (("tamanho", Lido["tamanho"]), ("blocos", Lido["blocos"]),
                             ("cabecalho", Lido["dados"][:16].hex()), ("final", Lido["dados"][-4:].hex())):
            if Valor != Esperado[Campo]:
                Divergencias.append(f"stream {Id} {Campo}: lido {Valor} esperado {Esperado[Campo]}")
    if len(Streams) != len(ORACULO_SELFTEST["streams"]):
        Divergencias.append(f"quantidade de streams: lido {len(Streams)}")
    return Divergencias


def RecusaMagicErrado() -> bool:
    """True quando o leitor levanta ValueError para um arquivo sem magic MSF (caso negativo do selftest)."""
    try:
        LerSuperbloco(bytes(64))
    except ValueError:
        return True
    return False


# ---------- info stream e /names (P76-0) ----------

def HashV1(Texto: bytes) -> int:
    """Hash V1 do LLVM/MS, igual ao hashStringV1 de nativo\\dbg\\pdb_escrever.c (mapa de fluxos e /names)."""
    Resultado = 0
    Pos = 0
    while Pos + 4 <= len(Texto):
        Resultado ^= struct.unpack_from("<I", Texto, Pos)[0]
        Pos += 4
    if len(Texto) - Pos >= 2:
        Resultado ^= struct.unpack_from("<H", Texto, Pos)[0]
        Pos += 2
    if Pos < len(Texto):
        Resultado ^= Texto[Pos]
    Resultado |= 0x20202020
    Resultado ^= Resultado >> 11
    return Resultado ^ (Resultado >> 16)


def LerVetorBits(Dados: bytes, Pos: int) -> tuple:
    (Quantidade,) = LerCampos(U32, Dados, Pos)
    return LerU32s(Dados, Pos + 4, Quantidade), Pos + 4 + 4 * Quantidade


def LerMapaNomeado(Dados: bytes, Pos: int) -> tuple:
    """Mapa nome -> indice de fluxo: bloco de strings, Size, Capacity, bits presentes/removidos e pares chave/valor."""
    (TamBloco,) = LerCampos(U32, Dados, Pos)
    Strings = LerCampos(struct.Struct(f"<{TamBloco}s"), Dados, Pos + 4)[0]
    Pos += 4 + TamBloco
    Tamanho, _Capacidade = LerCampos(CONTAGEM, Dados, Pos)
    Presentes, Pos = LerVetorBits(Dados, Pos + CONTAGEM.size)
    _Removidos, Pos = LerVetorBits(Dados, Pos)
    Posicoes = [32 * Palavra + Bit for Palavra, Valor in enumerate(Presentes) for Bit in range(32) if Valor >> Bit & 1]
    if len(Posicoes) != Tamanho:
        raise ValueError(f"mapa de nomes: Size {Tamanho} != {len(Posicoes)} bits presentes")
    Mapa = {}
    for _ in Posicoes:
        Chave, Valor = LerCampos(PAR, Dados, Pos)
        Pos += PAR.size
        Nome, _Depois = LerCString(Strings, Chave, len(Strings))
        Mapa[Nome] = Valor
    return Mapa, Pos


def LerInfo(Dados: bytes) -> dict:
    """Info stream: versao, assinatura, idade, GUID de 16 B, mapa de fluxos nomeados e caracteristicas."""
    Versao, Assinatura, Idade = LerCampos(INFO_CABECALHO, Dados, 0)
    (Guid,) = LerCampos(GUID, Dados, INFO_CABECALHO.size)
    Nomeados, Pos = LerMapaNomeado(Dados, INFO_CABECALHO.size + GUID.size)
    if (len(Dados) - Pos) % 4:
        raise ValueError("info stream: caracteristicas nao alinhadas a 4")
    return {"versao": Versao, "assinatura": Assinatura, "idade": Idade, "guid": Guid.hex(), "nomeados": Nomeados,
            "caracteristicas": LerU32s(Dados, Pos, (len(Dados) - Pos) // 4)}


def AcheiBalde(Tabela: list, Valor: int, Nome: str) -> bool:
    """Sondagem linear a partir de hash % baldes: o nome tem de achar o proprio valor antes de um balde vazio."""
    Baldes = len(Tabela)
    Pos = HashV1(Nome.encode()) % Baldes
    for _ in range(Baldes):
        if Tabela[Pos] == Valor:
            return True
        if Tabela[Pos] == 0:
            return False
        Pos = (Pos + 1) % Baldes
    return False


def LerNomes(Dados: bytes) -> dict:
    """Tabela /names: cabecalho 0xEFFEEFFE/1, bloco de strings, baldes (offset do nome; 0 = vazio) e contagem."""
    Assinatura, Versao, TamBloco = LerCampos(NOMES_CABECALHO, Dados, 0)
    if Assinatura != NOMES_ASSINATURA or Versao != NOMES_VERSAO:
        raise ValueError(f"/names: assinatura 0x{Assinatura:08X} ou versao {Versao} invalida")
    Strings = LerCampos(struct.Struct(f"<{TamBloco}s"), Dados, NOMES_CABECALHO.size)[0]
    Pos = NOMES_CABECALHO.size + TamBloco
    (QtdBaldes,) = LerU32s(Dados, Pos, 1)
    Tabela = LerU32s(Dados, Pos + 4, QtdBaldes)
    (Contagem,) = LerU32s(Dados, Pos + 4 + 4 * QtdBaldes, 1)
    Nomes = {Valor: LerCString(Strings, Valor, len(Strings))[0] for Valor in Tabela if Valor}
    Achados = [f"/names: '{Nome}' nao acha o seu balde" for Valor, Nome in Nomes.items()
               if not AcheiBalde(Tabela, Valor, Nome)]
    if len(Nomes) != Contagem:
        Achados.append(f"/names: contagem {Contagem} != {len(Nomes)} baldes preenchidos")
    return {"tamanho_strings": TamBloco, "nomes": Nomes, "achados": Achados}


# ---------- construtores sinteticos ----------

def ConstruirInfo(Idade: int, Guid: bytes, Nomeados: list) -> bytes:
    """Info stream com o mapa de nomes (Capacity 8, linear probing); Nomeados = [(nome, indice de fluxo)]."""
    Strings, Ocupados = b"", {}
    for Nome, Valor in Nomeados:
        Balde = HashV1(Nome.encode()) % CAPACIDADE_MAPA
        while Balde in Ocupados:
            Balde = (Balde + 1) % CAPACIDADE_MAPA
        Ocupados[Balde] = (len(Strings), Valor)
        Strings += Nome.encode() + b"\0"
    Palavra = sum(1 << Balde for Balde in Ocupados)
    Pares = b"".join(PAR.pack(*Ocupados[Balde]) for Balde in sorted(Ocupados))
    Mapa = U32.pack(len(Strings)) + Strings + CONTAGEM.pack(len(Nomeados), CAPACIDADE_MAPA)
    Mapa += U32.pack(1) + U32.pack(Palavra) + U32.pack(0) + Pares
    return (INFO_CABECALHO.pack(INFO_VERSAO, 0, Idade) + GUID.pack(Guid) + Mapa + U32.pack(INFO_CARACTERISTICA))


def ConstruirNomes(Textos: list, Baldes: int = 8) -> bytes:
    """Tabela /names: string 0 e a cadeia vazia; cada texto entra no balde hash % Baldes com sondagem linear."""
    Strings, Offsets = b"\0", []
    for Texto in Textos:
        Offsets.append(len(Strings))
        Strings += Texto.encode() + b"\0"
    Tabela = [0] * Baldes
    for Texto, Offset in zip(Textos, Offsets):
        Pos = HashV1(Texto.encode()) % Baldes
        while Tabela[Pos]:
            Pos = (Pos + 1) % Baldes
        Tabela[Pos] = Offset
    return (NOMES_CABECALHO.pack(NOMES_ASSINATURA, NOMES_VERSAO, len(Strings)) + Strings + U32.pack(Baldes)
            + b"".join(U32.pack(Valor) for Valor in Tabela) + U32.pack(len(Textos)))


def ConstruirPdbSintetico(Defeito=None, Tipos=b"", Locais=None) -> bytes:
    """PDB com 1 modulo (2 procs), 1 SC, SectionMap de 2 secoes + absoluta, SectionHdr no stream 7 e /names.
    Tipos = bytes do TPI (stream 2); Locais = [(nome, deslocamento, tipo)] do 1o proc (S_REGREL32)."""
    Modulo, TamSim, TamC13 = ConstruirModulo(PROCS_SELFTEST, Defeito, Locais)
    Contribs = [(2, 0, 0x10), (1, 0x10, 0x38)] if Defeito == "sc" else [(1, 0x10, 0x38)]
    ModInfo = ConstruirModInfo(FLUXO_MODULO, (TamSim, TamC13), (1, 0x10, 0x38), (ARQUIVO_SELFTEST, "fonte.obj"))
    Dbi = ConstruirDbi(ModInfo, ConstruirContribuicoes(Contribs), ConstruirMapaSecoes(),
                       ConstruirCabecalhosOpcionais())
    Info = ConstruirInfo(1, GUID_SELFTEST, [(NOME_FLUXO_NOMES, FLUXO_NOMES)])
    Payloads = [b"", Info, Tipos, Dbi, b"", ConstruirNomes([ARQUIVO_SELFTEST]), Modulo, bytes(2 * 40)]
    return ConstruirMsf(Payloads)


def ConstruirPdbComTipos(Defeito=None) -> bytes:
    """PDB com TPI de 6 tipos e 2 locais (S_REGREL32) no proc fj_alfa; o defeito cai no TPI, no modulo ou no local."""
    Locais = [(Nome, Deslocamento, 0x1099 if Defeito == "tiporegrel" and Indice == 0 else Tipo)
              for Indice, (Nome, Deslocamento, Tipo) in enumerate(LOCAIS_SELFTEST)]
    return ConstruirPdbSintetico(Defeito, ConstruirTpi(Defeito), Locais)


def ConstruirPdbVazio() -> bytes:
    """Mesmo formato do PDB do P75: DBI so com cabecalho (sem modulo, sem substreams)."""
    Info = ConstruirInfo(1, GUID_SELFTEST, [])
    return ConstruirMsf([b"", Info, b"", ConstruirDbi(b"", b"", b"", b""), b"", b"", b"", b""])


# ---------- analise do PDB inteiro ----------

def AnalisarPdb(Dados: bytes) -> dict:
    """MSF, info, /names e DBI. Erro de formato no MSF, no info ou no /names levanta ValueError."""
    Sb, Streams = LerTudo(Dados)
    Fluxos = {Stream["id"]: Stream["dados"] for Stream in Streams if not Stream["nula"]}
    if FLUXO_INFO not in Fluxos:
        raise ValueError("stream 1 (info) ausente")
    Info = LerInfo(Fluxos[FLUXO_INFO])
    Nomes = None
    if NOME_FLUXO_NOMES in Info["nomeados"]:
        Indice = Info["nomeados"][NOME_FLUXO_NOMES]
        if Indice not in Fluxos:
            raise ValueError(f"/names aponta para o stream {Indice}, que nao existe")
        Nomes = LerNomes(Fluxos[Indice])
    Dbi = AnalisarDbi(Fluxos, Nomes)
    Tipos = AnalisarTipos(Fluxos)
    ConferirTiposDosLocais(Dbi["modulos"], Tipos["tpi"], Tipos["achados"])
    return {"superbloco": Sb, "streams": Streams, "info": Info, "nomes": Nomes, "dbi": Dbi, "tipos": Tipos}


def ConferirModulo(Modulo: dict) -> list:
    Erros = []
    Procs = [(P["off"], P["nome"], P["inicio"], P["tamanho"], P["end"]) for P in Modulo["procs"]]
    Linhas = [(L["offcon"], [(X["off"], X["linha"]) for B in L["blocos"] for X in B["linhas"]])
              for L in Modulo["linhas"]]
    ProcsEsperados = [(4, "fj_alfa", 0x10, 0x20, 84), (88, "fj_beta", 0x30, 0x18, 168)]
    LinhasEsperadas = [(0x10, [(0, 10), (8, 11)]), (0x30, [(0, 20), (0x10, 21)])]
    if Procs != ProcsEsperados:
        Erros.append(f"procs: lido {Procs} esperado {ProcsEsperados}")
    if Linhas != LinhasEsperadas:
        Erros.append(f"linhas: lido {Linhas} esperado {LinhasEsperadas}")
    if [A.get("nome") for A in Modulo["arquivos"]] != [ARQUIVO_SELFTEST]:
        Erros.append(f"arquivos: lido {Modulo['arquivos']} esperado {ARQUIVO_SELFTEST}")
    if (Modulo["FluxoSimbolos"], Modulo["TamanhoSimbolos"], Modulo["TamanhoC13"]) != (FLUXO_MODULO, 172, 112):
        Erros.append(f"modulo: fluxo/tamanhos lidos {Modulo['FluxoSimbolos']}/{Modulo['TamanhoSimbolos']}/"
                     f"{Modulo['TamanhoC13']} esperado {FLUXO_MODULO}/168/112")
    return Erros


def ConferirPdbSintetico(Pdb: dict) -> list:
    Erros = []
    Dbi = Pdb["dbi"]
    if Pdb["info"]["nomeados"] != {NOME_FLUXO_NOMES: FLUXO_NOMES}:
        Erros.append(f"info: nomeados lido {Pdb['info']['nomeados']}")
    if Pdb["nomes"] is None or list(Pdb["nomes"]["nomes"].values()) != [ARQUIVO_SELFTEST]:
        Erros.append("/names: nome do arquivo nao lido")
    if len(Dbi["modulos"]) != 1:
        return Erros + [f"modulos: lido {len(Dbi['modulos'])} esperado 1"]
    Erros += ConferirModulo(Dbi["modulos"][0])
    Contribs = [(C["ISect"], C["Offset"], C["Tamanho"]) for C in Dbi["contribuicoes"]]
    if Contribs != [(1, 0x10, 0x38)]:
        Erros.append(f"SectionContribution: lido {Contribs}")
    if len(Dbi["secoes"]) != 3 or Dbi["secoes"][-1]["Flags"] != 0x208:
        Erros.append("SectionMap: 3 entradas com a absoluta no fim esperadas")
    if Dbi["cabecalhos"].get("SectionHdr") != 7:
        Erros.append(f"OptionalDbgHeader: SectionHdr lido {Dbi['cabecalhos'].get('SectionHdr')} esperado 7")
    if Dbi["achados"]:
        Erros.append(f"PDB valido com achados: {Dbi['achados']}")
    return Erros


def ConferirDbiSelftest() -> list:
    Erros = ConferirPdbSintetico(AnalisarPdb(ConstruirPdbSintetico()))
    Vazio = AnalisarPdb(ConstruirPdbVazio())["dbi"]
    if Vazio["tem_modulo"] or Vazio["achados"]:
        Erros.append(f"PDB vazio: deveria ser sem modulo e sem achado, lido {Vazio['achados']}")
    return Erros


def ConferirControles() -> list:
    """Cada defeito tem de ser acusado; um controle que passa em branco quebra a prova do leitor."""
    Erros = []
    for Nome, Defeito in CONTROLES_NEGATIVOS:
        Achados = AnalisarPdb(ConstruirPdbSintetico(Defeito))["dbi"]["achados"]
        if not Achados:
            Erros.append(f"controle negativo '{Nome}' passou sem acusar")
        else:
            print(f"controle negativo '{Nome}': acusado ({len(Achados)} achado(s)): {Achados[0]}")
    return Erros


def ConferirTiposSelftest() -> list:
    """PDB com TPI e locais sinteticos: o TPI e os locais tem de bater com o oraculo e nao pode haver achado."""
    Pdb = AnalisarPdb(ConstruirPdbComTipos())
    Erros = ConferirOraculoTipos(Pdb["tipos"], Pdb["dbi"]["modulos"])
    return Erros + [f"achado num PDB valido: {Achado}" for Achado in Pdb["tipos"]["achados"] + Pdb["dbi"]["achados"]]


def ConferirControlesTipos() -> list:
    """Cada defeito de TPI, de local ou de modulo tem de ser acusado."""
    Erros = []
    for Nome, Defeito in CONTROLES_TIPOS:
        Pdb = AnalisarPdb(ConstruirPdbComTipos(Defeito))
        Achados = Pdb["tipos"]["achados"] + Pdb["dbi"]["achados"]
        if not Achados:
            Erros.append(f"controle negativo '{Nome}' passou sem acusar")
        else:
            print(f"controle negativo '{Nome}': acusado ({len(Achados)} achado(s)): {Achados[0]}")
    return Erros


def ConferirMsfSelftest() -> list:
    Erros = []
    try:
        Sb, Streams = LerTudo(ConstruirMsf(PayloadsDoSelftest()))
        Erros += ConferirSelftest(Sb, Streams)
    except ValueError as Erro:
        Erros.append(f"leitor recusou o MSF sintetico: {Erro}")
    if not RecusaMagicErrado():
        Erros.append("magic errado foi aceito")
    return Erros


def Autoteste() -> int:
    Erros = (ConferirMsfSelftest() + ConferirDbiSelftest() + ConferirControles()
             + ConferirTiposSelftest() + ConferirControlesTipos())
    if Erros:
        for Linha in Erros:
            print(f"FALHOU {Linha}")
        return 1
    print(f"PASSOU selftest: MSF 3 streams e magic recusado; DBI 1 modulo, 2 procs, 2 linhas por proc, "
          f"/names 1 arquivo; PDB vazio sem modulo; {len(CONTROLES_NEGATIVOS)} controles negativos acusados; "
          f"TPI 6 tipos e 2 locais S_REGREL32 batem com o oraculo; {len(CONTROLES_TIPOS)} controles de tipo acusados")
    return 0


# ---------- despejo e veredito ----------

def ImprimirMsf(Sb: dict, Streams: list, Tamanho: int) -> None:
    print(f"superbloco: tamanho_bloco={Sb['TamanhoBloco']} NumBlocos={Sb['NumBlocos']} "
          f"NumBytesDiretorio={Sb['NumBytesDiretorio']} EnderecoMapa={Sb['EnderecoMapa']} "
          f"tamanho_arquivo={Tamanho} (esperado {Sb['NumBlocos'] * Sb['TamanhoBloco']})")
    print(f"streams: {len(Streams)}")
    for Stream in Streams:
        Nula = " (nula)" if Stream["nula"] else ""
        print(f"  stream {Stream['id']}: tamanho={Stream['tamanho']} blocos={Stream['blocos']}{Nula} "
              f"cabecalho={Stream['dados'][:16].hex()}")


def ImprimirInfo(Info: dict, Nomes) -> None:
    print(f"info: versao={Info['versao']} idade={Info['idade']} guid={Info['guid']} nomeados={Info['nomeados']}")
    if Nomes is not None:
        print(f"/names: strings={Nomes['tamanho_strings']} B nomes={sorted(Nomes['nomes'].values())}")


def TextoLinhas(Linha: dict) -> str:
    return ", ".join(f"{X['off']}->{X['linha']}" for B in Linha["blocos"] for X in B["linhas"])


def ImprimirModulo(Modulo: dict) -> None:
    Arquivos = [A.get("nome", "?") for A in Modulo["arquivos"]]
    print(f"modulo {Modulo['nome']} obj={Modulo['obj']} stream={Modulo['FluxoSimbolos']} procs={len(Modulo['procs'])} "
          f"arquivos={Arquivos}")
    for Proc in Modulo["procs"]:
        print(f"  proc {Proc['nome']} off={Proc['off']} inicio=0x{Proc['inicio']:X} tamanho=0x{Proc['tamanho']:X} "
              f"end={Proc['end']}")
    for Linha in Modulo["linhas"]:
        print(f"  linhas offcon=0x{Linha['offcon']:X}: {TextoLinhas(Linha)}")


def ImprimirDbi(Dbi: dict) -> None:
    if not Dbi["tem_modulo"]:
        print("DBI: sem modulo (DBI vazio ou sem ModInfo)")
    for Modulo in Dbi["modulos"]:
        ImprimirModulo(Modulo)
    print(f"DBI: SectionContribution={len(Dbi['contribuicoes'])} SectionMap={len(Dbi['secoes'])} "
          f"SectionHdr={Dbi['cabecalhos'].get('SectionHdr', '-')}")
    for Achado in Dbi["achados"]:
        print(f"DBI: {Achado}")


def ImprimirTipos(Tipos: dict, Modulos: list) -> None:
    Tpi, Ipi = Tipos["tpi"], Tipos["ipi"]
    if Tpi["vazio"]:
        print("TPI: vazio (sem registro de tipo)")
    else:
        print(f"TPI: {len(Tpi['tipos'])} registro(s) de 0x1000 a 0x{max(Tpi['tipos']):04X}")
    print("IPI: vazio" if Ipi["vazio"] else f"IPI: {Ipi['registros']} registro(s)")
    Locais = [Local for Modulo in Modulos for Local in Modulo["locais"]]
    print(f"locais: {len(Locais)} S_REGREL32" + ("" if Locais else " (vazio)"))
    for Local in Locais:
        print(f"  local {Local['nome']} proc={Local['proc']} deslocamento={Local['deslocamento']} "
              f"tipo_tpi=0x{Local['tipo_tpi']:04X} registro={Local['registro']}")
    for Achado in Tipos["achados"]:
        print(f"TIPOS: {Achado}")


def VeredictoTipos(Pdb: dict, ExigirTipos: bool, ExigirLocais: bool) -> int:
    """--exigir-tipos: TPI com registro e sem achado. --exigir-locais: ao menos um S_REGREL32."""
    if ExigirTipos:
        if Pdb["tipos"]["tpi"]["vazio"]:
            print("TPI: vazio (--exigir-tipos): o P77 ainda nao gravou os tipos")
            return 3
        if Pdb["tipos"]["achados"]:
            print(f"TPI: {len(Pdb['tipos']['achados'])} achado(s) de tipo (--exigir-tipos)")
            return 3
    if ExigirLocais and not any(Modulo["locais"] for Modulo in Pdb["dbi"]["modulos"]):
        print("locais: nenhum S_REGREL32 (--exigir-locais): o P77 ainda nao gravou os locais")
        return 3
    return 0


def VeredictoExigido(Pdb: dict, Exigir: tuple) -> int:
    ExigirModulo, ExigirTipos, ExigirLocais = Exigir
    Codigo = VeredictoModulo(Pdb["dbi"], ExigirModulo)
    return Codigo or VeredictoTipos(Pdb, ExigirTipos, ExigirLocais)


def VeredictoModulo(Dbi: dict, ExigirModulo: bool) -> int:
    if not ExigirModulo:
        return 0
    Procs = sum(len(Modulo["procs"]) for Modulo in Dbi["modulos"])
    if not Dbi["tem_modulo"]:
        print("DBI: sem modulo (--exigir-modulo): o DBI nao tem ModInfo; o P76 ainda nao gravou o modulo")
        return 3
    if Procs == 0:
        print("DBI: modulo sem proc (nenhum S_GPROC32 no stream do modulo)")
        return 3
    if Dbi["achados"]:
        print(f"DBI: {len(Dbi['achados'])} achado(s) estrutural(is) (--exigir-modulo)")
        return 3
    print(f"PASSOU DBI: {len(Dbi['modulos'])} modulo(s), {Procs} proc(s)")
    return 0


def Despejar(Caminho: Path, Exigir: tuple) -> int:
    Dados = Caminho.read_bytes()
    try:
        Pdb = AnalisarPdb(Dados)
    except ValueError as Erro:
        print(f"FALHOU {Erro}")
        return 2
    ImprimirMsf(Pdb["superbloco"], Pdb["streams"], len(Dados))
    ImprimirInfo(Pdb["info"], Pdb["nomes"])
    ImprimirDbi(Pdb["dbi"])
    ImprimirTipos(Pdb["tipos"], Pdb["dbi"]["modulos"])
    return VeredictoExigido(Pdb, Exigir)


def Principal() -> int:
    Parser = argparse.ArgumentParser(description="Despeja o MSF, info, /names e DBI de um PDB, ou roda o selftest.")
    Parser.add_argument("arquivo", nargs="?", type=Path, help="arquivo .pdb")
    Parser.add_argument("--selftest", action="store_true", help="PDB sintetico, PDB vazio e controles negativos")
    Parser.add_argument("--exigir-modulo", action="store_true",
                        help="sai 3 com 'DBI: ...' sem modulo/proc ou com achado estrutural")
    Parser.add_argument("--exigir-tipos", action="store_true",
                        help="sai 3 com 'TPI: ...' se o TPI estiver vazio ou com achado")
    Parser.add_argument("--exigir-locais", action="store_true", help="sai 3 se nao houver S_REGREL32")
    Argumentos = Parser.parse_args()
    if Argumentos.selftest:
        return Autoteste()
    if Argumentos.arquivo is None:
        Parser.error("informe o arquivo .pdb ou --selftest")
    Exigir = (Argumentos.exigir_modulo, Argumentos.exigir_tipos, Argumentos.exigir_locais)
    return Despejar(Argumentos.arquivo, Exigir)


if __name__ == "__main__":
    sys.exit(Principal())
