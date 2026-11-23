# -*- coding: utf-8 -*-
"""verificar_m_d4.py -- IA1-L2: metricas M-D4 e M-D10 (nos idiomas) do contrato. M-D4: cada uma das 43 entradas da tabela 3.4 (idiomas de outras linguagens) tem ao
menos um caso, e todo caso tem como 1o diagnostico o E0110 (ou o codigo proprio do caso: E0307 e E1001, que ja traziam a forma certa) na linha certa, com a forma da
Forja na forma_certa, uma sugestao e NENHUM outro diagnostico (0 enganoso). M-D10: aplicar so as correcoes "certa" (`forja corrigir --aplicar`) deixa o texto
exatamente igual ao programa certo e o programa certo passa no check, em >= 95% dos casos "certa"; as formas so_dica tem o programa certo conferido a mao.

Uso:
    python -I verificar_m_d4.py [--forja <forja.exe>] [--selftest]
Exit 0 se todos os casos passam e M-D10 >= 95%, 1 senao (uma linha por caso; as falhas dizem o que esperava e o que veio).
Casos em casos_m_d4.py (dados puros). Reaproveita Rodar, LerDiagnosticos, ConferirCerto e ConferirCorrigir do verificar_e0110_l1.py.
Depende de: so a biblioteca padrao.
Versao: 0.1.0
Changelog: 0.1.0 - IA1-L2: primeira versao.
Verificacao: python -I verificar_m_d4.py --selftest   |   python -I verificar_m_d4.py --forja <forja.exe>
"""

import argparse
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
from casos_m_d4 import CASOS, ENTRADAS  # noqa: E402
from verificar_e0110_l1 import ConferirCerto, ConferirCorrigir, FORJA_PADRAO, LerDiagnosticos, Rodar  # noqa: E402

META_M_D10 = 0.95
MINIMO_DE_ENTRADAS = 24


def AvaliarDiagnosticos(Caso, Diagnosticos):
    """Lista de problemas do check do programa errado (vazia = o caso passa nesta parte)."""
    Codigo = Caso.get("codigo", "E0110")
    Codigos = [D["codigo"] for D in Diagnosticos]
    if not Diagnosticos or Codigos[0] != Codigo:
        return [f"1o diagnostico {Codigos[:1]}, esperado {Codigo}"]
    Problemas = []
    if any(C != Codigo for C in Codigos):
        Problemas.append(f"diagnostico alem do {Codigo} (enganoso): {Codigos}")
    Primeiro = Diagnosticos[0]
    if Primeiro.get("linha") != Caso["linha"]:
        Problemas.append(f"linha {Primeiro.get('linha')}, esperada {Caso['linha']}")
    if Codigo == "E0110" and "forma de outra linguagem" not in Primeiro.get("causa", ""):
        Problemas.append("a causa nao diz 'forma de outra linguagem'")
    if Caso["chave"] not in Primeiro.get("forma_certa", ""):
        Problemas.append(f"forma_certa sem '{Caso['chave']}': {Primeiro.get('forma_certa')!r}")
    if Primeiro.get("confianca") != Caso["confianca"]:
        Problemas.append(f"confianca {Primeiro.get('confianca')}, esperada {Caso['confianca']}")
    if Caso["confianca"] == "certa" and not Primeiro.get("correcao"):
        Problemas.append("confianca certa sem edicao em `correcao`")
    if not Primeiro.get("sugestao"):
        Problemas.append("sem `sugestao`")
    return Problemas


def RodarCaso(Forja, Caso):
    """(problemas, corrigiu): problemas do caso (vazia = passou); corrigiu = o `corrigir --aplicar` de um caso "certa" deixou o programa certo e ele compila."""
    with tempfile.TemporaryDirectory(prefix="m_d4_") as Nome:
        Pasta = Path(Nome)
        Arquivo = Pasta / "errado.fj"
        Arquivo.write_bytes(Caso["errado"].encode("utf-8"))
        Codigo, Saida = Rodar(Forja, ["check", str(Arquivo)])
        Problemas = AvaliarDiagnosticos(Caso, LerDiagnosticos(Saida)) if Codigo == 1 else [f"check do errado saiu com exit {Codigo}"]
        Certo = ConferirCerto(Forja, Pasta, Caso)
        Corrigir = ConferirCorrigir(Forja, Pasta, Caso) if Caso["confianca"] == "certa" else []
    return Problemas + Certo + Corrigir, not Certo and not Corrigir


def EntradasSemCaso(Casos):
    """As entradas da tabela 3.4 que nenhum caso cobre."""
    Cobertas = {C["entrada"] for C in Casos}
    return [E for E in ENTRADAS if E not in Cobertas]


