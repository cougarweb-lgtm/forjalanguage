# -*- coding: utf-8 -*-
"""Estatistica do medir_rapido: IC por estatistica de ordem, vereditos, bimodal, geomedias e bootstrap.

Razao pareada = tempoB/tempoA do mesmo par (novo/base). IC por ESTATISTICA DE ORDEM das razoes ordenadas, so em 3 olhadas
(1-indexado): 12 pares -> [3o; 10o] (96,1%); 24 -> [7o; 18o] (97,7%); 36 -> [12o; 25o] (97,1%).
Veredito por bench: IC sup <= 1,03 APROVA; IC inf > 1,05 REPROVA; IC inteiro em [1,03; 1,05] FAIXA; indeciso pede mais pares.
Bootstrap dos pares (semente fixa, 1000 reamostragens, percentis 2,5 e 97,5) para as geomedias e o tick.

Uso: importado por medir_rapido.py.
Verificacao: python -I -B estat_rapido.py --selftest
Versao: 1.0.0
Changelog: 1.0.0 - primeira versao (decisao D0696).
"""

import math
import random
import statistics
import sys

VERSAO = "1.0.0"
OLHADAS = {12: (3, 10), 24: (7, 18), 36: (12, 25)}
PASSO_PARES = 12
MAX_PARES = 36
LIMITE_APROVA = 1.03
LIMITE_REPROVA = 1.05
LIMITE_BIMODAL = 0.02
VEREDITO_APROVA = "APROVA"
VEREDITO_REPROVA = "REPROVA"
VEREDITO_FAIXA = "FAIXA 3-5% (exige causa em asm)"
VEREDITO_INCONCLUSIVO = "INCONCLUSIVO"
VEREDITO_IDENTICO = "IDENTICO"
BOOTSTRAP_REAMOSTRAGENS = 1000
BOOTSTRAP_SEMENTE = 20261117
CODEGEN_PONTO, CODEGEN_SUP = 1.005, 1.010
TICK_PONTO, TICK_SUP = 1.005, 1.015


class ErroEstatistica(Exception):
    """Entrada sem base valida (lista vazia, razao <= 0, numero de pares fora das olhadas)."""


def Mediana(Valores):
    if not Valores:
        raise ErroEstatistica("mediana sem valores")
    return statistics.median(Valores)


def MediaGeometrica(Razoes):
    # Falha explicita sem base valida: nunca 0.0 (portao vazio).
    if not Razoes or any(R <= 0 for R in Razoes):
        raise ErroEstatistica(f"media geometrica sem base valida (razoes={Razoes})")
    return math.exp(sum(math.log(R) for R in Razoes) / len(Razoes))


def IcPorOrdem(Razoes):
    """IC [inf, sup] pela estatistica de ordem; so existe para 12, 24 ou 36 pares."""
    if len(Razoes) not in OLHADAS:
        raise ErroEstatistica(f"IC por ordem exige 12/24/36 pares, veio {len(Razoes)}")
    Ordenadas = sorted(Razoes)
    Inf, Sup = OLHADAS[len(Razoes)]
    return Ordenadas[Inf - 1], Ordenadas[Sup - 1]


def Bimodal(TemposA, TemposB, Razoes):
    """True se |mediana das razoes - razao dos minimos| > 2% (par instavel: vai direto a 36 pares)."""
    if not TemposA or not TemposB or min(TemposA) <= 0:
        raise ErroEstatistica("bimodal sem tempos validos")
    return abs(Mediana(Razoes) - min(TemposB) / min(TemposA)) > LIMITE_BIMODAL


def VereditoDoIc(Ic):
    """APROVA / REPROVA / FAIXA, ou None se o IC ainda nao decide."""
    Inf, Sup = Ic
    if Sup <= LIMITE_APROVA:
        return VEREDITO_APROVA
    if Inf > LIMITE_REPROVA:
        return VEREDITO_REPROVA
    if Inf >= LIMITE_APROVA and Sup <= LIMITE_REPROVA:
        return VEREDITO_FAIXA
    return None


