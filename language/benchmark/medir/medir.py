# medir.py
# Versão: 1.0.0
# Changelog: 1.0.0 — primeira versão: medição serial intercalada, pico de memória por Job Object, tabela e JSON.
# Verificação: python medir.py --selftest

import argparse
import ctypes
import datetime
import json
import statistics
import subprocess
import sys
import tempfile
import time
from ctypes import wintypes
from dataclasses import dataclass
from pathlib import Path

RAIZ = Path("C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark")
PASTA_RESULTADOS = RAIZ / "resultados"
GERADOR = RAIZ / "corpus" / "gerador" / "gerar_corpus.py"
TAMANHOS = (20000, 200000)
SEMENTE = 20261107
CREATE_SUSPENDED = 0x00000004
CREATE_NO_WINDOW = 0x08000000
JOB_INFO_EXTENDIDO = 9  # JobObjectExtendedLimitInformation
MB = 1024 * 1024
TEMPOS_CAMPOS = ("lex_ns", "parse_ns", "check_ns", "emit_ns", "total_ns", "linhas")
CANDIDATOS = {
    "c": ("C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/c/forja.exe",),
    "cpp": ("C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/cpp/forja.exe",),
    "py": (sys.executable, "C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/forja.py"),
}
KERNEL = ctypes.WinDLL("kernel32", use_last_error=True)
NTDLL = ctypes.WinDLL("ntdll", use_last_error=True)


class ErroMedicao(RuntimeError):
    """Falha da medição; a mensagem diz o que falhou e o valor recebido."""


class JobBasicLimitInfo(ctypes.Structure):
    _fields_ = [
        ("PerProcessUserTimeLimit", ctypes.c_longlong),
        ("PerJobUserTimeLimit", ctypes.c_longlong),
        ("LimitFlags", wintypes.DWORD),
        ("MinimumWorkingSetSize", ctypes.c_size_t),
        ("MaximumWorkingSetSize", ctypes.c_size_t),
        ("ActiveProcessLimit", wintypes.DWORD),
        ("Affinity", ctypes.c_size_t),
        ("PriorityClass", wintypes.DWORD),
        ("SchedulingClass", wintypes.DWORD),
    ]


class JobIoCounters(ctypes.Structure):
    _fields_ = [
        ("ReadOperationCount", ctypes.c_ulonglong),
        ("WriteOperationCount", ctypes.c_ulonglong),
        ("OtherOperationCount", ctypes.c_ulonglong),
        ("ReadTransferCount", ctypes.c_ulonglong),
        ("WriteTransferCount", ctypes.c_ulonglong),
        ("OtherTransferCount", ctypes.c_ulonglong),
    ]


class JobExtendedLimitInfo(ctypes.Structure):
    _fields_ = [
        ("BasicLimitInformation", JobBasicLimitInfo),
        ("IoInfo", JobIoCounters),
        ("ProcessMemoryLimit", ctypes.c_size_t),
        ("JobMemoryLimit", ctypes.c_size_t),
        ("PeakProcessMemoryUsed", ctypes.c_size_t),
        ("PeakJobMemoryUsed", ctypes.c_size_t),
    ]


@dataclass(frozen=True)
class Plano:
    """Entradas da medição: pasta dos corpora, candidatos, tamanhos e pasta de trabalho."""
    CorpusDir: Path
    Candidatos: tuple
    Tamanhos: tuple
    Pasta: Path


@dataclass(frozen=True)
class Comando:
    """Um comando a medir; ArquivoTempos só existe no build (--tempos)."""
    Nome: str
    Argv: list
    ArquivoTempos: Path | None


@dataclass(frozen=True)
class Execucao:
    """Resultado bruto de um processo: parede em ns, código de saída, pico em bytes e saída de texto."""
    ParedeNs: int
    CodigoSaida: int
    PicoBytes: int
    Saida: str


def ConfigurarWinApi() -> None:
    """Declara argtypes e restype: sem isso os handles de 64 bits são truncados."""
    KERNEL.CreateJobObjectW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p]
    KERNEL.CreateJobObjectW.restype = ctypes.c_void_p
    KERNEL.AssignProcessToJobObject.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    KERNEL.AssignProcessToJobObject.restype = wintypes.BOOL
    KERNEL.QueryInformationJobObject.argtypes = [
        ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
    ]
    KERNEL.QueryInformationJobObject.restype = wintypes.BOOL
    KERNEL.CloseHandle.argtypes = [ctypes.c_void_p]
    KERNEL.CloseHandle.restype = wintypes.BOOL
    NTDLL.NtResumeProcess.argtypes = [ctypes.c_void_p]
    NTDLL.NtResumeProcess.restype = ctypes.c_long


