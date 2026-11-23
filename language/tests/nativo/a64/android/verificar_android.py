"""Verificador A4.2/A4.3: ELF dinamico (.so e PIE) do alvo aarch64-linux-android, rodado no emulador Android.

Versao: 1.0.0 (2026-11-22)
Depende de: o compilador oficial (compiler/forja.exe, 0.46.0+; --forja aceita um candidato), WSL alpine-build (binutils aarch64: readelf, as, ld), adb do Android
SDK e o emulador emulator-5570 de pe (tool android_emulador). Reaproveita os helpers de ref/ref_android.py (so stdlib).

Casos (--conferir):
  1. libforja_ref.fj  -> libref.so (SONAME libref.so): readelf confere ET_DYN, e_machine 183, PT_LOAD align 0x4000, PT_DYNAMIC,
     PT_GNU_RELRO, DT_SONAME, DT_NEEDED liblog.so, BIND_NOW, DT_HASH, os exports no .dynsym, GLOB_DAT de __android_log_write, sem aviso.
  2. o PIE de referencia do A4.0 (ref_main.s com os nomes ref_*; o E0902 proibe export forja_*) montado com o ld do Alpine contra o
     .so da Forja: carrega no emulador, EXIT=42 e `forja: subiu` no logcat.
  3. pie_forja_libc.fj (PIE da Forja, write/getpid/_exit da libc): saida `ola` e EXIT=7.
  4. pie_forja_usa_so.fj + libforja_ref.so (PIE e .so, os dois da Forja, DT_RUNPATH pelo --runpath): EXIT=42 e `forja: subiu`.
  5. o ELF estatico aarch64-linux de uma fixture tem os mesmos bytes (sha256) do candidato anterior (--anterior).

Uso:
  python -I verificar_android.py --versao
  python -I verificar_android.py --conferir [--forja <exe>] [--anterior <exe>]
  python -I verificar_android.py --selftest     funcoes puras + falha plantada (sem emulador)

  6. (A4.4) libforja_heap.so (heap_soma(10) = 55 com lista no heap) chamado pelo pie_usa_heap: normal = `55` `55`;
     pie_usa_heap_ocupa (ocupa 0x4000_0000 e 0x8000_0000 com MAP_FIXED_NOREPLACE: as dicas baixas do .so falham e ele cai no
     fallback alto) = `55`; FORJA_A64_JANELA_ALTA=1 no ambiente do PIE (o heap do exe) = `55` `55`.

Changelog:
  1.1.0 (2026-11-22): A4.4 - caso 6 (heap no .so: janela baixa, fallback alto e janela alta forcada no PIE).
  1.0.0 (2026-11-22): primeira versao (A4.2/A4.3).
"""

import argparse
import hashlib
import importlib.util
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

VERSAO = "1.1.0"
PASTA = Path(__file__).resolve().parent
PASTA_REF = PASTA / "ref"
RAIZ_LANGUAGE = PASTA.parents[3]
FORJA_PADRAO = RAIZ_LANGUAGE / "compiler" / "forja.exe"
PASTA_REMOTA = "/data/local/tmp/fj_a42"
ALVO = "aarch64-linux-android"
FIXTURE_ESTATICA = RAIZ_LANGUAGE / "tests" / "nativo" / "a64" / "fixtures" / "aritmetica_bool.fj"


def CarregarRef():
    """Importa ref/ref_android.py pelo caminho (o python -I nao poe a pasta do script no sys.path)."""
    Spec = importlib.util.spec_from_file_location("ref_android", PASTA_REF / "ref_android.py")
    Modulo = importlib.util.module_from_spec(Spec)
    Spec.loader.exec_module(Modulo)
    return Modulo


def ConferirSaida(Saida: str, CodigoEsperado: int, LogEsperado: bool, Ref) -> list:
    """Devolve a lista de falhas de uma execucao no emulador (vazia = ok)."""
    Falhas = []
    Codigo = Ref.ExtrairExit(Saida)
    if Codigo != CodigoEsperado:
        Falhas.append(f"EXIT={Codigo}, esperado {CodigoEsperado}")
    return Falhas