def TaxaM_D10(Resultados):
    """Fracao dos casos "certa" cujo corrigir deixou o programa certo e compilando. Resultados: lista de (confianca, corrigiu)."""
    Certas = [Corrigiu for Confianca, Corrigiu in Resultados if Confianca == "certa"]
    return sum(Certas) / len(Certas) if Certas else 0.0


def Verificar(Forja):
    """Roda todos os casos e as metricas; devolve a quantidade de falhas."""
    Falhas = 0
    Resultados = []
    for Caso in CASOS:
        Problemas, Corrigiu = RodarCaso(Forja, Caso)
        Resultados.append((Caso["confianca"], Corrigiu))
        Falhas += 1 if Problemas else 0
        print(f"{'FALHOU' if Problemas else 'ok    '} {Caso['entrada']:<14} {Caso['nome']}")
        for Problema in Problemas:
            print(f"       {Problema}")
    Sem = EntradasSemCaso(CASOS)
    Taxa = TaxaM_D10(Resultados)
    print(f"M-D4: entradas={len(ENTRADAS)} sem_caso={len(Sem)} {Sem if Sem else ''} casos={len(CASOS)} falhas={Falhas}")
    print(f"M-D10 (idiomas): {Taxa:.1%} dos casos 'certa' (meta >= {META_M_D10:.0%})")
    return Falhas + len(Sem) + (0 if Taxa >= META_M_D10 else 1)


def AutoTeste():
    """Plantio: a avaliacao reprova codigo errado, diagnostico extra, linha errada, forma sem a chave, sem sugestao e o E0201 generico; aprova o certo.
    A cobertura reprova entrada sem caso e o M-D10 reprova abaixo da meta."""
    Caso = {"linha": 2, "chave": "none", "confianca": "certa"}
    Bom = {"codigo": "E0110", "linha": 2, "causa": "forma de outra linguagem: x", "forma_certa": "Troque por none.", "confianca": "certa", "correcao": [{}], "sugestao": "s"}
    Casos = [("bom", [Bom], 0), ("codigo", [dict(Bom, codigo="E0101")], 1), ("generico", [dict(Bom, codigo="E0201")], 1), ("extra", [Bom, dict(Bom, codigo="E0201")], 1),
             ("linha", [dict(Bom, linha=3)], 1), ("chave", [dict(Bom, forma_certa="Troque.")], 1), ("sem_sugestao", [dict(Bom, sugestao="")], 1),
             ("sem_correcao", [dict(Bom, correcao=[])], 1), ("confianca", [dict(Bom, confianca="so_dica")], 1), ("vazio", [], 1)]
    Erros = [Nome for Nome, Lista, Esperado in Casos if bool(AvaliarDiagnosticos(Caso, Lista)) != bool(Esperado)]
    Proprio = dict(Caso, codigo="E0307")
    if AvaliarDiagnosticos(Proprio, [dict(Bom, codigo="E0307", causa="str nao se soma")]):
        Erros.append("codigo_proprio_reprovado")
    Nomes = [C["nome"] for C in CASOS]
    if len(set(Nomes)) != len(Nomes):
        Erros.append("nomes de caso repetidos")
    if EntradasSemCaso(CASOS):
        Erros.append(f"entradas sem caso: {EntradasSemCaso(CASOS)}")
    if not EntradasSemCaso([C for C in CASOS if C["entrada"] != "range(n)"]):
        Erros.append("plantio: entrada sem caso nao foi pega")
    if len(ENTRADAS) < MINIMO_DE_ENTRADAS:
        Erros.append(f"so {len(ENTRADAS)} entradas (minimo {MINIMO_DE_ENTRADAS})")
    if TaxaM_D10([("certa", True)] * 19 + [("certa", False)]) < META_M_D10 or TaxaM_D10([("certa", True)] * 18 + [("certa", False)] * 2) >= META_M_D10:
        Erros.append("M-D10: o limiar de 95% nao separa 19/20 de 18/20")
    print("selftest:", "ok" if not Erros else f"FALHOU {Erros}")
    return 1 if Erros else 0


def Principal():
    Parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    Parser.add_argument("--forja", default=str(FORJA_PADRAO), help="forja.exe a verificar")
    Parser.add_argument("--selftest", action="store_true")
    Argumentos = Parser.parse_args()
    if Argumentos.selftest:
        return AutoTeste()
    if not Path(Argumentos.forja).is_file():
        print(f"forja nao encontrado: {Argumentos.forja}")
        return 2
    return 1 if Verificar(Argumentos.forja) else 0


if __name__ == "__main__":
    sys.exit(Principal())
