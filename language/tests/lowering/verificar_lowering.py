"""Verifica o lowering e o codegen da Forja com programas reais: compila cada NN_nome.fj em dev e em release, roda e compara.

Regras de um caso (arquivos ao lado do .fj):
  - NN_nome.esperado      stdout exato do exe em release (e em dev, quando nao ha .dev.esperado).
  - NN_nome.dev.esperado  (opcional) stdout exato do exe em dev; usado quando a falha de runtime so existe em dev.
  - NN_nome.codigo        (opcional) codigo R000x que o exe deve emitir no stderr nos dois modos, com exit 101.
  - NN_nome.dev.codigo    (opcional) idem, so em dev (estouro e faixa sao livres em release: ai o exit e 0).
  Sem .codigo/.dev.codigo o exe sai com 0. 'forja build' deve sair com 0 nos dois modos.

Versao: 1.1.2
Changelog: 1.1.2 - 0.21.0 F3: padrao x64, backend C aposentado (--backend so aceita x64; o oraculo C sai da bateria).
           1.1.1 - O0 P5: o padrao do release passou a --checagem nao; o build release do lowering leva --checagem sim (os .codigo de indice e nulo valem nos dois modos).
           1.1.0 - bateria enxuta: --backend c|x64 (padrao c). Com x64 os mesmos .esperado/.codigo valem para o nativo (stdout, exit e R-code), sem gcc; substitui a passada do lowering no verificar_nativo (C x x64 refazia o gcc dos 389 casos).
           1.0.4 - S3 P5b-2: o exe roda com stdin=DEVNULL (o caso 374 le a entrada padrao; sem isso herdava o console).
           1.0.3 - M4 P25b: o build leva --backend c explicito (o oraculo do lowering e o C; o padrao do dev passou a ser x64).
           1.0.2 - Linguagem v1.3 P6: .esperado lido como UTF-8 (caso 122: str com acentos, comparacao em bytes UTF-8).
           1.0.1 - Linguagem v1.2: aceita casos NNN_ (100 em diante).
           1.0.0 - Fase 5g-M: primeira versao (matriz do lowering, dev + release, paralela, --selftest).
Verificacao: python verificar_lowering.py --selftest   |   python verificar_lowering.py --forja C:/caminho/forja.exe
"""

import argparse
import difflib
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
MinimoDeCasos = 45
TimeoutCompilador = 90
TimeoutExe = 10
CodigoDeFalhaDeRuntime = 101
MaxLinhasDetalhe = 4
FalsoCompilador = "import sys\nsys.exit(3)\n"
Modos = ("dev", "release")


@dataclass
class CasoLowering:
    Nome: str
    Fj: Path
    Esperado: Path
    DevEsperado: Path | None
    Codigo: str | None
    CodigoDev: str | None


@dataclass
class Execucao:
    Codigo: int | None  # None = estourou o timeout
    Stdout: str
    Stderr: str


@dataclass
class Falha:
    Motivo: str
    Detalhe: list


@dataclass
class Resultado:
    Nome: str
    Falha: Falha | None


def Normalizar(Texto: str) -> str:
    # o exe MinGW escreve CRLF em stdout; o .esperado usa LF
    return Texto.replace("\r\n", "\n")


def LerCodigo(Caminho: Path) -> str | None:
    if not Caminho.is_file():
        return None
    return Caminho.read_text(encoding="ascii").strip() or None


def MontarCaso(Fj: Path) -> CasoLowering:
    Base = Fj.with_suffix("")
    DevEsperado = Base.with_suffix(".dev.esperado")
    return CasoLowering(
        Fj.stem, Fj, Base.with_suffix(".esperado"), DevEsperado if DevEsperado.is_file() else None,
        LerCodigo(Base.with_suffix(".codigo")), LerCodigo(Base.with_suffix(".dev.codigo")),
    )


def ListarCasos(Pasta: Path, Trecho: str | None) -> list:
    Fontes = sorted(Pasta.glob("[0-9][0-9]*_*.fj"))  # NN_ ou NNN_ (a partir do 100)
    return [MontarCaso(Fj) for Fj in Fontes if not Trecho or Trecho in Fj.stem]


def RodarProcesso(Argumentos: list, Limite: int) -> Execucao:
    try:
        Proc = subprocess.run(Argumentos, capture_output=True, stdin=subprocess.DEVNULL, timeout=Limite)
    except subprocess.TimeoutExpired:
        return Execucao(None, "", "")
    except OSError as Erro:
        return Execucao(-1, "", f"nao consegui iniciar {Argumentos[0]!r}: {Erro}")
    return Execucao(
        Proc.returncode, Proc.stdout.decode("utf-8", errors="replace"), Proc.stderr.decode("utf-8", errors="replace"),
    )


def LinhasUteis(Texto: str) -> list:
    return [Linha for Linha in Texto.splitlines() if Linha.strip()][:MaxLinhasDetalhe]


