# -*- coding: utf-8 -*-
"""Verifica o ligador do backend nativo (P5): forja_programa montado pelo codificador, ligado ao runtime lido por CoffLer.

Regras:
  - o item 1 roda teste_nativo.exe sem argumento: as verificacoes de unidade do P5 (inclui a ADDR64 sintetica no .reloc) tem de dar OK.
  - o item 2 roda teste_nativo.exe obj <pasta>, que grava caso1.exe e caso2.exe.
  - caso 1: stdout "42\\r\\n" e exit 0.
  - caso 2: stderr com o JSON R0002 do harness do P2a (forja_falha_estouro) e exit 101.
  - objdump -p dos dois exes: exit 0, sem "warning", imports so de KERNEL32.dll, com ExitProcess e so nomes dos 15 de KERNEL32_M1 (P17c: so os alcancaveis).

  - o item 6 (P7a) gera o examples\\01_ola.fj em dev e em release por BIR -> x64 -> PE (teste_nativo.exe x64, sem gcc/ld) e confere
    stdout+exit iguais aos bytes esperados (SaidaOla), sha256 igual em duas geracoes e objdump -p limpo.

  - o item 7 (M5 P27a) liga o 01_ola ao runtime Linux (teste_nativo.exe x64-linux, dev e release): a Imagem neutra sai sem unwind, sem import e sem .reloc, com a entrada _start; o resumo e igual em duas geracoes.

Changelog: 1.3.4 - 0.21.0 F3: item 6 deixa de rodar o backend C (aposentado na 0.21.0); confere so o x64 contra SaidaOla (stdout e exit), sha256 e objdump como antes; --forja segue aceito mas sem uso.
           1.3.3 - M5b P39-B1: o item 1 (teste_nativo sem argumentos) roda com cwd na pasta do teste_nativo (compiler\), por causa do ../third do teste_elf_membros; so o runner mudou.
           1.3.2 - M5 P44: ImportsEsperados ganha AcquireSRWLockExclusive e ReleaseSRWLockExclusive (a trava do estado compartilhado: todo exe Windows os importa); mesmos itens. 1.3.1 - M5 P43: ImportsEsperados ganha TlsAlloc, TlsGetValue e TlsSetValue (o slot do RtFio: todo exe Windows os importa); mesmos itens.
           1.3.0 - M5 P27a: item 7 (Imagem do 01_ola para o alvo linux, dev e release); TotalItens 6 -> 7.
           1.2.3 - M4 P25b: o build do oraculo C leva --backend c explicito (o padrao do dev passou a ser x64).
           1.2.2 - Linguagem v1.11 L16b: KERNEL32_M1 com 17 imports (AddVectoredExceptionHandler, so no rt de teste); mesmos itens.
           1.2.1 - Linguagem v1.11 L16a: KERNEL32_M1 com 16 imports (GetEnvironmentVariableA, so no rt de teste); mesmos itens.
           1.2.0 - M2 P17c: o ligador monta o .idata so dos __imp_ alcancaveis; o item de imports aceita um subconjunto dos 15 (com ExitProcess).
           1.1.1 - KERNEL32_M1 com 15 imports (console UTF-8).
           1.1.0 - backend x64 P7a: item 6 (01_ola x64 contra o backend C) e a opcao --forja.
           1.0.0 - backend x64 P5: primeira versao (5 itens do aceite do P5).
Verificacao: python -I ..\\tests\\nativo\\obj\\verificar_obj.py [--teste <teste_nativo.exe>] [--forja <forja.exe>] [--objdump <objdump>]
             (exige construir.bat antes: teste_nativo.exe na pasta compiler)
"""
import argparse
import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
RaizLinguagem = PastaAqui.parent.parent.parent
TesteNativoPadrao = RaizLinguagem / "compiler" / "teste_nativo.exe"
ForjaPadrao = RaizLinguagem / "compiler" / "forja.exe"
Ola = RaizLinguagem.parent / "examples" / "01_ola.fj"
SaidaOla = "Olá, Forja!\r\n42\r\n21\r\n".encode("utf-8")

MensagemCaso1 = b"42\r\n"
CodigoCaso1 = 0
CodigoCaso2 = 101
JsonCaso2 = (b'{"codigo": "R0002", "tipo": "runtime", "arquivo": "teste_rt.fj", "linha": 7, '
             b'"causa": "estouro de inteiro em soma i64: 9223372036854775807 e 1"}')
