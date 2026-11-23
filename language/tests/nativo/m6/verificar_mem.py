# -*- coding: utf-8 -*-
"""verificar_mem.py - M6 P70: R0009 com pilha .fj, Arena/Pool vivos no relatorio e quarentena do delete (D-M6-7, D-M6-15a).

Versao: 0.5.0
Changelog: 0.5.0 - 0.21.0 F4 passo 3: sai o backend C; as fixtures _c (release_zero_c, handle_esquecido_c, release_handle_c: --backend c) saem de VARIANTES e FIXTURES (o sujeito era o C gerado). As demais seguem no x64.
           0.4.0 - P71b: handles genericos (track_handle/untrack_handle): handle_esquecido (malloc do msvcrt / musl, windows, linux e backend c), handle_ok, handle_desconhecido (R0008), handle_e_heap (2 linhas R0009), release_handle (em --release as chamadas somem; x64 nos 2 alvos e backend c). {so} no nome do arquivo = win|linux.
           0.3.0 - P71a (correcao): fixture release_zero em --release (x64 windows e linux, e backend c no windows; 20 de 20): o MemSnapshot que sobra sem as chamadas e 0 0 0 (VARIANTES: arquivo, opcoes e alvos por fixture).
           0.2.0 - P71a: fixture vazamento_por_tick (snapshot/report_since de std:mem: 100 ticks, so os nascidos depois do snapshot, por local e pilha); o Rodar tambem acha a linha {"tipo": "mem_report"}.
           0.1.1 - P70 (main): o JSON do R-code e achado dentro da linha do stderr (o aviso UTF-16 do wsl.exe podia vir colado antes dele e a linha era descartada: 1 falha em ~20 do caso linux vaza_3_niveis, que sozinho passava 60 de 60 e 80 de 80 sob carga).
           0.1.0 - P70: primeira versao. Builda cada fixture de tests\\nativo\\m6\\mem\\ em dev (Windows .exe e Linux ELF por wsl alpine-build), roda (20x nas de thread e de delete duplo)
           e confere a saida contra regras fixas (oraculo = o proprio .fj: o que ele deve produzir esta no comentario do cabecalho).
Uso: python -I verificar_mem.py --forja <forja.exe> [--alvo windows|linux|ambos] [--rodadas N] [--fixture nome]
Depende de: wsl com a distro alpine-build (so no alvo linux). Saida 0 = tudo PASSOU; 1 = alguma FALHOU; 2 = ferramenta ausente.
Verificacao: python -I verificar_mem.py --forja ..\\..\\..\\compiler\\forja.exe
"""
import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PASTA = Path(__file__).resolve().parent / "mem"
WSL_DISTRO = "alpine-build"
TIMEOUT_S = 60
RODADAS_PADRAO = 20


def ParaWsl(Caminho: Path) -> str:
    """C:/x/y -> /mnt/c/x/y."""
    Texto = Caminho.as_posix()
    return "/mnt/" + Texto[0].lower() + Texto[2:] if len(Texto) > 2 and Texto[1] == ":" else Texto


def Rodar(Alvo: str, Exe: Path) -> tuple:
    """Executa o programa e devolve (exit, stdout, linhas JSON do stderr)."""
    Comando = [str(Exe)] if Alvo == "windows" else ["wsl.exe", "-d", WSL_DISTRO, "--", ParaWsl(Exe)]
    Saida = subprocess.run(Comando, capture_output=True, timeout=TIMEOUT_S)
    Texto = Saida.stderr.decode("utf-8", errors="replace").replace("\x00", "")
    Linhas = []
    for Linha in Texto.splitlines():
        Achados = [Pos for Pos in (Linha.find('{"codigo"'), Linha.find('{"tipo": "mem_report"')) if Pos >= 0]
        if Achados:
            Inicio = min(Achados)
            Linhas.append(json.loads(Linha[Inicio:]))
    return Saida.returncode, Saida.stdout.decode("utf-8", errors="replace").replace("\r", "").strip(), Linhas


# Fixtures que rodam com outro arquivo/opcoes: nome -> (arquivo .fj, opcoes extras do build, alvos). As demais rodam em dev, nos 2 alvos.
VARIANTES = {
    "release_zero": ("release_zero", ["--release"], ("windows", "linux")),
    "handle_esquecido": ("handle_esquecido_{so}", [], ("windows", "linux")),
    "release_handle": ("release_handle", ["--release"], ("windows", "linux")),
}


