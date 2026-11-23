"""Roda a suite dourada contra um compilador Forja e diz o que passou.

Versao: 1.0.0
Changelog: 1.0.0 - suite ok_/err_ em paralelo, tabela, registro JSONL e --selftest.
Verificacao: python rodar_suite.py --selftest
"""

import argparse
import difflib
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from datetime import datetime
from functools import partial
from pathlib import Path

VERSAO = "1.0.0"
TIMEOUT_COMPILADOR = 30
TIMEOUT_EXE = 10
MAX_LINHAS_DETALHE = 3
GCC_FLAGS = [
    "-std=c11", "-Wall", "-Wextra", "-Werror",
    "-Wno-unused-variable", "-Wno-unused-parameter",
    "-Wno-unused-function", "-Wno-unused-but-set-variable",
]
FALSO_COMPILADOR = "import sys\nsys.exit(3)\n"
CORPUS_PADRAO = Path(__file__).resolve().parents[1] / "corpus" / "dourado"


@dataclass
class CasoDourado:
    Nome: str
    Tipo: str  # "ok" ou "err", pelo prefixo do nome
    Fj: Path
    Esperado: Path


@dataclass
class Ambiente:
    Compilador: list
    Saida: Path


@dataclass
class Execucao:
    Codigo: int | None  # None quando estourou o timeout
    Stdout: str
    Stderr: str


@dataclass
class Falha:
    Motivo: str
    Detalhe: list


@dataclass
class Resultado:
    Nome: str
    Passou: bool
    Motivo: str
    Detalhe: list
    Segundos: float


@dataclass
class Configuracao:
    Compilador: str
    Corpus: Path
    Saida: Path
    Registro: Path | None
    So: str | None
    Jobs: int


def SepararComando(Texto: str) -> list:
    # posix=False: a barra invertida do Windows nao vira escape; as aspas saem logo depois
    return [Parte.strip("\"'") for Parte in shlex.split(Texto, posix=False)]


def TresPrimeiras(Texto: str) -> list:
    return [Linha for Linha in Texto.splitlines() if Linha.strip()][:MAX_LINHAS_DETALHE]


def Normalizar(Texto: str) -> str:
    # esperado.txt e stdout do .exe podem vir com CRLF; a comparacao e feita em LF nos dois lados
    return Texto.replace("\r\n", "\n")


def DiffLinhas(Esperado: list, Recebido: list) -> list:
    Mudancas = difflib.ndiff([str(Item) for Item in Esperado], [str(Item) for Item in Recebido])
    return [Linha for Linha in Mudancas if Linha[:1] in ("+", "-")][:MAX_LINHAS_DETALHE]


def RodarProcesso(Argumentos: list, Limite: int) -> Execucao:
    try:
        Proc = subprocess.run(Argumentos, capture_output=True, timeout=Limite)
    except subprocess.TimeoutExpired:
        return Execucao(None, "", "")
    except OSError as Erro:
        return Execucao(-1, "", f"nao consegui iniciar {Argumentos[0]!r}: {Erro}")
    return Execucao(
        Proc.returncode,
        Proc.stdout.decode("utf-8", errors="replace"),
        Proc.stderr.decode("utf-8", errors="replace"),
    )


def RodarCompilador(Compilador: list, Argumentos: list) -> Execucao:
    return RodarProcesso(Compilador + Argumentos, TIMEOUT_COMPILADOR)


def FalhaCodigo(Etapa: str, Rodada: Execucao, Esperado: int) -> Falha | None:
    if Rodada.Codigo is None:
        return Falha(f"{Etapa}_timeout", [f"{Etapa} passou de {TIMEOUT_COMPILADOR} s"])
    if Rodada.Codigo != Esperado:
        Detalhe = TresPrimeiras(Rodada.Stderr) or TresPrimeiras(Rodada.Stdout)
        Motivo = f"{Etapa}_exit_{Rodada.Codigo}"
        return Falha(Motivo, Detalhe or [f"{Etapa} saiu {Rodada.Codigo}, esperado {Esperado}"])
    return None


def ChecarCheckOk(Caso: CasoDourado, Contexto: Ambiente) -> Falha | None:
    Rodada = RodarCompilador(Contexto.Compilador, ["check", str(Caso.Fj)])
    Erro = FalhaCodigo("check", Rodada, 0)
    if Erro is None and Rodada.Stdout:
        Erro = Falha("stdout_nao_vazio", TresPrimeiras(Rodada.Stdout))
    return Erro


