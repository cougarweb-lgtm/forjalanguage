# verificar_erros_b15.py -- mede "todos os erros numa execucao, por causa-raiz" (contrato IA1, P-IA1-b15, criterio 1.1 item 6, M-D12)
"""Roda `forja check --max-erros 100` nas fixtures err_b15_* de tests\\ia_first\\b15 e confere:
  M-D12 independentes  as 30 fixtures err_b15_indep_* (3 erros independentes cada) saem com EXATAMENTE os 3 erros do .esperado.jsonl, na ordem (codigo, linha, coluna)
  M-D12 cascata        as 20 fixtures err_b15_casc_* (2 raizes + consequencias marcadas com "// consequencia" no fim da linha) saem so com as raizes: toda linha
                       a mais e consequencia ligada ("consequencia_de" no JSON) ou e erro SOLTO (vermelho); nenhuma linha marcada pode ter diagnostico solto
  marcas               toda fixture de cascata tem pelo menos uma linha marcada (a consequencia existe no fonte)
  determinismo         duas execucoes do mesmo check dao o mesmo stdout byte a byte (M-D6)
  resumo               `check --resumo` traz os mesmos (codigo, linha, coluna) que o check completo
  discrimina           com --antes <forja.exe>: o compilador anterior (b14) fica VERMELHO nesta mesma conferencia e o novo fica verde
Uso:
    python -I verificar_erros_b15.py --forja <forja.exe> [--antes <forja.exe do b14>] [--pasta <pasta das fixtures>]   (exit 0 so se tudo verde; 1 = vermelho; 2 = uso)
    python -I verificar_erros_b15.py --selftest                                                                         (prova o discriminador com saidas sinteticas)
Contra o compilador do b14 (um erro por declaracao, sem recuperacao) este verificador sai vermelho.
Versao: 0.1.0
Changelog: 0.1.0 - P-IA1-b15: primeira versao.
Verificacao: python -I verificar_erros_b15.py --selftest
"""

import argparse
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaPadrao = PastaAqui / "b15"
ForjaPadrao = PastaAqui.parent.parent / "compiler" / "forja.exe"
MARCA = "// consequencia"
TIMEOUT = 60
ESPERADO_INDEPENDENTES = 30
ESPERADO_CASCATAS = 20


def RodarCheck(Forja, Caminho, Resumo=False):
    Argumentos = [str(Forja), "check", "--max-erros", "100"] + (["--resumo"] if Resumo else []) + [Caminho.name]
    Processo = subprocess.run(Argumentos, capture_output=True, encoding="utf-8", timeout=TIMEOUT, cwd=str(Caminho.parent))
    return Processo.returncode, Processo.stdout


def LerLinhas(Saida):
    return [json.loads(Linha) for Linha in Saida.splitlines() if Linha.strip()]


def Chave(Diagnostico):
    return (Diagnostico.get("codigo"), Diagnostico.get("linha"), Diagnostico.get("coluna"))


def LerEsperado(Caminho):
    Arquivo = Caminho.with_suffix(".esperado.jsonl")
    return [Chave(json.loads(Linha)) for Linha in Arquivo.read_text(encoding="utf-8").splitlines() if Linha.strip()]


def LinhasMarcadas(Caminho):
    return {Numero for Numero, Texto in enumerate(Caminho.read_text(encoding="utf-8").split("\n"), start=1) if Texto.rstrip().endswith(MARCA)}


def ConferirIndependente(Obtidos, Esperado):
    """Problemas de uma fixture de 3 erros independentes: a lista tem de ser igual, na ordem."""
    Chaves = [Chave(D) for D in Obtidos]
    if Chaves == Esperado:
        return []
    return [f"esperado {Esperado}, obtido {Chaves}"]


def ConferirCascata(Obtidos, Esperado, Marcadas):
    """Problemas de uma fixture de cascata: so as raizes, na ordem; linha a mais so vale como consequencia ligada (consequencia_de); marcada nunca solta."""
    Problemas = []
    Raizes = [D for D in Obtidos if not D.get("consequencia_de")]
    Chaves = [Chave(D) for D in Raizes]
    if Chaves != Esperado:
        Problemas.append(f"raizes esperadas {Esperado}, obtidas {Chaves}")
    for Diagnostico in Raizes:
        if Diagnostico.get("linha") in Marcadas:
            Problemas.append(f"erro solto na linha marcada {Diagnostico.get('linha')}: {Diagnostico.get('codigo')}")
    for Diagnostico in Obtidos:
        if Diagnostico.get("consequencia_de") and not any(Diagnostico["consequencia_de"] == R.get("id") for R in Raizes):
            Problemas.append(f"consequencia_de {Diagnostico['consequencia_de']!r} nao aponta para um erro raiz")
    if not Marcadas:
        Problemas.append("a fixture de cascata nao tem linha marcada")
    return Problemas