def Construir(Forja: Path, Nome: str, Alvo: str, Pasta: Path) -> tuple:
    Exe = Pasta / (Nome + (".exe" if Alvo == "windows" else ".elf"))
    Arquivo, Extras, _ = VARIANTES.get(Nome, (Nome, [], ()))
    Arquivo = Arquivo.replace("{so}", "win" if Alvo == "windows" else "linux")
    Comando = [str(Forja), "build"] + (["--target", "x86_64-linux"] if Alvo == "linux" else []) + Extras + [str(PASTA / (Arquivo + ".fj")), "-o", str(Exe)]
    Saida = subprocess.run(Comando, capture_output=True, timeout=TIMEOUT_S)
    return Exe, Saida.returncode == 0, Saida.stdout.decode("utf-8", errors="replace")[:300]


def Procs(Linha: dict) -> list:
    return [Quadro["proc"] for Quadro in Linha.get("pilha", [])]


def ConferirTresNiveis(Alvo: str, Codigo: int, Saida: str, Linhas: list) -> str:
    if Codigo != 101 or Saida != "7" or len(Linhas) != 1 or Linhas[0]["codigo"] != "R0009":
        return f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"
    return "" if Procs(Linhas[0]) == ["nivel_b", "nivel_a", "main"] else f"pilha={Procs(Linhas[0])}"


def ConferirAlocador(Tipo: str, Criador: str):
    def Conferir(Alvo: str, Codigo: int, Saida: str, Linhas: list) -> str:
        if Codigo != 101 or Saida != "1" or len(Linhas) != 1:
            return f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"
        L = Linhas[0]
        if L["codigo"] != "R0009" or L["tipo"] != Tipo or L["linha"] != 10:
            return f"linha={L}"
        return "" if Procs(L) == [Criador, "montar", "main"] else f"pilha={Procs(L)}"
    return Conferir


def ConferirSeisThreads(Alvo: str, Codigo: int, Saida: str, Linhas: list) -> str:
    if Codigo != 101 or Saida != "6" or len(Linhas) != 1 or Linhas[0]["codigo"] != "R0009":
        return f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"
    if "6 blocos" not in Linhas[0]["causa"]:
        return f"causa={Linhas[0]['causa']!r}"
    return "" if Alvo == "windows" or Procs(Linhas[0]) == ["filha"] else f"pilha={Procs(Linhas[0])}"


def ConferirDeleteDuplo(Alvo: str, Codigo: int, Saida: str, Linhas: list) -> str:
    if Codigo != 101 or Saida != "10000" or len(Linhas) != 1:
        return f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"
    return "" if Linhas[0]["codigo"] == "R0008" and "ja passou por delete" in Linhas[0]["causa"] else f"linha={Linhas[0]}"


def ConferirVazamentoPorTick(Alvo: str, Codigo: int, Saida: str, Linhas: list) -> str:
    """100 ticks: report_since mostra so os 50 blocos (400 bytes) nascidos depois do snapshot, no local de vaza_par; o bloco de antes fica fora; depois o R0009 do fim."""
    if Codigo != 101 or Saida != "100" or len(Linhas) != 2:
        return f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"
    Rel, Fim = Linhas
    if Rel.get("tipo") != "mem_report" or Fim.get("codigo") != "R0009":
        return f"linhas={Rel.get('tipo')},{Fim.get('codigo')}"
    if (Rel["vivos_antes"], Rel["bytes_antes"], Rel["novos_blocos"], Rel["novos_bytes"], Rel["vivos_agora"]) != (1, 8, 50, 400, 51):
        return f"contadores={Rel}"
    if len(Rel["grupos"]) != 1 or Rel["grupos"][0]["blocos"] != 50 or Rel["grupos"][0]["bytes"] != 400 or Rel["outros"]["blocos"] != 0:
        return f"grupos={Rel['grupos']}"
    Pilha = Procs(Rel["grupos"][0])
    return "" if Pilha == ["vaza_par", "tick", "main"] else f"pilha={Pilha}"


def ConferirReleaseZero(Alvo: str, Codigo: int, Saida: str, Linhas: list) -> str:
    """Release: snapshot() e report_since() somem e o MemSnapshot sobra zerado: serie, vivos e bytes = 0, sem linha no stderr, exit 0."""
    if Codigo != 0 or Saida != "0\n0\n0" or Linhas:
        return f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"
    return ""


def ConferirHandleEsquecido(Alvo: str, Codigo: int, Saida: str, Linhas: list) -> str:
    """malloc da lib C registrado por track_handle e sem untrack: 1 R0009 tipo handle kind malloc; a pilha .fj so existe no runtime nativo (o oraculo C nao tem)."""
    if Codigo != 101 or Saida != "3" or len(Linhas) != 1:
        return f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"
    L = Linhas[0]
    if L["codigo"] != "R0009" or L["tipo"] != "handle" or L["kind"] != "malloc":
        return f"linha={L}"
    return "" if "pilha" not in L or Procs(L) == ["track_handle", "abrir_recurso", "main"] else f"pilha={Procs(L)}"


