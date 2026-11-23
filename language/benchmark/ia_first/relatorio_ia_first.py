# -*- coding: utf-8 -*-
"""relatorio_ia_first.py - metricas B (benchmark com subagentes) recalculadas do BRUTO das celulas (P-IA1-a07, contrato 1 e 4.6).

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao: junta os resultado.json validos de uma ou mais rodadas (a tentativa mais nova de cada
        celula), agrega por modelo x linguagem x braco x conjunto (mediana, media, p90, IC95 por bootstrap reamostrando TAREFAS,
        10.000 reamostras, semente fixa) e emite uma linha {id, valor, meta, verde} por metrica B: B-U6, B-W1..B-W5, B-D9 e
        G-INDICE. Metas que dependem de trabalho futuro (maquina, linha de base, conjuntos P e R, tempo em maquina quieta)
        saem verde=null ("pendente"), nunca verdes de mentira. Indice = 0,4*pass@1 + 0,3*pass@5 + 0,2*(1-min(idas,5)/5)
        + 0,1*(1-min(tokens,150000)/150000), media das tarefas de E, Cx e Dx (idas = idas ate o verde, 5 se nunca).

Depende de: Python 3.9+. Reversao: apagar este arquivo.
"""
from pathlib import Path

import lint_casa
from ia_comum import (LIMITE_IDAS, TOKENS_REF, IcDiferenca, IcMedia, LerJson, Media, Mediana, Metrica, Percentil)

PISO = {"haiku": {"u6": 0.95, "w1": 0.80, "w1m": 0.90, "idx": 0.82}, "sonnet": {"u6": 0.97, "w1": 0.90, "w1m": 0.95, "idx": 0.88}}


def Resultados(PastasRodadas):
    """Todos os resultado.json das rodadas: (validos, invalidos, para)."""
    Validos, Invalidos, Para = [], [], []
    for Pasta in PastasRodadas:
        for Arquivo in sorted((Path(Pasta) / "celulas").glob("*/resultado.json")):
            Res = LerJson(Arquivo)
            (Validos if Res["valida"] else Invalidos).append(Res)
            if Res.get("PARA"):
                Para.append(Res["id"])
    return Validos, Invalidos, Para


def Agrupar(Validos):
    """(modelo, lingua, braco, conjunto) -> {tarefa: [resultados das repeticoes]}."""
    Por = {}
    for Res in Validos:
        Chave = (Res["modelo"], Res["lingua"], Res["braco"], Res["conjunto"])
        Por.setdefault(Chave, {}).setdefault(Res["tarefa"], []).append(Res)
    return Por


def IdasDe(Res):
    return Res["idas_ate_verde"] if Res["idas_ate_verde"] is not None else LIMITE_IDAS


def PorTarefa(Grupo, Funcao):
    """{tarefa: media da Funcao sobre as repeticoes}; valores None sao ignorados."""
    Saida = {}
    for Tarefa, Reps in (Grupo or {}).items():
        Valores = [V for V in (Funcao(R) for R in Reps) if V is not None]
        if Valores:
            Saida[Tarefa] = Media(Valores)
    return Saida


def Celulas(Grupo):
    return [R for Reps in (Grupo or {}).values() for R in Reps]


def ChaveDoGrupo(Dados, Modelo, Lingua, Conj, ChaveBraco="braco_forja"):
    Braco = Dados[ChaveBraco] if Lingua == "forja" else Lingua
    return (Modelo, Lingua, Braco, Conj)


def Fonte(Dados, Modelo, Lingua, Conj):
    return Dados["por"].get(ChaveDoGrupo(Dados, Modelo, Lingua, Conj))


def FonteBase(Dados, Modelo, Lingua, Conj):
    return Dados["base"].get(ChaveDoGrupo(Dados, Modelo, Lingua, Conj, "braco_base"))


def Veredito(Partes):
    """False se alguma parte falhou; None (pendente) se alguma nao pode ser medida; True se todas passaram."""
    if any(P is False for P in Partes.values()):
        return False
    return None if any(P is None for P in Partes.values()) else True


def Pendente(Motivo):
    return {"motivo": Motivo}, None


def MaiorOuIgual(Valor, Limite):
    return None if Valor is None else Valor >= Limite


def NaoInferior(A, B, Margem):
    """True se o limite inferior do IC95 de media(A)-media(B) fica acima de -Margem."""
    Ic = IcDiferenca(A, B)
    return None if Ic is None else Ic[0] > -Margem


