#!/usr/bin/env python3
"""verificar_wasm.py - verifica o alvo wasm32-wasip1 (forja build --target wasm32-wasip1) contra o oraculo x64 nativo.

Objetivo: cada fonte do corpus (os .fj soltos da m1, o main do pacote modulo/ e examples/01_ola.fj, se existir) e cada modo (dev e release) e compilada duas vezes:
(a) ORACULO: forja --backend x64 [--release] <fonte> -o <exe>; o .exe roda e (stdout, stderr, exit) vira o esperado.
(b) CANDIDATO: forja_wasm build --target wasm32-wasip1 [--release] <fonte> -o <wasm>; node --no-warnings rodar_wasm.mjs <wasm> roda.
Compara: stdout (CRLF -> LF dos dois lados), stderr (CRLF -> LF e NormalizarLinhasR, sem o campo pilha), exit.
Sem divergencia aceita: qualquer diferenca reprova a tarefa.

Uso:   python verificar_wasm.py [--forja <forja.exe>] [--candidato <forja_wasm.exe>] [--lista] [--modos dev,release] [--filtro texto] [--jobs N] [--timeout-seg N]
Saida: uma linha por tarefa com o status (ok, diverge, falha de build, falha de run) e o motivo; no fim o resumo. Exit 0 so com tudo ok.
Escrita: pasta temporaria (tempfile), apagada ao fim.
Versao: 1.0.0
Changelog: 1.0.0 - primeira versao (oraculo x64 x candidato wasm32-wasip1; selftest embutido, sem compilador).
Verificacao: python language/tests/nativo/wasm/verificar_wasm.py --selftest
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LANGUAGE = AQUI.parents[2]
sys.path.insert(0, str(AQUI.parents[1]))  # tests\: rcode_normalizar
from rcode_normalizar import NormalizarLinhasR  # noqa: E402

RODAR_WASM = AQUI / "rodar_wasm.mjs"
FORJA_PADRAO = LANGUAGE / "compiler" / "forja.exe"
CANDIDATO_PADRAO = LANGUAGE / "compiler" / "forja_wasm.exe"
CORPUS_M1 = LANGUAGE / "tests" / "nativo" / "m1"
CORPUS_WASM = AQUI / "corpus"  # fixtures do proprio backend wasm (faixas de estouro por classe etc.)
EXEMPLO_OLA = LANGUAGE.parent / "examples" / "01_ola.fj"
ALVO_WASM = "wasm32-wasip1"
MODOS_VALIDOS = ("dev", "release")
STATUS_VALIDOS = ("ok", "diverge", "falha de build", "falha de run")
TEMPO_PADRAO = 120
# 0.40.0: o exe nu do x64 morre mudo no estouro de pilha (0xC00000FD); o R0016 e do `forja run`, que e o oraculo dessas fixtures
PREFIXO_ORACULO_RUN = "pilha_estouro_"
TEMPO_SELFTEST = 60
CRLF = b"\r\n"
LF = b"\n"


@dataclass
class Tarefa:
    Nome: str
    Fonte: Path
    Modo: str


@dataclass
class Saida:
    Stdout: bytes
    Stderr: bytes
    Exit: int


def CorpusPadrao() -> list:
    """Fontes do corpus: os .fj soltos da m1, o main do pacote modulo/ (principal.fj) e o 01_ola se existir."""
    Fontes = sorted(CORPUS_M1.glob("*.fj"))
    Fontes.append(CORPUS_M1 / "modulo" / "principal.fj")
    Fontes.extend(sorted(CORPUS_WASM.glob("*.fj")))
    if EXEMPLO_OLA.is_file():
        Fontes.append(EXEMPLO_OLA)
    return Fontes


def ListarTarefas(Fontes: list, Modos: list) -> list:
    """Uma tarefa por (fonte, modo), na ordem das fontes e dentro de cada uma na ordem dos modos."""
    return [Tarefa(Fonte.relative_to(LANGUAGE.parent).as_posix(), Fonte, Modo) for Fonte in Fontes for Modo in Modos]


def FlagRelease(Modo: str) -> list:
    return ["--release"] if Modo == "release" else []


def ComandoOraculo(Forja: Path, Tarefa_: Tarefa, Arquivo: Path) -> list:
    return [str(Forja), "build", "--backend", "x64", *FlagRelease(Tarefa_.Modo), str(Tarefa_.Fonte), "-o", str(Arquivo)]


def ComandoCandidato(Candidato: Path, Tarefa_: Tarefa, Arquivo: Path) -> list:
    return [str(Candidato), "build", "--target", ALVO_WASM, *FlagRelease(Tarefa_.Modo), str(Tarefa_.Fonte), "-o", str(Arquivo)]


def ComandoNode(Wasm: Path) -> list:
    return ["node", "--no-warnings", str(RODAR_WASM), str(Wasm)]


def Executar(Comando: list, Trabalho: Path, Tempo: int) -> tuple:
    """(erro ou None, Saida): o erro diz que o processo nao abriu ou passou do Tempo."""
    try:
        Processo = subprocess.run(Comando, capture_output=True, timeout=Tempo, stdin=subprocess.DEVNULL, cwd=Trabalho)
    except subprocess.TimeoutExpired:
        return f"passou de {Tempo} s", None
    except OSError as Erro:
        return f"nao abriu: {Erro}", None
    return None, Saida(Processo.stdout, Processo.stderr, Processo.returncode)


def Compilar(Comando: list, Arquivo: Path, Tempo: int) -> str | None:
    """None se o build saiu 0 e gerou o arquivo; senao o texto do erro (inicio da saida do forja)."""
    Erro, Processo = Executar(Comando, LANGUAGE, Tempo)
    if Erro:
        return f"build {Erro}"
    if Processo.Exit != 0 or not Arquivo.is_file():
        Texto = (Processo.Stdout + Processo.Stderr).decode("utf-8", "replace").replace("\n", " | ")
        return f"build exit {Processo.Exit}: {Texto[:400]}"
    return None


def CompararSaidas(Oraculo: Saida, Candidato_: Saida) -> list:
    """Motivos de divergencia (lista vazia = aprovado): stdout com CRLF -> LF, stderr pelo NormalizarLinhasR, exit."""
    Motivos = []
    StdoutOraculo = Oraculo.Stdout.replace(CRLF, LF)
    StdoutCandidato = Candidato_.Stdout.replace(CRLF, LF)
    if StdoutOraculo != StdoutCandidato:
        Motivos.append(f"stdout difere: oraculo {StdoutOraculo[:80]!r} x candidato {StdoutCandidato[:80]!r}")
    StderrOraculo = NormalizarLinhasR(Oraculo.Stderr.replace(CRLF, LF))
    StderrCandidato = NormalizarLinhasR(Candidato_.Stderr.replace(CRLF, LF))
    if StderrOraculo != StderrCandidato:
        Motivos.append(f"stderr difere: oraculo {StderrOraculo[:120]!r} x candidato {StderrCandidato[:120]!r}")
    if Oraculo.Exit != Candidato_.Exit:
        Motivos.append(f"exit {Oraculo.Exit} (oraculo) x {Candidato_.Exit} (candidato)")
    return Motivos


def SemEnriquecimentoDoDriver(Stderr: bytes) -> bytes:
    """O `forja run` acrescenta explicacao/forma_certa/spec ao R-code; o runtime wasm emite so codigo, tipo e causa: filtra as linhas JSON do oraculo."""
    Linhas = []
    for Linha in Stderr.decode("utf-8", "replace").splitlines():
        try:
            Dados = json.loads(Linha)
        except ValueError:
            Linhas.append(Linha)
            continue
        Linhas.append(json.dumps({Chave: Dados[Chave] for Chave in ("codigo", "tipo", "causa") if Chave in Dados}, ensure_ascii=False))
    return ("\n".join(Linhas) + "\n").encode("utf-8")


def MontarRegistro(Tarefa_: Tarefa, Status: str, Motivo: str) -> dict:
    return {"tarefa": Tarefa_.Nome, "modo": Tarefa_.Modo, "status": Status, "motivo": Motivo}


def ExecutarTarefa(Tarefa_: Tarefa, Indice: int, Pasta: Path, Forja: Path, Candidato: Path, Tempo: int) -> dict:
    """Compila o oraculo e o candidato, roda os dois e compara; cada tarefa tem a propria pasta de trabalho."""
    Trabalho = Pasta / f"t{Indice:03d}"
    Trabalho.mkdir(parents=True, exist_ok=True)
    Exe, Wasm = Trabalho / "oraculo.exe", Trabalho / "candidato.wasm"
    PorRun = Tarefa_.Fonte.name.startswith(PREFIXO_ORACULO_RUN)
    if not PorRun:
        Erro = Compilar(ComandoOraculo(Forja, Tarefa_, Exe), Exe, Tempo)
        if Erro:
            return MontarRegistro(Tarefa_, "falha de build", f"oraculo: {Erro}")
    Erro = Compilar(ComandoCandidato(Candidato, Tarefa_, Wasm), Wasm, Tempo)
    if Erro:
        return MontarRegistro(Tarefa_, "falha de build", f"candidato: {Erro}")
    ComandoOraculoRodar = [str(Candidato), "run", *FlagRelease(Tarefa_.Modo), str(Tarefa_.Fonte)] if PorRun else [str(Exe)]
    ErroOraculo, Oraculo = Executar(ComandoOraculoRodar, Trabalho, Tempo)
    if PorRun and Oraculo:
        Oraculo = Saida(Oraculo.Stdout, SemEnriquecimentoDoDriver(Oraculo.Stderr), Oraculo.Exit)
    ErroCandidato, Resultado = Executar(ComandoNode(Wasm), Trabalho, Tempo)
    if ErroOraculo or ErroCandidato:
        return MontarRegistro(Tarefa_, "falha de run", f"oraculo: {ErroOraculo or 'ok'}; candidato: {ErroCandidato or 'ok'}")
    Motivos = CompararSaidas(Oraculo, Resultado)
    return MontarRegistro(Tarefa_, "diverge" if Motivos else "ok", "; ".join(Motivos))


def Paralelos() -> int:
    return min(8, os.cpu_count() or 4)


def RodarTarefas(Tarefas: list, Forja: Path, Candidato: Path, Jobs: int, Tempo: int) -> list:
    with tempfile.TemporaryDirectory(prefix="forja_wasm_") as Pasta:
        with ThreadPoolExecutor(max_workers=Jobs or Paralelos()) as Piscina:
            return list(Piscina.map(lambda Item: ExecutarTarefa(Item[1], Item[0], Path(Pasta), Forja, Candidato, Tempo), enumerate(Tarefas)))


def ImprimirLista(Tarefas: list) -> None:
    for Tarefa_ in Tarefas:
        print(f"{Tarefa_.Nome} [{Tarefa_.Modo}]")
    print(f"tarefas: {len(Tarefas)} ({len({T.Fonte for T in Tarefas})} fontes)")


def ImprimirRegistros(Registros: list) -> None:
    for Registro in Registros:
        print(f"{Registro['status']:<15} {Registro['tarefa']} [{Registro['modo']}]")
        if Registro["motivo"]:
            print(f"    {Registro['motivo'][:300]}")


def Resumir(Registros: list) -> int:
    Total = len(Registros)
    if not Total:
        print("verificar_wasm: nenhuma tarefa (confira --filtro e --modos)")
        return 1
    Contagens = {Status: sum(1 for R in Registros if R["status"] == Status) for Status in STATUS_VALIDOS}
    print(f"verificar_wasm: {Contagens['ok']}/{Total} tarefas ok (diverge {Contagens['diverge']}, "
          f"falha de build {Contagens['falha de build']}, falha de run {Contagens['falha de run']})")
    return 0 if Contagens["ok"] == Total else 1


def ModuloDeSaida(Codigo: int) -> bytes:
    """Modulo WASI minimo: _start chama proc_exit(Codigo). Os bytes sao validados por WebAssembly.validate no selftest."""
    return b"".join([
        b"\x00asm\x01\x00\x00\x00",  # magic + versao 1
        b"\x01\x08\x02\x60\x01\x7f\x00\x60\x00\x00",  # tipos: 0 = (i32)->() e 1 = ()->()
        b"\x02\x24\x01", BytesDoNome("wasi_snapshot_preview1"), BytesDoNome("proc_exit"), b"\x00\x00",  # import proc_exit, tipo 0
        b"\x03\x02\x01\x01",  # funcao _start, tipo 1
        b"\x05\x03\x01\x00\x01",  # memoria: 1 pagina
        b"\x07\x13\x02", BytesDoNome("memory"), b"\x02\x00", BytesDoNome("_start"), b"\x00\x01",  # exports: memory e _start (funcao 1)
        b"\x0a\x08\x01\x06\x00\x41", bytes([Codigo]), b"\x10\x00\x0b",  # corpo: i32.const Codigo; call 0; end
    ])


def BytesDoNome(Nome: str) -> bytes:
    """Nome no formato WASM: tamanho em 1 byte + ASCII (so nomes curtos)."""
    Dado = Nome.encode("ascii")
    return bytes([len(Dado)]) + Dado


def RodarModulosDeTeste(Pasta: Path) -> tuple:
    """(falhas, {nome: Saida ou None}): roda o rodar_wasm.mjs sobre o modulo A (sai 7), o B (sai 0) e um arquivo de lixo (INVALIDO, exit 4)."""
    Falhas, Rodadas = [], {}
    for Nome, Dados in {"a": ModuloDeSaida(7), "b": ModuloDeSaida(0), "lixo": os.urandom(64)}.items():
        Arquivo = Pasta / f"modulo_{Nome}.wasm"
        Arquivo.write_bytes(Dados)
        Erro, Rodadas[Nome] = Executar(ComandoNode(Arquivo), Pasta, TEMPO_SELFTEST)
        if Erro:
            Falhas.append(f"modulo {Nome}: {Erro}")
    return Falhas, Rodadas


def ConferirModulosDeTeste(Rodadas: dict) -> list:
    Falhas = []
    for Nome, Codigo in (("a", 7), ("b", 0), ("lixo", 4)):
        Rodada = Rodadas[Nome]
        if Rodada is not None and Rodada.Exit != Codigo:
            Falhas.append(f"modulo {Nome} saiu {Rodada.Exit}, esperado {Codigo}")
    Lixo = Rodadas["lixo"]
    if Lixo is not None and not Lixo.Stderr.startswith(b"INVALIDO"):
        Falhas.append("lixo sem INVALIDO no stderr")
    return Falhas


def ConferirComparacao(Rodadas: dict) -> list:
    """Oraculo = modulo B (exit 0 real): o candidato A (exit 7) tem de REPROVAR e o B tem de APROVAR."""
    Oraculo, CandidatoA, CandidatoB = Rodadas["b"], Rodadas["a"], Rodadas["b"]
    if Oraculo is None or CandidatoA is None:
        return []  # a falha de execucao ja foi registrada por RodarModulosDeTeste
    Falhas = []
    if not CompararSaidas(Oraculo, CandidatoA):
        Falhas.append("comparacao APROVOU A (exit 7) contra oraculo exit 0")
    if CompararSaidas(Oraculo, CandidatoB):
        Falhas.append("comparacao REPROVOU B contra oraculo exit 0")
    return Falhas


def Selftest() -> int:
    """Sem compilador: o runner sobre modulos embutidos e a comparacao (A reprova, B aprova)."""
    with tempfile.TemporaryDirectory(prefix="forja_wasm_selftest_") as Pasta:
        Falhas, Rodadas = RodarModulosDeTeste(Path(Pasta))
        Falhas += ConferirModulosDeTeste(Rodadas) + ConferirComparacao(Rodadas)
    print("verificar_wasm selftest:", "VERDE" if not Falhas else f"FALHOU {Falhas}")
    return 1 if Falhas else 0


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Verifica o alvo wasm32-wasip1 contra o oraculo x64 nativo (rodar_wasm.mjs).")
    Analisador.add_argument("--forja", default=str(FORJA_PADRAO), help="oraculo: forja.exe, compila com --backend x64 (padrao: language\\compiler\\forja.exe)")
    Analisador.add_argument("--candidato", default=str(CANDIDATO_PADRAO), help="candidato: forja com --target wasm32-wasip1 (padrao: language\\compiler\\forja_wasm.exe)")
    Analisador.add_argument("--lista", action="store_true", help="so lista as tarefas e sai 0")
    Analisador.add_argument("--modos", default="dev,release", help="dev, release ou dev,release")
    Analisador.add_argument("--filtro", default="", help="so as tarefas cujo nome contem este texto")
    Analisador.add_argument("--jobs", type=int, default=0, help="tarefas em paralelo (0 = min(8, nucleos))")
    Analisador.add_argument("--timeout-seg", type=int, default=TEMPO_PADRAO, help="limite de cada build ou execucao, em segundos")
    Analisador.add_argument("--selftest", action="store_true", help="sem compilador: roda o runner sobre modulos embutidos")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        return Selftest()
    Modos = Argumentos.modos.split(",")
    if not set(Modos) <= set(MODOS_VALIDOS):
        Analisador.error("--modos aceita dev e release")
    Tarefas = [T for T in ListarTarefas(CorpusPadrao(), Modos) if Argumentos.filtro in T.Nome]
    if Argumentos.lista:
        ImprimirLista(Tarefas)
        return 0
    Registros = RodarTarefas(Tarefas, Path(Argumentos.forja).resolve(), Path(Argumentos.candidato).resolve(), Argumentos.jobs, Argumentos.timeout_seg)
    ImprimirRegistros(Registros)
    return Resumir(Registros)


if __name__ == "__main__":
    sys.exit(Principal())
