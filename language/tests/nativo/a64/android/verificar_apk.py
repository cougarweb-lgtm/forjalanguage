"""verificar_apk.py 1.0.0 (2026-11-22): prova A5.2, APK de codigo Forja sobe no emulador (NativeActivity).

Depende de: compilador forja oficial 0.46.0+ (compiler/forja.exe; --forja aceita um candidato), tools/forja_apk (APK) e adb do SDK
Android com o emulador ligado (python C:/CLAUDE_AGI/tools/android_emulador/android_emulador.py iniciar).
Uso:
  python -I -B verificar_apk.py --conferir [--forja <exe>] [--serial emulator-5570]
  python -I -B verificar_apk.py --selftest [--forja <exe>] [--serial emulator-5570]
Saida: 0 OK; 1 FALHOU ou ferramenta/ambiente; 2 uso.
Changelog:
  1.0.0 (2026-11-22) criacao (tarefa A5.2). --conferir compila libforja_app.fj, empacota pela forja_apk,
    desinstala, instala, abre, espera 3 s e prova: pid vivo + linha `forja : subiu` do MESMO pid no logcat +
    NativeActivity no dumpsys. Grava out/forja_app.apk e desinstala ao fim. --selftest: 7 casos da avaliacao
    pura + func_name inexistente deve reprovar + controle positivo deve passar.
Reversao: apagar este arquivo, libforja_app.fj e a pasta out/.
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

VERSAO = "1.0.0"
PACOTE = "br.forja.app"
FUNC = "app_on_create"
FUNC_INEXISTENTE = "app_nao_existe"
SERIAL_PADRAO = "emulator-5570"
ESPERA_VIVO_SEG = 3
RAIZ = Path(__file__).resolve().parent
LANGUAGE = RAIZ.parents[3]
FONTE = RAIZ / "libforja_app.fj"
SAIDA_APK = RAIZ / "out" / "forja_app.apk"
FORJA_PADRAO = LANGUAGE / "compiler" / "forja.exe"
FORJA_APK = Path("C:/CLAUDE_AGI/tools/forja_apk/forja_apk.py")
ADB_SDK = Path("C:/ANDROID_SDK/platform-tools/adb.exe")
LOG_OK = "11-22 23:20:09.667 10432 10432 I forja   : subiu"
DUMP_OK = "topResumedActivity=ActivityRecord{7e6dc73 u0 br.forja.app/android.app.NativeActivity t8}"
DIAG_RE = re.compile(r"AndroidRuntime|FATAL|signal|UnsatisfiedLink|dlopen|linker", re.IGNORECASE)


class FalhaFerramenta(Exception):
    """Erro de ferramenta ou ambiente; nao e prova reprovada."""


def Executar(Argumentos: list, Limite: int = 600) -> subprocess.CompletedProcess:
    return subprocess.run(Argumentos, capture_output=True, text=True, encoding="utf-8",
                          errors="replace", timeout=Limite)


def Adb(Serial: str, *Args: str) -> subprocess.CompletedProcess:
    Exe = str(ADB_SDK) if ADB_SDK.exists() else (shutil.which("adb") or "adb")
    try:
        return Executar([Exe, "-s", Serial, *Args], 120)
    except OSError as Erro:
        raise FalhaFerramenta("adb indisponivel: " + str(Erro)) from Erro


def DispositivoOnline(Serial: str) -> bool:
    Saida = Adb(Serial, "get-state")
    return Saida.returncode == 0 and Saida.stdout.strip() == "device"


def CompilarSo(Forja: Path, Pasta: Path) -> Path:
    So = Pasta / "libforja_app.so"
    R = Executar([str(Forja), "build", "--target", "aarch64-linux-android", "--modo", "dll",
                  "--release", str(FONTE), "-o", str(So)])
    if R.returncode != 0 or not So.exists():
        raise FalhaFerramenta("compilacao do .so falhou (rc %d): %s"
                              % (R.returncode, (R.stdout + R.stderr).strip()[-600:]))
    return So


def MontarInstalarAbrir(So: Path, Func: str, Serial: str, Apk: Path) -> str:
    Apk.parent.mkdir(parents=True, exist_ok=True)
    Cmd = [sys.executable, "-B", str(FORJA_APK), "--so", str(So), "--pacote", PACOTE,
           "--func", Func, "--saida", str(Apk), "--instalar", "--abrir", "--serial", Serial]
    R = Executar(Cmd)
    Saida = R.stdout + R.stderr
    for Linha in Saida.splitlines():
        print("  forja_apk: " + Linha)
    if R.returncode != 0:
        raise FalhaFerramenta("forja_apk rc %d" % R.returncode)
    return Saida


def LinhaSubiu(Logcat: str, Pid: str) -> str:
    if not Pid:
        return ""
    Padrao = re.compile(r"^\S+\s+\S+\s+" + re.escape(Pid) + r"\s+\d+\s+\w\s+forja\s*:\s*subiu\s*$", re.M)
    Achou = Padrao.search(Logcat)
    return Achou.group(0).strip() if Achou else ""


def AvaliarProva(Pid: str, Logcat: str, Dump: str, Abriu: bool) -> list:
    Pid = Pid.strip()
    if not Pid.isdigit():
        return ["processo " + PACOTE + " nao esta vivo (pidof vazio)"]
    Falhas = []
    if not LinhaSubiu(Logcat, Pid):
        Falhas.append("logcat sem a linha `forja : subiu` do pid " + Pid)
    if PACOTE + "/android.app.NativeActivity" not in Dump:
        Falhas.append("dumpsys sem a NativeActivity do pacote")
    if not Abriu:
        Falhas.append("am start -W sem `Status: ok`")
    return Falhas


def ImprimirDiagnostico(Logcat: str) -> None:
    Linhas = [L for L in Logcat.splitlines() if DIAG_RE.search(L)]
    print("diagnostico (logcat, ultimas linhas de erro):")
    for Linha in Linhas[-12:]:
        print("   " + Linha)


def ExecutarProva(So: Path, Serial: str, Func: str, Apk: Path) -> bool:
    Adb(Serial, "uninstall", PACOTE)
    Adb(Serial, "logcat", "-c")
    try:
        Saida = MontarInstalarAbrir(So, Func, Serial, Apk)
        time.sleep(ESPERA_VIVO_SEG)
        Pid = next(iter(Adb(Serial, "shell", "pidof", PACOTE).stdout.split()), "")
        Dump = Adb(Serial, "shell", "dumpsys", "activity", "activities").stdout
        Log = Adb(Serial, "logcat", "-d").stdout
        Falhas = AvaliarProva(Pid, Log, Dump, "Status: ok" in Saida)
    finally:
        Adb(Serial, "uninstall", PACOTE)
    if Falhas:
        print("FALHOU (func_name=" + Func + "): " + "; ".join(Falhas))
        ImprimirDiagnostico(Log)
        return False
    print("pid vivo apos " + str(ESPERA_VIVO_SEG) + " s: " + Pid)
    print("logcat: " + LinhaSubiu(Log, Pid))
    print("am start -W: Status: ok")
    print("dumpsys: NativeActivity do pacote presente")
    print("OK: " + PACOTE + " subiu no emulador com func_name=" + Func)
    return True


def ProvarAvaliacaoPura() -> tuple:
    Casos = [
        ("controle positivo", ("10432", LOG_OK, DUMP_OK, True), False),
        ("pid de outro processo", ("999", LOG_OK, DUMP_OK, True), True),
        ("processo morto", ("", LOG_OK, DUMP_OK, True), True),
        ("sem linha subiu", ("10432", "", DUMP_OK, True), True),
        ("linha de outro tag", ("10432", LOG_OK.replace("I forja", "I outro"), DUMP_OK, True), True),
        ("sem NativeActivity", ("10432", LOG_OK, "", True), True),
        ("am start sem Status ok", ("10432", LOG_OK, DUMP_OK, False), True),
    ]
    Furou = [Nome for Nome, Args, DeveReprovar in Casos if bool(AvaliarProva(*Args)) != DeveReprovar]
    return len(Casos), Furou


def Conferir(Forja: Path, Serial: str, Tmp: Path) -> int:
    if not DispositivoOnline(Serial):
        print("FALHOU: sem dispositivo " + Serial + " online (android_emulador.py iniciar)")
        return 1
    So = CompilarSo(Forja, Tmp)
    Ok = ExecutarProva(So, Serial, FUNC, SAIDA_APK)
    print("APK final: " + str(SAIDA_APK))
    return 0 if Ok else 1


def Selftest(Forja: Path, Serial: str, Tmp: Path) -> int:
    Total, Furou = ProvarAvaliacaoPura()
    print("avaliacao pura: " + str(Total) + " casos, " + ("OK" if not Furou else "FURO em " + ", ".join(Furou)))
    if not DispositivoOnline(Serial):
        print("FALHOU: sem dispositivo " + Serial + " online; o teste de ponta a ponta nao rodou")
        return 1
    So = CompilarSo(Forja, Tmp)
    if ExecutarProva(So, Serial, FUNC_INEXISTENTE, Tmp / "negativo.apk"):
        Furou.append("func_name inexistente passou (deveria reprovar)")
    if not ExecutarProva(So, Serial, FUNC, Tmp / "positivo.apk"):
        Furou.append("controle positivo (func_name certo) reprovou")
    if Furou:
        print("selftest FALHOU: " + "; ".join(Furou))
        return 1
    print("selftest VERDE (7 casos puros, func_name errado reprovado, controle positivo OK)")
    return 0


def Principal() -> int:
    Ap = argparse.ArgumentParser(description="Prova A5.2: APK de codigo Forja sobe no emulador.")
    Ap.add_argument("--conferir", action="store_true", help="compila, empacota, instala, abre e prova")
    Ap.add_argument("--selftest", action="store_true", help="planta func_name errado e prova que reprova")
    Ap.add_argument("--forja", default=str(FORJA_PADRAO), help="candidato do compilador")
    Ap.add_argument("--serial", default=SERIAL_PADRAO, help="serial do emulador")
    Ap.add_argument("--versao", action="version", version="verificar_apk " + VERSAO)
    Args = Ap.parse_args()
    if Args.conferir == Args.selftest:
        Ap.print_usage()
        print("use exatamente um: --conferir ou --selftest")
        return 2
    Forja = Path(Args.forja)
    if not Forja.exists():
        print("FALHOU: compilador nao encontrado: " + str(Forja))
        return 1
    with tempfile.TemporaryDirectory(prefix="verificar_apk_") as Pasta:
        try:
            if Args.selftest:
                return Selftest(Forja, Args.serial, Path(Pasta))
            return Conferir(Forja, Args.serial, Path(Pasta))
        except FalhaFerramenta as Erro:
            print("FALHOU: " + str(Erro))
            return 1


if __name__ == "__main__":
    sys.exit(Principal())
