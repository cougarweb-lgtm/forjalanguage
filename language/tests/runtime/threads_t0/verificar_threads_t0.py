"""verificar_threads_t0.py - P44 (T0-2): prova o estado compartilhado do runtime nativo sob SRW com threads de fora.

Versao: 0.2.2
Changelog: 0.2.2 - 0.21.0 F3: padrao x64, backend C aposentado (--backend so aceita x64).
           0.2.1 - M6e P93k: --guarda-memoria roda os casos de linhas com a guarda de memoria (so x64 dev).
           0.2.0 - M5 P45: --panic (panic em 2 threads de fora = 1 relatorio, exit 101), --erro (GetLastError por thread) e --dll (DLL chamada por 4 threads via ctypes, sem vazamento do fio).
           0.1.0 - M5 P44: primeira versao. Roda threads_t0.fj (8 threads x 10 mil) e threads_t0_um.fj (1 thread x 10 mil) com `forja run` (dev: R0009 ligado),
           confere exit 0, stderr vazio (R0009 = 0), contagem de linhas e o formato de CADA linha (nenhuma intercalada), uma linha por (thread, volta).
Uso: python -I verificar_threads_t0.py <caminho absoluto do forja.exe> [--vezes N] [--backend x64]
Verificacao: o proprio script (exit 0 = todas as rodadas passaram; 1 = qualquer falha, com o motivo na saida).
"""
import os
import re
import subprocess
import sys
from pathlib import Path

PASTA = Path(__file__).resolve().parent
LINHA = re.compile(r"^L t(\d\d) i(\d{5}) v(\d{5}) q(\d{5}) s(\d\d)$")
CASOS = (("threads_t0.fj", 8, 10000), ("threads_t0_um.fj", 1, 10000))


def ConferirLinhas(Texto: str, Fios: int, Voltas: int) -> str:
    """Devolve "" se todas as linhas inteiras e unicas; senao o motivo."""
    Linhas = Texto.replace("\r\n", "\n").split("\n")
    if Linhas and Linhas[-1] == "":
        Linhas.pop()
    if len(Linhas) != Fios * Voltas:
        return f"{len(Linhas)} linhas, esperado {Fios * Voltas}"
    Vistos = set()
    for Numero, TextoLinha in enumerate(Linhas, 1):
        Achou = LINHA.match(TextoLinha)
        if Achou is None:
            return f"linha {Numero} fora do formato: {TextoLinha!r}"
        Fio, Volta, V, Q, S = (int(Achou.group(K)) for K in range(1, 6))
        if Fio >= Fios or Volta >= Voltas or V != Volta or Q != Volta or S != 15:
            return f"linha {Numero} com campos errados: {TextoLinha!r}"
        if (Fio, Volta) in Vistos:
            return f"linha {Numero} repetida: {TextoLinha!r}"
        Vistos.add((Fio, Volta))
    return ""


def Rodar(Forja: str, Arquivo: str, Backend: str, Guarda: bool = False) -> subprocess.CompletedProcess:
    Comando = [Forja, "run", str(PASTA / Arquivo)]
    if Backend:
        Comando[2:2] = ["--backend", Backend]
    if Guarda:
        Comando[2:2] = ["--guarda-memoria"]
    return subprocess.run(Comando, capture_output=True, timeout=300)


def VerificarPanic(Forja: str, Backend: str, Release: bool) -> str:
    """P45: panic em 2 threads de fora ao mesmo tempo = EXATAMENTE 1 relatorio R0003 e exit 101. Devolve "" se passou."""
    Comando = [Forja, "run", str(PASTA / "panic_thread.fj")]
    if Backend:
        Comando[2:2] = ["--backend", Backend]
    if Release:
        Comando.insert(2, "--release")
    Resultado = subprocess.run(Comando, capture_output=True, timeout=120)
    Relatorios = [L for L in Resultado.stderr.decode("utf-8", "replace").splitlines() if '"R0003"' in L]
    Resto = [L for L in Resultado.stderr.decode("utf-8", "replace").splitlines() if L.strip() and L not in Relatorios]
    if Resultado.returncode != 101 or len(Relatorios) != 1 or Resto or Resultado.stdout:
        return f"exit {Resultado.returncode}, {len(Relatorios)} relatorios, resto {Resto[:2]!r}, stdout {Resultado.stdout[:80]!r}"
    return ""


def VerificarErro(Forja: str, Backend: str) -> str:
    """P45: o GetLastError de uma thread nao vaza para outra (std:file em 4 threads). Devolve "" se passou."""
    Comando = [Forja, "run", str(PASTA / "erro_thread.fj")]
    if Backend:
        Comando[2:2] = ["--backend", Backend]
    try:
        Resultado = subprocess.run(Comando, capture_output=True, timeout=120, cwd=str(PASTA))
    finally:
        (PASTA / "erro_thread_existe.tmp").unlink(missing_ok=True)
    Linhas = sorted(Resultado.stdout.decode("utf-8", "replace").splitlines())
    Esperado = [f"T {K} ok 2000" for K in range(4)]
    if Resultado.returncode != 0 or Resultado.stderr or Linhas != Esperado:
        return f"exit {Resultado.returncode}; stdout {Resultado.stdout[:200]!r}; stderr {Resultado.stderr[:200]!r}"
    return ""