def CriarJob() -> int:
    """Cria um Job Object anônimo; o pico de memória é lido dele depois da execução."""
    Job = KERNEL.CreateJobObjectW(None, None)
    if not Job:
        raise ctypes.WinError(ctypes.get_last_error())
    return Job


def FecharHandle(Handle: int) -> None:
    """Fecha um handle do Windows."""
    KERNEL.CloseHandle(Handle)


def AtribuirERetomar(Job: int, Proc: subprocess.Popen) -> None:
    """Põe o processo suspenso no job e só então o retoma; se falhar, mata o processo."""
    try:
        if not KERNEL.AssignProcessToJobObject(Job, Proc._handle):
            raise ctypes.WinError(ctypes.get_last_error())
        Status = NTDLL.NtResumeProcess(Proc._handle)
        if Status != 0:
            raise OSError(f"NtResumeProcess retornou NTSTATUS {Status & 0xFFFFFFFF:#010x} no pid {Proc.pid}")
    except OSError:
        Proc.kill()
        Proc.communicate()
        raise


def LerPicoDoJob(Job: int) -> int:
    """Pico de memória do job em bytes: o maior entre o pico do job e o pico do processo."""
    Info = JobExtendedLimitInfo()
    Retornado = wintypes.DWORD(0)
    Ok = KERNEL.QueryInformationJobObject(
        Job, JOB_INFO_EXTENDIDO, ctypes.addressof(Info), ctypes.sizeof(Info), ctypes.byref(Retornado),
    )
    if not Ok:
        raise ctypes.WinError(ctypes.get_last_error())
    return max(Info.PeakJobMemoryUsed, Info.PeakProcessMemoryUsed)


def ExecutarComJob(Argv: list) -> Execucao:
    """Roda Argv serial dentro de um Job Object (suspenso até entrar no job) e mede parede e pico."""
    Job = CriarJob()
    try:
        Inicio = time.perf_counter_ns()
        Proc = subprocess.Popen(
            Argv,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            creationflags=CREATE_SUSPENDED | CREATE_NO_WINDOW,
        )
        AtribuirERetomar(Job, Proc)
        SaidaPadrao, SaidaErro = Proc.communicate()
        Fim = time.perf_counter_ns()
        Pico = LerPicoDoJob(Job)
    finally:
        FecharHandle(Job)
    Texto = (SaidaPadrao + SaidaErro).decode("utf-8", errors="replace")
    return Execucao(Fim - Inicio, Proc.returncode, Pico, Texto)


def OrdemDaRodada(Indice: int, Candidatos: tuple) -> tuple:
    """Rotaciona a ordem dos candidatos por rodada: 0 → c,cpp,py; 1 → cpp,py,c; 2 → py,c,cpp."""
    Passo = Indice % len(Candidatos)
    return Candidatos[Passo:] + Candidatos[:Passo]


def MontarComandos(Chave: str, Entrada: Path, Pasta: Path) -> list:
    """Devolve os comandos check e build do candidato para um corpus, na ordem de execução."""
    Prefixo = list(CANDIDATOS[Chave])
    Tempos = Pasta / "t.json"
    Saida = Pasta / "saida.c"
    return [
        Comando("check", Prefixo + ["check", str(Entrada)], None),
        Comando(
            "build",
            Prefixo + ["build", "--modo", "c", str(Entrada), "-o", str(Saida), "--tempos", str(Tempos)],
            Tempos,
        ),
    ]


def LerTempos(Caminho: Path) -> dict:
    """Lê o JSON de --tempos do build; falha se o arquivo não saiu ou se faltar campo."""
    if not Caminho.is_file():
        raise ErroMedicao(f"build saiu com 0 mas não gravou --tempos em {Caminho}")
    Dados = json.loads(Caminho.read_text(encoding="utf-8"))
    Faltando = [Campo for Campo in TEMPOS_CAMPOS if Campo not in Dados]
    if Faltando:
        raise ErroMedicao(f"JSON de tempos sem os campos {Faltando}; valor recebido: {Dados}")
    return Dados