def ConferirReadelf(Texto: str, Soname: str, Needed: str, Exports: list, Importado: str) -> list:
    """Confere o readelf -hlSdrsW --dyn-syms de um .so da Forja contra o gabarito do A4.0. Vazia = ok."""
    Exigidos = [
        (r"Type:\s+DYN", "ET_DYN"),
        (r"Machine:\s+AArch64", "e_machine 183"),
        (r"LOAD\s+.*0x4000\s*$", "PT_LOAD align 0x4000"),
        (r"\bDYNAMIC\b\s+0x", "PT_DYNAMIC"),
        (r"\bGNU_RELRO\b", "PT_GNU_RELRO"),
        (r"\(SONAME\)\s+Library soname: \[" + re.escape(Soname) + r"\]", "DT_SONAME " + Soname),
        (r"\(NEEDED\)\s+Shared library: \[" + re.escape(Needed) + r"\]", "DT_NEEDED " + Needed),
        (r"\(FLAGS\)\s+BIND_NOW", "DT_FLAGS BIND_NOW"),
        (r"\(FLAGS_1\)\s+Flags: NOW", "DT_FLAGS_1 NOW"),
        (r"\(HASH\)", "DT_HASH"),
        (r"R_AARCH64_GLOB_DAT\s+0+\s+" + re.escape(Importado), "GLOB_DAT de " + Importado),
        (r"\.dynsym\s+DYNSYM", "secao .dynsym"),
    ]
    for Nome in Exports:
        Exigidos.append((r"FUNC\s+GLOBAL\s+DEFAULT\s+\d+\s+" + re.escape(Nome) + r"\s*$", "export " + Nome + " no .dynsym"))
    Falhas = []
    for Padrao, Rotulo in Exigidos:
        if re.search(Padrao, Texto, re.MULTILINE) is None:
            Falhas.append("readelf sem " + Rotulo)
    if re.search(r"^readelf: (Warning|Error)", Texto, re.MULTILINE):
        Falhas.append("readelf com aviso ou erro")
    return Falhas


def Sha256(Caminho: Path) -> str:
    return hashlib.sha256(Caminho.read_bytes()).hexdigest()


def Compilar(Forja: Path, Argumentos: list) -> None:
    Proc = subprocess.run([str(Forja)] + Argumentos, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=300)
    if Proc.returncode != 0:
        raise RuntimeError("forja " + " ".join(Argumentos) + f" saiu {Proc.returncode}:\n{Proc.stdout}{Proc.stderr}")


def ReadelfWsl(Ref, Arquivo: Path) -> str:
    Script = f"set -e\n{Ref.PREFIXO_BINUTILS}readelf -hlSdrsW --dyn-syms {Ref.ConverterWsl(str(Arquivo))} 2>&1\n"
    return Ref.RodarWsl(Script)


def MontarRefMain(Ref, Pasta: Path) -> None:
    """ref_main.s do A4.0 com os nomes ref_* e ligado (ld do Alpine) contra o libref.so da Forja."""
    Fonte = (PASTA_REF / "ref_main.s").read_text(encoding="utf-8").replace("forja_ref_", "ref_")
    (Pasta / "ref_main_fj.s").write_text(Fonte, encoding="utf-8", newline="\n")
    W = Ref.ConverterWsl
    A = Ref.PREFIXO_BINUTILS
    Script = "\n".join([
        "set -e",
        f"cd {W(str(Pasta))}",
        f"{A}as -o ref_main_fj.o ref_main_fj.s",
        f"{A}ld -pie --dynamic-linker=/system/bin/linker64 --hash-style=sysv -z now -z max-page-size=16384"
        f" --enable-new-dtags -rpath {PASTA_REMOTA} --allow-shlib-undefined -rpath-link {W(str(Ref.PASTA_SYSROOT))}"
        " -o ref_main_fj ref_main_fj.o libref.so",
        "echo MONTAGEM_OK",
    ]) + "\n"
    if "MONTAGEM_OK" not in Ref.RodarWsl(Script):
        raise RuntimeError("montagem do ref_main_fj nao concluiu")


