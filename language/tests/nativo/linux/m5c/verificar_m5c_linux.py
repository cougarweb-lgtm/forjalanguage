#!/usr/bin/env python3
"""verificar_m5c_linux.py - M5c-b P58: as fixtures m5c (thread P47, sync P49, atomic P50, async P51) no alpine-build (x86_64-linux, dev e release).

Objetivo: cada fixture tem de dar o MESMO stdout e o MESMO exit que no Windows. O Windows e o oraculo: `forja run` nativo, uma vez por fixture (e por argumento).
Do Linux: o stdout e o exit iguais; stderr com o mesmo numero de linhas (panic = 1 relatorio); async com 1 e 6 workers tem a mesma linha hash=.
Pre-requisito: language\\third\\musl\\x86_64 (semente) ao lado de um forja.exe REAL; WSL alpine-build.
Uso: python -I verificar_m5c_linux.py --forja <forja.exe> [--vezes N]   (padrao 5; 20 so no portao; exit 0 = verde; PULADA sem WSL)
Versao: 0.4.0
Changelog: 0.4.0 (15/11/2026) M5c-b P60 - volume_udp.fj vira a gemea Linux m5c\net\volume_udp_linux.fj (janela de credito no emissor: o UDP em loopback do Linux nao tem contrapressao e o kernel descarta na rajada de 48 MB; criterio igual); decisao do Opus aceita pelo main.
  0.3.0 (15/11/2026) M5c-b P59/P60 - grupos varena (varena_basico, varena_reset: P59) e net (eco, timeout, erros, volume, p54: P60) no mesmo verificador; o stdout compara sem os campos de medicao (ms=, peak=, pacotes_s=, recebidos=, perdidos=: tempo, RAM e contagem de UDP variam de SO para SO; o resto e igual ao Windows).
  0.2.0 (15/11/2026) M5c-b P58 - mxcsr_ftz.fj vira a gemea Linux mxcsr_ftz_linux.fj (pasta threads, helper C em vendor/linux); o oraculo segue a original no Windows.
  0.1.0 (15/11/2026) M5c-b P58 - primeira versao: 17 casos por rodada (thread 3, sync 4, atomic 4, async 6 com 1 e 6 workers), dev e release, oraculo = forja run no Windows.
"""
import argparse
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LINGUAGEM = AQUI.parents[3]
sys.path.insert(0, str(AQUI.parent / "abi_sysv"))  # verificar_abi_sysv (Construir, MODOS)
sys.path.insert(0, str(AQUI.parent))               # wsl_lote
import verificar_abi_sysv as Chamada  # noqa: E402
import wsl_lote  # noqa: E402

SUITE = "m5c"
VEZES_PADRAO = 5
LIMITE_PROGRAMA = 120
TEMPO_WINDOWS = 900
PASTA_M5C = LINGUAGEM / "tests" / "nativo" / "m5c"
EXIT_PANIC = 101

# (grupo, arquivo, argumentos apos "--"): o programa de async le o numero de workers do argumento.
CASOS = [
    ("thread", "spawn_10k.fj", []),
    ("thread", "mxcsr_ftz.fj", []),
    ("thread", "panic_filha.fj", []),
    ("sync", "contador_mutex.fj", []),
    ("sync", "produtor_consumidor.fj", []),
    ("sync", "try_lock.fj", []),
    ("sync", "cond_tempo.fj", []),
    ("atomic", "add_contadores.fj", []),
    ("atomic", "cas_laco.fj", []),
    ("atomic", "fence_publicacao.fj", []),
    ("atomic", "troca_ptr.fj", []),
    ("async", "soma_10k.fj", ["1"]),
    ("async", "soma_10k.fj", ["6"]),
    ("async", "hash_sim.fj", ["1"]),
    ("async", "hash_sim.fj", ["6"]),
    ("async", "panic_tarefa.fj", []),
    ("async", "ciclos_async.fj", []),
    ("varena", "varena_basico.fj", []),
    ("varena", "varena_reset.fj", []),
    ("net", "eco_udp.fj", []),
    ("net", "timeout_udp.fj", []),
    ("net", "erros_udp.fj", []),
    ("net", "volume_udp.fj", []),
    ("net", "p54_udp.fj", []),
]
# Campos de medicao do stdout (tempo, RAM, contagem de UDP): variam entre SO e entre rodadas; a linha OK/FALHA e o resto valem igual.
PADRAO_VOLATEIS = re.compile(r"\b(ms|peak|pacotes_s|recebidos|perdidos)=\S+")
# mxcsr_ftz.fj liga FTZ por _controlfp do msvcrt (so Windows). No Linux roda a gemea mxcsr_ftz_linux.fj (threads\, mesma saida "FTZ filha OK"); o oraculo segue a original no Windows.
GEMEAS_LINUX = {"mxcsr_ftz.fj": LINGUAGEM / "tests" / "nativo" / "linux" / "threads" / "mxcsr_ftz_linux.fj",
                # volume_udp.fj manda 48 MB em rajada sem contrapressao: no Linux o kernel descarta na fila cheia (RcvbufErrors); a gemea so controla o RITMO (janela de credito), mesmo criterio.
                "volume_udp.fj": LINGUAGEM / "tests" / "nativo" / "linux" / "m5c" / "net" / "volume_udp_linux.fj"}
