# -*- coding: utf-8 -*-
"""Verifica o exe PE32+ do M1 (P3): roda com stdout "ola\\r\\n" e exit 7, objdump -p limpo, imports so de KERNEL32,
sha256 igual em duas gravacoes, tamanho ate 3 KB e a autoconferencia recusando imagem corrompida (X0905).

Regras:
  - o exe e montado a mao por teste_nativo.exe pe <saida>; nao depende do forja.exe.
  - o 6o item grava de proposito uma imagem com a entrada fora do .text: tem de sair com X0905 e nao gravar arquivo.
  - Defender ou outro bloqueio do exe (ilegivel ou inexecutavel) aparece como PULADO e nao reprova; nao se insiste.
    O Defender passou a marcar o exe de 1,5 KB como Trojan:Win32/Bearfoos.A!ml (ML generico, 08/11/2026).

Changelog: 0.6.3 - M8 H1: ExportsEsperados leva forja_dll_info (toda DLL Forja a exporta).
           0.6.2 - 0.21.0 F3: item 12 deixa de gerar a DLL pelo gcc (o backend C saiu; sem --backend a Forja ja cai no x64, a comparacao era x64 contra x64); compara o x64 com o export directory congelado do C (ExportsEsperados, capturado uma vez com --backend c).
           0.6.1 - M6b P75: GerarOla passa --simbolos nao (o exe em dev leva o caminho absoluto do .pdb no RSDS e as duas geracoes tem nomes diferentes; a igualdade
                   byte a byte COM .pdb e do verificar_pdb, no mesmo caminho).
           0.6.0 - M3 P22a: itens 12 (DLL do --backend x64: nomes e ordinais do export directory iguais aos da DLL do gcc, ordinal base 1) e
                   13 (DLL x64 com Characteristics 0x2026, ImageBase 0x180000000, .reloc e .edata presentes, objdump -p limpo); TotalItens 13.
           0.5.0 - M3 P19a: item 11 (exe de unidade com .idata de 2 DLLs, teste_nativo.exe pe2: objdump -p lista KERNEL32.dll e ucrtbase.dll na ordem, sem aviso, e o exe roda);
                   TotalItens 11.
           0.4.0 - M2 P9a fase 2: item 9 (exe sem ADDR64 tem .reloc com um bloco ABSOLUTE de 12 B na pagina do .text, objdump -p limpo) e
                   item 10 (teste_nativo.exe passa com o caso da secao vazia -> X0905); TotalItens 10.
           0.3.0 - portao M1: item 8 (criterio 8): os bytes do exe real do 01_ola nao contem "GCC:", "GNU C", "mingw" nem "GNU ld" (maiusculas ou
                   minusculas); TotalItens 8. O rt_win.o tambem esta limpo (conferido a mao).
           0.2.0 - P7b do M1: item 7 (identificacao honesta do exe real do 01_ola gerado por forja build --backend x64): string "Forja Native <versao>",
                   MajorLinkerVersion/MinorLinkerVersion = versao do forja, VERSIONINFO (ProductName "Forja Native", FileVersion, FileDescription) lido
                   pelo proprio Windows (version.dll), manifesto asInvoker + Windows 10/11, sha256 igual em duas geracoes. Opcao --forja.
           0.1.1 - item barrado pelo antivirus vira PULADO em vez de FALHOU (exe de 1,5 KB e falso-positivo de ML).
           0.1.0 - backend x64 P3: primeira versao (6 itens do aceite do M1).
Verificacao: python -I ..\\tests\\nativo\\pe\\verificar_pe.py [--teste <teste_nativo.exe>] [--objdump <objdump>] [--forja <forja.exe>]
             (exige construir.bat antes: teste_nativo.exe e forja.exe na pasta compiler)
"""
import argparse
import ctypes
import hashlib
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
RaizLinguagem = PastaAqui.parent.parent.parent
TesteNativoPadrao = RaizLinguagem / "compiler" / "teste_nativo.exe"