def BuildarC(Caso: CasoDourado, Contexto: Ambiente, Esperado: int) -> Falha | None:
    ArquivoC = Contexto.Saida / (Caso.Nome + ".c")
    ArquivoC.unlink(missing_ok=True)  # sobra de rodada anterior nao pode mascarar o resultado
    Argumentos = ["build", "--modo", "c", str(Caso.Fj), "-o", str(ArquivoC)]
    Rodada = RodarCompilador(Contexto.Compilador, Argumentos)
    Erro = FalhaCodigo("build", Rodada, Esperado)
    if Erro is None and Esperado == 0 and not ArquivoC.exists():
        Erro = Falha("build_sem_c", [f"build saiu 0 mas nao gravou {ArquivoC}"])
    if Erro is None and Esperado == 1 and ArquivoC.exists():
        Erro = Falha("build_criou_c", [f"build com diagnostico gravou {ArquivoC}"])
    return Erro


def CompilarExe(Caso: CasoDourado, Contexto: Ambiente) -> Falha | None:
    ArquivoC = Contexto.Saida / (Caso.Nome + ".c")
    ArquivoExe = Contexto.Saida / (Caso.Nome + ".exe")
    Rodada = RodarProcesso(["gcc", *GCC_FLAGS, str(ArquivoC), "-o", str(ArquivoExe)], TIMEOUT_COMPILADOR)
    if Rodada.Codigo != 0:
        Detalhe = TresPrimeiras(Rodada.Stderr) or [f"gcc sem saida util, codigo {Rodada.Codigo}"]
        return Falha("gcc_falhou", Detalhe)
    return None


def RodarExe(Caso: CasoDourado, Contexto: Ambiente) -> Falha | None:
    ArquivoExe = Contexto.Saida / (Caso.Nome + ".exe")
    Rodada = RodarProcesso([str(ArquivoExe)], TIMEOUT_EXE)
    if Rodada.Codigo is None:
        return Falha("exe_timeout", [f"exe passou de {TIMEOUT_EXE} s"])
    if Rodada.Codigo != 0:
        Detalhe = TresPrimeiras(Rodada.Stderr) or [f"exe saiu {Rodada.Codigo}, esperado 0"]
        return Falha(f"exe_exit_{Rodada.Codigo}", Detalhe)
    Esperado = Normalizar(Caso.Esperado.read_text(encoding="utf-8"))
    Obtido = Normalizar(Rodada.Stdout)
    if Obtido != Esperado:
        return Falha("saida_diferente", DiffLinhas(Esperado.splitlines(), Obtido.splitlines()))
    return None


def ChaveDiagnostico(Obj: dict) -> tuple:
    # o contrato compara so 4 campos; arquivo entra como basename, com / no lugar de \
    Arquivo = str(Obj.get("arquivo", "")).replace("\\", "/").rsplit("/", 1)[-1]
    return (Obj.get("codigo"), Obj.get("linha"), Obj.get("coluna"), Arquivo)


def ExtrairDiagnosticos(Texto: str) -> list:
    # contrato: 1 objeto JSON por linha, UTF-8, LF
    Chaves = []
    for Linha in Texto.splitlines():
        if not Linha.strip():
            continue
        try:
            Obj = json.loads(Linha)
        except json.JSONDecodeError as Erro:
            raise ValueError(f"linha nao e JSON: {Linha[:120]!r}") from Erro
        if not isinstance(Obj, dict):
            raise ValueError(f"linha de diagnostico nao e objeto JSON: {Linha[:120]!r}")
        Chaves.append(ChaveDiagnostico(Obj))
    return Chaves


def ChecarDiagnosticos(Caso: CasoDourado, Stdout: str) -> Falha | None:
    try:
        Recebidos = ExtrairDiagnosticos(Stdout)
    except ValueError as Erro:
        return Falha("stdout_nao_json", [str(Erro)])
    Esperados = ExtrairDiagnosticos(Caso.Esperado.read_text(encoding="utf-8"))
    if Recebidos != Esperados:
        return Falha("diagnostico_diferente", DiffLinhas(Esperados, Recebidos))
    return None


def BuildarOk(Caso: CasoDourado, Contexto: Ambiente) -> Falha | None:
    Erro = BuildarC(Caso, Contexto, 0)
    if Erro is None:
        Erro = CompilarExe(Caso, Contexto)
    return Erro


def VerificarOk(Caso: CasoDourado, Contexto: Ambiente) -> Falha | None:
    Erro = ChecarCheckOk(Caso, Contexto)
    if Erro is None:
        Erro = BuildarOk(Caso, Contexto)
    if Erro is None:
        Erro = RodarExe(Caso, Contexto)
    return Erro