def CompilarCaso(Compilador: list, Caso: CasoLowering, Exe: Path, Modo: str, *, Backend: str = "x64") -> Falha | None:
    Exe.unlink(missing_ok=True)  # sobra de rodada anterior nao pode mascarar o resultado
    Argumentos = Compilador + ["build", "--backend", Backend, str(Caso.Fj), "-o", str(Exe)] + (["--release", "--checagem", "sim"] if Modo == "release" else [])
    Rodada = RodarProcesso(Argumentos, TimeoutCompilador)
    if Rodada.Codigo is None:
        return Falha(f"{Modo}_build_timeout", [f"build passou de {TimeoutCompilador} s"])
    if Rodada.Codigo != 0:
        Detalhe = LinhasUteis(Rodada.Stdout) or LinhasUteis(Rodada.Stderr) or ["build sem saida"]
        return Falha(f"{Modo}_build_exit_{Rodada.Codigo}", Detalhe)
    return None


def CodigoEsperadoNoModo(Caso: CasoLowering, Modo: str) -> str | None:
    if Modo == "dev" and Caso.CodigoDev:
        return Caso.CodigoDev
    return Caso.Codigo


def StdoutEsperadoNoModo(Caso: CasoLowering, Modo: str) -> str:
    Fonte = Caso.DevEsperado if (Modo == "dev" and Caso.DevEsperado) else Caso.Esperado
    return Normalizar(Fonte.read_text(encoding="utf-8"))  # bytes UTF-8: o stdout do exe e decodificado como utf-8


def DiffLinhas(Esperado: str, Obtido: str) -> list:
    Mudancas = difflib.ndiff(Esperado.splitlines(), Obtido.splitlines())
    return [Linha for Linha in Mudancas if Linha[:1] in ("+", "-")][:MaxLinhasDetalhe]


def ConferirSaida(Caso: CasoLowering, Modo: str, Rodada: Execucao) -> Falha | None:
    CodigoR = CodigoEsperadoNoModo(Caso, Modo)
    ExitEsperado = CodigoDeFalhaDeRuntime if CodigoR else 0
    if Rodada.Codigo is None:
        return Falha(f"{Modo}_exe_timeout", [f"exe passou de {TimeoutExe} s"])
    if Rodada.Codigo != ExitEsperado:
        Detalhe = LinhasUteis(Rodada.Stderr) or [f"exe saiu {Rodada.Codigo}, esperado {ExitEsperado}"]
        return Falha(f"{Modo}_exit_{Rodada.Codigo}_esperado_{ExitEsperado}", Detalhe)
    if CodigoR and f'"{CodigoR}"' not in Rodada.Stderr:
        return Falha(f"{Modo}_codigo_diferente", [f"esperado {CodigoR}"] + LinhasUteis(Rodada.Stderr))
    Esperado = StdoutEsperadoNoModo(Caso, Modo)
    Obtido = Normalizar(Rodada.Stdout)
    if Obtido != Esperado:
        return Falha(f"{Modo}_stdout_diferente", DiffLinhas(Esperado, Obtido))
    return None


def VerificarModo(Compilador: list, Caso: CasoLowering, Pasta: Path, Modo: str, *, Backend: str = "x64") -> Falha | None:
    Exe = Pasta / f"{Caso.Nome}_{Modo}.exe"
    Erro = CompilarCaso(Compilador, Caso, Exe, Modo, Backend=Backend)
    if Erro is not None:
        return Erro
    return ConferirSaida(Caso, Modo, RodarProcesso([str(Exe)], TimeoutExe))


def VerificarCaso(Compilador: list, Pasta: Path, Caso: CasoLowering, *, Backend: str = "x64") -> Resultado:
    for Modo in Modos:
        try:
            Erro = VerificarModo(Compilador, Caso, Pasta, Modo, Backend=Backend)
        except Exception as Excecao:  # a matriz nao cai por causa de um caso
            Erro = Falha(f"{Modo}_excecao_{type(Excecao).__name__}", [str(Excecao)[:200]])
        if Erro is not None:
            return Resultado(Caso.Nome, Erro)
    return Resultado(Caso.Nome, None)


def RodarCasos(Compilador: list, Pasta: Path, Casos: list, Jobs: int, *, Backend: str = "x64") -> list:
    Pasta.mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(max_workers=Jobs) as Piscina:
        return list(Piscina.map(lambda Caso: VerificarCaso(Compilador, Pasta, Caso, Backend=Backend), Casos))


def Relatar(Resultados: list) -> int:
    Passou = sum(1 for Item in Resultados if Item.Falha is None)
    for Item in Resultados:
        if Item.Falha is None:
            continue
        print(f"FALHOU {Item.Nome}: {Item.Falha.Motivo}")
        for Linha in Item.Falha.Detalhe:
            print(f"    > {Linha}")
    print(f"{Passou}/{len(Resultados)} casos de lowering")
    return 0 if Passou == len(Resultados) and Resultados else 1


