#!/usr/bin/env python3
"""verificar_threads_linux.py - M5c-b P57: o runtime Linux entre threads (threads criadas por pthread_create do musl, via foreign) no alpine-build.

Objetivo: o threads_t0 do P44 (8 threads x 10 mil voltas = 80 mil linhas inteiras) e o threads_t0_um (1 thread, a ponte cria o fio sozinha) rodam em dev E release, N vezes, com
exit 0, stderr vazio (R0009 = 0 em dev) e as mesmas linhas do Windows (o conjunto de linhas do threads_t0 do Windows, rodado aqui com `forja run`, e o oraculo). panic_thread: panic em
2 threads de fora = exit 101 com UM relatorio R0003. erro_thread: o estado de erro do std:file de uma thread nao vaza para outra. Um ELF sem threads e sem foreign (examples\\01_ola.fj)
continua o ELF cru do M5: nenhum simbolo do musl nos bytes (a variante libc nao entrou).
Pre-requisito: language\\third\\musl\\x86_64 (semente) ao lado de um forja.exe REAL (copia em %TEMP% nao acha a semente); WSL alpine-build.
Uso: python -I verificar_threads_linux.py --forja <forja.exe> [--vezes N]   (exit 0 = tudo verde; PULADA sem WSL; --vezes padrao 20)
Versao: 0.3.0
Changelog: 0.3.0 (15/11/2026) M5c-b P63 - threads_es.fj (travas por grupo do runtime Linux: tabela de handles, ambiente, processos; 4 threads x 1500 voltas) e threads_pilha.fj (pilha da filha de 1 MiB), dev e release, N vezes.
  0.2.0 (15/11/2026) M5c-b P58 - gemea Linux mxcsr_ftz_linux.fj (FTZ herdado pela filha, saida "FTZ filha OK", dev e release, N vezes).
  0.1.0 (15/11/2026) M5c-b P57 - primeira versao (threads_t0 8x10 mil e 1x10 mil, panic_thread, erro_thread, ELF cru sem simbolo do musl).
"""
import argparse
import importlib.util
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

SUITE = "threads"
VEZES_PADRAO = 20
LIMITE_PROGRAMA = 120
OLA = LINGUAGEM.parent / "examples" / "01_ola.fj"
SIMBOLOS_DO_MUSL = (b"__libc_start_main", b"pthread_", b"__errno_location")
FONTE_WINDOWS = LINGUAGEM / "tests" / "runtime" / "threads_t0" / "verificar_threads_t0.py"
CASOS = (("threads_t0", 8, 10000), ("threads_t0_um", 1, 10000))
ESPERADO_ERRO = [f"T {K} ok 2000" for K in range(4)]
ESPERADO_ES = [f"T {K} ok 1500" for K in range(4)]  # P63: threads_es (travas por grupo: tabela de handles, ambiente, processos)
ESPERADO_PILHA = "pilha OK 1024"  # P63: threads_pilha (pilha da filha de 1 MiB)
GEMEA_MXCSR = "mxcsr_ftz_linux"  # P58: gemea Linux de tests\nativo\m5c\thread\mxcsr_ftz.fj (msvcrt _controlfp); mesma saida do original no Windows
GEMEA_MXCSR_SAIDA = "FTZ filha OK"


def CarregarVerificadorWindows():
    """O verificar_threads_t0.py do Windows (ConferirLinhas) como modulo."""
    Especificacao = importlib.util.spec_from_file_location("verificar_threads_t0", FONTE_WINDOWS)
    Modulo = importlib.util.module_from_spec(Especificacao)
    Especificacao.loader.exec_module(Modulo)
    return Modulo


def LinhasDoWindows(Forja: Path) -> list:
    """O oraculo: as linhas do threads_t0 do Windows (forja run, dev), ordenadas."""
    Fonte = LINGUAGEM / "tests" / "runtime" / "threads_t0" / "threads_t0.fj"
    Resultado = subprocess.run([str(Forja), "run", str(Fonte)], capture_output=True, timeout=300, stdin=subprocess.DEVNULL)
    if Resultado.returncode != 0 or Resultado.stderr:
        raise RuntimeError(f"threads_t0 do Windows: exit {Resultado.returncode} stderr {Resultado.stderr[:200]!r}")
    return sorted(Resultado.stdout.decode("utf-8", "replace").replace("\r\n", "\n").split("\n"))


