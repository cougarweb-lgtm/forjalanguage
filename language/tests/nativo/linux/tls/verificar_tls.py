#!/usr/bin/env python3
"""verificar_tls.py - M5c-b P61: PT_TLS no ligador ELF. Um __thread (initial-exec, GOTTPOFF relaxado para LE; local-exec, TPOFF32; alinhado a 64) de
vendor\\linux\\libtls.a usado por 6 fios Forja (std:thread, variante libc) e pela principal; a principal sozinha tambem (o musl monta o bloco do main pelo PT_TLS).

Objetivo: tls_threads (cada fio ve o SEU valor, o .tdata comeca com o inicial em cada um) e tls_main rodam em dev E release, N vezes, com exit 0 e stderr vazio;
o ola (sem TLS) segue sem PT_TLS; e o readelf confere 1 PT_TLS com filesz/memsz/align esperados e nenhuma relocacao de TLS pendente.
Valores esperados (layout medido no alpine-build com gcc, libtls.a deste build): .tdata 8 B (ie_dat, le_dat), .tbss 12 B com align 64 (al em 0, le_bss, ie_bss)
-> no bloco: .tbss em 64 (alinhado), memsz = 64 + 12 = 76 = 0x4c, filesz = 8, p_align = 0x40.
Pre-requisitos: vendor\\linux\\libtls.a (construir_tls.sh, no alpine-build); language\\third\\musl\\x86_64 ao lado de um forja.exe REAL; WSL alpine-build com readelf.
Uso: python -I verificar_tls.py --forja <forja.exe> [--vezes N]   (exit 0 = tudo verde; PULADA sem WSL; --vezes padrao 5)
Versao: 0.1.0
Changelog: 0.1.0 (15/11/2026) M5c-b P61 - primeira versao (threads de TLS 6x, main com TLS, ola sem PT_TLS, PT_TLS e relocacoes por readelf).
"""
import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LINGUAGEM = AQUI.parents[3]
sys.path.insert(0, str(AQUI.parent / "abi_sysv"))  # verificar_abi_sysv (Construir, MODOS)
sys.path.insert(0, str(AQUI.parent))               # wsl_lote
import verificar_abi_sysv as Chamada  # noqa: E402
import wsl_lote  # noqa: E402

SUITE = "tls"
VEZES_PADRAO = 5
LIMITE_PROGRAMA = 120
TEMPO_READELF = 60
LIB = AQUI / "vendor" / "linux" / "libtls.a"
OLA = LINGUAGEM.parent / "examples" / "01_ola.fj"
FIOS = 6
ESPERADO_FIOS = "".join(f"T {K:02} ok\n" for K in range(FIOS)) + "main ok\n"
ESPERADO_MAIN = "main inicio ok\nmain set ok\n"
ESPERADO_PT_TLS = {"filesz": 0x8, "memsz": 0x4C, "align": 0x40}
PADRAO_TLS = re.compile(r"^\s*TLS\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+\S+\s+(0x[0-9a-fA-F]+)", re.M)
CASOS = (("tls_threads", ESPERADO_FIOS), ("tls_main", ESPERADO_MAIN))


def Wsl(Args: list) -> subprocess.CompletedProcess:
    return subprocess.run(["wsl", "-d", wsl_lote.DISTRO, *Args], capture_output=True, timeout=TEMPO_READELF)


def Readelf(Elf: Path, Opcao: str) -> str:
    Resultado = Wsl(["readelf", Opcao, wsl_lote.CaminhoWsl(str(Elf))])
    return wsl_lote.TextoDoWsl(Resultado.stdout) + wsl_lote.TextoDoWsl(Resultado.stderr)


def ConferirPtTls(Elf: Path) -> str | None:
    """Um PT_TLS com filesz/memsz/align esperados, e nenhuma relocacao de TLS pendente no executavel."""
    Cabecalhos = Readelf(Elf, "-lW")
    Achados = PADRAO_TLS.findall(Cabecalhos)
    if len(Achados) != 1:
        return f"PT_TLS: {len(Achados)} achados (esperado 1)"
    _Off, _Va, _Pa, Filesz, Memsz, Align = (int(X, 16) for X in Achados[0])
    Obtido = {"filesz": Filesz, "memsz": Memsz, "align": Align}
    if Obtido != ESPERADO_PT_TLS:
        return f"PT_TLS {Obtido} != {ESPERADO_PT_TLS}"
    Relocs = Readelf(Elf, "-rW")
    if "TPOFF" in Relocs or "GOTTPOFF" in Relocs:
        return "relocacao de TLS pendente no executavel (TPOFF/GOTTPOFF)"
    return None


def ConferirSemTls(Elf: Path) -> str | None:
    """O ola nao tem PT_TLS (o ELF cru segue como era)."""
    if "TLS" in Readelf(Elf, "-lW"):
        return "ola com PT_TLS sem ter TLS"
    return None