def ConferirHandleOk(Alvo: str, Codigo: int, Saida: str, Linhas: list) -> str:
    return "" if (Codigo, Saida, Linhas) == (0, "5", []) else f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"


def ConferirHandleDesconhecido(Alvo: str, Codigo: int, Saida: str, Linhas: list) -> str:
    if Codigo != 101 or Saida != "1" or len(Linhas) != 1:
        return f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"
    L = Linhas[0]
    if L["codigo"] != "R0008" or "untrack_handle" not in L["causa"]:
        return f"linha={L}"
    return "" if Procs(L) == ["untrack_handle", "main"] else f"pilha={Procs(L)}"


def ConferirHandleEHeap(Alvo: str, Codigo: int, Saida: str, Linhas: list) -> str:
    """Heap vazado + handle esquecido: as 2 linhas R0009 (heap primeiro, handle "socket" depois), exit 101."""
    if Codigo != 101 or Saida != "4" or len(Linhas) != 2:
        return f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"
    Heap, Handle = Linhas
    if Heap["codigo"] != "R0009" or Heap["tipo"] != "runtime" or Handle["codigo"] != "R0009" or Handle["tipo"] != "handle" or Handle["kind"] != "socket":
        return f"linhas={Linhas}"
    return "" if Procs(Heap) == ["criar", "main"] and Procs(Handle) == ["track_handle", "main"] else f"pilhas={Procs(Heap)},{Procs(Handle)}"


def ConferirReleaseHandle(Alvo: str, Codigo: int, Saida: str, Linhas: list) -> str:
    """Release: as 4 chamadas de handle somem; o untrack duplo (R0008 em dev) nao existe: stdout 9, sem linha, exit 0."""
    return "" if (Codigo, Saida, Linhas) == (0, "9", []) else f"exit={Codigo} stdout={Saida!r} linhas={len(Linhas)}"


FIXTURES = [
    ("handle_esquecido", 1, ConferirHandleEsquecido),
    ("handle_ok", 1, ConferirHandleOk),
    ("handle_desconhecido", 1, ConferirHandleDesconhecido),
    ("handle_e_heap", 1, ConferirHandleEHeap),
    ("release_handle", 20, ConferirReleaseHandle),
    ("release_zero", 20, ConferirReleaseZero),
    ("vazamento_por_tick", 1, ConferirVazamentoPorTick),
    ("vaza_3_niveis", 1, ConferirTresNiveis),
    ("arena_viva", 1, ConferirAlocador("arena", "arena_create")),
    ("pool_vivo", 1, ConferirAlocador("pool", "pool_create")),
    ("vaza_6_threads", None, ConferirSeisThreads),
    ("delete_duplo", None, ConferirDeleteDuplo),
]


def VerificarFixture(Forja: Path, Fixture: tuple, Alvo: str, Rodadas: int, Pasta: Path) -> tuple:
    Nome, Fixas, Conferir = Fixture
    Exe, Ok, Detalhe = Construir(Forja, Nome, Alvo, Pasta)
    if not Ok:
        return False, "build falhou: " + Detalhe
    Vezes = Fixas if Fixas else Rodadas
    for Volta in range(Vezes):
        Codigo, Saida, Linhas = Rodar(Alvo, Exe)
        Erro = Conferir(Alvo, Codigo, Saida, Linhas)
        if Erro:
            return False, f"rodada {Volta + 1}/{Vezes}: {Erro}"
    return True, f"{Vezes} de {Vezes}"


def Principal() -> int:
    Parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Parser.add_argument("--forja", required=True)
    Parser.add_argument("--alvo", choices=["windows", "linux", "ambos"], default="ambos")
    Parser.add_argument("--rodadas", type=int, default=RODADAS_PADRAO)
    Parser.add_argument("--fixture")
    Args = Parser.parse_args()
    Forja = Path(Args.forja).resolve()
    Alvos = ["windows", "linux"] if Args.alvo == "ambos" else [Args.alvo]
    if "linux" in Alvos and shutil.which("wsl.exe") is None:
        print("wsl.exe fora do PATH")
        return 2
    Falhas = 0
    with tempfile.TemporaryDirectory(prefix="verificar_mem_") as Tmp:
        for Alvo in Alvos:
            for Fixture in FIXTURES:
                if Args.fixture and Args.fixture != Fixture[0]:
                    continue
                if Alvo not in VARIANTES.get(Fixture[0], (None, None, ("windows", "linux")))[2]:
                    continue
                Ok, Detalhe = VerificarFixture(Forja, Fixture, Alvo, Args.rodadas, Path(Tmp))
                Falhas += 0 if Ok else 1
                print(f"{'PASSOU' if Ok else 'FALHOU'} {Alvo} {Fixture[0]}: {Detalhe}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Principal())