def Empurrar(Ref, Arquivos: list) -> None:
    Ref.RodarAdb(["shell", f"rm -rf {PASTA_REMOTA}; mkdir -p {PASTA_REMOTA}"])
    Ref.RodarAdb(["push"] + [str(A) for A in Arquivos] + [PASTA_REMOTA + "/"], 120)
    for A in Arquivos:
        Ref.RodarAdb(["shell", f"chmod 755 {PASTA_REMOTA}/{A.name}"])


def Rodar(Ref, Nome: str, Ambiente: str = ""):
    """Roda o executavel no emulador (Ambiente = "VAR=valor " antes do comando); devolve (saida do shell, logcat da tag forja)."""
    Ref.RodarAdb(["logcat", "-c"])
    Saida = Ref.RodarAdb(["shell", f"cd {PASTA_REMOTA} && {Ambiente}./{Nome}; echo EXIT=$?"], 120)
    Log = ""
    for _ in range(4):
        Log = Ref.RodarAdb(["logcat", "-d", "-s", "forja:*"])
        if Ref.LogcatTemSubiu(Log):
            break
        time.sleep(0.5)
    return Saida, Log


def Relatar(Rotulo: str, Falhas: list, Saida: str = "") -> bool:
    if Falhas:
        print(f"FALHOU {Rotulo}: " + "; ".join(Falhas))
        return False
    print(f"OK {Rotulo}" + (f" | {Saida.strip()}" if Saida else ""))
    return True


def CasoSo(Ref, Forja: Path, Tmp: Path) -> bool:
    Compilar(Forja, ["build", "--target", ALVO, "--modo", "dll", str(PASTA / "libforja_ref.fj"), "-o", str(Tmp / "libref.so")])
    Texto = ReadelfWsl(Ref, Tmp / "libref.so")
    Falhas = ConferirReadelf(Texto, "libref.so", "liblog.so", ["ref_valor", "ref_logar"], "__android_log_write")
    return Relatar("readelf do .so (SONAME libref.so)", Falhas)


def CasoRefMain(Ref, Tmp: Path) -> bool:
    MontarRefMain(Ref, Tmp)
    Empurrar(Ref, [Tmp / "libref.so", Tmp / "ref_main_fj"])
    Saida, Log = Rodar(Ref, "ref_main_fj")
    Falhas = ConferirSaida(Saida, 42, True, Ref)
    if not Ref.LogcatTemSubiu(Log):
        Falhas.append("logcat sem 'forja: subiu'")
    print(f"  logcat: {Log.strip().splitlines()[-1] if Log.strip() else '(vazio)'}")
    return Relatar("PIE de referencia (A4.0) carrega o .so da Forja", Falhas, Saida)


def CasoPieLibc(Ref, Forja: Path, Tmp: Path) -> bool:
    Compilar(Forja, ["build", "--target", ALVO, str(PASTA / "pie_forja_libc.fj"), "-o", str(Tmp / "pie_forja_libc")])
    Empurrar(Ref, [Tmp / "pie_forja_libc"])
    Saida, _ = Rodar(Ref, "pie_forja_libc")
    Falhas = ConferirSaida(Saida, 7, False, Ref)
    if "ola" not in Saida:
        Falhas.append("sem 'ola' do write da libc")
    return Relatar("PIE da Forja chama a libc (write/getpid/_exit)", Falhas, Saida.replace("\n", " "))


