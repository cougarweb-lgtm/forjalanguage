# -*- coding: utf-8 -*-
"""Hash de exe/ELF da Forja sem a versao do forja (P3): zera SO os bytes de versao ancorados antes do sha256.

Ancoras (fonte: compiler\\nativo\\nativo.c, AssinarRdata):
  - texto "Forja Native " + versao + NUL no .rdata (PE e ELF);
  - PE: MajorLinkerVersion/MinorLinkerVersion (cabecalho opcional +2..+3) e MajorImageVersion/MinorImageVersion (+44..+47);
  - PE: FileVersion/ProductVersion do VS_FIXEDFILEINFO (16 B depois da assinatura BD 04 EF FE 00 00 01 00);
  - PE: valores UTF-16 de ProductVersion/FileVersion no StringFileInfo;
  - PE com --simbolos: GUID do RSDS (16 B depois de "RSDS").
Nada alem disso e zerado (.text, .data, imports, relocacoes, tamanhos, entrada e cabecalhos de secao ficam intactos).
Se a versao sobrar em texto fora dessas ancoras, levanta ErroMascaraVersao em vez de devolver hash.

Uso:
    python -I mascara_versao.py --selftest
Versao: 1.0.2
Changelog: 1.0.2 (18/11/2026) J1 - VERSOES_FORJA ganha 0.20.4; 1.0.1 (18/11/2026) P3 - VERSOES_FORJA ganha 0.20.3 (fallback do `forja --versao` sem forja); 1.0.0 (18/11/2026) P3 - primeira versao: HashSemVersao/BytesSemVersao (ancoras PE e ELF), VersoesDoForja (versao de `forja --versao`; VERSOES_FORJA se o forja nao roda), selftest.
"""

import argparse
import functools
import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

VERSAO = "1.0.2"
FORJA_PADRAO = "C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/compiler/forja.exe"
PASTA_BENCH = Path(__file__).resolve().parents[1] / "bench"
VERSOES_FORJA = ("0.19.8", "0.20.0", "0.20.1", "0.20.2", "0.20.3", "0.20.4")  # fallback so se o forja nao roda; com forja, a versao vem de `forja --versao`
TIMEOUT_VERSAO = 30
TIMEOUT_BUILD = 120
PREFIXO_NATIVO = b"Forja Native "
ASSINATURA_FIXEDFILEINFO = b"\xbd\x04\xef\xfe\x00\x00\x01\x00"
CHAVES_VERSAO_UTF16 = ("ProductVersion", "FileVersion")
RE_VERSAO_DO_FORJA = re.compile(r"^forja (\d+\.\d+\.\d+)", re.M)


class ErroMascaraVersao(Exception):
    pass


@functools.lru_cache(maxsize=None)
def VersaoDoForjaUnica(Forja: str) -> str | None:
    """Versao de `forja --versao` ("forja 0.20.2 (Linguagem v1.12)" -> "0.20.2"); None se o forja nao roda ou a saida nao bate."""
    try:
        Processo = subprocess.run([Forja, "--versao"], capture_output=True, text=True, timeout=TIMEOUT_VERSAO, stdin=subprocess.DEVNULL)
    except (OSError, subprocess.SubprocessError):
        return None  # forja ausente ou quebrado: o chamador cai no fallback VERSOES_FORJA
    Casou = RE_VERSAO_DO_FORJA.search(Processo.stdout)
    return Casou.group(1) if Processo.returncode == 0 and Casou else None


def VersoesDoForja(*Forjas) -> tuple:
    """Versoes a neutralizar para os exes destes forjas (None ignora): a de `forja --versao` de cada um; VERSOES_FORJA se nenhum responde."""
    Achadas = {VersaoDoForjaUnica(str(Forja)) for Forja in Forjas if Forja is not None}
    Achadas.discard(None)
    return tuple(sorted(Achadas)) or VERSOES_FORJA


def NormalizarVersoes(Versao) -> tuple:
    """str -> (str,); None -> VERSOES_FORJA; sequencia -> tupla."""
    if Versao is None:
        return VERSOES_FORJA
    return (Versao,) if isinstance(Versao, str) else tuple(Versao)


def LerOrigem(Origem) -> bytes:
    """Origem e o caminho do exe/ELF ou os proprios bytes."""
    return bytes(Origem) if isinstance(Origem, (bytes, bytearray)) else Path(Origem).read_bytes()


