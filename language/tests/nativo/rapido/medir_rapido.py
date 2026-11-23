# -*- coding: utf-8 -*-
"""medir_rapido: instrumento `--rapido` (decisao D0696) para decidir uma fatia do forja em minutos, nao em horas.

Compara o forja NOVO com a BASE so no x64 release (1x por lado, sem O0/O2/backend C): benches de exe identico valem 1,000
sem cronometro; os que mudaram sao cronometrados em pares intercalados A,B,B,A, 1 nucleo fixo, IC por estatistica de ordem
em 12/24/36 pares (veredito por bench) e bootstrap para geomedia total, codegen e tick. Cronometra sob a trava EXCLUSIVA
da bancada (trava_bancada.py); compilar/conferir/Ir ficam sob a COMPARTILHADA.

Uso:
    python -I -B tests\\nativo\\rapido\\medir_rapido.py --contra <base_forja.exe> [--novo <forja.exe>] [--bench NOME ...]
        [--forcar] [--calibrar] [--ir] [--kernels] [--sem-historico] [--espera-s N]
    --calibrar: A==A em todos os benches (cada IC em [0,97; 1,03] e contendo 1,000; tolera 1 de 14); reprovado vira INSTAVEL
    (sempre 36 pares + Ir nas rodadas seguintes; a lista fica no historico, tipo rapido_calibracao).
Saida: 0 APROVADO; 1 REPROVADO ou PENDENTE; 2 erro. Veredito tambem em %TEMP%\\medir_rapido_veredito.txt; historico tipo `rapido`.
Verificacao: python -I -B tests\\nativo\\rapido\\medir_rapido.py --selftest
Versao: 1.0.0
Changelog: 1.0.0 - primeira versao: classificacao por SHA do x64, pares intercalados, vereditos por ordem, bootstrap, calibracao A==A,
           Ir com cache em disco (por sha do forja e bench), kernels, trava de bancada, historico tipo rapido.
"""

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

import cronometro_rapido as Cron  # noqa: E402
import estat_rapido as Est  # noqa: E402
import preparo_rapido as Prep  # noqa: E402
import relatorio_rapido as Rel  # noqa: E402
import trava_bancada as Trava  # noqa: E402

MB = Prep.MB
VERSAO = "1.0.0"
ESPERA_PADRAO_S = 3600.0
ARQUIVO_VEREDITO = Path(tempfile.gettempdir()) / "medir_rapido_veredito.txt"
PASTA_CACHE_IR = Path(tempfile.gettempdir()) / "medir_rapido_ir"
TIPO_CALIBRACAO = "rapido_calibracao"
FALHAS_TOLERADAS_CALIBRACAO = 1
VEREDITO_CAL_OK = "CAL OK"
VEREDITO_INSTAVEL = "INSTAVEL"


def ConstruirParser():
    Ap = argparse.ArgumentParser(description="Medicao rapida novo/base do forja: x64 release, pares intercalados, IC por ordem.")
    Ap.add_argument("--contra", metavar="BASE_EXE", help="forja BASE (caminho absoluto)")
    Ap.add_argument("--novo", default=MB.FORJA_PADRAO, metavar="FORJA_EXE", help="forja NOVO (padrao: compiler\\forja.exe)")
    Ap.add_argument("--bench", nargs="+", default=None, metavar="NOME", help="so estes benches")
    Ap.add_argument("--forcar", action="store_true", help="cronometra tambem os benches de exe identico")
    Ap.add_argument("--calibrar", action="store_true", help="A==A em todos os benches (usa so a base)")
    Ap.add_argument("--ir", action="store_true", help="Ir (instrucoes) dos benches cujo exe mudou, via medir_linux")
    Ap.add_argument("--kernels", action="store_true", help="os kernels de otim\\kernels\\gerados, novo/base")
    Ap.add_argument("--sem-historico", action="store_true", help="nao grava no otim_historico.jsonl")
    Ap.add_argument("--espera-s", type=float, default=ESPERA_PADRAO_S, help="espera maxima pela trava EXCLUSIVA da bancada")
    Ap.add_argument("--historico", default=str(MB.HISTORICO_PADRAO), help="arquivo JSONL do historico")
    Ap.add_argument("--selftest", action="store_true", help="testa as funcoes com dados sinteticos (sem forja) e sai")
    return Ap