def CasoPieUsaSo(Ref, Forja: Path, Tmp: Path) -> bool:
    Compilar(Forja, ["build", "--target", ALVO, "--modo", "dll", str(PASTA / "libforja_ref.fj"), "-o", str(Tmp / "libforja_ref.so")])
    Compilar(Forja, ["build", "--target", ALVO, "--runpath", PASTA_REMOTA, str(PASTA / "pie_forja_usa_so.fj"), "-o", str(Tmp / "pie_forja_usa_so")])
    Empurrar(Ref, [Tmp / "libforja_ref.so", Tmp / "pie_forja_usa_so"])
    Saida, Log = Rodar(Ref, "pie_forja_usa_so")
    Falhas = ConferirSaida(Saida, 42, True, Ref)
    if not Ref.LogcatTemSubiu(Log):
        Falhas.append("logcat sem 'forja: subiu'")
    return Relatar("PIE da Forja + .so da Forja (DT_RUNPATH)", Falhas, Saida)


def SaidaDoPrograma(Saida: str) -> str:
    """Stdout do programa: o texto antes da linha EXIT=, sem as quebras das pontas."""
    return Saida.split("EXIT=")[0].strip()


def CasoHeapSo(Ref, Forja: Path, Tmp: Path) -> bool:
    """A4.4(C): heap no .so (heap_soma(10) = 55) com janela baixa, fallback alto (PIE ocupa as dicas baixas) e a flag no ambiente do PIE."""
    Compilar(Forja, ["build", "--target", ALVO, "--modo", "dll", str(PASTA / "libforja_heap.fj"), "-o", str(Tmp / "libforja_heap.so")])
    Compilar(Forja, ["build", "--target", ALVO, "--runpath", PASTA_REMOTA, str(PASTA / "pie_usa_heap.fj"), "-o", str(Tmp / "pie_usa_heap")])
    Compilar(Forja, ["build", "--target", ALVO, "--runpath", PASTA_REMOTA, str(PASTA / "pie_usa_heap_ocupa.fj"), "-o", str(Tmp / "pie_usa_heap_ocupa")])
    Empurrar(Ref, [Tmp / "libforja_heap.so", Tmp / "pie_usa_heap", Tmp / "pie_usa_heap_ocupa"])
    Casos = [
        ("janela baixa", "pie_usa_heap", "", "55\n55"),
        ("fallback alto (dicas baixas ocupadas)", "pie_usa_heap_ocupa", "", "55"),
        ("janela alta forcada no PIE", "pie_usa_heap", "FORJA_A64_JANELA_ALTA=1 ", "55\n55"),
    ]
    Resultados = []
    for Rotulo, Nome, Ambiente, Esperado in Casos:
        Saida, _ = Rodar(Ref, Nome, Ambiente)
        Falhas = ConferirSaida(Saida, 0, False, Ref)
        if SaidaDoPrograma(Saida) != Esperado:
            Falhas.append(f"stdout {SaidaDoPrograma(Saida)!r}, esperado {Esperado!r}")
        Resultados.append(Relatar(f"heap no .so: {Rotulo}", Falhas, SaidaDoPrograma(Saida).replace("\n", " ")))
    return all(Resultados)


def CasoEstatico(Forja: Path, Anterior, Tmp: Path) -> bool:
    if Anterior is None:
        print("PULADO bytes do ELF estatico (sem --anterior)")
        return True
    Compilar(Forja, ["build", "--target", "aarch64-linux", str(FIXTURE_ESTATICA), "-o", str(Tmp / "est_novo")])
    Compilar(Anterior, ["build", "--target", "aarch64-linux", str(FIXTURE_ESTATICA), "-o", str(Tmp / "est_antigo")])
    Novo, Antigo = Sha256(Tmp / "est_novo"), Sha256(Tmp / "est_antigo")
    return Relatar("ELF estatico aarch64-linux identico ao do candidato anterior", [] if Novo == Antigo else [f"{Novo[:12]} != {Antigo[:12]}"], Novo[:16])