def BU6(Dados, Modelo):
    Forja = PorTarefa(Fonte(Dados, Modelo, "forja", "L"), lambda R: float(R["pass1"]))
    C = PorTarefa(Fonte(Dados, Modelo, "c", "L"), lambda R: float(R["pass1"]))
    if not Forja:
        return Pendente("sem celulas L da Forja")
    Partes = {"abs": MaiorOuIgual(Media(list(Forja.values())), PISO[Modelo]["u6"]),
              "contra_c": MaiorOuIgual(Media(list(Forja.values())) - Media(list(C.values())) if C else None, -0.02)}
    return {"forja": Media(list(Forja.values())), "c": Media(list(C.values())) if C else None, "partes": Partes}, Veredito(Partes)


def BW1(Dados, Modelo):
    Forja = PorTarefa(Fonte(Dados, Modelo, "forja", "E"), lambda R: float(R["pass1"]))
    C = PorTarefa(Fonte(Dados, Modelo, "c", "E"), lambda R: float(R["pass1"]))
    if not Forja:
        return Pendente("sem celulas E da Forja")
    Maquina = PorTarefa(Fonte(Dados, Modelo, "forja", "E"), lambda R: float(R["pass1_mais_maquina"]) if R["idas_maquina"] else None)
    Base = PorTarefa(FonteBase(Dados, Modelo, "forja", "E"), lambda R: float(R["pass1"]))
    Partes = {"abs": MaiorOuIgual(Media(list(Forja.values())), PISO[Modelo]["w1"]), "nao_inferior_a_c": NaoInferior(Forja, C, 0.05) if C else None,
              "mais_uma_ida_de_maquina": MaiorOuIgual(Media(list(Maquina.values())) if Maquina else None, PISO[Modelo]["w1m"]),
              "ganho_15pp": MaiorOuIgual(Media(list(Forja.values())) - Media(list(Base.values())) if Base else None, 0.15)}
    return {"pass1_forja": Media(list(Forja.values())), "partes": Partes}, Veredito(Partes)


def BW2(Dados, Modelo):
    Forja = PorTarefa(Fonte(Dados, Modelo, "forja", "E"), lambda R: float(R["pass5"]))
    if not Forja:
        return Pendente("sem celulas E da Forja")
    Valor = Media(list(Forja.values()))
    return {"pass5_forja": Valor}, Valor >= 0.95  # Emenda 3: B-W2 = 0.95


def BW3(Dados, Modelo):
    Idas = [IdasDe(R) for R in Celulas(Fonte(Dados, Modelo, "forja", "E"))]
    if not Idas:
        return Pendente("sem celulas E da Forja")
    Med, P90 = Mediana(Idas), Percentil(Idas, 90)
    return {"mediana": Med, "p90": P90}, Med <= 2 and P90 <= 3


def RazaoDeTokens(Forja, C):
    Razoes = [Forja[T] / C[T] for T in Forja if T in C and C[T] > 0]
    return Mediana(Razoes)


def BW4(Dados, Modelo):
    Tokens = lambda R: R["tokens"]["total"] if R.get("tokens") else None  # lint-ok: iaf-nome-generico seletor curto
    Forja = PorTarefa(Fonte(Dados, Modelo, "forja", "E"), Tokens)
    C = PorTarefa(Fonte(Dados, Modelo, "c", "E"), Tokens)
    Base = PorTarefa(FonteBase(Dados, Modelo, "forja", "E"), Tokens)
    Razao = RazaoDeTokens(Forja, C) if Forja and C else None
    if Razao is None:
        return Pendente("sem tokens (transcript) de Forja e C nas mesmas tarefas")
    Queda = 1 - Media(list(Forja.values())) / Media(list(Base.values())) if Base else None
    Partes = {"razao_forja_c": Razao <= 1.0, "queda_25pct": MaiorOuIgual(Queda, 0.25)}
    return {"razao_mediana": Razao, "queda_vs_base": Queda, "partes": Partes}, Veredito(Partes)


def BW5(Dados, Modelo):
    Cels = Celulas(Fonte(Dados, Modelo, "forja", "E"))
    if not Cels:
        return Pendente("sem celulas E da Forja")
    Congelados = lambda R: [A for A in R["lint_bw5"] if A["simbolo"] in lint_casa.SIMBOLOS_PROIBIDOS]  # lint-ok: iaf-nome-generico seletor curto
    Sujas = sum(1 for R in Cels if Congelados(R))
    SoLibm = sum(1 for R in Cels if R["lint_bw5"] and not Congelados(R))
    return {"solucoes": len(Cels), "com_foreign_proibido": Sujas, "libm_informativo": SoLibm}, Sujas == 0