def ResolverForjas(Args):
    Base = Path(Args.contra)
    Novo = Base if Args.calibrar else Path(Args.novo)
    for Rotulo, Forja in (("--contra", Base), ("--novo", Novo)):
        if not Forja.is_absolute() or not Forja.is_file():
            raise MB.ErroBench(f"{Rotulo} precisa ser o caminho ABSOLUTO de um forja.exe existente: {Forja}")
    return Base, Novo


def LerInstaveis(Historico):
    """Benches reprovados na calibracao A==A mais recente do historico."""
    for Linha in reversed(Historico):
        if Linha.get("tipo") == TIPO_CALIBRACAO:
            return set(Linha.get("instaveis", []))
    return set()


def CronometrarLados(Lados, Pasta, Opcoes):
    """Pares dos benches que mudaram (ou todos com forcar/calibrar); identicos viram razao 1,000. Sob a trava EXCLUSIVA."""
    Resultados = []
    for Lado in Lados:
        if not (Lado.Mudou or Opcoes["forcar"]):
            Resultados.append(Rel.ResultadoIdentico(Lado.Nome))
            continue
        Instavel = Lado.Nome in Opcoes["instaveis"]
        Copias = Cron.PrepararCopias(Lado.ExeBase, Lado.ExeNovo, Pasta / "par" / Lado.Nome)
        Medido = Cron.CronometrarBench(Copias, Sempre36=Instavel)
        Resultado = Rel.ResultadoMedido(Lado.Nome, Medido, Mudou=Lado.Mudou, Instavel=Instavel)
        Resultados.append(Resultado)
        print(f"  {Lado.Nome:<22} razao {Resultado['mediana']:.3f} {Rel.FormatarIc(Resultado['ic'])} em {Resultado['pares']} pares: "
              f"{Resultado['veredito']}", flush=True)
    return Resultados


def CronometrarKernels(Lados, Pasta):
    """Razao mediana novo/base por kernel (12 pares fixos); identicos = 1,000. Sob a trava EXCLUSIVA."""
    Razoes = {}
    for Lado in Lados:
        if not Lado.Mudou:
            Razoes[Lado.Nome] = 1.0
            continue
        Copias = Cron.PrepararCopias(Lado.ExeBase, Lado.ExeNovo, Pasta / "par_kernel" / Lado.Nome)
        Razoes[Lado.Nome] = Cron.CronometrarFixo(Copias)
    return Razoes


def ResumoKernels(Razoes, Erros):
    if not Razoes:
        return "kernels: nenhum medido" + (f" ({len(Erros)} falha(s))" if Erros else "")
    Pior = max(Razoes, key=Razoes.get)
    Mudaram = sum(1 for R in Razoes.values() if R != 1.0)
    return (f"kernels: {len(Razoes)} ({Mudaram} cronometrados) geomedia novo/base {Est.MediaGeometrica(list(Razoes.values())):.4f} | "
            f"pior {Pior} {Razoes[Pior]:.3f}" + (f" | {len(Erros)} falha(s)" if Erros else ""))


def CompletarPesosDoTick(Resultados, Lados, Historico):
    """Identico sem historico no tick: mede o A avulso (3 execucoes) para pesar. Sob a trava EXCLUSIVA."""
    PorNome = {L.Nome: L for L in Lados}
    Avulsos = {}
    for Nome in Rel.NomesSemPesoMedido(Resultados, Historico):
        Exe = PorNome[Nome].ExeBase
        Avulsos[Nome] = Cron.MedianaAvulsa(Exe, Exe.parent)
    return Avulsos


def ArquivoCacheIr(Sha):
    return PASTA_CACHE_IR / f"{Sha}.json"


