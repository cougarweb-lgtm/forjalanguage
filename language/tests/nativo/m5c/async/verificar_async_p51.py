"""verificar_async_p51.py - fixtures do std:async (M5 P51): soma exata de 10 mil tarefas com 1 e 6 workers, hash de simulacao igual com 1 e 6 workers, panic numa tarefa (exit 101) e 200 ciclos new/close sem R0009.

Uso: python -I verificar_async_p51.py <forja.exe absoluto> [--vezes N]
Versao: 0.2.0
Changelog: 0.2.0 - 0.21.0 F3: matriz so x64 (dev-x64, rel-x64); backend C aposentado. 0.1.0 - M5 P51: 4 fixtures x 4 modos (x64/c, dev/release), N vezes cada (default 3); hash comparado entre 1 e 6 workers; qualquer falha = exit 1.
"""
import pathlib
import subprocess
import sys

PastaFixtures = pathlib.Path(__file__).resolve().parent
TEMPO_LIMITE_S = 900

MODOS = [
    ("dev-x64", ["--backend", "x64"]),
    ("rel-x64", ["--release", "--backend", "x64"]),
]


def RodarUma(Forja, Arquivo, Opcoes, Argumentos):
    Cmd = [Forja, "run", *Opcoes, str(PastaFixtures / Arquivo)]
    if Argumentos:
        Cmd += ["--", *Argumentos]
    Resposta = subprocess.run(
        Cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=TEMPO_LIMITE_S
    )
    return Resposta.returncode, Resposta.stdout, Resposta.stderr


def ConferirSoma(Rc, Saida):
    if Rc != 0:
        return f"rc={Rc} (esperado 0)"
    if "soma OK 49995000" not in Saida:
        return f"saida sem 'soma OK 49995000': {Saida.strip()[:200]!r}"
    return None


def LinhaHash(Saida):
    for Linha in Saida.splitlines():
        if Linha.startswith("hash="):
            return Linha.strip()
    return None


def ConferirHash(Rc1, Saida1, Rc6, Saida6):
    if Rc1 != 0 or Rc6 != 0:
        return f"rc 1w={Rc1} 6w={Rc6} (esperado 0 e 0)"
    Hash1, Hash6 = LinhaHash(Saida1), LinhaHash(Saida6)
    if Hash1 is None or Hash6 is None:
        return f"sem linha hash= (1w={Saida1.strip()[:120]!r} 6w={Saida6.strip()[:120]!r})"
    if Hash1 != Hash6:
        return f"hash diferente: 1 worker {Hash1} x 6 workers {Hash6}"
    return None


def ConferirPanic(Rc, Saida):
    if Rc != 101:
        return f"rc={Rc} (esperado 101)"
    if "tarefa quebrou" in Saida or "sucesso" in Saida:
        return f"saida inesperada: {Saida.strip()[:200]!r}"
    return None


def ConferirCiclos(Rc, Saida):
    if Rc != 0:
        return f"rc={Rc} (esperado 0)"
    if "ciclos OK" not in Saida or "falhou" in Saida:
        return f"saida sem 'ciclos OK': {Saida.strip()[:200]!r}"
    return None


def RodarModo(Forja, Opcoes, Vezes):
    Falhas = []
    for Volta in range(1, Vezes + 1):
        Rc, Saida, Erro = RodarUma(Forja, "soma_10k.fj", Opcoes, ["1"])
        Motivo = ConferirSoma(Rc, Saida)
        if Motivo is None:
            Rc, Saida, Erro = RodarUma(Forja, "soma_10k.fj", Opcoes, ["6"])
            Motivo = ConferirSoma(Rc, Saida)
        if Motivo:
            Falhas.append(f"soma_10k volta {Volta}: {Motivo} | stderr={Erro.strip()[:200]!r}")

        Rc1, Saida1, _ = RodarUma(Forja, "hash_sim.fj", Opcoes, ["1"])
        Rc6, Saida6, Erro6 = RodarUma(Forja, "hash_sim.fj", Opcoes, ["6"])
        Motivo = ConferirHash(Rc1, Saida1, Rc6, Saida6)
        if Motivo:
            Falhas.append(f"hash_sim volta {Volta}: {Motivo} | stderr6={Erro6.strip()[:200]!r}")

        Rc, Saida, Erro = RodarUma(Forja, "panic_tarefa.fj", Opcoes, [])
        Motivo = ConferirPanic(Rc, Saida)
        if Motivo:
            Falhas.append(f"panic_tarefa volta {Volta}: {Motivo} | stderr={Erro.strip()[:200]!r}")

        Rc, Saida, Erro = RodarUma(Forja, "ciclos_async.fj", Opcoes, [])
        Motivo = ConferirCiclos(Rc, Saida)
        if Motivo:
            Falhas.append(f"ciclos_async volta {Volta}: {Motivo} | stderr={Erro.strip()[:200]!r}")
    return Falhas


def Main(Argv):
    if not Argv:
        print("uso: python -I verificar_async_p51.py <forja.exe absoluto> [--vezes N]")
        return 2
    Forja = Argv[0]
    Vezes = int(Argv[Argv.index("--vezes") + 1]) if "--vezes" in Argv else 3
    Falhas = []
    for Nome, Opcoes in MODOS:
        Falhas += [f"{Nome}: {F}" for F in RodarModo(Forja, Opcoes, Vezes)]
        print(f"{Nome}: {Vezes} voltas")
    for Falha in Falhas:
        print("FALHA", Falha)
    Total = len(MODOS) * Vezes * 6
    print(f"total={Total} falhas={len(Falhas)}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Main(sys.argv[1:]))