def RodarComando(Cmd: Comando, Chave: str, Tamanho: int, Indice: int) -> dict:
    """Roda um comando medido e devolve a amostra; código de saída ≠ 0 interrompe a medição."""
    if Cmd.ArquivoTempos is not None and Cmd.ArquivoTempos.exists():
        Cmd.ArquivoTempos.unlink()
    Medida = ExecutarComJob(Cmd.Argv)
    if Medida.CodigoSaida != 0:
        raise ErroMedicao(
            f"candidato={Chave} corpus={Tamanho} comando={Cmd.Nome} saiu com código {Medida.CodigoSaida}; "
            f"comando: {' '.join(Cmd.Argv)}; saída: {Medida.Saida.strip()[:2000]!r}"
        )
    Amostra = {
        "rodada": Indice,
        "candidato": Chave,
        "corpus": Tamanho,
        "comando": Cmd.Nome,
        "parede_ns": Medida.ParedeNs,
        "codigo_saida": Medida.CodigoSaida,
        "peak_mem_bytes": Medida.PicoBytes,
        "tempos": None,
    }
    if Cmd.ArquivoTempos is not None:
        Amostra["tempos"] = LerTempos(Cmd.ArquivoTempos)
    return Amostra


def RodarUmCandidato(PlanoMedicao: Plano, Chave: str, Indice: int) -> list:
    """Roda check e build de um candidato em cada corpus do plano; devolve as amostras."""
    Itens = []
    for Tamanho in PlanoMedicao.Tamanhos:
        Entrada = PlanoMedicao.CorpusDir / f"corpus_{Tamanho}.fj"
        for Cmd in MontarComandos(Chave, Entrada, PlanoMedicao.Pasta):
            Itens.append(RodarComando(Cmd, Chave, Tamanho, Indice))
    return Itens


def RodarMedicao(PlanoMedicao: Plano, Rodadas: int, Aquecimento: int) -> list:
    """Roda aquecimento (descartado) e rodadas medidas, com ordem rotacionada; serial, nunca em paralelo."""
    PlanoMedicao.Pasta.mkdir(parents=True, exist_ok=True)
    Total = Aquecimento + Rodadas
    Medidas = []
    for Indice in range(Total):
        Ordem = OrdemDaRodada(Indice, PlanoMedicao.Candidatos)
        Fase = "aquecimento" if Indice < Aquecimento else "medida"
        print(f"[{Indice + 1}/{Total}] {Fase} ordem={','.join(Ordem)}", file=sys.stderr)
        for Chave in Ordem:
            Itens = RodarUmCandidato(PlanoMedicao, Chave, Indice)
            if Indice >= Aquecimento:
                Medidas.extend(Itens)
    return Medidas


def EstatisticaSerie(Valores: list) -> dict:
    """Mediana, quartis 25/75 e IQR de uma série; com uma só medida, IQR é zero."""
    if len(Valores) < 2:
        Q1 = Q3 = Valores[0]
    else:
        Quartis = statistics.quantiles(Valores, n=4, method="inclusive")
        Q1, Q3 = Quartis[0], Quartis[2]
    return {"mediana": statistics.median(Valores), "q1": Q1, "q3": Q3, "iqr": Q3 - Q1}


def ValorTempo(Campo: str, Valor: float) -> float:
    """Converte ns para ms; 'linhas' é contagem e volta sem conversão."""
    return Valor / 1e6 if Campo.endswith("_ns") else float(Valor)


def NomeSaida(Campo: str) -> str:
    """Nome do campo no resumo: lex_ns vira lex_ms; linhas fica como está."""
    return Campo[:-3] + "_ms" if Campo.endswith("_ns") else Campo


def ResumirGrupo(Chave: tuple, Itens: list) -> dict:
    """Estatísticas de um grupo: parede e pico sempre; tempos internos só quando há build."""
    Candidato, Corpus, NomeComando = Chave
    Linha = {
        "candidato": Candidato,
        "corpus": Corpus,
        "comando": NomeComando,
        "n": len(Itens),
        "parede_ms": EstatisticaSerie([I["parede_ns"] / 1e6 for I in Itens]),
        "pico_mem_mb": EstatisticaSerie([I["peak_mem_bytes"] / MB for I in Itens]),
    }
    if Itens[0]["tempos"] is not None:
        Linha["tempos_ms"] = {
            NomeSaida(Campo): EstatisticaSerie([ValorTempo(Campo, I["tempos"][Campo]) for I in Itens])
            for Campo in TEMPOS_CAMPOS
        }
    return Linha


def ResumirAmostras(Amostras: list) -> list:
    """Agrupa por (candidato, corpus, comando) e calcula as estatísticas de cada grupo."""
    Grupos = {}
    for Amostra in Amostras:
        Chave = (Amostra["candidato"], Amostra["corpus"], Amostra["comando"])
        Grupos.setdefault(Chave, []).append(Amostra)
    return [ResumirGrupo(Chave, Grupos[Chave]) for Chave in sorted(Grupos)]