def ConstruirTodos(Forja: Path, Pasta: Path) -> tuple:
    """{(nome, modo): elf} e falhas de build: as fixtures desta pasta e o ola, em dev e release."""
    Fontes = [("tls_threads", AQUI / "tls_threads.fj"), ("tls_main", AQUI / "tls_main.fj"), ("ola", OLA)]
    Elfs, Falhas = {}, []
    for Nome, Fonte in Fontes:
        for Modo in Chamada.MODOS:
            Saida = Pasta / f"{Nome}_{Modo}.elf"
            Erro = Chamada.Construir(Forja, Fonte, Modo, Saida)
            if Erro:
                Falhas.append((f"{Nome}/{Modo}", f"build: {Erro}"))
            else:
                Elfs[(Nome, Modo)] = Saida
    return Elfs, Falhas


def Programas(Elfs: dict, Vezes: int) -> list:
    Lista = []
    for Rodada in range(Vezes):
        for Modo in Chamada.MODOS:
            for Nome, _Esperado in CASOS:
                Lista.append(wsl_lote.Programa(f"{Nome}/{Modo}/{Rodada}", Elfs[(Nome, Modo)], Limite=LIMITE_PROGRAMA))
    for Modo in Chamada.MODOS:
        Lista.append(wsl_lote.Programa(f"ola/{Modo}", Elfs[("ola", Modo)], Limite=LIMITE_PROGRAMA))
    return Lista


def AvaliarSaida(R, Esperado: str) -> str | None:
    if R is None:
        return "sem resultado do lote"
    Texto = R.Stdout.decode("utf-8", "replace").replace("\r\n", "\n")
    if R.Exit != 0 or R.Stderr or Texto != Esperado:
        return f"exit {R.Exit}; stdout {Texto[:200]!r} (esperado {Esperado!r}); stderr {R.Stderr[:200]!r}"
    return None


def AvaliarOla(R) -> str | None:
    """O ola (sem TLS) so precisa rodar com exit 0 e stderr vazio: a saida dele e a do exemplo."""
    if R is None:
        return "sem resultado do lote"
    if R.Exit != 0 or R.Stderr:
        return f"exit {R.Exit}; stderr {R.Stderr[:200]!r}"
    return None


def Avaliar(Elfs: dict, Resultados: dict, Vezes: int) -> list:
    Falhas = []
    for Rodada in range(Vezes):
        for Modo in Chamada.MODOS:
            for Nome, Esperado in CASOS:
                Rotulo = f"{Nome}/{Modo}/{Rodada}"
                Motivo = AvaliarSaida(Resultados.get(Rotulo), Esperado)
                if Motivo:
                    Falhas.append((Rotulo, Motivo))
    for Modo in Chamada.MODOS:
        for Nome, _Esperado in CASOS:
            Motivo = ConferirPtTls(Elfs[(Nome, Modo)])
            if Motivo:
                Falhas.append((f"readelf {Nome}/{Modo}", Motivo))
        Motivo = ConferirSemTls(Elfs[("ola", Modo)])
        if Motivo:
            Falhas.append((f"readelf ola/{Modo}", Motivo))
        Motivo = AvaliarOla(Resultados.get(f"ola/{Modo}"))
        if Motivo:
            Falhas.append((f"ola/{Modo}", Motivo))
    return Falhas


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="PT_TLS no ligador ELF (M5c-b P61).")
    Analisador.add_argument("--forja", required=True, help="forja.exe real (a semente musl fica ao lado dele)")
    Analisador.add_argument("--vezes", type=int, default=VEZES_PADRAO)
    Argumentos = Analisador.parse_args()
    Forja = Path(Argumentos.forja).resolve()
    if not wsl_lote.DistroDisponivel():
        print("tls: PULADA (WSL/distro ausente)")
        return 0
    if not LIB.exists():
        print(f"tls: falta {LIB} (rode construir_tls.sh no alpine-build)")
        return 1
    Pasta = Path(tempfile.gettempdir()) / "forja_tls"
    Pasta.mkdir(parents=True, exist_ok=True)
    Elfs, Falhas = ConstruirTodos(Forja, Pasta)
    if Falhas:
        for Rotulo, Motivo in Falhas:
            print(f"FALHOU {Rotulo}: {Motivo}")
        return 1
    Resultados = {R.Nome: R for R in wsl_lote.ExecutarLote(Programas(Elfs, Argumentos.vezes), SUITE)}
    Falhas = Avaliar(Elfs, Resultados, Argumentos.vezes)
    for Rotulo, Motivo in Falhas:
        print(f"FALHOU {Rotulo}: {Motivo}")
    Total = Argumentos.vezes * len(Chamada.MODOS) * len(CASOS) + len(Chamada.MODOS) * 3
    print(f"tls: {Total - len(Falhas)}/{Total} ({Argumentos.vezes} repeticoes, dev e release; PT_TLS 1 e sem relocacao TLS pendente)")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Principal())