def ChecarMatriz(Casos: list) -> list:
    Problemas = []
    if len(Casos) < MinimoDeCasos:
        Problemas.append(f"matriz tem {len(Casos)} casos, minimo {MinimoDeCasos}")
    for Caso in Casos:
        if not Caso.Esperado.is_file():
            Problemas.append(f"{Caso.Nome}: falta {Caso.Esperado.name}")
    return Problemas


def ChecarComparacao() -> list:
    Problemas = []
    with tempfile.TemporaryDirectory(prefix="verificar_lowering_cmp_") as Temp:
        Esperado = Path(Temp) / "x.esperado"
        Esperado.write_bytes(b"1\n")
        Caso = CasoLowering("x", Path("x.fj"), Esperado, None, "R0002", None)
        if ConferirSaida(Caso, "release", Execucao(101, "1\r\n", '{"codigo": "R0002"}')) is not None:
            Problemas.append("comparacao: rejeitou uma saida correta (CRLF + codigo)")
        if ConferirSaida(Caso, "release", Execucao(101, "2\n", '{"codigo": "R0002"}')) is None:
            Problemas.append("comparacao: aceitou stdout errado")
        if ConferirSaida(Caso, "release", Execucao(0, "1\n", "")) is None:
            Problemas.append("comparacao: aceitou exit 0 onde o codigo exigia 101")
    Livre = CasoLowering("y", Path("y.fj"), Path("y.esperado"), None, None, "R0001")
    if CodigoEsperadoNoModo(Livre, "dev") != "R0001" or CodigoEsperadoNoModo(Livre, "release") is not None:
        Problemas.append("comparacao: .dev.codigo vazou para o release")
    return Problemas


def ChecarCompiladorFalso(Casos: list) -> list:
    with tempfile.TemporaryDirectory(prefix="verificar_lowering_selftest_") as Temp:
        Script = Path(Temp) / "compilador_falso.py"
        Script.write_text(FalsoCompilador, encoding="utf-8")
        Resultados = RodarCasos([sys.executable, str(Script)], Path(Temp) / "saida", Casos[:3], 3)
    if any(Item.Falha is None or not Item.Falha.Motivo for Item in Resultados):
        return ["compilador falso (exit 3) nao reprovou todos os casos com motivo"]
    return []


def Selftest() -> int:
    Casos = ListarCasos(PastaAqui, None)
    Problemas = ChecarMatriz(Casos) + ChecarComparacao()
    if not Problemas:
        Problemas += ChecarCompiladorFalso(Casos)
    if Problemas:
        print("SELFTEST FALHOU: " + "; ".join(Problemas))
        return 1
    print(f"SELFTEST OK ({len(Casos)} casos na matriz)")
    return 0


def MontarParser() -> argparse.ArgumentParser:
    Parser = argparse.ArgumentParser(description="Verifica o lowering da Forja (dev + release).")
    Parser.add_argument("--forja", help="caminho do forja.exe (obrigatorio, exceto com --selftest)")
    Parser.add_argument("--so", help="roda so os casos cujo nome contem este trecho")
    Parser.add_argument("--saida", help="pasta de trabalho (padrao: pasta temporaria nova)")
    Parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1, help="casos em paralelo (padrao: nucleos)")
    Parser.add_argument("--backend", choices=("x64",), default="x64", help="backend que compila os casos (padrao x64; o backend C foi aposentado na 0.21.0)")
    Parser.add_argument("--selftest", action="store_true", help="confere a matriz e a propria logica")
    return Parser


def ValidarForja(Texto: str | None) -> str | None:
    if not Texto:
        return "falta --forja"
    if not Path(Texto).resolve().is_file():
        return f"forja nao encontrado: {Texto!r}"
    return None


def Main(Argumentos: list | None = None) -> int:
    sys.stdout.reconfigure(errors="replace")
    Args = MontarParser().parse_args(Argumentos)
    if Args.selftest:
        return Selftest()
    Erro = ValidarForja(Args.forja)
    if Erro or Args.jobs < 1:
        print(f"ERRO de entrada: {Erro or '--jobs precisa ser >= 1'}", file=sys.stderr)
        return 2
    Casos = ListarCasos(PastaAqui, Args.so)
    if not Casos:
        print(f"ERRO de entrada: nenhum caso em {PastaAqui} com --so={Args.so!r}", file=sys.stderr)
        return 2
    Pasta = Path(Args.saida).resolve() if Args.saida else Path(tempfile.mkdtemp(prefix="verificar_lowering_"))
    return Relatar(RodarCasos([str(Path(Args.forja).resolve())], Pasta, Casos, Args.jobs, Backend=Args.backend))


if __name__ == "__main__":
    sys.exit(Main())