def VerificarErro(Caso: CasoDourado, Contexto: Ambiente) -> Falha | None:
    Rodada = RodarCompilador(Contexto.Compilador, ["check", str(Caso.Fj)])
    Erro = FalhaCodigo("check", Rodada, 1)
    if Erro is None:
        Erro = ChecarDiagnosticos(Caso, Rodada.Stdout)
    if Erro is None:
        Erro = BuildarC(Caso, Contexto, 1)
    return Erro


def VerificarCaso(Caso: CasoDourado, Contexto: Ambiente) -> Falha | None:
    if not Caso.Esperado.is_file():
        return Falha("esperado_ausente", [f"falta o arquivo {Caso.Esperado}"])
    if Caso.Tipo == "ok":
        return VerificarOk(Caso, Contexto)
    return VerificarErro(Caso, Contexto)


def RodarCaso(Caso: CasoDourado, Contexto: Ambiente) -> Resultado:
    Inicio = time.perf_counter()
    try:
        Erro = VerificarCaso(Caso, Contexto)
    except Exception as Excecao:
        # a suite nao cai por causa de um caso: a excecao vira falha classificada
        Erro = Falha(f"excecao_{type(Excecao).__name__}", [str(Excecao)[:200]])
    Segundos = time.perf_counter() - Inicio
    if Erro is None:
        return Resultado(Caso.Nome, True, "", [], Segundos)
    return Resultado(Caso.Nome, False, Erro.Motivo, Erro.Detalhe, Segundos)


def RodarSuite(Conf: Configuracao, Casos: list) -> list:
    Conf.Saida.mkdir(parents=True, exist_ok=True)
    Contexto = Ambiente(SepararComando(Conf.Compilador), Conf.Saida)
    with ThreadPoolExecutor(max_workers=Conf.Jobs) as Piscina:
        return list(Piscina.map(partial(RodarCaso, Contexto=Contexto), Casos))


def ListarCasos(Pasta: Path, Trecho: str | None) -> list:
    Casos = []
    for Fj in sorted(Pasta.glob("*.fj")):
        Nome = Fj.stem
        if Trecho and Trecho not in Nome:
            continue
        if Nome.startswith("ok_"):
            Casos.append(CasoDourado(Nome, "ok", Fj, Pasta / (Nome + ".esperado.txt")))
        elif Nome.startswith("err_"):
            Casos.append(CasoDourado(Nome, "err", Fj, Pasta / (Nome + ".esperado.jsonl")))
    return Casos


def ImprimirTabela(Resultados: list) -> None:
    Largura = max((len(Item.Nome) for Item in Resultados), default=4)
    print(f"{'CASO':<{Largura}}  {'RESULTADO':<9}  {'MOTIVO':<24}  TEMPO")
    for Item in Resultados:
        Situacao = "PASSOU" if Item.Passou else "FALHOU"
        print(f"{Item.Nome:<{Largura}}  {Situacao:<9}  {Item.Motivo:<24}  {Item.Segundos:6.2f}s")
        for Linha in Item.Detalhe:
            print(f"    > {Linha}")
    print(f"PASSOU {sum(Item.Passou for Item in Resultados)}/{len(Resultados)}")


def CodigoFinal(Resultados: list) -> int:
    return 0 if all(Item.Passou for Item in Resultados) else 1


def GravarRegistro(Caminho: Path, Comando: str, Resultados: list) -> None:
    Falhas = [{"nome": Item.Nome, "motivo": Item.Motivo} for Item in Resultados if not Item.Passou]
    Linha = {
        "data_hora": datetime.now().astimezone().isoformat(timespec="seconds"),
        "comando": Comando,
        "total": len(Resultados),
        "passou": len(Resultados) - len(Falhas),
        "falharam": Falhas,
    }
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    with Caminho.open("a", encoding="utf-8", newline="\n") as Arquivo:
        Arquivo.write(json.dumps(Linha, ensure_ascii=False) + "\n")


def ValidarCompilador(Texto: str) -> str | None:
    try:
        Partes = SepararComando(Texto)
    except ValueError as Erro:
        return f"--compilador mal formado ({Erro}): {Texto!r}"
    if not Partes:
        return "--compilador vazio"
    if shutil.which(Partes[0]) is None and not Path(Partes[0]).is_file():
        return f"compilador nao encontrado: {Partes[0]!r}"
    return None


def ValidarEntrada(Args: argparse.Namespace) -> str | None:
    if not Args.compilador:
        return "falta --compilador"
    if not Args.corpus:
        return "falta --corpus"
    if not Path(Args.corpus).is_dir():
        return f"pasta do corpus nao existe: {Args.corpus!r}"
    if Args.saida and Path(Args.saida).is_file():
        return f"--saida aponta para um arquivo, nao para pasta: {Args.saida!r}"
    if Args.jobs < 1:
        return f"--jobs precisa ser >= 1, recebido {Args.jobs}"
    return ValidarCompilador(Args.compilador)


