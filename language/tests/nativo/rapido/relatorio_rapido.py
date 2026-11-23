# -*- coding: utf-8 -*-
"""Relatorio do medir_rapido: resultado por bench, geomedias com bootstrap, tick, tabela, veredito final e historico.

Objetivo: transformar as razoes pareadas (novo/base) em decisao. Geomedia total, geomedia de codegen (SEM formato_texto)
e tick (soma dos tempos de particulas_soa, transformada_aos_soa e aabb_raio) vem do bootstrap dos pares (estat_rapido).
Bench de exe identico entra com razao 1,000; no tick pesa pelo tempo do historico (otim_historico.jsonl) ou, sem ele,
pelo tempo medido do A.

Uso: importado por medir_rapido.py.
Verificacao: python -I -B relatorio_rapido.py --selftest
Versao: 1.0.0
Changelog: 1.0.0 - primeira versao (decisao D0696).
"""

import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

import estat_rapido as Est  # noqa: E402

VERSAO = "1.0.0"
BENCH_FORA_CODEGEN = "formato_texto"
BENCHS_TICK = ("particulas_soa", "transformada_aos_soa", "aabb_raio")
LIMITE_LINHAS_VEREDITO = 10
FINAL_APROVADO = "APROVADO"
FINAL_REPROVADO = "REPROVADO"
FINAL_PENDENTE = "PENDENTE"


def ResultadoIdentico(Nome):
    return {"nome": Nome, "mudou": False, "veredito": Est.VEREDITO_IDENTICO, "razoes": [1.0], "mediana": 1.0,
            "ic": (1.0, 1.0), "pares": 0, "bimodal": False, "instavel": False, "peso_a": None}


def ResultadoMedido(Nome, Cron, *, Mudou, Instavel=False):
    """Une o dict de cronometro_rapido.CronometrarBench com os campos do relatorio."""
    return {"nome": Nome, "mudou": Mudou, "veredito": Cron["veredito"], "razoes": Cron["razoes"],
            "mediana": Est.Mediana(Cron["razoes"]), "ic": Cron["ic"], "pares": Cron["pares"], "bimodal": Cron["bimodal"],
            "instavel": Instavel, "peso_a": Est.Mediana(Cron["tempos_a"])}


def TempoDoHistorico(Historico, Nome):
    """Tempo x64 (ms) mais recente do bench no historico: linhas 'contra' (x64_ms) ou de rodada (t_exec_ms.x64)."""
    for Linha in reversed(Historico):
        if Linha.get("tipo") == "contra" and Nome in Linha.get("x64_ms", {}):
            return Linha["x64_ms"][Nome]
        Medido = Linha.get("t_exec_ms")
        if Linha.get("bench") == Nome and isinstance(Medido, dict) and Medido.get("x64"):
            return Medido["x64"]
    return None


def NomesSemPesoMedido(Resultados, Historico):
    """Benches do tick que sao identicos, nao pareados e sem historico: o main cronometra o A avulso para pesar."""
    Faltam = []
    for R in Resultados:
        if R["nome"] in BENCHS_TICK and R["peso_a"] is None and TempoDoHistorico(Historico, R["nome"]) is None:
            Faltam.append(R["nome"])
    return Faltam


def PesosDoTick(Resultados, Historico, PesosAvulsos):
    """{bench do tick presente: peso em ms}; historico para os identicos, medido do A para os pareados e os avulsos."""
    Pesos = {}
    for R in Resultados:
        if R["nome"] not in BENCHS_TICK:
            continue
        Peso = R["peso_a"] or TempoDoHistorico(Historico, R["nome"]) or PesosAvulsos.get(R["nome"])
        if Peso:
            Pesos[R["nome"]] = Peso
    return Pesos


def Agregado(Pares, Metrica, Criterio=None):
    Ponto, Inf, Sup = Est.Bootstrap(Pares, Metrica)
    return {"ponto": Ponto, "inf": Inf, "sup": Sup, "ok": None if Criterio is None else Criterio(Ponto, Sup)}


def CalcularAgregados(Resultados, Pesos):
    """total, codegen e tick (None quando nao ha benches para a conta)."""
    Pares = {R["nome"]: R["razoes"] for R in Resultados}
    Codegen = {N: V for N, V in Pares.items() if N != BENCH_FORA_CODEGEN}
    Saida = {"total": Agregado(Pares, Est.MetricaGeomedia()) if Pares else None,
             "codegen": Agregado(Codegen, Est.MetricaGeomedia(), Est.CriterioCodegen) if Codegen else None,
             "tick": None}
    if Pesos:
        ParesTick = {N: Pares[N] for N in Pesos}
        Saida["tick"] = Agregado(ParesTick, Est.MetricaTick(Pesos), Est.CriterioTick)
    return Saida