def IntervalosAscii(Dados: bytearray, Versao: str) -> list:
    """(inicio, fim) da versao dentro de 'Forja Native <versao>' + NUL (o NUL fecha a ancora)."""
    Forma = PREFIXO_NATIVO + Versao.encode("ascii") + b"\0"
    Intervalos, Pos = [], Dados.find(Forma)
    while Pos >= 0:
        Inicio = Pos + len(PREFIXO_NATIVO)
        Intervalos.append((Inicio, Inicio + len(Versao)))
        Pos = Dados.find(Forma, Pos + len(Forma))
    return Intervalos


def IntervalosUtf16(Dados: bytearray, Versao: str) -> list:
    """(inicio, fim) do valor UTF-16 da versao logo depois da chave ProductVersion/FileVersion do StringFileInfo (pula o padding ate o DWORD)."""
    Valor = Versao.encode("utf-16-le")
    Intervalos = []
    for Chave in CHAVES_VERSAO_UTF16:
        Forma = Chave.encode("utf-16-le") + b"\0\0"
        Pos = Dados.find(Forma)
        while Pos >= 0:
            Inicio = Pos + len(Forma)
            while Dados[Inicio:Inicio + 2] == b"\0\0":  # padding de alinhamento antes do valor
                Inicio += 2
            if Dados[Inicio:Inicio + len(Valor)] == Valor:
                Intervalos.append((Inicio, Inicio + len(Valor)))
            Pos = Dados.find(Forma, Pos + len(Forma))
    return Intervalos


def PontosDeTextoDaVersao(Dados: bytearray, Versoes: tuple) -> list:
    """Intervalos dos bytes de versao ancorados em texto (ASCII do .rdata e UTF-16 do StringFileInfo), para cada versao."""
    Intervalos = []
    for Versao in Versoes:
        Intervalos += IntervalosAscii(Dados, Versao) + IntervalosUtf16(Dados, Versao)
    return Intervalos


def PontosFixosDoPe(Dados: bytearray) -> list:
    """Campos PE que o forja preenche com a versao, sem texto: linker, ImageVersion, VS_FIXEDFILEINFO e GUID do RSDS. Vazio se nao e PE (o ELF nao tem nada disso)."""
    if Dados[:2] != b"MZ":
        return []
    Pe = int.from_bytes(Dados[0x3C:0x40], "little")
    if Pe + 72 > len(Dados):  # ImageVersion vai ate Pe+72; sem isso o corte sairia calado
        raise ErroMascaraVersao(f"PE truncado: e_lfanew {Pe} nao cabe em {len(Dados)} bytes")
    Intervalos = [(Pe + 26, Pe + 28), (Pe + 68, Pe + 72)]  # Pe+24 e o cabecalho opcional: +2 linker, +44 ImageVersion
    Assinatura = Dados.find(ASSINATURA_FIXEDFILEINFO)
    if Assinatura >= 0:
        Intervalos.append((Assinatura + 8, Assinatura + 24))
    Rsds = Dados.find(b"RSDS")
    if Rsds >= 0:
        Intervalos.append((Rsds + 4, Rsds + 20))
    return Intervalos


def ConferirSemVersaoSobrando(Dados: bytearray, Versoes: tuple) -> None:
    """Falha alto se a versao ainda aparece em texto (ASCII ou UTF-16) depois de zerar as ancoras: nao se gera hash mascarando texto que nao e versao."""
    for Versao in Versoes:
        for Forma in (Versao.encode("ascii"), Versao.encode("utf-16-le")):
            Pos = Dados.find(Forma)
            if Pos >= 0:
                raise ErroMascaraVersao(f"versao {Versao} aparece no offset {Pos} fora das ancoras (Forja Native / StringFileInfo / PE); nao gero hash")


def BytesSemVersao(Origem, Versao=None) -> bytes:
    """Bytes de Origem (caminho ou bytes) com as ancoras de versao zeradas. Versao: str, sequencia de str ou None (VERSOES_FORJA). Levanta ErroMascaraVersao se sobrar versao fora das ancoras."""
    Dados = bytearray(LerOrigem(Origem))
    Versoes = NormalizarVersoes(Versao)
    for Inicio, Fim in PontosDeTextoDaVersao(Dados, Versoes) + PontosFixosDoPe(Dados):
        Dados[Inicio:Fim] = b"\0" * (Fim - Inicio)
    ConferirSemVersaoSobrando(Dados, Versoes)
    return bytes(Dados)