CASOS_LINUX = CASOS
HASHES = [("hash_sim.fj", "1"), ("hash_sim.fj", "6")]


def RotuloDe(Arquivo: str, Argumentos: list) -> str:
    return Arquivo[:-3] + "".join(f"-{A}" for A in Argumentos)


def ExitEsperado(Arquivo: str) -> int:
    return EXIT_PANIC if Arquivo.startswith("panic_") else 0


def NormalizarTexto(Bruto: bytes) -> str:
    return Bruto.decode("utf-8", "replace").replace("\r\n", "\n")


def SemVolateis(Texto: str) -> str:
    """O stdout sem os campos de medicao (PADRAO_VOLATEIS): so eles diferem do Windows, o resto tem de bater."""
    return PADRAO_VOLATEIS.sub(lambda Casa: Casa.group(1) + "=_", Texto)


def LinhasNaoVazias(Texto: str) -> int:
    return len([L for L in Texto.splitlines() if L.strip()])


def LinhaHash(Texto: str) -> str | None:
    for Linha in Texto.splitlines():
        if Linha.startswith("hash="):
            return Linha.strip()
    return None


def RodarNoWindows(Forja: Path, Arquivo: str, Argumentos: list) -> tuple:
    """O oraculo: (exit, stdout normalizado, stderr) do forja run no Windows (dev, x64)."""
    Cmd = [str(Forja), "run", str(PastaDaFixture(Arquivo))]
    if Argumentos:
        Cmd += ["--", *Argumentos]
    Resultado = subprocess.run(Cmd, capture_output=True, timeout=TEMPO_WINDOWS, stdin=subprocess.DEVNULL)
    return Resultado.returncode, NormalizarTexto(Resultado.stdout), NormalizarTexto(Resultado.stderr)


def PastaDaFixture(Arquivo: str) -> Path:
    """Cada fixture mora no grupo dela (thread, sync, atomic, async): a 1a palavra do nome e o grupo."""
    for Grupo in ("thread", "sync", "atomic", "async", "varena", "net"):
        if (PASTA_M5C / Grupo / Arquivo).is_file():
            return PASTA_M5C / Grupo / Arquivo
    raise FileNotFoundError(Arquivo)


def ConstruirTodos(Forja: Path, Pasta: Path) -> tuple:
    """{(arquivo, modo): elf} e a lista de falhas de build (um ELF por arquivo e modo)."""
    Arquivos = sorted({Arquivo for _Grupo, Arquivo, _Args in CASOS_LINUX})
    Tarefas = [(Arquivo, Modo) for Arquivo in Arquivos for Modo in Chamada.MODOS]
    Elfs, Falhas = {}, []

    def Fazer(Tarefa):
        Arquivo, Modo = Tarefa
        Saida = Pasta / f"{Arquivo[:-3]}_{Modo}.elf"
        Fonte = GEMEAS_LINUX.get(Arquivo, PastaDaFixture(Arquivo))
        return Tarefa, Saida, Chamada.Construir(Forja, Fonte, Modo, Saida)

    with ThreadPoolExecutor(max_workers=4) as Piscina:
        for (Arquivo, Modo), Saida, Erro in Piscina.map(Fazer, Tarefas):
            if Erro:
                Falhas.append((f"{Arquivo}/{Modo}", f"build: {Erro}"))
            else:
                Elfs[(Arquivo, Modo)] = Saida
    return Elfs, Falhas


def Oraculos(Forja: Path) -> dict:
    """{rotulo: (exit, stdout, linhas de stderr)} do Windows, uma vez por caso."""
    Lista = {}
    for _Grupo, Arquivo, Argumentos in CASOS_LINUX:
        Rc, Saida, Erro = RodarNoWindows(Forja, Arquivo, Argumentos)
        Lista[RotuloDe(Arquivo, Argumentos)] = (Rc, SemVolateis(Saida), LinhasNaoVazias(Erro))
    return Lista


def OraculoForaDoEsperado(Lista: dict) -> list:
    """O proprio Windows tem de dar o exit esperado; senao o oraculo esta podre e nada do Linux vale."""
    Falhas = []
    for _Grupo, Arquivo, Argumentos in CASOS_LINUX:
        Rotulo = RotuloDe(Arquivo, Argumentos)
        Rc = Lista[Rotulo][0]
        if Rc != ExitEsperado(Arquivo):
            Falhas.append((f"oraculo/{Rotulo}", f"Windows exit {Rc} (esperado {ExitEsperado(Arquivo)})"))
    return Falhas