def LerCacheIr(Sha):
    Caminho = ArquivoCacheIr(Sha)
    if not Caminho.is_file():
        return {}
    return json.loads(Caminho.read_text(encoding="utf-8"))


def GravarCacheIr(Sha, Dados):
    PASTA_CACHE_IR.mkdir(parents=True, exist_ok=True)
    ArquivoCacheIr(Sha).write_text(json.dumps(Dados, ensure_ascii=False, sort_keys=True), encoding="utf-8")


def IrDaForja(Forja, Nomes, Pasta):
    """{bench: {ir_x64, ir_O2}} da forja; cache em disco por (sha do forja, bench). So contagem de instrucoes, nunca tempo."""
    Sha = MB.ShaCurtoForja(Forja)
    Cache = LerCacheIr(Sha)
    if all(N in Cache for N in Nomes):
        return Cache
    try:
        Resultado = MB.ImportarMedirLinux().RodarIrComTrava(Path(Forja), Pasta / f"ir_{Sha}")
    except (RuntimeError, OSError, ImportError, subprocess.TimeoutExpired) as Erro:
        print(f"aviso: Ir de {Forja} (sha {Sha}) nao medido: {Erro}")
        return Cache
    for B in Resultado["benchs"]:
        Cache[B["nome"]] = {"ir_x64": B["ir_x64"], "ir_O2": B["ir_O2"]}
    GravarCacheIr(Sha, Cache)
    return Cache


def RazoesIr(IrBase, IrNovo, Nomes):
    """{bench: {razao, base, novo}} novo/base em Ir para os benches que estao nos dois lados."""
    Saida = {}
    for Nome in Nomes:
        if Nome in IrBase and Nome in IrNovo and IrBase[Nome]["ir_x64"]:
            Saida[Nome] = {"base": IrBase[Nome]["ir_x64"], "novo": IrNovo[Nome]["ir_x64"],
                           "razao": IrNovo[Nome]["ir_x64"] / IrBase[Nome]["ir_x64"]}
    return Saida


def MedirIr(Forjas, Resultados, Pasta):
    """Ir so dos benches que mudaram de exe (ou instaveis medidos). Devolve (por bench, texto-resumo)."""
    Nomes = [R["nome"] for R in Resultados if R["mudou"] or R["instavel"]]
    if not Nomes:
        return {}, "Ir: nenhum bench mudou de exe"
    Irs = RazoesIr(IrDaForja(Forjas[0], Nomes, Pasta), IrDaForja(Forjas[1], Nomes, Pasta), Nomes)
    if not Irs:
        return {}, "Ir: nao medido (sem WSL ou erro; ver aviso)"
    Pior = max(Irs, key=lambda N: Irs[N]["razao"])
    return Irs, (f"Ir novo/base: {len(Irs)} bench(es), geomedia {Est.MediaGeometrica([I['razao'] for I in Irs.values()]):.4f} | "
                 f"pior {Pior} {Irs[Pior]['razao']:.4f}")


def Contexto(Forjas, Pasta):
    Agora = datetime.now()
    return {"data": Agora.isoformat(timespec="seconds"), "rodada": Agora.strftime("%Y%m%d-%H%M%S"), "versao": VERSAO,
            "sha_base": MB.ShaCurtoForja(Forjas[0]), "sha_novo": MB.ShaCurtoForja(Forjas[1]),
            "versao_base": MB.VersaoForja(Forjas[0], Pasta), "versao_novo": MB.VersaoForja(Forjas[1], Pasta)}


def GravarVeredito(Linhas):
    ARQUIVO_VEREDITO.write_text("\n".join(Linhas) + "\n", encoding="utf-8")


def GravarOuAvisar(Args, Linhas):
    if Args.sem_historico:
        return
    try:
        MB.GravarHistorico(Path(Args.historico), Linhas)
    except OSError as Erro:
        print(f"aviso: historico nao gravado: {Erro}")