def HashSemVersao(Origem, Versao=None) -> str:
    """sha256 de BytesSemVersao(Origem, Versao): o hash do exe/ELF nao muda quando so muda a versao do forja que o gerou."""
    return hashlib.sha256(BytesSemVersao(Origem, Versao)).hexdigest()


def Afirmar(Condicao: bool, Mensagem: str) -> None:
    if not Condicao:
        raise AssertionError(Mensagem)


def ConstruirExeDeTeste(Forja: str, Saida: Path) -> None:
    """Build release x64 do bench pequeno com o forja; Saida e caminho novo."""
    Processo = subprocess.run([Forja, "build", "--release", "--backend", "x64", str(PASTA_BENCH / "vec4_local.fj"), "-o", str(Saida)], capture_output=True, timeout=TIMEOUT_BUILD, stdin=subprocess.DEVNULL)
    Afirmar(Processo.returncode == 0 and Saida.is_file(), f"build do forja falhou (exit {Processo.returncode}): {Processo.stderr[-300:]!r}")


def NovaVersao(Versao: str) -> str:
    """Versao com o ultimo digito +1 (mod 10): mesmo tamanho, para simular o bump no selftest."""
    Partes = Versao.split(".")
    Partes[-1] = str((int(Partes[-1]) + 1) % 10)
    return ".".join(Partes)


def SimularBumpDeVersao(Dados: bytes, De: str, Para: str) -> bytes:
    """Copia com a versao trocada como um bump faria (mesmo tamanho): texto + NUL, UTF-16, linker, ImageVersion e VS_FIXEDFILEINFO. Offsets proprios do teste."""
    Nova = Dados.replace(De.encode("ascii") + b"\0", Para.encode("ascii") + b"\0").replace(De.encode("utf-16-le"), Para.encode("utf-16-le"))
    Maior, Menor, Correcao = (int(P) for P in Para.split("."))
    Mutavel = bytearray(Nova)
    if Nova[:2] == b"MZ":
        Pe = int.from_bytes(Nova[0x3C:0x40], "little")
        Mutavel[Pe + 26:Pe + 28] = bytes([Maior, Menor])  # MajorLinkerVersion, MinorLinkerVersion
        Mutavel[Pe + 68:Pe + 72] = Maior.to_bytes(2, "little") + Menor.to_bytes(2, "little")  # MajorImageVersion, MinorImageVersion
    Assinatura = Nova.find(ASSINATURA_FIXEDFILEINFO)
    if Assinatura >= 0:
        Campos = ((Maior << 16) | Menor).to_bytes(4, "little") + (Correcao << 16).to_bytes(4, "little")
        Mutavel[Assinatura + 8:Assinatura + 16] = Campos  # FileVersion
        Mutavel[Assinatura + 16:Assinatura + 24] = Campos  # ProductVersion
    return bytes(Mutavel)


def PosicaoNoTexto(Dados: bytes) -> int:
    """Offset no meio da secao .text (cabecalhos PE lidos aqui, sem o modulo)."""
    Pe = int.from_bytes(Dados[0x3C:0x40], "little")
    Secoes = Pe + 24 + int.from_bytes(Dados[Pe + 20:Pe + 22], "little")
    for K in range(int.from_bytes(Dados[Pe + 6:Pe + 8], "little")):
        Cabecalho = Secoes + 40 * K
        if Dados[Cabecalho:Cabecalho + 8].rstrip(b"\0") == b".text":
            Tamanho = int.from_bytes(Dados[Cabecalho + 16:Cabecalho + 20], "little")
            return int.from_bytes(Dados[Cabecalho + 20:Cabecalho + 24], "little") + Tamanho // 2
    raise AssertionError("secao .text nao achada no PE")


def TesteVersoesDoForja(Forja: str) -> None:
    Versao = VersaoDoForjaUnica(Forja)
    Afirmar(Versao is not None and re.fullmatch(r"\d+\.\d+\.\d+", Versao) is not None, f"forja --versao nao respondeu versao: {Versao!r}")
    Afirmar(VersoesDoForja(Forja) == (Versao,), "a versao do forja deveria ser a unica neutralizada")
    Afirmar(VersoesDoForja("C:/nao_existe/forja.exe") == VERSOES_FORJA, "forja inexistente deveria cair no fallback VERSOES_FORJA")