def ConstruirTodos(Forja: Path, Pasta: Path) -> tuple:
    """{(nome, modo): elf} e a lista de falhas de build. Fixtures desta pasta mais o ola."""
    Fontes = [(F.stem, F) for F in sorted(AQUI.glob("*.fj"))] + [("ola", OLA)]
    Tarefas = [(Nome, Fonte, Modo) for Nome, Fonte in Fontes for Modo in Chamada.MODOS]
    Elfs, Falhas = {}, []

    def Fazer(Tarefa):
        Nome, Fonte, Modo = Tarefa
        Saida = Pasta / f"{Nome}_{Modo}.elf"
        return Tarefa, Saida, Chamada.Construir(Forja, Fonte, Modo, Saida)

    with ThreadPoolExecutor(max_workers=4) as Piscina:
        for (Nome, _Fonte, Modo), Saida, Erro in Piscina.map(Fazer, Tarefas):
            if Erro:
                Falhas.append((f"{Nome}/{Modo}", f"build: {Erro}"))
            else:
                Elfs[(Nome, Modo)] = Saida
    return Elfs, Falhas


def AvaliarCarga(Windows, Verificador, R, Fios: int, Voltas: int, OraculoLinhas) -> str | None:
    """threads_t0 / threads_t0_um: exit 0, stderr vazio, linhas inteiras e iguais ao Windows (so o de 8 fios compara com o oraculo)."""
    if R is None:
        return "sem resultado do lote"
    if R.Estourou or R.Exit != 0:
        return f"exit {R.Exit} (estourou={R.Estourou}): {R.Stderr[:300]!r}"
    if R.Stderr:
        return f"stderr nao vazio (R0009?): {R.Stderr[:300]!r}"
    Texto = R.Stdout.decode("utf-8", "replace")
    Motivo = Verificador.ConferirLinhas(Texto, Fios, Voltas)
    if Motivo:
        return Motivo
    if OraculoLinhas is not None and sorted(Texto.replace("\r\n", "\n").split("\n")) != OraculoLinhas:
        return "linhas diferentes das do Windows"
    return None


def AvaliarPanic(R) -> str | None:
    if R is None:
        return "sem resultado do lote"
    Linhas = R.Stderr.decode("utf-8", "replace").splitlines()
    Relatorios = [L for L in Linhas if '"R0003"' in L]
    Resto = [L for L in Linhas if L.strip() and L not in Relatorios]
    if R.Exit != 101 or len(Relatorios) != 1 or Resto or R.Stdout:
        return f"exit {R.Exit}, {len(Relatorios)} relatorios, resto {Resto[:2]!r}, stdout {R.Stdout[:80]!r}"
    return None


def AvaliarErro(R) -> str | None:
    if R is None:
        return "sem resultado do lote"
    Linhas = sorted(R.Stdout.decode("utf-8", "replace").splitlines())
    if R.Exit != 0 or R.Stderr or Linhas != ESPERADO_ERRO:
        return f"exit {R.Exit}; stdout {R.Stdout[:200]!r}; stderr {R.Stderr[:200]!r}"
    return None


def AvaliarEs(R) -> str | None:
    """threads_es (P63): exit 0, stderr vazio (R0009 = 0) e as 4 linhas 'T <id> ok 1500' (marcador, ambiente e processo sem corrupcao)."""
    if R is None:
        return "sem resultado do lote"
    Linhas = sorted(R.Stdout.decode("utf-8", "replace").splitlines())
    if R.Exit != 0 or R.Stderr or Linhas != ESPERADO_ES:
        return f"exit {R.Exit}; stdout {R.Stdout[:300]!r}; stderr {R.Stderr[:200]!r}"
    return None


def AvaliarPilha(R) -> str | None:
    """threads_pilha (P63): a filha recursa >= 256 KiB e volta com 'pilha OK 1024' (com a pilha padrao do musl seria SIGSEGV)."""
    if R is None:
        return "sem resultado do lote"
    if R.Exit != 0 or R.Stderr or R.Stdout.decode("utf-8", "replace").strip() != ESPERADO_PILHA:
        return f"exit {R.Exit}; stdout {R.Stdout[:200]!r}; stderr {R.Stderr[:200]!r}"
    return None


def ConferirCru(Elf: Path, R) -> str | None:
    """O ELF do ola: nenhum simbolo do musl nos bytes e roda (exit 0)."""
    Bytes = Elf.read_bytes()
    Achados = [S.decode() for S in SIMBOLOS_DO_MUSL if S in Bytes]
    if Achados:
        return f"ELF sem threads tem simbolo do musl: {Achados}"
    if R is None or R.Exit != 0:
        return f"ola nao rodou: exit {None if R is None else R.Exit}"
    return None


def AvaliarGemeaMxcsr(R) -> str | None:
    """Gemea Linux de mxcsr_ftz.fj (P58): exit 0, stderr vazio e a MESMA saida do original no Windows ("FTZ filha OK")."""
    if R is None:
        return "sem resultado do lote"
    Texto = R.Stdout.decode("utf-8", "replace").replace("\r\n", "\n")
    if R.Exit != 0 or R.Stderr or Texto.strip() != GEMEA_MXCSR_SAIDA:
        return f"exit {R.Exit}; stdout {Texto[:200]!r}; stderr {R.Stderr[:200]!r}"
    return None