MensagemEsperada = b"ola\r\n"
CodigoEsperado = 7
DllEsperada = "KERNEL32.dll"
ImportsEsperados = {"GetStdHandle", "WriteFile", "ExitProcess"}
TamanhoMaximoExe = 3 * 1024
CodigoX0905 = b"X0905"
CodigoSaidaRecusa = 1
TotalItens = 13
DllsDoPe2 = ["KERNEL32.dll", "ucrtbase.dll"]
FonteBiblioteca = RaizLinguagem / "tests" / "cli" / "biblioteca.fj"
CaracteristicasDll = 0x2026
ImageBaseDll = 0x180000000
# export directory da DLL do C (gcc) de biblioteca.fj, congelado na 0.21.0 (capturado uma vez com --backend c; igual ao do x64): (ordinal, hint, nome)
# M8 H1: toda DLL Forja ganha forja_dll_info (entre fatorial e somar na ordem ASCII); os de biblioteca.fj seguem os do C.
ExportsEsperados = [(1, 0, "fatorial"), (2, 1, "forja_dll_info"), (3, 2, "somar")]
PadraoExportNome = re.compile(r"\[\s*\d+\]\s*\+base\[\s*(\d+)\]\s+(\d+)\s+(\S+)")
RotulosAlheios = (b"gcc:", b"gnu c", b"mingw", b"gnu ld")  # comparados em minusculas
Tempo = 30
ForjaPadrao = RaizLinguagem / "compiler" / "forja.exe"
ExemploOla = RaizLinguagem.parent / "examples" / "01_ola.fj"
ForjaExe = ForjaPadrao
ChavesVersao = {
    "ProductName": "Forja Native",
    "FileDescription": "Executavel gerado pelo backend nativo da Forja",
}
GuidWindows10 = b"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"
OffsetLinkerNoPe = 4 + 20 + 2  # assinatura PE + cabecalho COFF + Magic: Major e Minor do linker vem em seguida
PadraoImport = re.compile(r"\s*[0-9a-f]{8}\s+<none>\s+[0-9a-f]+\s+(\S+)")


def Rodar(Argumentos):
    return subprocess.run(Argumentos, capture_output=True, timeout=Tempo)


def TextoDe(Bruto):
    return Bruto.decode("utf-8", errors="replace")


class ExeBloqueado(Exception):
    """O antivirus local barrou o exe de 1,5 KB: falso-positivo de ML, o item nao tem como rodar."""


def ConfirmarLegivel(Caminho):
    try:
        Caminho.read_bytes()
    except OSError as Erro:
        raise ExeBloqueado(f"{Caminho.name} ilegivel logo apos gravar (Defender?): {Erro}") from Erro


def GravarPeComModo(TesteNativo, Modo, Caminho):
    Rodada = Rodar([str(TesteNativo), Modo, str(Caminho)])
    if Rodada.returncode != 0 or not Caminho.exists():
        return None, f"teste_nativo {Modo} saiu {Rodada.returncode}: {TextoDe(Rodada.stderr)[:200]!r}"
    ConfirmarLegivel(Caminho)
    return Caminho, ""


def GravarPe(TesteNativo, Caminho):
    return GravarPeComModo(TesteNativo, "pe", Caminho)


def ItemExeRodaComSaida(TesteNativo, Objdump, Pasta):
    Exe, Motivo = GravarPe(TesteNativo, Pasta / "a.exe")
    if Exe is None:
        return False, Motivo
    try:
        Execucao = Rodar([str(Exe)])
    except OSError as Erro:
        raise ExeBloqueado(f"nao consegui executar o exe (Defender ou bloqueio?): {Erro}") from Erro
    if Execucao.stdout != MensagemEsperada:
        return False, f"stdout {Execucao.stdout!r} diferente de {MensagemEsperada!r}"
    if Execucao.returncode != CodigoEsperado:
        return False, f"exit {Execucao.returncode}, esperado {CodigoEsperado}"
    return True, "stdout = ola\\r\\n e exit 7"


def ItemObjdumpSemAviso(TesteNativo, Objdump, Pasta):
    Exe, Motivo = GravarPe(TesteNativo, Pasta / "a.exe")
    if Exe is None:
        return False, Motivo
    Rodada = Rodar([Objdump, "-p", str(Exe)])
    Saida = TextoDe(Rodada.stdout) + TextoDe(Rodada.stderr)
    if Rodada.returncode != 0:
        return False, f"objdump -p saiu {Rodada.returncode}"
    if "warning" in Saida.lower():
        return False, "objdump -p imprimiu aviso (warning)"
    return True, "objdump -p exit 0 e sem warning"


def ImportsDoTexto(Texto):
    Dlls = {}
    Atual = None
    for Linha in Texto.splitlines():
        if "DLL Name:" in Linha:
            Atual = Linha.split("DLL Name:", 1)[1].strip()
            Dlls[Atual] = []
            continue
        Casou = PadraoImport.match(Linha) if Atual is not None else None
        if Casou:
            Dlls[Atual].append(Casou.group(1))
    return Dlls