def BD9(Dados, Modelo):
    Cx, Dx = (Fonte(Dados, Modelo, "forja", C) for C in ("Cx", "Dx"))
    Cf, Df = (Fonte(Dados, Modelo, "forja", C) for C in ("C_forja", "D_forja"))
    if not (Cx and Dx and Cf and Df):
        return Pendente("faltam celulas de Cx, Dx, C_forja ou D_forja")
    Pass1 = lambda R: float(R["pass1"])  # lint-ok: iaf-nome-generico seletor curto
    Pass3 = lambda R: float(R["idas_ate_verde"] is not None and R["idas_ate_verde"] <= 3)  # lint-ok: iaf-nome-generico seletor curto
    CxC, DxC = PorTarefa(Fonte(Dados, Modelo, "c", "Cx"), Pass1), PorTarefa(Fonte(Dados, Modelo, "c", "Dx"), Pass3)
    DxF = PorTarefa(Dx, Pass3)
    Ic = IcDiferenca(DxF, DxC) if DxC else None
    Partes = {"cx_pass1_90": MaiorOuIgual(Media(list(PorTarefa(Cx, Pass1).values())), 0.90),
              "cx_idas_mediana_1": Mediana([IdasDe(R) for R in Celulas(Cx)]) == 1,
              "cx_nao_inferior_a_c": NaoInferior(PorTarefa(Cx, Pass1), CxC, 0.03) if CxC else None,
              "dx_pass3_90": MaiorOuIgual(Media(list(DxF.values())), 0.90), "dx_maior_que_c": None if Ic is None else Ic[0] > 0,
              "c_forja_pass1_90": MaiorOuIgual(Media(list(PorTarefa(Cf, Pass1).values())), 0.90),
              "d_forja_pass3_90": MaiorOuIgual(Media(list(PorTarefa(Df, Pass3).values())), 0.90)}
    return {"partes": Partes}, Veredito(Partes)


def IndiceDaTarefa(Reps):
    Tokens = [R["tokens"]["total"] for R in Reps if R.get("tokens")]
    if len(Tokens) != len(Reps):
        return None
    Idas = Media([IdasDe(R) for R in Reps])
    return (0.4 * Media([float(R["pass1"]) for R in Reps]) + 0.3 * Media([float(R["pass5"]) for R in Reps])
            + 0.2 * (1 - min(Idas, LIMITE_IDAS) / LIMITE_IDAS) + 0.1 * (1 - min(Media(Tokens), TOKENS_REF) / TOKENS_REF))


def IndiceDe(Dados, Modelo, Lingua, Base=False):
    """{(conjunto, tarefa): indice} sobre E, Cx e Dx; vazio se alguma celula nao tem tokens."""
    Saida = {}
    Buscar = FonteBase if Base else Fonte
    for Conj in ("E", "Cx", "Dx"):
        for Tarefa, Reps in (Buscar(Dados, Modelo, Lingua, Conj) or {}).items():
            Valor = IndiceDaTarefa(Reps)
            if Valor is None:
                return {}
            Saida[(Conj, Tarefa)] = Valor
    return Saida


def GIndice(Dados, Modelo):
    Forja, C = IndiceDe(Dados, Modelo, "forja"), IndiceDe(Dados, Modelo, "c")
    if not Forja:
        return Pendente("sem E/Cx/Dx da Forja com tokens medidos")
    Base = IndiceDe(Dados, Modelo, "forja", Base=True)
    Ic = IcDiferenca(Forja, C) if C else None
    Valor = Media(list(Forja.values()))
    Partes = {"piso": Valor >= PISO[Modelo]["idx"], "maior_que_c": None if Ic is None else Ic[0] > 0,
              "ganho_010": MaiorOuIgual(Valor - Media(list(Base.values())) if Base else None, 0.10)}
    return {"indice_forja": Valor, "indice_c": Media(list(C.values())) if C else None, "partes": Partes}, Veredito(Partes)