def Programas(Elfs: dict, Vezes: int) -> list:
    Lista = []
    for Rodada in range(Vezes):
        for Modo in Chamada.MODOS:
            for Nome, _Fios, _Voltas in CASOS:
                Lista.append(wsl_lote.Programa(f"{Nome}/{Modo}/{Rodada}", Elfs[(Nome, Modo)], Limite=LIMITE_PROGRAMA))
    for Modo in Chamada.MODOS:
        for Nome in ("panic_thread", "erro_thread", "ola"):
            Lista.append(wsl_lote.Programa(f"{Nome}/{Modo}", Elfs[(Nome, Modo)], Limite=LIMITE_PROGRAMA))
        for Rodada in range(Vezes):
            Lista.append(wsl_lote.Programa(f"{GEMEA_MXCSR}/{Modo}/{Rodada}", Elfs[(GEMEA_MXCSR, Modo)], Limite=LIMITE_PROGRAMA))
            for Nome in ("threads_es", "threads_pilha"):
                Lista.append(wsl_lote.Programa(f"{Nome}/{Modo}/{Rodada}", Elfs[(Nome, Modo)], Limite=LIMITE_PROGRAMA))
    return Lista


def Avaliar(Forja: Path, Elfs: dict, Resultados: dict, Vezes: int) -> list:
    Verificador = CarregarVerificadorWindows()
    Oraculo = LinhasDoWindows(Forja)
    Falhas = []
    for Rodada in range(Vezes):
        for Modo in Chamada.MODOS:
            for Nome, Fios, Voltas in CASOS:
                Rotulo = f"{Nome}/{Modo}/{Rodada}"
                Motivo = AvaliarCarga(None, Verificador, Resultados.get(Rotulo), Fios, Voltas, Oraculo if Fios == 8 else None)
                if Motivo:
                    Falhas.append((Rotulo, Motivo))
    for Modo in Chamada.MODOS:
        for Rotulo, Avaliador in ((f"panic_thread/{Modo}", AvaliarPanic), (f"erro_thread/{Modo}", AvaliarErro)):
            Motivo = Avaliador(Resultados.get(Rotulo))
            if Motivo:
                Falhas.append((Rotulo, Motivo))
        Motivo = ConferirCru(Elfs[("ola", Modo)], Resultados.get(f"ola/{Modo}"))
        if Motivo:
            Falhas.append((f"ola/{Modo}", Motivo))
    for Modo in Chamada.MODOS:
        for Rodada in range(Vezes):
            Rotulo = f"{GEMEA_MXCSR}/{Modo}/{Rodada}"
            Motivo = AvaliarGemeaMxcsr(Resultados.get(Rotulo))
            if Motivo:
                Falhas.append((Rotulo, Motivo))
            for Nome, Avaliador in (("threads_es", AvaliarEs), ("threads_pilha", AvaliarPilha)):
                Motivo = Avaliador(Resultados.get(f"{Nome}/{Modo}/{Rodada}"))
                if Motivo:
                    Falhas.append((f"{Nome}/{Modo}/{Rodada}", Motivo))
    return Falhas


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Runtime Linux entre threads (P57).")
    Analisador.add_argument("--forja", required=True, help="forja.exe real (a semente musl fica ao lado dele)")
    Analisador.add_argument("--vezes", type=int, default=VEZES_PADRAO)
    Argumentos = Analisador.parse_args()
    Forja = Path(Argumentos.forja).resolve()
    if not wsl_lote.DistroDisponivel():
        print("threads: PULADA (WSL/distro ausente)")
        return 0
    Pasta = Path(tempfile.gettempdir()) / "forja_threads"
    Pasta.mkdir(parents=True, exist_ok=True)
    Elfs, Falhas = ConstruirTodos(Forja, Pasta)
    if Falhas:
        for Rotulo, Motivo in Falhas:
            print(f"FALHOU {Rotulo}: {Motivo}")
        return 1
    Resultados = {R.Nome: R for R in wsl_lote.ExecutarLote(Programas(Elfs, Argumentos.vezes), SUITE)}
    Falhas = Avaliar(Forja, Elfs, Resultados, Argumentos.vezes)
    for Rotulo, Motivo in Falhas:
        print(f"FALHOU {Rotulo}: {Motivo}")
    Total = Argumentos.vezes * len(Chamada.MODOS) * (len(CASOS) + 3) + len(Chamada.MODOS) * 3
    print(f"threads: {Total - len(Falhas)}/{Total} ({Argumentos.vezes} repeticoes, dev e release; R0009 = 0)")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Principal())