def ItemImports(TesteNativo, Objdump, Pasta):
    Exe, Motivo = GravarPe(TesteNativo, Pasta / "a.exe")
    if Exe is None:
        return False, Motivo
    Rodada = Rodar([Objdump, "-p", str(Exe)])
    Dlls = ImportsDoTexto(TextoDe(Rodada.stdout))
    if list(Dlls) != [DllEsperada]:
        return False, f"DLLs importadas {list(Dlls)}, esperado [{DllEsperada}]"
    if set(Dlls[DllEsperada]) != ImportsEsperados or len(Dlls[DllEsperada]) != len(ImportsEsperados):
        return False, f"funcoes {Dlls[DllEsperada]}, esperado {sorted(ImportsEsperados)}"
    return True, f"{DllEsperada} = {', '.join(sorted(ImportsEsperados))}"


def ItemDuasDlls(TesteNativo, Objdump, Pasta):
    """P19a do M3: o .idata de 2 DLLs (teste_nativo.exe pe2): objdump -p lista KERNEL32.dll e ucrtbase.dll NESSA ordem, sem aviso, e o exe ainda roda."""
    Exe, Motivo = GravarPeComModo(TesteNativo, "pe2", Pasta / "duas.exe")
    if Exe is None:
        return False, Motivo
    Rodada = Rodar([Objdump, "-p", str(Exe)])
    Saida = TextoDe(Rodada.stdout) + TextoDe(Rodada.stderr)
    if Rodada.returncode != 0 or "warning" in Saida.lower():
        return False, f"objdump -p saiu {Rodada.returncode} ou imprimiu aviso"
    Dlls = ImportsDoTexto(TextoDe(Rodada.stdout))
    if list(Dlls) != DllsDoPe2:
        return False, f"DLLs importadas {list(Dlls)}, esperado {DllsDoPe2} (nessa ordem)"
    if sorted(Dlls["ucrtbase.dll"]) != ["abs", "strlen"] or set(Dlls[DllEsperada]) != ImportsEsperados:
        return False, f"funcoes {Dlls}, esperado KERNEL32 {sorted(ImportsEsperados)} e ucrtbase abs, strlen"
    try:
        Execucao = Rodar([str(Exe)])
    except OSError as Erro:
        raise ExeBloqueado(f"nao consegui executar o exe de 2 DLLs (Defender ou bloqueio?): {Erro}") from Erro
    if Execucao.stdout != MensagemEsperada or Execucao.returncode != CodigoEsperado:
        return False, f"o exe de 2 DLLs rodou com stdout {Execucao.stdout!r} e exit {Execucao.returncode}"
    return True, "objdump -p: KERNEL32.dll e ucrtbase.dll na ordem, sem aviso; o exe roda (ola, exit 7)"


def ItemSha256Igual(TesteNativo, Objdump, Pasta):
    Primeiro, Motivo = GravarPe(TesteNativo, Pasta / "b1.exe")
    if Primeiro is None:
        return False, Motivo
    Segundo, Motivo = GravarPe(TesteNativo, Pasta / "b2.exe")
    if Segundo is None:
        return False, Motivo
    HashUm = hashlib.sha256(Primeiro.read_bytes()).hexdigest()
    HashDois = hashlib.sha256(Segundo.read_bytes()).hexdigest()
    if HashUm != HashDois:
        return False, f"sha256 diferente: {HashUm} x {HashDois}"
    return True, f"sha256 {HashUm[:16]}... igual nas duas gravacoes"


def ItemTamanhoAte3Kb(TesteNativo, Objdump, Pasta):
    Exe, Motivo = GravarPe(TesteNativo, Pasta / "a.exe")
    if Exe is None:
        return False, Motivo
    Tamanho = Exe.stat().st_size
    if Tamanho > TamanhoMaximoExe:
        return False, f"exe com {Tamanho} bytes, limite {TamanhoMaximoExe}"
    return True, f"exe com {Tamanho} bytes (limite {TamanhoMaximoExe})"