def ConferirFixture(Forja, Caminho):
    """(nome, problemas) de uma fixture; roda o check completo duas vezes e o --resumo uma."""
    Problemas = []
    Codigo, Saida = RodarCheck(Forja, Caminho)
    if Codigo != 1:
        return Caminho.name, [f"exit {Codigo}, esperado 1"]
    try:
        Obtidos = LerLinhas(Saida)
    except json.JSONDecodeError as Falha:
        return Caminho.name, [f"stdout nao e JSON ({Falha})"]
    Esperado = LerEsperado(Caminho)
    if "_indep_" in Caminho.name:
        Problemas += ConferirIndependente(Obtidos, Esperado)
    else:
        Problemas += ConferirCascata(Obtidos, Esperado, LinhasMarcadas(Caminho))
    if RodarCheck(Forja, Caminho)[1] != Saida:
        Problemas.append("duas execucoes dao saidas diferentes")
    try:
        Resumido = [Chave(D) for D in LerLinhas(RodarCheck(Forja, Caminho, Resumo=True)[1])]
    except json.JSONDecodeError as Falha:
        return Caminho.name, Problemas + [f"--resumo nao e JSON ({Falha})"]
    if Resumido != [Chave(D) for D in Obtidos]:
        Problemas.append(f"--resumo difere do check completo: {Resumido}")
    return Caminho.name, Problemas


def ListarFixtures(Pasta):
    return sorted(Pasta.glob("err_b15_*.fj"))


def ConferirPasta(Forja, Pasta):
    Fixtures = ListarFixtures(Pasta)
    with ThreadPoolExecutor() as Pool:
        return list(Pool.map(lambda Caminho: ConferirFixture(Forja, Caminho), Fixtures))


def Metricas(Resultados):
    """Linhas {id, valor, verde} a partir de [(nome, problemas)]."""
    Independentes = [R for R in Resultados if "_indep_" in R[0]]
    Cascatas = [R for R in Resultados if "_casc_" in R[0]]
    Saida = []
    for Id, Grupo, Total in (("M-D12 independentes", Independentes, ESPERADO_INDEPENDENTES), ("M-D12 cascata", Cascatas, ESPERADO_CASCATAS)):
        Ruins = [R for R in Grupo if R[1]]
        Saida.append({"id": Id, "valor": {"fixtures": len(Grupo), "esperadas": Total, "ok": len(Grupo) - len(Ruins), "problemas": Ruins[:5]},
                      "verde": len(Grupo) == Total and not Ruins})
    return Saida


def Discrimina(Atual, Antes):
    """verde se o compilador anterior falha em pelo menos uma fixture e o atual em nenhuma."""
    FalhasAntes = sum(1 for R in Antes if R[1])
    FalhasAtual = sum(1 for R in Atual if R[1])
    return {"id": "discrimina", "valor": {"antes_vermelhas": FalhasAntes, "atual_vermelhas": FalhasAtual}, "verde": FalhasAntes > 0 and FalhasAtual == 0}


def Autoteste():
    Raiz = {"codigo": "E0301", "linha": 3, "coluna": 5, "id": "E0301@a.fj:3:5"}
    Outra = {"codigo": "E0201", "linha": 6, "coluna": 9}
    Esperado = [Chave(Raiz), Chave(Outra)]
    assert ConferirIndependente([Raiz, Outra], Esperado) == []
    assert ConferirIndependente([Raiz], Esperado) != []  # erro que faltou
    assert ConferirIndependente([Outra, Raiz], Esperado) != []  # fora de ordem
    Solto = {"codigo": "E0201", "linha": 4, "coluna": 7}
    assert ConferirCascata([Raiz, Outra], Esperado, {4}) == []
    assert ConferirCascata([Raiz, Solto, Outra], Esperado, {4}) != []  # consequencia solta
    Ligado = dict(Solto, consequencia_de=Raiz["id"])
    assert ConferirCascata([Raiz, Ligado, Outra], Esperado, {4}) == []  # consequencia ligada vale
    Orfao = dict(Solto, consequencia_de="E9999@x.fj:1:1")
    assert ConferirCascata([Raiz, Orfao, Outra], Esperado, {4}) != []  # ligada a um erro que nao existe
    assert ConferirCascata([Raiz, Outra], Esperado, set()) != []  # sem linha marcada
    assert Discrimina([("a", [])], [("a", ["x"])])["verde"] and not Discrimina([("a", [])], [("a", [])])["verde"]
    print("autoteste OK: 10 verificacoes")
    return True


def Principal():
    Analisador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Analisador.add_argument("--forja", type=Path, default=ForjaPadrao, help="forja.exe a medir")
    Analisador.add_argument("--antes", type=Path, help="forja.exe anterior (b14): tem de ficar vermelho")
    Analisador.add_argument("--pasta", type=Path, default=PastaPadrao, help="pasta das fixtures err_b15_*")
    Analisador.add_argument("--selftest", action="store_true")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        return 0 if Autoteste() else 1
    if not Argumentos.forja.exists() or not Argumentos.pasta.is_dir():
        print("forja.exe ou pasta das fixtures nao encontrados", file=sys.stderr)
        return 2
    Resultados = ConferirPasta(Argumentos.forja, Argumentos.pasta)
    Linhas = Metricas(Resultados)
    if Argumentos.antes is not None:
        Linhas.append(Discrimina(Resultados, ConferirPasta(Argumentos.antes, Argumentos.pasta)))
    for Linha in Linhas:
        print(json.dumps(Linha, ensure_ascii=False))
    return 0 if all(L["verde"] for L in Linhas) else 1


if __name__ == "__main__":
    sys.exit(Principal())