def Conferir(Forja: Path, Anterior) -> int:
    Ref = CarregarRef()
    if not Forja.exists():
        print(f"candidato nao existe: {Forja}")
        return 2
    with tempfile.TemporaryDirectory(prefix="fj_a42_") as Pasta:
        Saida = Path(Pasta)
        Resultados = [CasoSo(Ref, Forja, Saida), CasoRefMain(Ref, Saida), CasoPieLibc(Ref, Forja, Saida), CasoPieUsaSo(Ref, Forja, Saida), CasoHeapSo(Ref, Forja, Saida), CasoEstatico(Forja, Anterior, Saida)]
    print("OK" if all(Resultados) else "FALHOU")
    return 0 if all(Resultados) else 1


def Selftest() -> int:
    Ref = CarregarRef()
    Bom = "\n".join([
        "  Type:                              DYN (Shared object file)",
        "  Machine:                           AArch64",
        "  LOAD           0x000000 0x0000000000000000 0x0000000000000000 0x0002dc 0x0002dc RW  0x4000",
        "  DYNAMIC        0x010000 0x0000000000010000 0x0000000000010000 0x0000d0 0x0000d0 RW  0x8",
        "  GNU_RELRO      0x010000 0x0000000000010000 0x0000000000010000 0x0001f4 0x0001f4 R   0x1",
        " 0x000000000000000e (SONAME)             Library soname: [libref.so]",
        " 0x0000000000000001 (NEEDED)             Shared library: [liblog.so]",
        " 0x0000000000000004 (HASH)               0x158",
        " 0x000000000000001e (FLAGS)              BIND_NOW",
        " 0x000000006ffffffb (FLAGS_1)            Flags: NOW",
        "0000000000007ff8  0000000400000401 R_AARCH64_GLOB_DAT     0000000000000000 __android_log_write + 0",
        "  [ 5] .dynsym           DYNSYM          0000000000000180 000180 000078 18   A  6   1  8",
        "     1: 0000000000008090     0 FUNC    GLOBAL DEFAULT    2 ref_valor",
        "     2: 00000000000080c0     0 FUNC    GLOBAL DEFAULT    2 ref_logar",
    ])
    Args = ("libref.so", "liblog.so", ["ref_valor", "ref_logar"], "__android_log_write")
    Falhas = []
    if ConferirReadelf(Bom, *Args):
        Falhas.append("ConferirReadelf rejeitou um readelf bom")
    if not ConferirReadelf(Bom.replace("BIND_NOW", "NADA"), *Args):
        Falhas.append("falha plantada (sem BIND_NOW) nao detectada")
    if not ConferirReadelf(Bom.replace("ref_logar", "outro"), *Args):
        Falhas.append("falha plantada (export ausente) nao detectada")
    if not ConferirReadelf(Bom + "\nreadelf: Warning: algo", *Args):
        Falhas.append("falha plantada (aviso do readelf) nao detectada")
    if ConferirSaida("x\nEXIT=42\n", 42, False, Ref):
        Falhas.append("ConferirSaida rejeitou EXIT certo")
    if not ConferirSaida("x\nEXIT=41\n", 42, False, Ref):
        Falhas.append("falha plantada (EXIT=41) nao detectada")
    if Falhas:
        print("SELFTEST FALHOU: " + ", ".join(Falhas))
        return 1
    print("SELFTEST OK")
    return 0


def Principal() -> int:
    Parser = argparse.ArgumentParser(description="Verificador A4.2/A4.3 (ELF dinamico android)")
    Grupo = Parser.add_mutually_exclusive_group(required=True)
    Grupo.add_argument("--versao", action="store_true")
    Grupo.add_argument("--conferir", action="store_true")
    Grupo.add_argument("--selftest", action="store_true")
    Parser.add_argument("--forja", default=str(FORJA_PADRAO))
    Parser.add_argument("--anterior", default=None)
    Args = Parser.parse_args()
    if Args.versao:
        print(f"verificar_android {VERSAO}")
        return 0
    if Args.selftest:
        return Selftest()
    return Conferir(Path(Args.forja), Path(Args.anterior) if Args.anterior else None)


if __name__ == "__main__":
    sys.exit(Principal())