def ItemAutoconferenciaRecusa(TesteNativo, Objdump, Pasta):
    Corrompido = Pasta / "corrompido.exe"
    Rodada = Rodar([str(TesteNativo), "corrompido", str(Corrompido)])
    if Rodada.returncode != CodigoSaidaRecusa:
        return False, f"saida {Rodada.returncode}, esperado {CodigoSaidaRecusa}"
    if CodigoX0905 not in Rodada.stderr:
        return False, "stderr sem X0905"
    if Corrompido.exists():
        return False, "a imagem corrompida foi gravada"
    return True, "imagem com entrada fora do .text recusada com X0905, sem arquivo"


def VersaoDoForja():
    Saida = TextoDe(Rodar([str(ForjaExe), "--versao"]).stdout)
    Achado = re.match(r"forja (\d+)\.(\d+)\.(\d+)", Saida)
    if not Achado:
        raise OSError(f"forja --versao com saida inesperada: {Saida!r}")
    return Saida.split()[1], int(Achado.group(1)), int(Achado.group(2))


def LerVersionInfo(Caminho, Chave):
    """Valor de StringFileInfo\\040904B0\\<Chave> lido pelo version.dll do Windows (o mesmo que o Explorer usa)."""
    Dll = ctypes.WinDLL("version", use_last_error=True)
    Dll.GetFileVersionInfoSizeW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p]
    Dll.GetFileVersionInfoW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    Dll.VerQueryValueW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_void_p),
                                   ctypes.POINTER(ctypes.c_uint32)]
    Tamanho = Dll.GetFileVersionInfoSizeW(str(Caminho), None)
    if Tamanho == 0:
        return None
    Bloco = ctypes.create_string_buffer(Tamanho)
    Ponteiro = ctypes.c_void_p()
    Comprimento = ctypes.c_uint32()
    if not Dll.GetFileVersionInfoW(str(Caminho), 0, Tamanho, Bloco):
        return None
    if not Dll.VerQueryValueW(Bloco, f"\\StringFileInfo\\040904B0\\{Chave}", ctypes.byref(Ponteiro), ctypes.byref(Comprimento)):
        return None
    return ctypes.wstring_at(Ponteiro.value)


def GerarOla(Pasta, Nome):
    Exe = Pasta / Nome
    Rodada = Rodar([str(ForjaExe), "build", "--backend", "x64", "--simbolos", "nao", str(ExemploOla), "-o", str(Exe)])
    if Rodada.returncode != 0 or not Exe.exists():
        return None, f"forja build --backend x64 saiu {Rodada.returncode}: {TextoDe(Rodada.stderr + Rodada.stdout)[:200]!r}"
    ConfirmarLegivel(Exe)
    return Exe, ""


def ConferirCabecalhoEString(Bytes, Versao, Maior, Menor):
    if f"Forja Native {Versao}\0".encode("ascii") not in Bytes:
        return f"string 'Forja Native {Versao}' ausente do exe"
    Posicao = struct.unpack_from("<I", Bytes, 0x3C)[0] + OffsetLinkerNoPe
    if (Bytes[Posicao], Bytes[Posicao + 1]) != (Maior, Menor):
        return f"MajorLinkerVersion/MinorLinkerVersion {Bytes[Posicao]}.{Bytes[Posicao + 1]}, esperado {Maior}.{Menor}"
    if b"asInvoker" not in Bytes or GuidWindows10 not in Bytes:
        return "manifesto sem asInvoker ou sem o GUID de compatibilidade do Windows 10/11"
    return None


def ConferirVersionInfo(Exe, Versao):
    Esperado = dict(ChavesVersao, FileVersion=Versao)
    for Chave, Valor in Esperado.items():
        Lido = LerVersionInfo(Exe, Chave)
        if Lido != Valor:
            return f"VERSIONINFO {Chave} = {Lido!r}, esperado {Valor!r}"
    return None


def ItemExeRealIdentificado(TesteNativo, Objdump, Pasta):
    Versao, Maior, Menor = VersaoDoForja()
    Primeiro, Motivo = GerarOla(Pasta, "ola_a.exe")
    if Primeiro is None:
        return False, Motivo
    Segundo, Motivo = GerarOla(Pasta, "ola_b.exe")
    if Segundo is None:
        return False, Motivo
    Bytes = Primeiro.read_bytes()
    if hashlib.sha256(Bytes).digest() != hashlib.sha256(Segundo.read_bytes()).digest():
        return False, "sha256 do 01_ola diferente em duas geracoes"
    Problema = ConferirCabecalhoEString(Bytes, Versao, Maior, Menor) or ConferirVersionInfo(Primeiro, Versao)
    if Problema:
        return False, Problema
    return True, f"01_ola x64 ({len(Bytes)} bytes): 'Forja Native {Versao}', linker {Maior}.{Menor}, VERSIONINFO e manifesto lidos, sha256 igual"