DllEsperada = "KERNEL32.dll"
ImportsEsperados = {
    "GetStdHandle", "WriteFile", "GetFileType", "ExitProcess", "GetProcessHeap", "HeapAlloc",
    "HeapReAlloc", "HeapFree", "VirtualAlloc", "VirtualFree", "QueryPerformanceCounter",
    "QueryPerformanceFrequency", "GetConsoleMode", "GetConsoleOutputCP", "SetConsoleOutputCP",
    "GetEnvironmentVariableA", "AddVectoredExceptionHandler", "TlsAlloc", "TlsGetValue", "TlsSetValue", "AcquireSRWLockExclusive", "ReleaseSRWLockExclusive",
}
PadraoImport = re.compile(r"\s*[0-9a-f]{8}\s+<none>\s+[0-9a-f]+\s+(\S+)")
PadraoDll = re.compile(r"DLL Name:\s*(\S+)")
Tempo = 60
TotalItens = 7


def Rodar(Argumentos, Pasta=None):
    return subprocess.run(Argumentos, capture_output=True, timeout=Tempo, cwd=Pasta)


def TextoDe(Bruto):
    return Bruto.decode("utf-8", errors="replace")


def ItemUnidade(TesteNativo):
    # P39-B1: o teste_elf_membros usa ../third relativo ao cwd; o cwd e a pasta do teste_nativo (compiler\).
    Rodada = Rodar([str(TesteNativo)], TesteNativo.resolve().parent)
    Ok = Rodada.returncode == 0 and b"OK:" in Rodada.stdout
    return Ok, f"teste_nativo saiu {Rodada.returncode}: {TextoDe(Rodada.stdout + Rodada.stderr)[-300:]!r}"


def ItemGerar(TesteNativo, Pasta):
    Rodada = Rodar([str(TesteNativo), "obj", str(Pasta)])
    Ok = Rodada.returncode == 0 and (Pasta / "caso1.exe").exists() and (Pasta / "caso2.exe").exists()
    return Ok, f"teste_nativo obj saiu {Rodada.returncode}: {TextoDe(Rodada.stderr)[:300]!r}"


def ItemCaso1(Exe):
    Rodada = Rodar([str(Exe)])
    Ok = Rodada.returncode == CodigoCaso1 and Rodada.stdout == MensagemCaso1
    return Ok, f"caso1 saiu {Rodada.returncode} com stdout {Rodada.stdout!r}"


def ItemCaso2(Exe):
    Rodada = Rodar([str(Exe)])
    Ok = Rodada.returncode == CodigoCaso2 and Rodada.stdout == b"" and Rodada.stderr.rstrip(b"\r\n") == JsonCaso2
    return Ok, f"caso2 saiu {Rodada.returncode} com stderr {Rodada.stderr!r}"


def ImportsDoObjdump(Texto):
    Dlls = PadraoDll.findall(Texto)
    Nomes = set(PadraoImport.findall(Texto))
    return Dlls, Nomes


def ItemObjdump(Objdump, Exes):
    for Exe in Exes:
        Rodada = Rodar([Objdump, "-p", str(Exe)])
        Texto = TextoDe(Rodada.stdout + Rodada.stderr)
        if Rodada.returncode != 0 or "warning" in Texto.lower():
            return False, f"objdump -p de {Exe.name} saiu {Rodada.returncode} ou tem warning"
        Dlls, Nomes = ImportsDoObjdump(Texto)
        if Dlls != [DllEsperada] or not Nomes or not Nomes <= ImportsEsperados or "ExitProcess" not in Nomes:
            return False, f"imports de {Exe.name}: dlls={Dlls} nomes={sorted(Nomes)}"
    return True, "objdump -p limpo; so KERNEL32.dll, so nomes dos 15 de KERNEL32_M1 (o ligador importa so os alcancaveis)"


def GerarX64(TesteNativo, Arquivo, Saida, Release):
    Argumentos = [str(TesteNativo), "x64", str(Arquivo), str(Saida)] + (["--release"] if Release else [])
    return Rodar(Argumentos)


