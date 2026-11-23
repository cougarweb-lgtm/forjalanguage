"""verificar_varena_p52.py - fixtures do std:mem VirtualArena (M5 P52): reserva de 4 GiB sem commit, commit de 64 MiB com leitura de volta, erros tipados, 1000 ciclos de reset sem recommit e md5 dos 4 objetos de runtime (D-R8).

Uso: python -I verificar_varena_p52.py <forja.exe absoluto> [--vezes N]
Versao: 0.2.0
Changelog: 0.2.0 - 0.21.0 F3: matriz so x64 (dev-x64, rel-x64); backend C aposentado. 0.1.0 - M5 P52: 2 fixtures x 4 modos (dev/release x x64/c), N vezes cada (default 3); md5 de rt_win_dev/rel/ryu/es contra md5_runtime_win_t0.txt; qualquer falha = exit 1.
"""
import hashlib
import pathlib
import re
import subprocess
import sys

PastaFixtures = pathlib.Path(__file__).resolve().parent
PastaCompilador = PastaFixtures.parents[3] / "compiler"
ReferenciaMd5 = PastaFixtures.parent / "md5_runtime_win_t0.txt"
ObjetosD8 = ["rt_win_dev.o", "rt_win_rel.o", "rt_win_ryu.o", "rt_win_es.o"]
TEMPO_LIMITE_S = 900

MODOS = [
    ("dev-x64", ["--backend", "x64"]),
    ("rel-x64", ["--release", "--backend", "x64"]),
]


def RodarUma(Forja, Arquivo, Opcoes):
    Cmd = [Forja, "run", *Opcoes, str(PastaFixtures / Arquivo)]
    Resposta = subprocess.run(
        Cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=TEMPO_LIMITE_S
    )
    return Resposta.returncode, Resposta.stdout, Resposta.stderr


def ConferirBasico(Rc, Saida, Erro):
    if Rc != 0:
        return f"rc={Rc} (esperado 0) | stderr={Erro.strip()[:200]!r}"
    if "varena basico OK" not in Saida or "FALHA" in Saida:
        return f"saida sem 'varena basico OK': {Saida.strip()[:300]!r}"
    return None


def ConferirReset(Rc, Saida, Erro):
    if Rc != 0:
        return f"rc={Rc} (esperado 0) | stderr={Erro.strip()[:200]!r}"
    if "reset OK" not in Saida or "FALHA" in Saida:
        return f"saida sem 'reset OK': {Saida.strip()[:300]!r}"
    return None


def RodarModo(Forja, Opcoes, Vezes):
    Falhas = []
    for Volta in range(1, Vezes + 1):
        Rc, Saida, Erro = RodarUma(Forja, "varena_basico.fj", Opcoes)
        Motivo = ConferirBasico(Rc, Saida, Erro)
        if Motivo:
            Falhas.append(f"varena_basico volta {Volta}: {Motivo}")
        Rc, Saida, Erro = RodarUma(Forja, "varena_reset.fj", Opcoes)
        Motivo = ConferirReset(Rc, Saida, Erro)
        if Motivo:
            Falhas.append(f"varena_reset volta {Volta}: {Motivo}")
        elif Volta == 1:
            print("  " + " | ".join(L.strip() for L in Saida.splitlines() if L.startswith("ciclo")))
    return Falhas


def ConferirMd5():
    Esperado = {}
    Atual = None
    for Linha in ReferenciaMd5.read_text(encoding="utf-8").splitlines():
        Nome = Linha.strip().split("/")[-1]
        if Nome.endswith(".o"):
            Atual = Nome
        elif Linha.strip().startswith("md5:") and Atual:
            Esperado[Atual] = Linha.split(":", 1)[1].strip()
    Falhas = []
    for Nome in ObjetosD8:
        Caminho = PastaCompilador / Nome
        if not Caminho.exists():
            Falhas.append(f"md5 {Nome}: arquivo ausente em {PastaCompilador}")
            continue
        Digest = hashlib.md5(Caminho.read_bytes()).hexdigest()
        if Digest != Esperado.get(Nome):
            Falhas.append(f"md5 {Nome}: {Digest} (esperado {Esperado.get(Nome)})")
    return Falhas


def Main(Argv):
    if not Argv:
        print("uso: python -I verificar_varena_p52.py <forja.exe absoluto> [--vezes N]")
        return 2
    Forja = Argv[0]
    Vezes = int(Argv[Argv.index("--vezes") + 1]) if "--vezes" in Argv else 3
    Falhas = ConferirMd5()
    for Nome, Opcoes in MODOS:
        Falhas += [f"{Nome}: {F}" for F in RodarModo(Forja, Opcoes, Vezes)]
        print(f"{Nome}: {Vezes} voltas")
    for Falha in Falhas:
        print("FALHA", Falha)
    Total = len(MODOS) * Vezes * 2 + len(ObjetosD8)
    print(f"total={Total} falhas={len(Falhas)}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Main(sys.argv[1:]))