def TesteTrocaDeVersao(Dados: bytes, Versao: str) -> None:
    """Bump simulado so nas ancoras (texto, UTF-16, linker, ImageVersion, VS_FIXEDFILEINFO): hash igual."""
    Para = NovaVersao(Versao)
    Afirmar(PREFIXO_NATIVO + Versao.encode("ascii") + b"\0" in Dados, "o exe de teste nao tem a ancora 'Forja Native <versao>'")
    Copia = SimularBumpDeVersao(Dados, Versao, Para)
    Afirmar(Copia != Dados, "o bump simulado nao mudou nenhum byte")
    Afirmar(HashSemVersao(Dados, Versao) == HashSemVersao(Copia, Para), "trocar so a versao nas ancoras deveria manter o hash")
    Afirmar(HashSemVersao(Dados, (Versao, Para)) == HashSemVersao(Copia, (Versao, Para)), "com as duas versoes na lista tambem deveria manter")


def TesteByteDoTextoMuda(Dados: bytes, Versao: str) -> None:
    Mudado = bytearray(Dados)
    Mudado[PosicaoNoTexto(Dados)] ^= 0x01
    Afirmar(HashSemVersao(bytes(Mudado), Versao) != HashSemVersao(Dados, Versao), "1 byte do .text mudado deveria mudar o hash")


def TesteVersaoForaDasAncoras(Dados: bytes, Versao: str) -> None:
    Solta = Dados + b"\0" + Versao.encode("ascii") + b"\0"  # versao em texto solta no fim do arquivo
    try:
        HashSemVersao(Solta, Versao)
    except ErroMascaraVersao:
        return
    raise AssertionError("versao fora das ancoras deveria levantar ErroMascaraVersao")


def TesteElf(Versao: str) -> None:
    """ELF sintetico com a ancora 'Forja Native <versao>' + NUL: troca de versao mantem o hash; 1 byte muda o hash; nada de PE e tocado."""
    Elf = b"\x7fELF" + bytes(60) + PREFIXO_NATIVO + Versao.encode("ascii") + b"\0" + bytes(range(256))
    Para = NovaVersao(Versao)
    Afirmar(HashSemVersao(Elf, Versao) == HashSemVersao(SimularBumpDeVersao(Elf, Versao, Para), Para), "ELF com a versao trocada deveria manter o hash")
    Mudado = bytearray(Elf)
    Mudado[-1] ^= 0x01
    Afirmar(HashSemVersao(bytes(Mudado), Versao) != HashSemVersao(Elf, Versao), "ELF com 1 byte mudado deveria mudar o hash")


def Selftest() -> bool:
    with tempfile.TemporaryDirectory(prefix="mascara_versao_st_") as Pasta:
        Exe = Path(Pasta) / "alvo.exe"
        try:
            ConstruirExeDeTeste(FORJA_PADRAO, Exe)
        except AssertionError as Erro:
            print(f"FALHOU ConstruirExe: {Erro}")
            return False
        Dados = Exe.read_bytes()
    Versao = VersaoDoForjaUnica(FORJA_PADRAO)
    if Versao is None:
        print(f"FALHOU VersaoDoForja: {FORJA_PADRAO} --versao nao respondeu")
        return False
    Casos = [("VersaoDoForja", lambda: TesteVersoesDoForja(FORJA_PADRAO)),
             ("TrocaDeVersaoNasAncoras", lambda: TesteTrocaDeVersao(Dados, Versao)),
             ("ByteDoTextoMuda", lambda: TesteByteDoTextoMuda(Dados, Versao)),
             ("VersaoForaDasAncoras", lambda: TesteVersaoForaDasAncoras(Dados, Versao)),
             ("ElfSintetico", lambda: TesteElf(Versao))]
    Falhas = 0
    for Nome, Teste in Casos:
        try:
            Teste()
        except Exception as Erro:
            print(f"FALHOU {Nome}: {Erro}")
            Falhas += 1
            continue
        print(f"OK {Nome}")
    print(f"selftest mascara_versao {VERSAO}: {len(Casos) - Falhas}/{len(Casos)}")
    return Falhas == 0


def Main() -> int:
    Analisador = argparse.ArgumentParser(description="Hash de exe/ELF da Forja sem os bytes de versao (P3).")
    Analisador.add_argument("--selftest", action="store_true")
    Args = Analisador.parse_args()
    if Args.selftest:
        return 0 if Selftest() else 1
    Analisador.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(Main())
