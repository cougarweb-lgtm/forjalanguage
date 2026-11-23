"""Casos de cli do forja mapa e das perguntas da parte 2 do forja consultar (IA1-I3): callers, callees, imports, api, testes, entrada e o mapa em JSON e em resumo.

Importado pelo motor verificar_cli.py; a prova ampla (gabarito de dezenas de pontos, tamanhos do resumo, tempo em engine\\forja) mora em tests\\consultar\\verificar_mapa.py;
aqui ficam os casos curtos do motor, sobre os projetos tests\\consultar\\proj e proj2.

Regras:
  - cada pergunta devolve 1 linha de JSON com consultar_versao e pergunta; callers/callees/imports/api/testes/entrada trazem as chaves de cada uma;
  - mapa --json e 1 linha de JSON (tamanho, entrada, modulos, api, tipos, testes, dependencias); mapa --resumo (e sem opcao) e texto que comeca por "mapa <raiz>:"; 2 execucoes = bytes iguais;
  - uso errado (opcao desconhecida, --json com --resumo, --limite ruim, --modulo inexistente, pergunta sem argumento) = exit 2; projeto com erro = exit 1 com o diagnostico do check.

Versao: 1.0.0
Changelog: 1.0.0 - IA1-I3: primeira versao (3 casos).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json

from verificar_cli import ConferirExecucao, PastaAqui, Rodar

PROJETO = str(PastaAqui.parent / "consultar" / "proj")
PROJETO2 = str(PastaAqui.parent / "consultar" / "proj2")
PERGUNTAS_NOVAS = ("callers", "callees", "imports", "api", "testes", "entrada")


def JsonDe(Forja, Argumentos, Problemas):
    """O JSON de forja <Argumentos> (exit 0), ou None (e um problema anotado)."""
    Rodada = Rodar(Forja, Argumentos)
    Problemas.extend(ConferirExecucao(Rodada, 0))
    try:
        return json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        Problemas.append(f"forja {' '.join(Argumentos)} nao devolveu JSON")
        return None


def CasoConsultarParte2(Forja, PastaSaidas):
    Problemas = []
    Callers = JsonDe(Forja, ["consultar", "callers", "somar", PROJETO, "--json"], Problemas)
    Callees = JsonDe(Forja, ["consultar", "callees", "main", PROJETO], Problemas)
    Imports = JsonDe(Forja, ["consultar", "imports", PROJETO2], Problemas)
    Api = JsonDe(Forja, ["consultar", "api", "lib/util", PROJETO2], Problemas)
    Testes = JsonDe(Forja, ["consultar", "testes", PROJETO], Problemas)
    Entrada = JsonDe(Forja, ["consultar", "entrada", PROJETO2], Problemas)
    if None in (Callers, Callees, Imports, Api, Testes, Entrada):
        return Problemas
    for Resposta, Pergunta in ((Callers, "callers"), (Callees, "callees"), (Imports, "imports"), (Api, "api"), (Testes, "testes"), (Entrada, "entrada")):
        if Resposta.get("pergunta") != Pergunta or "consultar_versao" not in Resposta:
            Problemas.append(f"{Pergunta}: resposta sem pergunta/consultar_versao: {list(Resposta)}")
    if Callers["chamadores"] != ["main:main", "main:test:somar usa o padrao"] or Callers["total"] != 3:
        Problemas.append(f"callers de somar fora do esperado: {Callers['chamadores']} total {Callers['total']}")
    if [C["nome"] for C in Callees["chamados"]][:2] != ["somar", "dividir"]:
        Problemas.append(f"callees de main fora do esperado: {[C['nome'] for C in Callees['chamados']]}")
    if [M["modulo"] for M in Imports["modulos"]] != ["main", "lib/nativo", "lib/util"] or [L["lib"] for L in Imports["links"]] != ["kernel32"]:
        Problemas.append(f"imports de proj2 fora do esperado: {[M['modulo'] for M in Imports['modulos']]} links {Imports['links']}")
    if [A["nome"] for A in Api["api"]] != ["LIMITE", "dobro", "Par", "Modo"]:
        Problemas.append(f"api de lib/util fora do esperado: {[A['nome'] for A in Api['api']]}")
    if [T["nome"] for T in Testes["testes"]] != ["somar usa o padrao", "dividir"] or [E["entrada"] for E in Entrada["entradas"]] != ["export", "main"]:
        Problemas.append("testes de proj ou entrada de proj2 fora do esperado")
    return Problemas


def CasoMapaSaidas(Forja, PastaSaidas):
    Problemas = []
    Json = JsonDe(Forja, ["mapa", PROJETO2, "--json"], Problemas)
    Resumo = Rodar(Forja, ["mapa", PROJETO2, "--resumo"])
    Padrao = Rodar(Forja, ["mapa", PROJETO2])
    Outra = Rodar(Forja, ["mapa", PROJETO2, "--resumo"])
    Problemas += ConferirExecucao(Resumo, 0)
    if Json is not None:
        Faltam = [C for C in ("tamanho", "entrada", "modulos", "api", "tipos", "testes", "dependencias") if C not in Json]
        if Json.get("pergunta") != "mapa" or Faltam:
            Problemas.append(f"mapa --json sem as chaves {Faltam} (pergunta {Json.get('pergunta')})")
    if not Resumo.stdout.startswith("mapa main:"):
        Problemas.append(f"mapa --resumo nao comeca por 'mapa main:': {Resumo.stdout[:40]!r}")
    if Resumo.stdout != Padrao.stdout or Resumo.stdout != Outra.stdout:
        Problemas.append("mapa sem opcao, --resumo e uma 2a execucao deveriam dar os mesmos bytes")
    Modulo = Rodar(Forja, ["mapa", PROJETO2, "--modulo", "lib/util", "--limite", "1"])
    Problemas += ConferirExecucao(Modulo, 0)
    if "modulo lib/util" not in Modulo.stdout or "+" not in Modulo.stdout:
        Problemas.append("mapa --modulo lib/util --limite 1 sem o cabecalho do modulo ou sem o corte '+K'")
    return Problemas


def CasoMapaUsoErrado(Forja, PastaSaidas):
    Problemas = []
    Desconhecida = Rodar(Forja, ["consultar", "nada", PROJETO])
    Problemas += ConferirExecucao(Desconhecida, 2)
    Faltam = [P for P in PERGUNTAS_NOVAS if P not in Desconhecida.stderr]
    if Faltam:
        Problemas.append(f"pergunta desconhecida nao lista {Faltam} no stderr")
    Erros = [["consultar", "callers", PROJETO], ["consultar", "callers", "Mundo", PROJETO], ["consultar", "callees", "Mundo", PROJETO], ["consultar", "api", "nao/existe", PROJETO],
             ["consultar", "imports", "--modulo", "main", PROJETO], ["consultar", "testes", PROJETO, "sobra"], ["mapa", "--zz"], ["mapa", "--json", "--resumo"],
             ["mapa", PROJETO, "--limite"], ["mapa", PROJETO, "--limite", "0"], ["mapa", PROJETO, "--json", "--limite", "3"], ["mapa", PROJETO, "--modulo", "nao/existe"],
             ["mapa", PROJETO, PROJETO2], ["mapa", str(PastaAqui)]]
    for Argumentos in Erros:
        Problemas += [f"{' '.join(Argumentos[:3])}: {P}" for P in ConferirExecucao(Rodar(Forja, Argumentos), 2)]
    Problemas += ConferirExecucao(Rodar(Forja, ["mapa", "erro_tipo.fj"]), 1)
    return Problemas