def GeomediaSimples(Razoes):
    return Est.MediaGeometrica(list(Razoes.values())) if Razoes else None


def FormatarIc(Ic):
    return "[%.3f; %.3f]" % Ic


def FormatarIr(Ir):
    if not Ir:
        return "-"
    return "%.4f" % Ir["razao"]


def LinhaDaTabela(R, Irs):
    Marca = " [INSTAVEL]" if R["instavel"] else ""
    Bimodal = " bimodal" if R["bimodal"] else ""
    return (f"{R['nome']:<22} {R['mediana']:>7.3f} {FormatarIc(R['ic']):>17} {R['pares']:>5} "
            f"{'sim' if R['mudou'] else 'nao':>5} {FormatarIr(Irs.get(R['nome'])):>7}  {R['veredito']}{Marca}{Bimodal}")


def MontarTabela(Resultados, Irs):
    Cabecalho = f"{'bench':<22} {'razao':>7} {'IC':>17} {'pares':>5} {'mudou':>5} {'Ir n/b':>7}  veredito"
    return [Cabecalho] + [LinhaDaTabela(R, Irs) for R in Resultados]


def FormatarAgregado(Rotulo, Agr):
    if Agr is None:
        return f"{Rotulo}: n/d"
    Estado = "" if Agr["ok"] is None else (" OK" if Agr["ok"] else " FORA DO CRITERIO")
    return f"{Rotulo}: {Agr['ponto']:.4f} IC [{Agr['inf']:.4f}; {Agr['sup']:.4f}]{Estado}"


def DecidirFinal(Resultados, Agregados):
    """APROVADO / REPROVADO / PENDENTE (faixa ou inconclusivo) pelos vereditos por bench e pelos criterios dos agregados."""
    Vereditos = [R["veredito"] for R in Resultados]
    Reprova = Est.VEREDITO_REPROVA in Vereditos
    ForaCriterio = any(Agregados[K] is not None and Agregados[K]["ok"] is False for K in ("codegen", "tick"))
    if Reprova or ForaCriterio:
        return FINAL_REPROVADO
    if any(V in (Est.VEREDITO_FAIXA, Est.VEREDITO_INCONCLUSIVO) for V in Vereditos):
        return FINAL_PENDENTE
    return FINAL_APROVADO


def ContarVereditos(Resultados):
    Contagem = {}
    for R in Resultados:
        Contagem[R["veredito"]] = Contagem.get(R["veredito"], 0) + 1
    return ", ".join(f"{V}={N}" for V, N in sorted(Contagem.items()))


def MontarVeredito(Contexto, Resultados, Agregados):
    """No maximo 10 linhas: forjas, contagem, 3 geomedias, kernels, Ir e a decisao."""
    Final = DecidirFinal(Resultados, Agregados)
    Mudaram = sum(1 for R in Resultados if R["mudou"])
    Linhas = [f"medir_rapido: base {Contexto['sha_base']} -> novo {Contexto['sha_novo']} | {Mudaram} de {len(Resultados)} benches mudaram de exe",
              "benches: " + ContarVereditos(Resultados),
              FormatarAgregado("geomedia total", Agregados["total"]),
              FormatarAgregado("geomedia codegen (sem formato_texto)", Agregados["codegen"]),
              FormatarAgregado("tick (particulas_soa+transformada_aos_soa+aabb_raio)", Agregados["tick"]),
              Contexto.get("kernels") or "kernels: nao medidos (--kernels)",
              Contexto.get("ir") or "Ir: nao medido (--ir)"]
    if Contexto.get("erros"):
        Linhas.append("FALHAS: " + "; ".join(f"{N}: {E[:80]}" for N, E in sorted(Contexto["erros"].items())))
    Linhas.append(f"VEREDITO FINAL: {Final}")
    return Final, Linhas[:LIMITE_LINHAS_VEREDITO] if len(Linhas) <= LIMITE_LINHAS_VEREDITO else Linhas[:LIMITE_LINHAS_VEREDITO - 1] + Linhas[-1:]