def MontarConfiguracao(Args: argparse.Namespace) -> Configuracao:
    Saida = Path(Args.saida).resolve() if Args.saida else Path(tempfile.mkdtemp(prefix="rodar_suite_"))
    Registro = Path(Args.registro) if Args.registro else None
    return Configuracao(Args.compilador, Path(Args.corpus).resolve(), Saida, Registro, Args.so, Args.jobs)


def MontarParser() -> argparse.ArgumentParser:
    Parser = argparse.ArgumentParser(description="Roda a suite dourada contra um compilador Forja.")
    Parser.add_argument("--version", action="version", version=VERSAO)
    Parser.add_argument("--compilador", help='prefixo do comando, ex.: "python C:/x/forja.py"')
    Parser.add_argument("--corpus", help="pasta com os casos ok_*/err_*")
    Parser.add_argument("--saida", help="pasta de trabalho (padrao: pasta temporaria nova)")
    Parser.add_argument("--registro", help="arquivo .jsonl que recebe uma linha por execucao")
    Parser.add_argument("--so", help="roda so os casos cujo nome contem este trecho")
    Parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1, help="casos em paralelo (padrao: nucleos)")
    Parser.add_argument("--selftest", action="store_true", help="confere a propria suite com um compilador falso")
    return Parser


def ConferirSelftest(Resultados: list, Codigo: int, Registro: Path) -> list:
    Problemas = []
    if not Resultados:
        Problemas.append("a suite nao rodou nenhum caso")
    if any(Item.Motivo.startswith("excecao_") for Item in Resultados):
        Problemas.append("houve excecao dentro da suite")
    if any(Item.Passou or not Item.Motivo for Item in Resultados):
        Problemas.append("nem todo caso falhou com motivo")
    if Codigo != 1:
        Problemas.append(f"exit da suite = {Codigo}, esperado 1")
    Linhas = Registro.read_text(encoding="utf-8").splitlines()
    if len(Linhas) != 1:
        Problemas.append(f"registro tem {len(Linhas)} linhas, esperado 1")
    return Problemas


def RodarSelftest() -> list:
    Corpus = CORPUS_PADRAO
    if not Corpus.is_dir():
        return [f"corpus dourado nao existe: {Corpus}"]
    with tempfile.TemporaryDirectory(prefix="rodar_suite_selftest_") as Pasta:
        Raiz = Path(Pasta)
        Script = Raiz / "compilador_falso.py"
        Script.write_text(FALSO_COMPILADOR, encoding="utf-8")
        Conf = Configuracao(
            f'"{sys.executable}" "{Script}"', Corpus, Raiz / "saida",
            Raiz / "registro.jsonl", None, os.cpu_count() or 1,
        )
        Resultados = RodarSuite(Conf, ListarCasos(Corpus, None))
        GravarRegistro(Conf.Registro, Conf.Compilador, Resultados)
        return ConferirSelftest(Resultados, CodigoFinal(Resultados), Conf.Registro)


def Selftest() -> int:
    try:
        Problemas = RodarSelftest()
    except (OSError, ValueError) as Erro:
        Problemas = [f"erro ao rodar o selftest: {Erro}"]
    if Problemas:
        print("SELFTEST FALHOU: " + "; ".join(Problemas))
        return 1
    print("SELFTEST OK")
    return 0


def Main(Argumentos: list | None = None) -> int:
    sys.stdout.reconfigure(errors="replace")
    Args = MontarParser().parse_args(Argumentos)
    if Args.selftest:
        return Selftest()
    Erro = ValidarEntrada(Args)
    if Erro:
        print(f"ERRO de entrada: {Erro}", file=sys.stderr)
        return 2
    Conf = MontarConfiguracao(Args)
    Casos = ListarCasos(Conf.Corpus, Conf.So)
    if not Casos:
        print(f"ERRO de entrada: nenhum caso ok_*/err_* em {str(Conf.Corpus)!r} com --so={Conf.So!r}", file=sys.stderr)
        return 2
    try:
        Resultados = RodarSuite(Conf, Casos)
        ImprimirTabela(Resultados)
        if Conf.Registro:
            GravarRegistro(Conf.Registro, Conf.Compilador, Resultados)
    except OSError as Erro:
        print(f"ERRO de entrada/saida: {Erro}", file=sys.stderr)
        return 2
    return CodigoFinal(Resultados)


if __name__ == "__main__":
    sys.exit(Main())