def Programas(Elfs: dict, Vezes: int) -> list:
    Lista = []
    for Rodada in range(Vezes):
        for Modo in Chamada.MODOS:
            for _Grupo, Arquivo, Argumentos in CASOS_LINUX:
                Nome = f"{RotuloDe(Arquivo, Argumentos)}/{Modo}/{Rodada}"
                Lista.append(wsl_lote.Programa(Nome, Elfs[(Arquivo, Modo)], Args=list(Argumentos), Limite=LIMITE_PROGRAMA))
    return Lista


def ConferirContraWindows(Esperado: tuple, R) -> str | None:
    """exit, stdout e numero de linhas de stderr iguais ao Windows."""
    if R is None:
        return "sem resultado do lote"
    Rc, Saida, LinhasErro = Esperado
    Texto = SemVolateis(NormalizarTexto(R.Stdout))
    if R.Exit != Rc:
        return f"exit {R.Exit} (Windows {Rc}): {NormalizarTexto(R.Stderr)[:300]!r}"
    if Texto != Saida:
        return f"stdout diferente do Windows: {Texto[:200]!r} x {Saida[:200]!r}"
    if LinhasNaoVazias(NormalizarTexto(R.Stderr)) != LinhasErro:
        return f"stderr com {LinhasNaoVazias(NormalizarTexto(R.Stderr))} linha(s) (Windows {LinhasErro}): {NormalizarTexto(R.Stderr)[:300]!r}"
    return None


def ConferirHashes(Resultados: dict, Rodada: int, Modo: str) -> str | None:
    """async: o hash= com 1 worker e o com 6 workers tem de ser o mesmo nesta rodada e modo."""
    Linhas = []
    for Arquivo, Workers in HASHES:
        R = Resultados.get(f"{Arquivo[:-3]}-{Workers}/{Modo}/{Rodada}")
        if R is None:
            return "hash: sem resultado do lote"
        Linhas.append(LinhaHash(NormalizarTexto(R.Stdout)))
    if None in Linhas or Linhas[0] != Linhas[1]:
        return f"hash 1 worker {Linhas[0]} x 6 workers {Linhas[1]}"
    return None


def Avaliar(Oraculo: dict, Resultados: dict, Vezes: int) -> list:
    Falhas = []
    for Rodada in range(Vezes):
        for Modo in Chamada.MODOS:
            for _Grupo, Arquivo, Argumentos in CASOS_LINUX:
                Rotulo = f"{RotuloDe(Arquivo, Argumentos)}/{Modo}/{Rodada}"
                Motivo = ConferirContraWindows(Oraculo[RotuloDe(Arquivo, Argumentos)], Resultados.get(Rotulo))
                if Motivo:
                    Falhas.append((Rotulo, Motivo))
            Motivo = ConferirHashes(Resultados, Rodada, Modo)
            if Motivo:
                Falhas.append((f"hash_sim/{Modo}/{Rodada}", Motivo))
    return Falhas


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Fixtures m5c (thread, sync, atomic, async) no Linux x86_64 (P58).")
    Analisador.add_argument("--forja", required=True, help="forja.exe real (a semente musl fica ao lado dele)")
    Analisador.add_argument("--vezes", type=int, default=VEZES_PADRAO)
    Argumentos = Analisador.parse_args()
    Forja = Path(Argumentos.forja).resolve()
    if not wsl_lote.DistroDisponivel():
        print("m5c-linux: PULADA (WSL/distro ausente)")
        return 0
    Oraculo = Oraculos(Forja)
    Falhas = OraculoForaDoEsperado(Oraculo)
    if Falhas:
        for Rotulo, Motivo in Falhas:
            print(f"FALHOU {Rotulo}: {Motivo}")
        return 1
    Pasta = Path(tempfile.gettempdir()) / "forja_m5c_linux"
    Pasta.mkdir(parents=True, exist_ok=True)
    Elfs, Falhas = ConstruirTodos(Forja, Pasta)
    if Falhas:
        for Rotulo, Motivo in Falhas:
            print(f"FALHOU {Rotulo}: {Motivo}")
        return 1
    Resultados = {R.Nome: R for R in wsl_lote.ExecutarLote(Programas(Elfs, Argumentos.vezes), SUITE)}
    Falhas = Avaliar(Oraculo, Resultados, Argumentos.vezes)
    for Rotulo, Motivo in Falhas:
        print(f"FALHOU {Rotulo}: {Motivo}")
    print(f"m5c-linux: gemeas Linux em uso: {', '.join(f'{K} -> {V.name}' for K, V in GEMEAS_LINUX.items())}")
    Total = Argumentos.vezes * len(Chamada.MODOS) * (len(CASOS_LINUX) + len(HASHES) // 2)
    print(f"m5c-linux: {Total - len(Falhas)}/{Total} ({Argumentos.vezes} repeticoes, dev e release; exit e stdout = Windows)")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Principal())