def ItemSemRotuloDeCompiladorAlheio(TesteNativo, Objdump, Pasta):
    """Criterio 8 do M1: o exe real do 01_ola nao carrega assinatura de gcc/ld (nem do runtime rt_win.o): o exe e da Forja."""
    Exe, Motivo = GerarOla(Pasta, "ola_rotulos.exe")
    if Exe is None:
        return False, Motivo
    Bytes = Exe.read_bytes().lower()
    Achados = [Rotulo.decode("ascii") for Rotulo in RotulosAlheios if Rotulo in Bytes]
    if Achados:
        return False, f"o exe do 01_ola carrega rotulo de compilador alheio: {Achados}"
    return True, f"01_ola x64 ({len(Bytes)} bytes) sem {', '.join(repr(R.decode('ascii')) for R in RotulosAlheios)} (maiusculas ou minusculas)"


def LerSecoesPe(Dados):
    """Tabela de secoes do PE lida direto do arquivo: nome -> (RVA, SizeOfRawData, PointerToRawData)."""
    Lfanew = struct.unpack_from("<I", Dados, 0x3C)[0]
    Qtd = struct.unpack_from("<H", Dados, Lfanew + 6)[0]
    TamOpcional = struct.unpack_from("<H", Dados, Lfanew + 20)[0]
    Secoes = {}
    for Indice in range(Qtd):
        Off = Lfanew + 24 + TamOpcional + Indice * 40
        Nome = Dados[Off:Off + 8].rstrip(b"\0").decode("ascii", errors="replace")
        Secoes[Nome] = struct.unpack_from("<III", Dados, Off + 12)
    return Secoes


def ItemRelocSemAddr64(TesteNativo, Objdump, Pasta):
    """Programa sem ADDR64 (o exe de teste do pe): o .reloc existe, com um bloco na pagina do .text (12 B, uma entrada ABSOLUTE) e objdump -p limpo."""
    Exe, Motivo = GravarPe(TesteNativo, Pasta / "reloc.exe")
    if Exe is None:
        return False, Motivo
    Dados = Exe.read_bytes()
    Secoes = LerSecoesPe(Dados)
    if ".reloc" not in Secoes or ".text" not in Secoes:
        return False, f"secoes {sorted(Secoes)} sem .reloc"
    Rva, Tam, Ptr = Secoes[".reloc"]
    Pagina, TamBloco, Entrada = struct.unpack_from("<IIH", Dados, Ptr)
    if Pagina != Secoes[".text"][0] or TamBloco != 12 or Entrada != 0:
        return False, f".reloc com pagina {Pagina:#x}, bloco {TamBloco} B, entrada {Entrada:#x}; esperado pagina do .text, 12 B, 0"
    Rodada = Rodar([Objdump, "-p", str(Exe)])
    Saida = TextoDe(Rodada.stdout) + TextoDe(Rodada.stderr)
    if Rodada.returncode != 0 or "warning" in Saida.lower():
        return False, f"objdump -p saiu {Rodada.returncode} ou imprimiu aviso"
    return True, f".reloc RVA {Rva:#x} (bruto {Tam} B): bloco da pagina {Pagina:#x} com 1 entrada ABSOLUTE; objdump -p limpo"


def ItemSecaoVaziaRecusada(TesteNativo, Objdump, Pasta):
    """Caso de unidade (teste_nativo.c, TestaSecoesImagem): ImagemSecao recusa secao sem bytes com X0905 "secao '.vazia' sem bytes".
    O caso so existe na unidade (nenhum programa gera secao vazia): aqui so se confirma que o teste_nativo.exe passa inteiro."""
    Rodada = Rodar([str(TesteNativo)])
    Saida = TextoDe(Rodada.stdout)
    if Rodada.returncode != 0 or "OK:" not in Saida:
        return False, f"teste_nativo.exe saiu {Rodada.returncode}: {(Saida + TextoDe(Rodada.stderr)).strip()[-200:]!r}"
    return True, "teste_nativo.exe passa (inclui secao vazia recusada com X0905 'secao .vazia. sem bytes')"


