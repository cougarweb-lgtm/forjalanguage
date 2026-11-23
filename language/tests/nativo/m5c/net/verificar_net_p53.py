"""verificar_net_p53.py - fixtures do std:net UDP (M5 P53) em 127.0.0.1: eco de 1200 bytes, timeout medido, erros tipados (porta em uso, endereco invalido, close duplo); forja test std:net; D-R8 (01_ola sem WS2_32 e os 4 .o de runtime com o md5 de md5_runtime_win_t0.txt).

Uso: python -I verificar_net_p53.py <forja.exe absoluto> [--vezes N]
Versao: 0.2.0
Changelog: 0.2.0 - 0.21.0 F3: matriz so x64 (dev-x64, rel-x64); backend C aposentado. 0.1.0 - M5 P53: 3 fixtures x 4 modos (dev/release x x64/c), N vezes cada (default 3); forja test std:net 1x por modo; 01_ola sem WS2_32 no exe; md5 de rt_win_dev/rel/ryu/es; qualquer falha = exit 1.
"""
import hashlib
import pathlib
import subprocess
import sys
import tempfile

PastaFixtures = pathlib.Path(__file__).resolve().parent
PastaRaiz = PastaFixtures.parents[3]
PastaCompilador = PastaRaiz / "compiler"
ReferenciaMd5 = PastaFixtures.parent / "md5_runtime_win_t0.txt"
ObjetosD8 = ["rt_win_dev.o", "rt_win_rel.o", "rt_win_ryu.o", "rt_win_es.o"]
TEMPO_LIMITE_S = 900

MODOS = [
    ("dev-x64", ["--backend", "x64"]),
    ("rel-x64", ["--release", "--backend", "x64"]),
]

# fixture -> frases que a saida precisa ter (e nenhuma FALHA)
FIXTURES = {
    "eco_udp.fj": ["eco 1200 OK"],
    "timeout_udp.fj": ["timeout 100 OK", "zero OK"],
    "erros_udp.fj": ["porta_em_uso OK", "endereco OK", "fechado OK"],
}


def Rodar(Cmd):
    return subprocess.run(Cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=TEMPO_LIMITE_S)


def ConferirFixture(Forja, Arquivo, Opcoes):
    Resposta = Rodar([Forja, "run", *Opcoes, str(PastaFixtures / Arquivo)])
    if Resposta.returncode != 0:
        return f"rc={Resposta.returncode} (esperado 0) | stderr={Resposta.stderr.strip()[:200]!r}"
    Saida = Resposta.stdout
    if "FALHA" in Saida:
        return f"saida com FALHA: {Saida.strip()[:300]!r}"
    Faltam = [F for F in FIXTURES[Arquivo] if F not in Saida]
    if Faltam:
        return f"saida sem {Faltam}: {Saida.strip()[:300]!r}"
    return None


def RodarModo(Forja, Opcoes, Vezes):
    Falhas = []
    for Volta in range(1, Vezes + 1):
        for Arquivo in FIXTURES:
            Motivo = ConferirFixture(Forja, Arquivo, Opcoes)
            if Motivo:
                Falhas.append(f"{Arquivo} volta {Volta}: {Motivo}")
            elif Volta == 1 and Arquivo == "timeout_udp.fj":
                Resposta = Rodar([Forja, "run", *Opcoes, str(PastaFixtures / Arquivo)])
                print("  " + " | ".join(L.strip() for L in Resposta.stdout.splitlines() if "ms=" in L))
    return Falhas


def ConferirSuite(Forja):
    Resposta = Rodar([Forja, "test", "std:net"])
    if Resposta.returncode != 0:
        return [f"forja test std:net rc={Resposta.returncode}: {(Resposta.stdout + Resposta.stderr).strip()[:300]!r}"]
    return []


def ConferirD8Ola(Forja):
    Falhas = []
    Pasta = pathlib.Path(tempfile.mkdtemp(prefix="p53_ola_"))
    Exe = Pasta / "ola.exe"
    Resposta = Rodar([Forja, "build", str(PastaRaiz.parent / "examples" / "01_ola.fj"), "-o", str(Exe)])
    if Resposta.returncode != 0 or not Exe.exists():
        return [f"01_ola nao compilou: rc={Resposta.returncode}"]
    Bytes = Exe.read_bytes()
    if b"WS2_32" in Bytes.upper():
        Falhas.append("01_ola: o exe tem WS2_32 (sem std:net nao pode ter import novo)")
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
        print("uso: python -I verificar_net_p53.py <forja.exe absoluto> [--vezes N]")
        return 2
    Forja = Argv[0]
    Vezes = int(Argv[Argv.index("--vezes") + 1]) if "--vezes" in Argv else 3
    Falhas = ConferirMd5()
    Falhas += ConferirD8Ola(Forja)
    Falhas += ConferirSuite(Forja)
    for Nome, Opcoes in MODOS:
        Falhas += [f"{Nome}: {F}" for F in RodarModo(Forja, Opcoes, Vezes)]
        print(f"{Nome}: {Vezes} voltas")
    for Falha in Falhas:
        print("FALHA", Falha)
    Total = len(MODOS) * Vezes * len(FIXTURES) + len(ObjetosD8) + 2
    print(f"total={Total} falhas={len(Falhas)}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Main(sys.argv[1:]))
