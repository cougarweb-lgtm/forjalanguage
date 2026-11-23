"""Casos de cli do forja consultar (IA1-I2): as 6 perguntas em JSON, determinismo e usos errados.

Importado pelo motor verificar_cli.py; a prova ampla (gabarito de 60+ pontos, M-U4, M-I3 e o confronto de todo uso do 'forja ast' com 'consultar refs')
mora em tests\\consultar\\verificar_consultar.py; aqui ficam os casos curtos do motor, sobre o projeto tests\\consultar\\proj.

Regras:
  - cada pergunta devolve 1 linha de JSON com consultar_versao e pergunta; duas execucoes dao bytes iguais;
  - simbolos lista as declaracoes (categoria, entrada main); assinatura traz params e retorno; campos traz os campos de struct e os membros de enum; refs traz alvos com usos;
  - pergunta desconhecida, falta de argumento, nome ou posicao inexistente, opcao fora de lugar = exit 2; projeto com erro = exit 1 com o diagnostico do check.

Versao: 1.2.0
Changelog: 1.2.0 - 0.25.1: CasoE1001ListaTodosOsModulos aceita a lista cortada pela marca ' (+N)' (os vendors do PREPARO passaram do buffer de 440 bytes; a causa do Diag tem 512): nenhum nome inventado, std inteira, listados + N = modulos reais; MarcaDeCorte; DisponiveisDaCausa ignora a marca.
           1.1.0 - M6b P79: +9 casos da descoberta sem arquivo hospedeiro (api std:/vendor: sem alvo, E1001 com a lista completa e o parecido, spec --receitas).
           1.0.0 - IA1-I2: primeira versao (3 casos; projeto tests\\consultar\\proj e fixture erro_tipo.fj).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json
import re
from pathlib import Path

from verificar_cli import ConferirExecucao, PastaAqui, Rodar

PROJETO = str(PastaAqui.parent / "consultar" / "proj")
MAIN = str(PastaAqui.parent / "consultar" / "proj" / "main.fj")
RAIZ_LANGUAGE = PastaAqui.parent.parent  # language\: tem vendor\ e doc\, e e a pasta atual do vendor: sem arquivo


def ConsultaDe(Forja, Argumentos, Problemas):
    """O JSON de forja consultar <Argumentos>, ou None (e um problema anotado)."""
    Rodada = Rodar(Forja, ["consultar"] + Argumentos)
    Problemas.extend(ConferirExecucao(Rodada, 0))
    try:
        return json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        Problemas.append(f"forja consultar {' '.join(Argumentos)} nao devolveu JSON")
        return None


def CasoConsultarPerguntas(Forja, PastaSaidas):
    Problemas = []
    Simbolos = ConsultaDe(Forja, ["simbolos", PROJETO, "--json"], Problemas)
    Assinatura = ConsultaDe(Forja, ["assinatura", "somar", PROJETO, "--json"], Problemas)
    Campos = ConsultaDe(Forja, ["campos", "Mundo", PROJETO, "--json"], Problemas)
    Refs = ConsultaDe(Forja, ["refs", "somar", PROJETO, "--json"], Problemas)
    if None in (Simbolos, Assinatura, Campos, Refs):
        return Problemas
    for Resposta, Pergunta in ((Simbolos, "simbolos"), (Assinatura, "assinatura"), (Campos, "campos"), (Refs, "refs")):
        if Resposta.get("pergunta") != Pergunta or "consultar_versao" not in Resposta:
            Problemas.append(f"{Pergunta}: resposta sem pergunta/consultar_versao: {list(Resposta)}")
    Categorias = {S["nome"]: S["categoria"] for S in Simbolos["simbolos"]}
    if Categorias.get("main") != "proc" or Categorias.get("Mundo") != "struct" or Categorias.get("Erro") != "enum":
        Problemas.append(f"simbolos sem main/Mundo/Erro com a categoria certa: {Categorias}")
    if [P["nome"] for P in Assinatura["params"]] != ["a", "b"] or Assinatura["retorno"] != "i32":
        Problemas.append(f"assinatura de somar fora do esperado: {Assinatura['params']} -> {Assinatura['retorno']}")
    if not Campos["campos"] or Refs["alvos"][0]["total"] < 1:
        Problemas.append("campos de Mundo vazio ou refs de somar sem uso")
    return Problemas


def CasoConsultarPosicoes(Forja, PastaSaidas):
    Problemas = []
    Primeira = Rodar(Forja, ["consultar", "tipo", f"{MAIN}:12:6", "--json"])
    Segunda = Rodar(Forja, ["consultar", "tipo", f"{MAIN}:12:6", "--json"])
    Problemas += ConferirExecucao(Primeira, 0)
    if Primeira.stdout != Segunda.stdout:
        Problemas.append("duas execucoes de consultar tipo deram bytes diferentes")
    Escopo = ConsultaDe(Forja, ["escopo", f"{MAIN}:13:5", "--json"], Problemas)
    if Escopo is not None and "locais" not in Escopo:
        Problemas.append(f"escopo sem a chave 'locais': {list(Escopo)}")
    return Problemas


def CasoConsultarUsoErrado(Forja, PastaSaidas):
    Problemas = []
    Desconhecida = Rodar(Forja, ["consultar", "nada", PROJETO])
    Problemas += ConferirExecucao(Desconhecida, 2)
    if "simbolos" not in Desconhecida.stderr or "campos" not in Desconhecida.stderr:
        Problemas.append("pergunta desconhecida nao lista as perguntas no stderr")
    for Argumentos in (["refs", "nao_existe", PROJETO], ["assinatura", "Mundo", PROJETO], ["tipo", "sem_posicao", PROJETO], ["simbolos", "--modulo", "x:y", PROJETO],
                       ["campos", PROJETO], ["simbolos", "--zz"], []):
        Problemas += [f"{' '.join(Argumentos)}: {P}" for P in ConferirExecucao(Rodar(Forja, ["consultar"] + Argumentos), 2)]
    Problemas += ConferirExecucao(Rodar(Forja, ["consultar", "simbolos", "erro_tipo.fj"]), 1)
    return Problemas


# ---------- P79 (D0653-2): descoberta sem arquivo hospedeiro ----------

def CausaDoStdout(Texto):
    """O campo causa do diagnostico JSON impresso (ou None)."""
    Achado = re.search(r'"causa": "([^"]*)"', Texto)
    return Achado.group(1) if Achado else None


def ModulosDaStd():
    """Os nomes da tabela STD de frontend\\modulos_std.c (a fonte da verdade), com o prefixo std:."""
    Fonte = (RAIZ_LANGUAGE / "compiler" / "frontend" / "modulos_std.c").read_text(encoding="utf-8")
    return {f"std:{Nome}" for Nome in re.findall(r'\{"(\w+)", forja_std_\w+_ini', Fonte)}


def ModulosDoVendor():
    """Os .fj de language\\vendor, como vendor:<caminho com / sem .fj> (o que o E1001 lista)."""
    Pasta = RAIZ_LANGUAGE / "vendor"
    return {"vendor:" + Arq.relative_to(Pasta).as_posix()[:-3] for Arq in Pasta.rglob("*.fj")}


PADRAO_MARCA_DE_CORTE = re.compile(r" \(\+(\d+)\)\)?$")


def MarcaDeCorte(Causa):
    """N da marca ' (+N)' que o compilador poe quando a lista de modulos nao cabe no buffer; 0 se a lista veio inteira."""
    Achado = PADRAO_MARCA_DE_CORTE.search(Causa or "")
    return int(Achado.group(1)) if Achado else 0


def DisponiveisDaCausa(Causa):
    """Os modulos que a causa lista depois de 'disponiveis: ' (ou None se nao lista); a marca ' (+N)' de corte nao conta como modulo."""
    if Causa is None or "disponiveis: " not in Causa:
        return None
    Achado = PADRAO_MARCA_DE_CORTE.search(Causa)
    if Achado:
        Causa = Causa[:Achado.start()]
    Lista = Causa.split("disponiveis: ", 1)[1].rstrip(")")
    Nomes = set()
    for Grupo in Lista.split("; "):
        Prefixo, _, Itens = Grupo.partition(": ")
        Nomes |= {f"{Prefixo}:{Item.strip()}" for Item in Itens.split(",") if Item.strip()}
    return Nomes


def CasoApiSemHostStd(Forja, PastaSaidas):
    Problemas = []
    for Modulo, Trecho in (("std:mem", "Arena"), ("std:text", "split"), ("std:math", "sin"), ("std:json", "parse")):
        Rodada = Rodar(Forja, ["consultar", "api", Modulo], Pasta=RAIZ_LANGUAGE)
        Problemas += [f"{Modulo}: {P}" for P in ConferirExecucao(Rodada, 0, TrechoStdout=Trecho)]
    return Problemas


def CasoApiSemHostVendor(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["consultar", "api", "vendor:json/cjson"], Pasta=RAIZ_LANGUAGE)
    return ConferirExecucao(Rodada, 0, TrechoStdout="CJson")


def CasoApiSemHostJson(Forja, PastaSaidas):
    Problemas = []
    Dados = ConsultaDe(Forja, ["api", "std:json", "--json"], Problemas)
    if Dados is not None and (Dados.get("pergunta") != "api" or not Dados.get("api")):
        Problemas.append(f"api sem host: JSON sem pergunta 'api' ou sem itens: {list(Dados)}")
    Primeira = Rodar(Forja, ["consultar", "api", "std:json", "--json"], Pasta=RAIZ_LANGUAGE)
    Segunda = Rodar(Forja, ["consultar", "api", "std:json", "--json"], Pasta=PastaAqui)
    if Primeira.stdout != Segunda.stdout:
        Problemas.append("api sem host: duas execucoes (cwds diferentes) deram bytes diferentes")
    return Problemas


def CasoE1001ListaTodosOsModulos(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["consultar", "api", "std:nomeinexistente"], Pasta=RAIZ_LANGUAGE)
    Problemas = ConferirExecucao(Rodada, 1)
    Obtidos = DisponiveisDaCausa(CausaDoStdout(Rodada.stdout))
    if Obtidos is None:
        return Problemas + ["E1001 de std:nomeinexistente sem a lista 'disponiveis:'"]
    # A lista cabe num buffer fixo (LISTA_MODULOS_TAM, 440 bytes; a causa do Diag tem 512): com os vendors do PREPARO ela passa disso e o compilador
    # corta com a marca ' (+N)'. O corte e o desenho previsto, entao o contrato e: nenhum nome inventado, a std inteira presente (vem 1o) e
    # modulos listados + N da marca = todos os modulos reais (nada some sem ser contado).
    Causa = CausaDoStdout(Rodada.stdout) or ""
    Corte = MarcaDeCorte(Causa)
    EsperadosStd = ModulosDaStd()
    Esperados = EsperadosStd | ModulosDoVendor()
    if Obtidos - Esperados:
        Problemas.append(f"lista do E1001 tem nome que nao existe: {sorted(Obtidos - Esperados)}")
    if EsperadosStd - Obtidos:
        Problemas.append(f"lista do E1001 sem modulos da std: {sorted(EsperadosStd - Obtidos)}")
    if len(Obtidos) + Corte != len(Esperados):
        Problemas.append(f"lista do E1001 nao fecha a conta: {len(Obtidos)} listados + {Corte} da marca != {len(Esperados)} modulos reais")
    return Problemas


def CasoE1001ParecidoTexto(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["consultar", "api", "std:texto"], Pasta=RAIZ_LANGUAGE)
    Problemas = ConferirExecucao(Rodada, 1)
    if "quis dizer std:text?" not in (CausaDoStdout(Rodada.stdout) or ""):
        Problemas.append("std:texto sem 'quis dizer std:text?' na causa")
    return Problemas


def CasoE1001ParecidoVendor(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["consultar", "api", "vendor:jsn/cjson"], Pasta=RAIZ_LANGUAGE)
    Problemas = ConferirExecucao(Rodada, 1)
    if "quis dizer vendor:json/cjson?" not in (CausaDoStdout(Rodada.stdout) or ""):
        Problemas.append("vendor:jsn/cjson sem 'quis dizer vendor:json/cjson?' na causa")
    return Problemas


def CasoE1001SemParecidoNaoSugere(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["consultar", "api", "std:nomeinexistente"], Pasta=RAIZ_LANGUAGE)
    Problemas = ConferirExecucao(Rodada, 1)
    if "quis dizer" in (CausaDoStdout(Rodada.stdout) or ""):
        Problemas.append("std:nomeinexistente sugeriu um parecido que nao existe")
    return Problemas


def CasoReceitasEmbutido(Forja, PastaSaidas):
    Rodada = Rodar(Forja, ["spec", "--receitas"])
    Problemas = ConferirExecucao(Rodada, 0)
    Doc = (RAIZ_LANGUAGE / "doc" / "receitas.txt").read_text(encoding="utf-8")
    if Rodada.stdout != Doc:
        Problemas.append("spec --receitas difere de doc\\receitas.txt (bytes do texto embutido)")
    return Problemas


def CasoReceitasUsoErrado(Forja, PastaSaidas):
    return ConferirExecucao(Rodar(Forja, ["spec", "--receitas", "--json"]), 2)