def GerarBiblioteca(Pasta, Nome, ComX64):
    """DLL do tests/cli/biblioteca.fj pelo forja: ComX64 = --backend x64 explicito (sem gcc); sem a flag a Forja cai no x64 (padrao desde a 0.21.0; o caminho C/gcc saiu)."""
    Dll = Pasta / Nome
    Argumentos = [str(ForjaExe), "build", "--modo", "dll"] + (["--backend", "x64"] if ComX64 else []) + [str(FonteBiblioteca), "-o", str(Dll)]
    Rodada = Rodar(Argumentos)
    if Rodada.returncode != 0 or not Dll.exists():
        return None, f"forja build --modo dll{' --backend x64' if ComX64 else ''} saiu {Rodada.returncode}: {TextoDe(Rodada.stderr + Rodada.stdout)[:200]!r}"
    ConfirmarLegivel(Dll)
    return Dll, ""


def ExportsDoObjdump(Texto):
    """(Ordinal Base, [(ordinal, hint, nome)]) da tabela [Ordinal/Name Pointer] do objdump -p, na ordem em que o objdump a lista."""
    Base = re.search(r"Ordinal Base\s+(\d+)", Texto)
    Tabela = Texto.split("[Ordinal/Name Pointer] Table", 1)[-1].split("The Function Table", 1)[0] if "[Ordinal/Name Pointer] Table" in Texto else ""
    Nomes = [(int(O), int(H), N) for O, H, N in PadraoExportNome.findall(Tabela)]
    return (int(Base.group(1)) if Base else None), Nomes


def ItemExportsIguaisAoGcc(TesteNativo, Objdump, Pasta):
    """P22a do M3: a DLL do --backend x64 (mesmo .fj) tem no export directory os mesmos nomes, na mesma ordem, com ordinal base 1 e a mesma numeracao do congelado do C (ExportsEsperados)."""
    Nativa, Motivo = GerarBiblioteca(Pasta, "biblioteca_x64.dll", True)
    if Nativa is None:
        return False, Motivo
    Rodada = Rodar([Objdump, "-p", str(Nativa)])
    Saida = TextoDe(Rodada.stdout) + TextoDe(Rodada.stderr)
    if Rodada.returncode != 0 or "warning" in Saida.lower():
        return False, "objdump -p da DLL saiu com erro ou imprimiu aviso"
    Base, ExportsNativa = ExportsDoObjdump(Saida)
    if not ExportsNativa or Base != 1:
        return False, f"ordinal base {Base} (esperado 1) ou export directory vazio ({ExportsNativa})"
    if ExportsNativa != ExportsEsperados:
        return False, f"export directory da DLL nativa {ExportsNativa} diferente do congelado do C {ExportsEsperados}"
    return True, f"{len(ExportsNativa)} exports ({', '.join(N for _, _, N in ExportsNativa)}), ordinal base 1, iguais aos congelados do C"


def ItemDllTemRelocLimpo(TesteNativo, Objdump, Pasta):
    """P22a do M3: a DLL x64 sai como DLL (Characteristics 0x2026, ImageBase 0x180000000), com .reloc presente (diretorio 5 nao vazio, bloco bem formado) e objdump -p limpo."""
    Dll, Motivo = GerarBiblioteca(Pasta, "biblioteca_reloc.dll", True)
    if Dll is None:
        return False, Motivo
    Dados = Dll.read_bytes()
    Secoes = LerSecoesPe(Dados)
    Lfanew = struct.unpack_from("<I", Dados, 0x3C)[0]
    Caracteristicas = struct.unpack_from("<H", Dados, Lfanew + 22)[0]
    Base = struct.unpack_from("<Q", Dados, Lfanew + 24 + 24)[0]
    if Caracteristicas != CaracteristicasDll or Base != ImageBaseDll:
        return False, f"Characteristics {Caracteristicas:#x} e ImageBase {Base:#x}, esperado {CaracteristicasDll:#x} e {ImageBaseDll:#x}"
    if ".reloc" not in Secoes or ".edata" not in Secoes:
        return False, f"secoes {sorted(Secoes)} sem .reloc ou .edata"
    Rva, Tam, Ptr = Secoes[".reloc"]
    Pagina, TamBloco, Entrada = struct.unpack_from("<IIH", Dados, Ptr)
    TextoRva = Secoes[".text"][0]
    if TamBloco < 12 or TamBloco % 4 != 0 or not TextoRva <= Pagina < Rva:  # P69: o runtime dev leva o rastreio e o 1o ponteiro absoluto pode cair em .rdata/.data (nao na 1a pagina do .text)
        return False, f".reloc com pagina {Pagina:#x} e bloco de {TamBloco} B; esperado uma pagina da imagem (do .text ate o .reloc) e bloco de 12 B ou mais, multiplo de 4"
    Rodada = Rodar([Objdump, "-p", str(Dll)])
    Saida = TextoDe(Rodada.stdout) + TextoDe(Rodada.stderr)
    if Rodada.returncode != 0 or "warning" in Saida.lower():
        return False, f"objdump -p saiu {Rodada.returncode} ou imprimiu aviso"
    if "Base Relocation Directory [.reloc]" not in Saida or "Export Directory [.edata" not in Saida:
        return False, "objdump -p sem os diretorios de exportacao e de relocacao"
    return True, f"DLL (0x2026, ImageBase 0x180000000), .reloc RVA {Rva:#x} (bruto {Tam} B, bloco {TamBloco} B), .edata presente, objdump -p limpo"