SCRIPT_DLL = '''
import ctypes, sys, threading
from ctypes import wintypes
class PMC(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD)] + [(N, ctypes.c_size_t) for N in
        ("PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage", "QuotaPagedPoolUsage", "QuotaPeakNonPagedPoolUsage", "QuotaNonPagedPoolUsage",
         "PagefileUsage", "PeakPagefileUsage", "PrivateUsage")]
def Privado():
    P = PMC(); P.cb = ctypes.sizeof(P)
    k = ctypes.WinDLL("kernel32"); k.GetCurrentProcess.restype = ctypes.c_void_p
    ps = ctypes.WinDLL("psapi"); ps.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.POINTER(PMC), wintypes.DWORD]
    ps.GetProcessMemoryInfo(k.GetCurrentProcess(), ctypes.byref(P), P.cb)
    return P.PrivateUsage
Dll = ctypes.CDLL(sys.argv[1])
Dll.trabalho.argtypes = [ctypes.c_int64]; Dll.trabalho.restype = ctypes.c_int64
ESPERADO = 1000 * len("x00000-00000")
Erros = []
def Fio():
    for _ in range(3):
        R = Dll.trabalho(1000)
        if R != ESPERADO: Erros.append(R)
def Rodada(N):
    Fios = [threading.Thread(target=Fio) for _ in range(N)]
    [F.start() for F in Fios]; [F.join() for F in Fios]
Rodada(4)
Antes = Privado()
for _ in range(60): Rodada(4)
Depois = Privado()
print("erros", len(Erros), "crescimento_mib", (Depois - Antes) // (1024 * 1024))
'''


def VerificarDll(Forja: str, Backend: str) -> str:
    """P45: DLL chamada por 4 threads (ctypes), 60 rodadas de 4 threads novas: sem R-code e sem vazamento do fio (16 MiB de frame por thread vazada = 3,8 GiB; o limite e 64 MiB)."""
    Saida = Path(os.environ.get("TEMP", ".")) / f"dll_threads_{Backend or 'x64'}.dll"
    Comando = [Forja, "build", "--modo", "dll", "--release", str(PASTA / "dll_threads.fj"), "-o", str(Saida)]
    if Backend:
        Comando[2:2] = ["--backend", Backend]
    Construcao = subprocess.run(Comando, capture_output=True, timeout=300)
    if Construcao.returncode != 0:
        return f"build: {Construcao.stdout[:200]!r}"
    Resultado = subprocess.run([sys.executable, "-I", "-c", SCRIPT_DLL, str(Saida)], capture_output=True, timeout=300)
    Texto = Resultado.stdout.decode().strip()
    if Resultado.returncode != 0 or Resultado.stderr or not Texto.startswith("erros 0 crescimento_mib "):
        return f"exit {Resultado.returncode}; stdout {Texto!r}; stderr {Resultado.stderr[:300]!r}"
    if int(Texto.rsplit(" ", 1)[1]) > 64:
        return f"fio vazando: {Texto}"
    return ""


def Principal() -> int:
    Argumentos = sys.argv[1:]
    if not Argumentos:
        print("uso: verificar_threads_t0.py <forja.exe> [--vezes N] [--backend x64] [--panic] [--erro] [--dll]")
        return 2
    Forja = Argumentos[0]
    Vezes = int(Argumentos[Argumentos.index("--vezes") + 1]) if "--vezes" in Argumentos else 1
    Backend = Argumentos[Argumentos.index("--backend") + 1] if "--backend" in Argumentos else ""
    Extras = (("--panic", lambda: VerificarPanic(Forja, Backend, False) or VerificarPanic(Forja, Backend, True)),
              ("--erro", lambda: VerificarErro(Forja, Backend)), ("--dll", lambda: VerificarDll(Forja, Backend)))
    Pedidos = [(Nome, Verificar) for Nome, Verificar in Extras if Nome in Argumentos]
    if Pedidos:
        for Rodada in range(1, Vezes + 1):
            for Nome, Verificar in Pedidos:
                Motivo = Verificar()
                if Motivo:
                    print(f"FALHA rodada {Rodada} {Nome}: {Motivo}")
                    return 1
            print(f"rodada {Rodada}/{Vezes} ok ({', '.join(N for N, _ in Pedidos)})")
        return 0
    for Rodada in range(1, Vezes + 1):
        for Arquivo, Fios, Voltas in CASOS:
            Resultado = Rodar(Forja, Arquivo, Backend, "--guarda-memoria" in Argumentos)
            Motivo = ConferirLinhas(Resultado.stdout.decode("utf-8", "replace"), Fios, Voltas)
            if Resultado.returncode != 0:
                Motivo = Motivo or "saida"
                Motivo = f"exit {Resultado.returncode} ({Motivo}); stderr: {Resultado.stderr[:300]!r}"
            elif Resultado.stderr:
                Motivo = f"stderr nao vazio (R0009?): {Resultado.stderr[:300]!r}"
            if Motivo:
                print(f"FALHA rodada {Rodada} {Arquivo}: {Motivo}")
                return 1
        print(f"rodada {Rodada}/{Vezes} ok ({CASOS[0][1] * CASOS[0][2]} + {CASOS[1][1] * CASOS[1][2]} linhas inteiras, R0009 = 0)")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