def ConferirOla(TesteNativo, Pasta, Release):
    """Gera o 01_ola por BIR -> x64 -> PE (sem gcc/ld) e confere stdout+exit contra os bytes esperados (SaidaOla)."""
    Rotulo = "release" if Release else "dev"
    ExeX64 = Pasta / f"ola_x64_{Rotulo}.exe"
    Gerado = GerarX64(TesteNativo, Ola, ExeX64, Release)
    if Gerado.returncode != 0:
        return False, f"{Rotulo}: x64 saiu {Gerado.returncode} ({TextoDe(Gerado.stderr)[:200]!r})"
    Nativo = Rodar([str(ExeX64)])
    Correto = Nativo.stdout == SaidaOla and Nativo.returncode == 0
    return Correto, f"{Rotulo}: x64 exit {Nativo.returncode} stdout {Nativo.stdout!r}"


def Sha256(Caminho):
    return hashlib.sha256(Caminho.read_bytes()).hexdigest()


def ItemOla(TesteNativo, Objdump, Pasta):
    Partes = []
    for Release in (False, True):
        Ok, Motivo = ConferirOla(TesteNativo, Pasta, Release)
        Partes.append(Motivo)
        if not Ok:
            return False, " | ".join(Partes)
    Primeiro = Pasta / "ola_x64_dev.exe"
    Segundo = Pasta / "ola_x64_dev_2.exe"
    GerarX64(TesteNativo, Ola, Segundo, False)
    if not Segundo.exists() or Sha256(Primeiro) != Sha256(Segundo):
        return False, "sha256 diferente em duas geracoes do 01_ola"
    Ok, Motivo = ItemObjdump(Objdump, [Primeiro, Pasta / "ola_x64_release.exe"])
    return Ok, " | ".join(Partes) + f" | sha256 igual em duas geracoes | {Motivo}"


def ItemOlaLinux(TesteNativo):
    """M5 P27a: a Imagem neutra do 01_ola para x86_64-linux (conferida pelo proprio modo x64-linux), em dev e em release, com o mesmo resumo em duas geracoes."""
    Resumos = []
    for Release in (False, True):
        Argumentos = [str(TesteNativo), "x64-linux", str(Ola)] + (["--release"] if Release else [])
        Primeira, Segunda = Rodar(Argumentos), Rodar(Argumentos)
        if Primeira.returncode != 0 or not Primeira.stdout.startswith(b"x64-linux: "):
            return False, f"x64-linux {'release' if Release else 'dev'} saiu {Primeira.returncode}: {TextoDe(Primeira.stderr)[:300]!r}"
        if Primeira.stdout != Segunda.stdout:
            return False, f"resumo diferente em duas geracoes: {Primeira.stdout!r} e {Segunda.stdout!r}"
        Resumos.append(TextoDe(Primeira.stdout).strip())
    return True, " | ".join(Resumos)


def Principal():
    Parser = argparse.ArgumentParser()
    Parser.add_argument("--teste", default=str(TesteNativoPadrao))
    Parser.add_argument("--forja", default=str(ForjaPadrao))
    Parser.add_argument("--objdump", default="objdump")
    Args = Parser.parse_args()
    TesteNativo = Path(Args.teste)
    Resultados = [ItemUnidade(TesteNativo)]
    with tempfile.TemporaryDirectory() as Diretorio:
        Pasta = Path(Diretorio)
        Resultados.append(ItemGerar(TesteNativo, Pasta))
        Caso1, Caso2 = Pasta / "caso1.exe", Pasta / "caso2.exe"
        if Resultados[-1][0]:
            Resultados.append(ItemCaso1(Caso1))
            Resultados.append(ItemCaso2(Caso2))
            Resultados.append(ItemObjdump(Args.objdump, [Caso1, Caso2]))
        Resultados.append(ItemOla(TesteNativo, Args.objdump, Pasta))
        Resultados.append(ItemOlaLinux(TesteNativo))
    for Numero, (Ok, Motivo) in enumerate(Resultados, start=1):
        print(f"item {Numero}: {'OK' if Ok else 'FALHOU'} - {Motivo}")
    Aprovados = sum(1 for Ok, _ in Resultados if Ok)
    print(f"verificar_obj: {Aprovados}/{TotalItens}")
    return 0 if Aprovados == TotalItens else 1


if __name__ == "__main__":
    sys.exit(Principal())