ItensPe = [
    ("exe roda com stdout e exit", ItemExeRodaComSaida),
    ("objdump -p sem aviso", ItemObjdumpSemAviso),
    ("imports so KERNEL32", ItemImports),
    ("sha256 igual em duas gravacoes", ItemSha256Igual),
    ("tamanho ate 3 KB", ItemTamanhoAte3Kb),
    ("autoconferencia recusa X0905", ItemAutoconferenciaRecusa),
    ("exe real do 01_ola identificado (Forja Native)", ItemExeRealIdentificado),
    ("exe real do 01_ola sem rotulo de compilador alheio", ItemSemRotuloDeCompiladorAlheio),
    ("exe sem ADDR64 tem .reloc e objdump -p limpo", ItemRelocSemAddr64),
    ("unidade com secao vazia recusada com X0905", ItemSecaoVaziaRecusada),
    ("exe de unidade com 2 DLLs (KERNEL32 e ucrtbase) na ordem", ItemDuasDlls),
    ("DLL x64: export directory igual ao da DLL do gcc", ItemExportsIguaisAoGcc),
    ("DLL x64: .reloc presente e objdump -p limpo", ItemDllTemRelocLimpo),
]


def Verificar(TesteNativo, Objdump):
    Pasta = Path(tempfile.mkdtemp(prefix="verificar_pe_"))
    Passou = 0
    Pulados = 0
    for Indice, (Nome, Funcao) in enumerate(ItensPe, 1):
        try:
            Ok, Detalhe = Funcao(TesteNativo, Objdump, Pasta)
        except ExeBloqueado as Bloqueio:
            Pulados += 1
            print(f"PULADO item {Indice} {Nome}: {Bloqueio}")
            continue
        except (OSError, subprocess.SubprocessError) as Erro:
            Ok, Detalhe = False, f"erro ao rodar: {Erro}"
        if Ok:
            Passou += 1
            print(f"OK item {Indice} {Nome}: {Detalhe}")
        else:
            print(f"FALHOU item {Indice} {Nome}: {Detalhe}")
    if Passou + Pulados == TotalItens:
        Aviso = f" ({Pulados} pulados: antivirus barrou o exe de teste; o verificar_obj cobre o exe real)" if Pulados else ""
        print(f"OK: {Passou}/{TotalItens} itens do PE{Aviso}")
        return 0
    print(f"FALHOU: {Passou}/{TotalItens} itens do PE (arquivos em {Pasta})")
    return 1


def Principal():
    Analisador = argparse.ArgumentParser(description="Verifica o exe PE32+ do M1 (P3).")
    Analisador.add_argument("--teste", type=Path, default=TesteNativoPadrao, help="caminho do teste_nativo.exe")
    Analisador.add_argument("--objdump", default="objdump", help="objdump do MinGW (para -p)")
    Analisador.add_argument("--forja", type=Path, default=ForjaPadrao, help="caminho do forja.exe (item 7)")
    Argumentos = Analisador.parse_args()
    if not Argumentos.teste.exists() or not Argumentos.forja.exists():
        print(f"teste_nativo.exe ou forja.exe nao encontrado ({Argumentos.teste}, {Argumentos.forja}); rode construir.bat", file=sys.stderr)
        return 2
    global ForjaExe
    ForjaExe = Argumentos.forja.resolve()
    return Verificar(Argumentos.teste, Argumentos.objdump)


if __name__ == "__main__":
    sys.exit(Principal())