def AvaliarPares(Razoes, Bimodais=False):
    """(veredito ou None, IC ou None): None = faltam pares. Aos 36 sem decisao: INCONCLUSIVO. Bimodal so decide aos 36."""
    Total = len(Razoes)
    if Total not in OLHADAS or (Bimodais and Total < MAX_PARES):
        return None, None
    Ic = IcPorOrdem(Razoes)
    Veredito = VereditoDoIc(Ic)
    if Veredito is None and Total == MAX_PARES:
        Veredito = VEREDITO_INCONCLUSIVO
    return Veredito, Ic


def CalibracaoOk(Ic):
    """A==A: IC dentro de [0,97; 1,03] e contendo 1,000."""
    Inf, Sup = Ic
    return Inf >= 0.97 and Sup <= 1.03 and Inf <= 1.0 <= Sup


def PercentilInterpolado(Ordenados, Fracao):
    Posicao = Fracao * (len(Ordenados) - 1)
    Baixo = int(math.floor(Posicao))
    Alto = min(Baixo + 1, len(Ordenados) - 1)
    return Ordenados[Baixo] + (Ordenados[Alto] - Ordenados[Baixo]) * (Posicao - Baixo)


def Bootstrap(Pares, Metrica, *, Semente=BOOTSTRAP_SEMENTE, Reamostragens=BOOTSTRAP_REAMOSTRAGENS):
    """Pares = {bench: [razoes pareadas]}; Metrica({bench: razao mediana}) -> numero. Devolve (ponto, IC inf, IC sup).

    Cada reamostragem sorteia pares com reposicao dentro de cada bench; o gerador local com semente fixa torna o IC reprodutivel.
    """
    if not Pares or any(not V for V in Pares.values()):
        raise ErroEstatistica("bootstrap sem pares")
    Sorteio = random.Random(Semente)
    Ponto = Metrica({N: Mediana(V) for N, V in Pares.items()})
    Valores = []
    for _ in range(Reamostragens):
        Medianas = {N: Mediana(Sorteio.choices(V, k=len(V))) for N, V in Pares.items()}
        Valores.append(Metrica(Medianas))
    Valores.sort()
    return Ponto, PercentilInterpolado(Valores, 0.025), PercentilInterpolado(Valores, 0.975)


def MetricaGeomedia(Excluir=()):
    """Metrica de geomedia das razoes por bench, sem os nomes de Excluir (formato_texto no codegen)."""
    def Calcular(Medianas):
        return MediaGeometrica([R for N, R in Medianas.items() if N not in Excluir])
    return Calcular


def MetricaTick(Pesos):
    """Razao dos tempos somados: soma(peso*razao)/soma(peso) nos benches do tick; Pesos = {bench: tempo de referencia}."""
    def Calcular(Medianas):
        Total = sum(Pesos.values())
        return sum(Pesos[N] * Medianas[N] for N in Pesos) / Total
    return Calcular


def CriterioCodegen(Ponto, Sup):
    return Ponto <= CODEGEN_PONTO and Sup <= CODEGEN_SUP


def CriterioTick(Ponto, Sup):
    return Ponto <= TICK_PONTO and Sup <= TICK_SUP


def Afirmar(Condicao, Mensagem):
    if not Condicao:
        raise AssertionError(Mensagem)


def TesteIcOrdem():
    Razoes = [1.0 + I / 1000.0 for I in range(12)]
    Afirmar(IcPorOrdem(Razoes) == (Razoes[2], Razoes[9]), "12 pares: 3o e 10o")
    Razoes = [1.0 + I / 1000.0 for I in range(24)]
    Afirmar(IcPorOrdem(list(reversed(Razoes))) == (Razoes[6], Razoes[17]), "24 pares: 7o e 18o (embaralhado)")
    Razoes = [1.0 + I / 1000.0 for I in range(36)]
    Afirmar(IcPorOrdem(Razoes) == (Razoes[11], Razoes[24]), "36 pares: 12o e 25o")
    try:
        IcPorOrdem([1.0] * 13)
    except ErroEstatistica:
        return
    Afirmar(False, "13 pares deveria falhar")


