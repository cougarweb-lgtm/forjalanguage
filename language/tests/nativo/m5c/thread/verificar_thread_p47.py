"""verificar_thread_p47.py - fixtures do std:thread (M5 P47): 10 mil spawn/join, FTZ herdado e panic da filha = 101.

Uso: python -I verificar_thread_p47.py <forja.exe> [--vezes N]
Versao: 0.3.0
Changelog: 0.3.0 - 0.21.0 F3: matriz so x64 (dev-x64, rel-x64); backend C aposentado. 0.2.0 - M5 P47: nomes em CamelCase (PY-R01, lint pos-edicao). 0.1.0 - M5 P47: 3 fixtures x 4 modos (x64/c, dev/release), N vezes cada; qualquer falha = exit 1.
"""
import pathlib
import subprocess
import sys

PastaFixtures = pathlib.Path(__file__).resolve().parent

# (arquivo, saida esperada na stdout, codigo de saida esperado)
FIXTURES = [
    ("spawn_10k.fj", "10000 OK", 0),
    ("mxcsr_ftz.fj", "FTZ filha OK", 0),
    ("panic_filha.fj", None, 101),
]

MODOS = [
    ("dev-x64", ["--backend", "x64"]),
    ("rel-x64", ["--release", "--backend", "x64"]),
]


def RodarUma(Forja, Arquivo, Opcoes):
    Cmd = [Forja, "run", *Opcoes, str(PastaFixtures / Arquivo)]
    Resposta = subprocess.run(Cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=600)
    return Resposta.returncode, Resposta.stdout, Resposta.stderr


def Conferir(EsperadoSaida, EsperadoRc, Rc, Saida):
    if Rc != EsperadoRc:
        return f"rc={Rc} (esperado {EsperadoRc})"
    if EsperadoSaida is not None and EsperadoSaida not in Saida:
        return f"saida sem '{EsperadoSaida}': {Saida.strip()[:200]!r}"
    if EsperadoSaida is None and "nao devia" in Saida:
        return "a principal passou do join de uma filha em panico"
    return None


def Main(Argv):
    if not Argv:
        print("uso: python -I verificar_thread_p47.py <forja.exe> [--vezes N]")
        return 2
    Forja = Argv[0]
    Vezes = int(Argv[Argv.index("--vezes") + 1]) if "--vezes" in Argv else 20
    Falhas = []
    Total = 0
    for Arquivo, EsperadoSaida, EsperadoRc in FIXTURES:
        for Nome, Opcoes in MODOS:
            for Volta in range(1, Vezes + 1):
                Total += 1
                Rc, Saida, Erro = RodarUma(Forja, Arquivo, Opcoes)
                Motivo = Conferir(EsperadoSaida, EsperadoRc, Rc, Saida)
                if Motivo:
                    Falhas.append(f"{Arquivo} {Nome} volta {Volta}: {Motivo} | stderr={Erro.strip()[:200]!r}")
            print(f"{Arquivo} {Nome}: {Vezes} voltas")
    for Falha in Falhas:
        print("FALHA", Falha)
    print(f"total={Total} falhas={len(Falhas)}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Main(sys.argv[1:]))