def Calibrar(Resultados, Ctx, Args):
    """A==A: cada bench tem de ter IC em [0,97; 1,03] com 1,000 dentro; tolera 1 falha; as que falham viram INSTAVEL."""
    Instaveis = []
    for R in Resultados:
        Bom = Est.CalibracaoOk(R["ic"])
        R["veredito"] = VEREDITO_CAL_OK if Bom else VEREDITO_INSTAVEL
        R["instavel"] = not Bom
        if not Bom:
            Instaveis.append(R["nome"])
    print("\n".join(Rel.MontarTabela(Resultados, {})))
    Passou = len(Instaveis) <= FALHAS_TOLERADAS_CALIBRACAO
    Texto = (f"CALIBRACAO {'OK' if Passou else 'FALHOU'}: {len(Instaveis)} de {len(Resultados)} instavel(is) "
             f"(tolera {FALHAS_TOLERADAS_CALIBRACAO}): {', '.join(Instaveis) or '-'}")
    print(Texto)
    GravarVeredito([Texto])
    GravarOuAvisar(Args, [{"tipo": TIPO_CALIBRACAO, "data": Ctx["data"], "rodada": Ctx["rodada"], "sha_forja": Ctx["sha_base"],
                           "versao_medir_rapido": VERSAO, "instaveis": Instaveis, "passou": Passou,
                           "benchs": {R["nome"]: {"ic": list(R["ic"]), "pares": R["pares"]} for R in Resultados}}])
    return 0 if Passou else 1


def ColetarMedidas(Args, Forjas, Pasta, Instaveis):
    """Fase compartilhada (compilar e conferir) e fase EXCLUSIVA (cronometrar). Devolve (resultados, lados, erros, kernels, pesos)."""
    Historico = MB.LerHistorico(Path(Args.historico))
    with Trava.TravaBancada(Trava.MODO_COMPARTILHADO, TimeoutS=Args.espera_s):
        Lados, Erros = Prep.CompilarBenchs(Args.bench, Forjas, Pasta / "benchs")
        LadosK, ErrosK = Prep.CompilarKernels(Forjas, Pasta / "kernels") if Args.kernels else ([], {})
    Opcoes = {"forcar": Args.forcar or Args.calibrar, "instaveis": Instaveis}
    print(f"cronometrando {sum(1 for L in Lados if L.Mudou or Opcoes['forcar'])} de {len(Lados)} benches (trava EXCLUSIVA)...", flush=True)
    with Trava.TravaBancada(Trava.MODO_EXCLUSIVO, TimeoutS=Args.espera_s), Cron.AfinidadeFixa() as Nucleo:
        print(f"  afinidade: nucleo {Nucleo}", flush=True)
        Resultados = CronometrarLados(Lados, Pasta, Opcoes)
        Razoes = CronometrarKernels(LadosK, Pasta) if Args.kernels else {}
        Pesos = CompletarPesosDoTick(Resultados, Lados, Historico)
    return Resultados, Lados, {**Erros, **ErrosK}, (Razoes, ErrosK), Pesos


def Relatar(Args, Forjas, Pasta, Medidas):
    Resultados, _, Erros, (Razoes, ErrosK), Pesos = Medidas
    Ctx = Contexto(Forjas, Pasta)
    Ctx["erros"] = Erros
    if Args.calibrar:
        return Calibrar(Resultados, Ctx, Args)
    Irs = {}
    if Args.ir:
        Irs, Ctx["ir"] = MedirIr(Forjas, Resultados, Pasta)
    if Args.kernels:
        Ctx["kernels"] = ResumoKernels(Razoes, ErrosK)
    Historico = MB.LerHistorico(Path(Args.historico))
    Agregados = Rel.CalcularAgregados(Resultados, Rel.PesosDoTick(Resultados, Historico, Pesos))
    Final, Linhas = Rel.MontarVeredito(Ctx, Resultados, Agregados)
    print("\n".join(Rel.MontarTabela(Resultados, Irs)))
    print("\n".join(Linhas))
    GravarVeredito(Linhas)
    GravarOuAvisar(Args, [Rel.MontarLinhaHistorico(Ctx, Resultados, Agregados, Final)])
    return 0 if Final == Rel.FINAL_APROVADO else 1