def MontarResultado(Amostras: list, Rodadas: int, Aquecimento: int) -> dict:
    """Monta o dicionário gravado em JSON: metadados, amostras brutas e resumo."""
    return {
        "data_hora": datetime.datetime.now().isoformat(timespec="seconds"),
        "rodadas": Rodadas,
        "aquecimento": Aquecimento,
        "amostras_brutas": Amostras,
        "resumo": ResumirAmostras(Amostras),
    }


def GravarResultado(Caminho: Path, Dados: dict) -> None:
    """Grava o JSON de resultado, criando a pasta se faltar."""
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    Caminho.write_text(json.dumps(Dados, ensure_ascii=False, indent=2), encoding="utf-8")


def PadraoSaida() -> Path:
    """Caminho padrão do JSON: resultados\\medicao_<AAAAMMDD_HHMMSS>.json."""
    Agora = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    return PASTA_RESULTADOS / f"medicao_{Agora}.json"


def ImprimirTabela(Resumo: list) -> None:
    """Uma linha por candidato/corpus/comando: mediana e IQR de parede (ms) e de pico (MB)."""
    print(f"{'candidato':<10}{'corpus':>8}  {'comando':<7}{'n':>4}  {'parede med ms':>14}  {'IQR ms':>9}"
          f"  {'pico med MB':>12}  {'IQR MB':>8}")
    for Linha in Resumo:
        Parede = Linha["parede_ms"]
        Pico = Linha["pico_mem_mb"]
        print(f"{Linha['candidato']:<10}{Linha['corpus']:>8}  {Linha['comando']:<7}{Linha['n']:>4}  "
              f"{Parede['mediana']:>14.2f}  {Parede['iqr']:>9.2f}  {Pico['mediana']:>12.2f}  {Pico['iqr']:>8.2f}")


def ValidarCandidatos(Texto: str) -> tuple:
    """Lê --candidatos (ex.: c,cpp,py) e recusa nome fora de CANDIDATOS."""
    Escolhidos = tuple(P.strip() for P in Texto.split(",") if P.strip())
    Desconhecidos = [P for P in Escolhidos if P not in CANDIDATOS]
    if not Escolhidos or Desconhecidos:
        raise ErroMedicao(
            f"--candidatos aceita só {list(CANDIDATOS)}; valor recebido: {Texto!r}; desconhecidos: {Desconhecidos}"
        )
    return Escolhidos


def VerificarCorpora(CorpusDir: Path, Tamanhos: tuple) -> None:
    """Confere que existe corpus_<N>.fj para cada tamanho em --corpus-dir."""
    for Tamanho in Tamanhos:
        Arquivo = CorpusDir / f"corpus_{Tamanho}.fj"
        if not Arquivo.is_file():
            raise ErroMedicao(f"corpus não encontrado: {Arquivo}; valor recebido em --corpus-dir: {CorpusDir}")


def RodarPrincipal(Args: argparse.Namespace) -> int:
    """Fluxo da medição real: valida, roda, grava o JSON e imprime a tabela."""
    if Args.CorpusDir is None:
        raise ErroMedicao("--corpus-dir é obrigatório fora do --selftest; valor recebido: None")
    if Args.Rodadas < 1 or Args.Aquecimento < 0:
        raise ErroMedicao(
            f"--rodadas deve ser >= 1 e --aquecimento >= 0; valores recebidos: "
            f"rodadas={Args.Rodadas}, aquecimento={Args.Aquecimento}"
        )
    Candidatos = ValidarCandidatos(Args.Candidatos)
    CorpusDir = Path(Args.CorpusDir)
    VerificarCorpora(CorpusDir, TAMANHOS)
    with tempfile.TemporaryDirectory() as Tmp:
        PlanoMedicao = Plano(CorpusDir, Candidatos, TAMANHOS, Path(Tmp))
        Amostras = RodarMedicao(PlanoMedicao, Args.Rodadas, Args.Aquecimento)
    Dados = MontarResultado(Amostras, Args.Rodadas, Args.Aquecimento)
    Saida = Path(Args.Saida) if Args.Saida else PadraoSaida()
    GravarResultado(Saida, Dados)
    ImprimirTabela(Dados["resumo"])
    print(f"JSON gravado em {Saida}")
    return 0