def TesteVeredito():
    Afirmar(VereditoDoIc((0.99, 1.03)) == VEREDITO_APROVA, "sup 1,03 aprova")
    Afirmar(VereditoDoIc((0.99, 1.0301)) is None, "sup acima de 1,03 e inf baixo: indeciso")
    Afirmar(VereditoDoIc((1.0501, 1.20)) == VEREDITO_REPROVA, "inf > 1,05 reprova")
    Afirmar(VereditoDoIc((1.05, 1.20)) is None, "inf = 1,05 nao reprova")
    Afirmar(VereditoDoIc((1.031, 1.05)) == VEREDITO_FAIXA, "IC inteiro entre 1,03 e 1,05: faixa")
    Afirmar(VereditoDoIc((1.029, 1.04)) is None, "inf abaixo de 1,03: nao e faixa")


def TesteAvaliar():
    Afirmar(AvaliarPares([1.0] * 11) == (None, None), "11 pares: ainda nao olha")
    Afirmar(AvaliarPares([1.0] * 12)[0] == VEREDITO_APROVA, "12 pares estaveis aprovam")
    Afirmar(AvaliarPares([1.0] * 12, Bimodais=True) == (None, None), "bimodal so decide aos 36")
    Largo = [0.9 + 0.2 * I / 35 for I in range(36)]
    Afirmar(AvaliarPares(Largo)[0] == VEREDITO_INCONCLUSIVO, "36 pares sem decisao: inconclusivo")


def TesteBimodal():
    A = [100.0] * 12
    Afirmar(not Bimodal(A, [101.0] * 12, [1.01] * 12), "estavel nao e bimodal")
    Afirmar(Bimodal(A, [100.0] * 12, [1.10] * 12), "mediana longe da razao dos minimos e bimodal")


def TesteGeomedia():
    Afirmar(abs(MediaGeometrica([2.0, 8.0]) - 4.0) < 1e-12, "geomedia de 2 e 8 = 4")
    Medianas = {"formato_texto": 10.0, "a": 1.0, "b": 1.0}
    Afirmar(MetricaGeomedia(("formato_texto",))(Medianas) == 1.0, "codegen sem formato_texto")
    Afirmar(MetricaGeomedia()(Medianas) > 1.0, "total com formato_texto")
    Afirmar(abs(MetricaTick({"a": 1.0, "b": 3.0})({"a": 2.0, "b": 1.0}) - 1.25) < 1e-12, "tick pondera pelo peso")


def TesteBootstrap():
    Pares = {"a": [1.0 + (I * 7 % 36) / 900.0 for I in range(36)], "b": [1.0 + (I * 5 % 36) / 1500.0 for I in range(36)]}
    Um = Bootstrap(Pares, MetricaGeomedia())
    Dois = Bootstrap(Pares, MetricaGeomedia())
    Afirmar(Um == Dois, "bootstrap com semente fixa e reprodutivel")
    Afirmar(Um[1] <= Um[2], "IC ordenado")
    Afirmar(Bootstrap(Pares, MetricaGeomedia(), Semente=1) != Um, "outra semente muda o IC")
    Afirmar(CriterioCodegen(1.004, 1.010) and not CriterioCodegen(1.006, 1.007), "criterio codegen")
    Afirmar(CriterioTick(1.005, 1.015) and not CriterioTick(1.005, 1.016), "criterio tick")
    Afirmar(CalibracaoOk((0.98, 1.02)) and not CalibracaoOk((1.001, 1.02)) and not CalibracaoOk((0.96, 1.02)), "calibracao")


def Selftest():
    for Teste in (TesteIcOrdem, TesteVeredito, TesteAvaliar, TesteBimodal, TesteGeomedia, TesteBootstrap):
        Teste()
    print(f"estat_rapido {VERSAO}: selftest OK")
    return True


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(0 if Selftest() else 1)
    print(__doc__)
