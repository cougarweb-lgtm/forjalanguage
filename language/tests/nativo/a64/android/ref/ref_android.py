"""Gabarito A4.0: par .so + PIE dinamico AArch64 montados com binutils do Alpine e rodados no emulador Android.

Versao: 1.0.0 (2026-11-22)
Depende de: WSL alpine-build (aarch64-alpine-linux-musl-as/ld/readelf) e adb do Android SDK, com o emulador
emulator-5570 de pe (tool android_emulador 1.1.0). Nao usa a biblioteca de terceiros; so stdlib.

Uso:
  python -I ref_android.py --versao
  python -I ref_android.py --montar      puxa as libs guest do emulador para sysroot/ e monta os ELF
  python -I ref_android.py --conferir    --montar + empurra para /data/local/tmp/ref + roda + prova EXIT=42 e logcat
  python -I ref_android.py --selftest    testes das funcoes puras (sem emulador)

Changelog:
  1.0.0 (2026-11-22): primeira versao. LD_LIBRARY_PATH nao resolveu libref.so no tradutor; DT_RUNPATH absoluto
  (/data/local/tmp/ref) resolve. Sysroot = libc, libdl, liblog, libm e linker64 (plano B).
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

VERSAO = "1.0.0"
PASTA_REF = Path(__file__).resolve().parent
PASTA_SYSROOT = PASTA_REF / "sysroot"
ADB = r"C:\ANDROID_SDK\platform-tools\adb.exe"
SERIAL = "emulator-5570"
DISTRO_WSL = "alpine-build"
PASTA_REMOTA = "/data/local/tmp/ref"
LIBS_GUEST = ["libc", "libdl", "liblog", "libm"]
CAMINHO_LINKER = "/apex/com.android.runtime/bin/linker64"
PREFIXO_BINUTILS = "aarch64-alpine-linux-musl-"
PASTA_BUILD_WSL = "/tmp/forja_ref_build"


def ConverterWsl(Caminho: str) -> str:
    """Converte C:\\x\\y em /mnt/c/x/y para o WSL."""
    Forma = Path(Caminho).resolve().as_posix()
    Casa = re.match(r"^([A-Za-z]):/(.*)$", Forma)
    if Casa is None:
        raise ValueError(f"caminho sem letra de unidade: {Caminho}")
    return f"/mnt/{Casa.group(1).lower()}/{Casa.group(2)}"


def ExtrairExit(Saida: str):
    """Devolve o codigo da linha EXIT=N ou None."""
    Casa = re.search(r"^EXIT=(\d+)\s*$", Saida, re.MULTILINE)
    return int(Casa.group(1)) if Casa else None


def LogcatTemSubiu(Saida: str) -> bool:
    """Confere a linha de logcat 'forja: subiu' (tag forja, prioridade I)."""
    return re.search(r"\bforja\s*:\s*subiu\b", Saida) is not None


def RodarAdb(Args: list, Tempo: int = 120) -> str:
    Comando = [ADB, "-s", SERIAL] + Args
    Proc = subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=Tempo)
    return Proc.stdout + Proc.stderr


def RodarWsl(Script: str, Tempo: int = 600) -> str:
    """Roda um script sh do WSL. O script vai para %TEMP% (sem quoting de shell no caminho)."""
    Arquivo = Path(tempfile.gettempdir()) / "forja_ref_android.sh"
    with open(Arquivo, "w", encoding="utf-8", newline="\n") as Fh:
        Fh.write(Script)
    Comando = ["wsl.exe", "-d", DISTRO_WSL, "--", "sh", ConverterWsl(str(Arquivo))]
    Proc = subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=Tempo)
    Saida = Proc.stdout + Proc.stderr
    if Proc.returncode != 0:
        raise RuntimeError(f"WSL falhou (rc={Proc.returncode}):\n{Saida}")
    return Saida


def PuxarSysroot() -> None:
    """Puxa as libs arm64 do emulador (ABI arm64-v8a) para sysroot/. Recria do zero."""
    PASTA_SYSROOT.mkdir(exist_ok=True)
    for Lib in LIBS_GUEST:
        Destino = PASTA_SYSROOT / f"{Lib}.so"
        Saida = RodarAdb(["pull", f"/system/lib64/arm64/{Lib}.so", str(Destino)])
        if not Destino.exists():
            raise RuntimeError(f"pull de {Lib}.so falhou: {Saida}")
        print(f"sysroot: {Lib}.so {Destino.stat().st_size} B")
    Linker = PASTA_SYSROOT / "linker64"
    Saida = RodarAdb(["pull", CAMINHO_LINKER, str(Linker)])
    if not Linker.exists():
        raise RuntimeError(f"pull do linker64 falhou: {Saida}")
    print(f"sysroot: linker64 {Linker.stat().st_size} B (plano B qemu -L)")


def MontarScript() -> str:
    """Script sh: as + ld (Alpine) gera libref.so e ref_main e os readelf de gabarito."""
    R = ConverterWsl(str(PASTA_REF))
    A = PREFIXO_BINUTILS
    T = PASTA_BUILD_WSL
    Linhas = [
        "set -e",
        f"R={R}",
        f"T={T}",
        "mkdir -p $T",
        "cd $R",
        f"{A}as -o $T/libref.o libref.s",
        f"{A}as -o $T/ref_main.o ref_main.s",
        f"{A}ld -shared --hash-style=sysv -z now -z max-page-size=16384 -soname libref.so"
        f" -o libref.so $T/libref.o -L sysroot -rpath-link sysroot -llog --no-as-needed -lc",
        f"{A}ld -pie --dynamic-linker=/system/bin/linker64 --hash-style=sysv -z now -z max-page-size=16384"
        f" --enable-new-dtags -rpath {PASTA_REMOTA} --allow-shlib-undefined -rpath-link sysroot"
        " -o ref_main $T/ref_main.o libref.so",
        f"{A}readelf -hlSdrsW --dyn-syms libref.so > libref.readelf.txt",
        f"{A}readelf -hlSdrsW --dyn-syms ref_main > ref_main.readelf.txt",
        "echo MONTAGEM_OK",
    ]
    return "\n".join(Linhas) + "\n"


def MontarRef() -> None:
    Saida = RodarWsl(MontarScript())
    if "MONTAGEM_OK" not in Saida:
        raise RuntimeError(f"montagem nao concluiu:\n{Saida}")
    print("montagem: libref.so e ref_main OK (gabarito *.readelf.txt gravado)")


def EmularRef() -> bool:
    """Empurra o par, roda o PIE no emulador e confere EXIT=42 e a linha de logcat."""
    RodarAdb(["shell", f"rm -rf {PASTA_REMOTA}; mkdir -p {PASTA_REMOTA}"])
    RodarAdb(["push", str(PASTA_REF / "libref.so"), str(PASTA_REF / "ref_main"), PASTA_REMOTA + "/"], 120)
    RodarAdb(["shell", f"chmod 755 {PASTA_REMOTA}/ref_main"])
    RodarAdb(["logcat", "-c"])
    Saida = RodarAdb(["shell", f"cd {PASTA_REMOTA} && ./ref_main; echo EXIT=$?"], 120)
    Codigo = ExtrairExit(Saida)
    Log = ""
    for _ in range(4):
        Log = RodarAdb(["logcat", "-d", "-s", "forja:*"])
        if LogcatTemSubiu(Log):
            break
        time.sleep(0.5)
    print(f"saida do PIE: {Saida.strip()}")
    print(f"logcat: {Log.strip().splitlines()[-1] if Log.strip() else '(vazio)'}")
    return Codigo == 42 and LogcatTemSubiu(Log)


def Conferir() -> int:
    MontarRef()
    Ok = EmularRef()
    print("OK" if Ok else "FALHOU: esperado EXIT=42 e 'forja: subiu' no logcat")
    return 0 if Ok else 1


def Selftest() -> int:
    Falhas = []
    if ConverterWsl(r"C:\Users\x\a.s") != "/mnt/c/Users/x/a.s":
        Falhas.append("ConverterWsl")
    if ExtrairExit("ola\nEXIT=42\n") != 42 or ExtrairExit("sem codigo") is not None:
        Falhas.append("ExtrairExit")
    if not LogcatTemSubiu("I forja   : subiu") or LogcatTemSubiu("I forja : nada"):
        Falhas.append("LogcatTemSubiu")
    if Falhas:
        print("SELFTEST FALHOU: " + ", ".join(Falhas))
        return 1
    print("SELFTEST OK")
    return 0


def Principal() -> int:
    Parser = argparse.ArgumentParser(description="Gabarito A4.0 Bionic (ref_android)")
    Grupo = Parser.add_mutually_exclusive_group(required=True)
    Grupo.add_argument("--versao", action="store_true")
    Grupo.add_argument("--montar", action="store_true")
    Grupo.add_argument("--conferir", action="store_true")
    Grupo.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    if Args.versao:
        print(f"ref_android {VERSAO}")
        return 0
    if Args.selftest:
        return Selftest()
    if Args.montar:
        PuxarSysroot()
        MontarRef()
        return 0
    PuxarSysroot()
    return Conferir()


if __name__ == "__main__":
    sys.exit(Principal())