def GerarCorpusTeste(Tamanho: int, Pasta: Path) -> None:
    """Gera corpus_<Tamanho>.fj com o gerador da casa (seed fixa) na pasta dada."""
    Argv = [sys.executable, str(GERADOR), "--seed", str(SEMENTE), "--linhas", str(Tamanho), "--saida", str(Pasta)]
    Proc = subprocess.run(Argv, capture_output=True, encoding="utf-8", errors="replace", check=False)
    if Proc.returncode != 0:
        raise ErroMedicao(
            f"gerador falhou com código {Proc.returncode}; comando: {' '.join(Argv)}; "
            f"saída: {Proc.stderr.strip()[:1000]!r}"
        )


def VerificarAmostras(Amostras: list) -> None:
    """Confere 4 amostras (2 rodadas x check/build), pico > 1 MB e campos de tempos no build."""
    if len(Amostras) != 4:
        raise ErroMedicao(f"esperava 4 amostras (2 rodadas x check/build), veio {len(Amostras)}")
    for Amostra in Amostras:
        Pico = Amostra["peak_mem_bytes"]
        if Pico <= MB:
            raise ErroMedicao(f"pico de memória de {Amostra['comando']} = {Pico} bytes; esperava > {MB} bytes")
        if Amostra["comando"] == "build":
            Faltando = [Campo for Campo in TEMPOS_CAMPOS if Campo not in (Amostra["tempos"] or {})]
            if Faltando:
                raise ErroMedicao(f"campos de tempos ausentes no build: {Faltando}")


def VerificarJson(Saida: Path) -> None:
    """Confere que o JSON saiu, tem as chaves do contrato e o resumo do build traz tempos_ms."""
    if not Saida.is_file():
        raise ErroMedicao(f"JSON não foi gravado em {Saida}")
    Dados = json.loads(Saida.read_text(encoding="utf-8"))
    Chaves = ["data_hora", "rodadas", "aquecimento", "amostras_brutas", "resumo"]
    Faltando = [Chave for Chave in Chaves if Chave not in Dados]
    if Faltando:
        raise ErroMedicao(f"JSON sem as chaves {Faltando}: {Saida}")
    Builds = [Linha for Linha in Dados["resumo"] if Linha["comando"] == "build"]
    if not Builds or "tempos_ms" not in Builds[0]:
        raise ErroMedicao(f"resumo do build sem tempos_ms em {Saida}")


def Autoteste() -> int:
    """Roda 2 rodadas do candidato c no corpus de 20000 em pasta temporária e confere o resultado."""
    try:
        with tempfile.TemporaryDirectory() as Tmp:
            Raiz = Path(Tmp)
            GerarCorpusTeste(20000, Raiz)
            VerificarCorpora(Raiz, (20000,))
            PlanoMedicao = Plano(Raiz, ("c",), (20000,), Raiz / "trabalho")
            Amostras = RodarMedicao(PlanoMedicao, 2, 0)
            Saida = Raiz / "selftest.json"
            GravarResultado(Saida, MontarResultado(Amostras, 2, 0))
            VerificarAmostras(Amostras)
            VerificarJson(Saida)
    except ErroMedicao as Erro:
        print(f"SELFTEST FALHOU: {Erro}")
        return 1
    print("SELFTEST OK")
    return 0


def ConstruirParser() -> argparse.ArgumentParser:
    """Declara as opções da linha de comando."""
    Parser = argparse.ArgumentParser(
        description="Mede parede, pico de memória (Job Object) e tempos internos dos protótipos Forja.",
    )
    Parser.add_argument("--corpus-dir", dest="CorpusDir", help="pasta com corpus_20000.fj e corpus_200000.fj")
    Parser.add_argument("--rodadas", dest="Rodadas", type=int, default=30)
    Parser.add_argument("--aquecimento", dest="Aquecimento", type=int, default=3)
    Parser.add_argument("--saida", dest="Saida", default=None, help="JSON de saída (padrão: resultados\\medicao_<data>.json)")
    Parser.add_argument("--candidatos", dest="Candidatos", default="c,cpp,py")
    Parser.add_argument("--selftest", dest="Selftest", action="store_true")
    return Parser


def Principal(Argumentos: list) -> int:
    """Ponto de entrada: --selftest ou medição real; erro vira mensagem e exit 1."""
    Args = ConstruirParser().parse_args(Argumentos)
    try:
        if Args.Selftest:
            return Autoteste()
        return RodarPrincipal(Args)
    except (ErroMedicao, OSError) as Erro:
        print(f"ERRO: {Erro}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    ConfigurarWinApi()
    sys.exit(Principal(sys.argv[1:]))
