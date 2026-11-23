# -*- coding: utf-8 -*-
"""verificar_e0110_l1.py -- IA1-L1: cada idioma lexico/sintatico da tabela 3.4 do contrato (forma de outra linguagem) vira E0110 na linha certa, com a forma da
Forja na forma_certa, sem nenhum outro diagnostico (0 enganoso); a correcao certa, aplicada por `forja corrigir --aplicar`, deixa o texto exatamente igual ao
programa certo e o programa certo passa no check; as formas sem conserto mecanico (so_dica) tem o programa certo conferido a mao (check exit 0).

Uso:
    python -I verificar_e0110_l1.py [--forja <forja.exe>] [--selftest]
Exit 0 se todos os casos passam, 1 se algum falha (uma linha por caso; as falhas dizem o que esperava e o que veio).
Casos no arquivo casos_e0110_l1.py (dados puros: o L2 acrescenta os idiomas semanticos la, sem mexer aqui).
Depende de: so a biblioteca padrao.
Versao: 0.1.0
Changelog: 0.1.0 - IA1-L1: primeira versao.
Verificacao: python -I verificar_e0110_l1.py --selftest   |   python -I verificar_e0110_l1.py --forja <forja.exe>
"""

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
from casos_e0110_l1 import CASOS  # noqa: E402

FORJA_PADRAO = AQUI.parent.parent / "compiler" / "forja.exe"
TIMEOUT = 60


def Rodar(Forja, Argumentos):
    """Roda o forja e devolve (exit, stdout)."""
    Rodada = subprocess.run([str(Forja)] + Argumentos, capture_output=True, text=True, encoding="utf-8", timeout=TIMEOUT)
    return Rodada.returncode, Rodada.stdout + Rodada.stderr


def LerDiagnosticos(Saida):
    """Os objetos JSON com `codigo` da saida do check, na ordem."""
    Lista = []
    for Linha in Saida.splitlines():
        try:
            Objeto = json.loads(Linha)
        except ValueError:
            continue
        if isinstance(Objeto, dict) and "codigo" in Objeto:
            Lista.append(Objeto)
    return Lista


def AvaliarDiagnosticos(Caso, Diagnosticos):
    """Lista de problemas do check do programa errado (vazia = o caso passa nesta parte)."""
    Problemas = []
    Codigos = [D["codigo"] for D in Diagnosticos]
    if not Diagnosticos or Codigos[0] != "E0110":
        return [f"1o diagnostico {Codigos[:1]}, esperado E0110"]
    if any(Codigo != "E0110" for Codigo in Codigos):
        Problemas.append(f"diagnostico alem do E0110 (enganoso): {Codigos}")
    Primeiro = Diagnosticos[0]
    if Primeiro.get("linha") != Caso["linha"]:
        Problemas.append(f"linha {Primeiro.get('linha')}, esperada {Caso['linha']}")
    if "forma de outra linguagem" not in Primeiro.get("causa", ""):
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


def ConferirCerto(Forja, Pasta, Caso):
    """O programa certo do caso passa no check (exit 0, nenhum diagnostico)."""
    Arquivo = Pasta / "certo.fj"
    Arquivo.write_bytes(Caso["certo"].encode("utf-8"))
    Codigo, Saida = Rodar(Forja, ["check", str(Arquivo)])
    return [] if Codigo == 0 and not LerDiagnosticos(Saida) else [f"o programa certo nao passa no check (exit {Codigo}): {Saida[:200]!r}"]


def ConferirCorrigir(Forja, Pasta, Caso):
    """`forja corrigir --aplicar` deixa o texto igual ao programa certo (so nos casos de confianca certa)."""
    Arquivo = Pasta / "corrigir.fj"
    Arquivo.write_bytes(Caso["errado"].encode("utf-8"))
    Codigo, Saida = Rodar(Forja, ["corrigir", str(Arquivo), "--aplicar"])
    Depois = Arquivo.read_bytes().decode("utf-8")
    if Depois != Caso["certo"]:
        return [f"corrigir (exit {Codigo}) deixou o texto diferente do certo:\n--- veio\n{Depois}--- esperado\n{Caso['certo']}"]
    return []


def RodarCaso(Forja, Caso):
    """Problemas do caso (vazia = passou)."""
    with tempfile.TemporaryDirectory(prefix="e0110_l1_") as Nome:
        Pasta = Path(Nome)
        Arquivo = Pasta / "errado.fj"
        Arquivo.write_bytes(Caso["errado"].encode("utf-8"))
        Codigo, Saida = Rodar(Forja, ["check", str(Arquivo)])
        Problemas = AvaliarDiagnosticos(Caso, LerDiagnosticos(Saida)) if Codigo == 1 else [f"check do errado saiu com exit {Codigo}"]
        Problemas += ConferirCerto(Forja, Pasta, Caso)
        if Caso["confianca"] == "certa":
            Problemas += ConferirCorrigir(Forja, Pasta, Caso)
    return Problemas


def Verificar(Forja):
    """Roda todos os casos; devolve a quantidade de falhas."""
    Falhas = 0
    for Caso in CASOS:
        Problemas = RodarCaso(Forja, Caso)
        Falhas += 1 if Problemas else 0
        print(f"{'FALHOU' if Problemas else 'ok    '} {Caso['nome']}")
        for Problema in Problemas:
            print(f"       {Problema}")
    print(f"casos={len(CASOS)} falhas={Falhas}")
    return Falhas


def AutoTeste():
    """Plantio: a avaliacao reprova codigo errado, diagnostico extra, linha errada e forma sem a chave; aprova o diagnostico certo."""
    Caso = {"linha": 2, "chave": "and", "confianca": "certa"}
    Bom = {"codigo": "E0110", "linha": 2, "causa": "forma de outra linguagem: x", "forma_certa": "Troque por and.", "confianca": "certa", "correcao": [{}], "sugestao": "s"}
    Casos = [("bom", [Bom], 0), ("codigo", [dict(Bom, codigo="E0101")], 1), ("extra", [Bom, dict(Bom, codigo="E0301")], 1),
             ("linha", [dict(Bom, linha=3)], 1), ("chave", [dict(Bom, forma_certa="Troque.")], 1), ("vazio", [], 1)]
    Erros = [Nome for Nome, Lista, Esperado in Casos if bool(AvaliarDiagnosticos(Caso, Lista)) != bool(Esperado)]
    Nomes = [C["nome"] for C in CASOS]
    if len(set(Nomes)) != len(Nomes):
        Erros.append("nomes de caso repetidos")
    if len(CASOS) < 14:
        Erros.append(f"so {len(CASOS)} casos (minimo 14)")
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