METRICAS_B = [
    ("B-U6", "L: Forja >= 95% (Haiku) / 97% (Sonnet) e >= C - 2pp", BU6),
    ("B-W1", "E pass@1 >= 80%/90%; nao-inferior a C (5pp); +1 ida de maquina >= 90%/95%; ganho >= 15pp sobre a base", BW1),
    ("B-W2", "E pass@5 >= 98%", BW2), ("B-W3", "idas medianas <= 2 e p90 <= 3", BW3),
    ("B-W4", "tokens Forja/C <= 1,0 e queda >= 25% sobre a base", BW4), ("B-W5", "0% das solucoes com foreign proibido", BW5),
    ("B-D9", "Cx pass@1 >= 90% e idas = 1; Dx pass@3 >= 90% e > C; C_forja >= 90%; D_forja pass@3 >= 90%", BD9),
    ("G-INDICE", "Indice >= 0,82 (Haiku) / 0,88 (Sonnet), > C (IC95) e ganho >= 0,10 sobre a base", GIndice),
]
PENDENTES_B = [("B-W7", "ciclo (check+build+run) Forja x64 <= 0,5 x C -O0", "medido em maquina quieta fora das celulas (12 spawns em paralelo borram o tempo)"),
               ("B-I4", "conjunto P: acuracia >= 95% com tokens <= 40%", "conjunto P (a17) ainda nao existe"),
               ("B-E5", "conjunto R: pass@1 >= 90% com forja editar", "conjunto R (a17) e forja editar ainda nao existem")]


def Combinar(Veredictos):
    if any(V is False for V in Veredictos):
        return False
    return None if any(V is None for V in Veredictos) or not Veredictos else True


def Metricas(Dados):
    Modelos = sorted({Chave[0] for Chave in Dados["por"]})
    Linhas = []
    for Id, Meta, Funcao in METRICAS_B:
        Pares = {M: Funcao(Dados, M) for M in Modelos if M in PISO}
        Linhas.append(Metrica(Id, {M: P[0] for M, P in Pares.items()} or "sem dados", Meta,
                              Combinar([P[1] for P in Pares.values()]), modelos=sorted(Pares)))
    Linhas += [Metrica(Id, "pendente", Meta, None, depende=Motivo) for Id, Meta, Motivo in PENDENTES_B]
    return Linhas


def Resumo(Chave, Grupo):
    Cels = Celulas(Grupo)
    Pass1 = PorTarefa(Grupo, lambda R: float(R["pass1"]))
    Tokens = [R["tokens"]["total"] for R in Cels if R.get("tokens")]
    return {"modelo": Chave[0], "lingua": Chave[1], "braco": Chave[2], "conjunto": Chave[3], "tarefas": len(Grupo), "celulas": len(Cels),
            "pass1": Media(list(Pass1.values())), "pass1_ic95": IcMedia(Pass1),
            "pass5": Media(list(PorTarefa(Grupo, lambda R: float(R["pass5"])).values())),
            "idas_mediana": Mediana([IdasDe(R) for R in Cels]), "idas_p90": Percentil([IdasDe(R) for R in Cels], 90),
            "tokens_mediana": Mediana(Tokens), "parede_mediana_s": Mediana([R["parede_s"] for R in Cels if R.get("parede_s") is not None])}


def Backlog(Validos):
    """Histograma de codigos vistos nas idas da Forja (o backlog do compilador)."""
    Contagem = {}
    for Res in (R for R in Validos if R["lingua"] == "forja"):
        for Codigo, Vezes in Res["codigos"].items():
            Contagem[Codigo] = Contagem.get(Codigo, 0) + Vezes
    return dict(sorted(Contagem.items(), key=lambda Par: -Par[1]))


def Relatorio(PastasRodadas, PastasBase=(), BracoForja="P0", BracoBase=None):
    """Todas as linhas do relatorio: metricas B e, por ultimo, a linha {"grupos": ...}."""
    Validos, Invalidos, Para = Resultados(PastasRodadas)
    Base = Resultados(PastasBase)[0] if PastasBase else []
    Dados = {"por": Agrupar(Validos), "base": Agrupar(Base), "braco_forja": BracoForja,
             "braco_base": BracoBase or BracoForja}
    Linhas = Metricas(Dados)
    Linhas.append({"grupos": [Resumo(C, G) for C, G in sorted(Dados["por"].items())], "backlog_codigos": Backlog(Validos),
                   "celulas_validas": len(Validos), "celulas_invalidas": len(Invalidos), "PARA": Para,
                   "sem_contaminacao_verificada": sum(1 for R in Validos if not R["contaminacao_verificada"])})
    return Linhas