def Rodar(Args, Pasta):
    Forjas = ResolverForjas(Args)
    Instaveis = set() if Args.calibrar else LerInstaveis(MB.LerHistorico(Path(Args.historico)))
    Medidas = ColetarMedidas(Args, Forjas, Pasta, Instaveis)
    for Nome, Erro in sorted(Medidas[2].items()):
        print(f"FALHOU {Nome}: {Erro}")
    return Relatar(Args, Forjas, Pasta, Medidas)


def Main(Argumentos=None):
    Args = ConstruirParser().parse_args(Argumentos)
    if Args.selftest:
        return 0 if Selftest() else 1
    if not Args.contra:
        print("ERRO: --contra <base_forja.exe> e obrigatorio", file=sys.stderr)
        return 2
    Pasta = Path(tempfile.mkdtemp(prefix="medir_rapido_"))
    try:
        return Rodar(Args, Pasta)
    except (MB.ErroBench, Cron.ErroCronometro, Trava.ErroTrava, Est.ErroEstatistica, OSError) as Erro:
        print(f"ERRO: {Erro}", file=sys.stderr)
        return 2
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def Afirmar(Condicao, Mensagem):
    if not Condicao:
        raise AssertionError(Mensagem)


def TesteParser():
    Args = ConstruirParser().parse_args(["--contra", "C:/x.exe", "--bench", "a", "b", "--forcar", "--ir", "--kernels"])
    Afirmar(Args.bench == ["a", "b"] and Args.forcar and Args.ir and Args.kernels and not Args.calibrar, "parser")


def TesteInstaveis():
    Historico = [{"tipo": TIPO_CALIBRACAO, "instaveis": ["a"]}, {"tipo": "rapido"}, {"tipo": TIPO_CALIBRACAO, "instaveis": ["b", "c"]}]
    Afirmar(LerInstaveis(Historico) == {"b", "c"}, "usa a calibracao mais recente")
    Afirmar(LerInstaveis([]) == set(), "sem calibracao")


def TesteIr():
    Base = {"a": {"ir_x64": 100, "ir_O2": 90}, "b": {"ir_x64": 200, "ir_O2": 1}}
    Novo = {"a": {"ir_x64": 110, "ir_O2": 90}}
    Irs = RazoesIr(Base, Novo, ["a", "b"])
    Afirmar(list(Irs) == ["a"] and abs(Irs["a"]["razao"] - 1.1) < 1e-12, "razao de Ir so onde ha os dois lados")


def TesteResumoKernels():
    Afirmar("nenhum" in ResumoKernels({}, {}), "kernels vazios")
    Texto = ResumoKernels({"k1": 1.0, "k2": 1.04}, {})
    Afirmar("pior k2" in Texto and "(1 cronometrados)" in Texto, Texto)


def TesteCalibrar():
    Bons = Est.IcPorOrdem([0.99 + 0.002 * I for I in range(12)])
    Afirmar(Est.CalibracaoOk(Bons), "IC estreito em torno de 1 calibra")
    Afirmar(not Est.CalibracaoOk((1.002, 1.02)), "IC sem 1,000 nao calibra")


def TesteCacheIr():
    Sha = "selftest_" + datetime.now().strftime("%H%M%S%f")
    try:
        GravarCacheIr(Sha, {"a": {"ir_x64": 1, "ir_O2": 2}})
        Afirmar(LerCacheIr(Sha) == {"a": {"ir_x64": 1, "ir_O2": 2}}, "cache de Ir ida e volta")
        Afirmar(LerCacheIr(Sha + "x") == {}, "cache ausente = vazio")
    finally:
        ArquivoCacheIr(Sha).unlink(missing_ok=True)


def Selftest():
    for Modulo in (Est, Trava, Cron, Prep, Rel):
        Modulo.Selftest()
    for Teste in (TesteParser, TesteInstaveis, TesteIr, TesteResumoKernels, TesteCalibrar, TesteCacheIr):
        Teste()
    print(f"medir_rapido {VERSAO}: selftest OK")
    return True


if __name__ == "__main__":
    sys.exit(Main())
