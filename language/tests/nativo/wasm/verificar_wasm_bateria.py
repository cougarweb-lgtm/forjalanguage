r"""Suite "wasm" da bateria: junta as provas do alvo wasm32-wasip1 numa linha de resultado so.

Etapas (cada uma um subprocesso): verificar_wasm.py (corpus contra o x64, dev e release), verificar_run_test.py (run/test no wasm),
e os tres reactors (verificar_reactor.mjs com reactor_basico, verificar_reactor_cstr.mjs com reactor_cstr, verificar_reactor.mjs --struct com reactor_struct), em dev e release.
Sem Node no PATH a suite sai com a linha "wasm: PULADA (Node ausente)" e exit 0 (o alvo wasm roda o .wasm no Node).

Uso: python verificar_wasm_bateria.py --forja <forja.exe>
Saida: "RESULTADO: tudo certo (N/N etapas)" e exit 0, ou "RESULTADO: FALHOU (K/N etapas)" com as etapas que falharam; exit 1.
Verificacao: python tests\nativo\wasm\verificar_wasm_bateria.py --forja language\compiler\forja.exe
Versao: 1.0.0
Changelog: 1.0.0 - primeira versao (0.38.0).
"""
import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PASTA = Path(__file__).resolve().parent
TIMEOUT_ETAPA_SEG = 1200
REACTORS = (
    ("reactor_basico", "verificar_reactor.mjs"),
    ("reactor_cstr", "verificar_reactor_cstr.mjs"),
    ("reactor_struct", "verificar_reactor.mjs"),
)


def RodarEtapa(Comando):
    """(ok, ultima linha de saida): roda o comando e junta stdout+stderr."""
    try:
        Resp = subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=TIMEOUT_ETAPA_SEG)
    except subprocess.TimeoutExpired:
        return False, f"estourou {TIMEOUT_ETAPA_SEG} s"
    Saida = (Resp.stdout + Resp.stderr).strip().splitlines()
    return Resp.returncode == 0, (Saida[-1] if Saida else "(sem saida)")


def EtapaReactor(Forja, Nome, Verificador, Release, Pasta):
    """Compila reactors/<Nome>.fj com --modo dll e confere com o .mjs (o cstr recebe --release no modo release)."""
    Wasm = Pasta / f"{Nome}_{'rel' if Release else 'dev'}.wasm"
    Compilar = [Forja, "build", "--modo", "dll", "--target", "wasm32-wasip1", str(PASTA / "reactor" / f"{Nome}.fj"), "-o", str(Wasm)]
    if Release:
        Compilar.insert(2, "--release")
    Ok, Linha = RodarEtapa(Compilar)
    if not Ok:
        return False, f"build: {Linha}"
    Conferir = ["node", "--no-warnings", str(PASTA / Verificador), str(Wasm)]
    if Release and Nome == "reactor_cstr":
        Conferir.append("--release")
    if Nome == "reactor_struct":
        Conferir.append("--struct")
    return RodarEtapa(Conferir)


def ColetarEtapas(Forja, Pasta):
    Py = sys.executable
    Etapas = [
        ("verificar_wasm", lambda: RodarEtapa([Py, "-I", str(PASTA / "verificar_wasm.py"), "--forja", Forja, "--candidato", Forja])),
        ("verificar_run_test", lambda: RodarEtapa([Py, "-I", str(PASTA / "verificar_run_test.py"), "--forja", Forja])),
    ]
    for Nome, Verificador in REACTORS:
        for Release in (False, True):
            Modo = "release" if Release else "dev"
            Etapas.append((f"{Nome} {Modo}", lambda N=Nome, V=Verificador, R=Release: EtapaReactor(Forja, N, V, R, Pasta)))
    return Etapas


def Principal():
    Analisador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Analisador.add_argument("--forja", required=True)
    Args = Analisador.parse_args()
    if shutil.which("node") is None:
        print("wasm: PULADA (Node ausente)")
        return 0
    Falhas = []
    with tempfile.TemporaryDirectory(prefix="forja_wasm_bat_") as Temp:
        Etapas = ColetarEtapas(Args.forja, Path(Temp))
        for Nome, Rodar in Etapas:
            Ok, Linha = Rodar()
            print(f"{'ok  ' if Ok else 'FALHA'} {Nome}: {Linha}")
            if not Ok:
                Falhas.append(Nome)
    Total = len(Etapas)
    if Falhas:
        print(f"RESULTADO: FALHOU ({Total - len(Falhas)}/{Total} etapas): {', '.join(Falhas)}")
        return 1
    print(f"RESULTADO: tudo certo ({Total}/{Total} etapas)")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