def MontarLinhaHistorico(Contexto, Resultados, Agregados, Final):
    """Linha tipo 'rapido' do otim_historico.jsonl (so razoes; tempo nunca vai para cache)."""
    PorBench = {R["nome"]: {"razao": R["mediana"], "ic": list(R["ic"]), "pares": R["pares"], "veredito": R["veredito"],
                            "mudou": R["mudou"]} for R in Resultados}
    Corte = {K: (None if V is None else {C: V[C] for C in ("ponto", "inf", "sup", "ok")}) for K, V in Agregados.items()}
    return {"tipo": "rapido", "data": Contexto["data"], "rodada": Contexto["rodada"], "versao_medir_rapido": Contexto["versao"],
            "sha_base": Contexto["sha_base"], "sha_novo": Contexto["sha_novo"], "benchs": PorBench, "agregados": Corte,
            "veredito_final": Final}


def Afirmar(Condicao, Mensagem):
    if not Condicao:
        raise AssertionError(Mensagem)


def ResultadoSintetico(Nome, Razao, Tempo=100.0):
    Razoes = [Razao + 0.001 * (I % 3) for I in range(12)]
    Cron = {"razoes": Razoes, "tempos_a": [Tempo] * 12, "tempos_b": [Tempo * Razao] * 12, "ic": Est.IcPorOrdem(Razoes),
            "veredito": Est.VEREDITO_APROVA if Razao <= 1.0 else Est.VEREDITO_REPROVA, "bimodal": False, "pares": 12}
    return ResultadoMedido(Nome, Cron, Mudou=True)


def TesteGeomediaSemFormatoTexto():
    Resultados = [ResultadoIdentico("a"), ResultadoIdentico("b"), ResultadoSintetico("formato_texto", 1.5)]
    Agr = CalcularAgregados(Resultados, {})
    Afirmar(abs(Agr["codegen"]["ponto"] - 1.0) < 1e-9, "codegen ignora formato_texto")
    Afirmar(Agr["total"]["ponto"] > 1.1, "total inclui formato_texto")
    Afirmar(Agr["tick"] is None, "sem pesos nao ha tick")


def TesteTickPesos():
    Resultados = [ResultadoIdentico("particulas_soa"), ResultadoSintetico("aabb_raio", 1.10, 100.0)]
    Historico = [{"tipo": "contra", "x64_ms": {"particulas_soa": 300.0}}]
    Pesos = PesosDoTick(Resultados, Historico, {})
    Afirmar(Pesos == {"particulas_soa": 300.0, "aabb_raio": 100.0}, f"pesos do tick: {Pesos}")
    Esperado = (300.0 * 1.0 + 100.0 * Resultados[1]["mediana"]) / 400.0
    Afirmar(abs(CalcularAgregados(Resultados, Pesos)["tick"]["ponto"] - Esperado) < 1e-9, "tick ponderado")
    Afirmar(NomesSemPesoMedido([ResultadoIdentico("fnv_hash"), ResultadoIdentico("aabb_raio")], []) == ["aabb_raio"],
            "tick sem historico precisa do peso avulso")
    Afirmar(TempoDoHistorico([{"bench": "x", "t_exec_ms": {"x64": 7.0}}], "x") == 7.0, "historico de rodada")


def TesteVeredito():
    Contexto = {"sha_base": "aaaa", "sha_novo": "bbbb", "data": "d", "rodada": "r", "versao": VERSAO}
    Bom = [ResultadoIdentico("a"), ResultadoSintetico("b", 0.99)]
    Final, Linhas = MontarVeredito(Contexto, Bom, CalcularAgregados(Bom, {}))
    Afirmar(Final == FINAL_APROVADO and len(Linhas) <= LIMITE_LINHAS_VEREDITO, f"aprovado: {Final} {Linhas}")
    Mau = [ResultadoIdentico("a"), ResultadoSintetico("b", 1.2)]
    Afirmar(MontarVeredito(Contexto, Mau, CalcularAgregados(Mau, {}))[0] == FINAL_REPROVADO, "reprovado")
    Faixa = dict(ResultadoIdentico("a"), veredito=Est.VEREDITO_FAIXA)
    Afirmar(DecidirFinal([Faixa], {"codegen": None, "tick": None}) == FINAL_PENDENTE, "faixa e pendente")
    Linha = MontarLinhaHistorico(Contexto, Bom, CalcularAgregados(Bom, {}), Final)
    Afirmar(Linha["tipo"] == "rapido" and "b" in Linha["benchs"], "linha de historico")


def Selftest():
    for Teste in (TesteGeomediaSemFormatoTexto, TesteTickPesos, TesteVeredito):
        Teste()
    print(f"relatorio_rapido {VERSAO}: selftest OK")
    return True


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(0 if Selftest() else 1)
    print(__doc__)
