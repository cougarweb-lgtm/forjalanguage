# -*- coding: utf-8 -*-
"""verificar_l3c3.py -- IA1-L3c3/L3f: o compilador aceita os nomes novos de vendor:json, vendor:compression e vendor:net e RECUSA os antigos com E0110 (o forja migrar os converte).

Reaproveita os tres pontos genericos do L3c2 (alias de membro de enum, de rotulo de parametro de proc de modulo e de proc/tipo de modulo) com 73 linhas novas em common\\renomeados.c.
Confere, numa pasta temporaria com uma juncao `vendor` para language\\vendor (removida no fim, sem tocar no destino):
  - fixtures novas (fixtures_l3c3): ok_json_novo, ok_compression_novo, ok_colisao_usuario (identificadores do usuario com os nomes novos) e ok_dois_modulos: `forja check` limpo e `forja run`
    imprime o .esperado.txt no backend x64 e em --release;
  - formas antigas ok_json_{antigo,misto} e ok_compression_{antigo,misto}: `forja check` sai 1 com E0110 "forma antiga" primeiro; `forja migrar --aplicar` numa copia deixa o fonte que
    compila e imprime o .esperado.txt;
  - ok_net_{novo,antigo,misto}: so check (o net.fj ja acusa E0709 no repo, rede_c_fclose e rede_c_remove descartados, e por isso o migrar nao analisa quem importa vendor:net): o novo so tem os
    2 E0709 do net.fj; o antigo e o misto dao E0110 "forma antiga" primeiro;
  - err_*: `forja check` sai com a LISTA de diagnosticos do .esperado.jsonl (codigo/linha/coluna), com causa, explicacao e forma_certa (rotulo duplicado antigo+novo, membro inexistente, membro de
    outro enum/modulo, rotulo de proc do usuario ou de outro modulo).
Uso:
    python -I verificar_l3c3.py [--forja <forja.exe>] [--selftest]
Exit 0 se tudo passa, 1 se algum caso falha, 2 se o forja nao existe.
Depende de: so a biblioteca padrao (cmd mklink /J, do Windows) e verificar_l3c.py (mesma pasta).
Versao: 0.2.2
Changelog: 0.2.2 - 0.21.0 F4 residuo: sai o backend C dos modos de execucao (via verificar_l3c.py 0.2.2).
           0.2.1 - S2g-json: as fixtures de json acompanham o vendor:json 0.2.0 (rotulos index/value/text/path, JsonError.invalid_path, get_path) e ganharam 2 err_E0110 (caminho; indice, valor e texto).
           0.2.0 - IA1-L3f: a forma antiga e recusada (E0110) e o migrar a converte; sai a comparacao de exe entre as formas (politica D0591); err_ compara a lista inteira.
           0.1.0 - IA1-L3c3: primeira versao.
Verificacao: python -I verificar_l3c3.py --selftest   |   python -I verificar_l3c3.py --forja <forja.exe>
"""

import argparse
import importlib.util
import shutil
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
FIXTURES = AQUI / "fixtures_l3c3"
FORJA_PADRAO = AQUI.parent.parent / "compiler" / "forja.exe"
OK_COM_ESPERADO = {
    "ok_json.esperado.txt": ["ok_json_novo"],
    "ok_json_path.esperado.txt": ["ok_json_path"],
    "ok_compression.esperado.txt": ["ok_compression_novo"],
    "ok_colisao_usuario.esperado.txt": ["ok_colisao_usuario"],
    "ok_dois_modulos.esperado.txt": ["ok_dois_modulos"],
}
ANTIGOS_COM_ESPERADO = {
    "ok_json.esperado.txt": ["ok_json_antigo", "ok_json_misto"],
    "ok_compression.esperado.txt": ["ok_compression_antigo", "ok_compression_misto"],
}
SO_CHECK = ("ok_net_novo", "ok_net_antigo", "ok_net_misto")
ARQUIVO_DA_DIVIDA = "net.fj"
E0709_DA_DIVIDA = 0  # IA1-L8: o net.fj ganhou `_ = ` nos 2 descartes e nao tem mais divida
MINIMO_ERR = 9


def CarregarVizinho(Nome):
    """Um verificar_*.py desta pasta como modulo (python -I nao poe a pasta do script no sys.path)."""
    Spec = importlib.util.spec_from_file_location(Nome, AQUI / f"{Nome}.py")
    Modulo = importlib.util.module_from_spec(Spec)
    Spec.loader.exec_module(Modulo)
    return Modulo


L3C = CarregarVizinho("verificar_l3c")


def EsperadoDe(Nome):
    return (FIXTURES / Nome).read_text(encoding="utf-8").splitlines()


def CasosDeExecucao(Forja, Raiz):
    """[(nome, problemas)] dos ok_ novos com saida esperada (nos 3 modos) e das formas antigas (E0110, migrar e a mesma saida)."""
    Resultados = []
    for Arquivo, Nomes in OK_COM_ESPERADO.items():
        for Nome in Nomes:
            Resultados.append((Nome, L3C.ProblemasDeExecucao(Forja, str(Raiz), f"{Nome}.fj", EsperadoDe(Arquivo))))
    for Arquivo, Nomes in ANTIGOS_COM_ESPERADO.items():
        for Nome in Nomes:
            Resultados.append((Nome + "_migrado", L3C.ProblemasDaFormaAntiga(Forja, str(Raiz), f"{Nome}.fj", EsperadoDe(Arquivo))))
    return Resultados


def DiagnosticosDoNet(Forja, Raiz, Nome):
    """O check de um ok_net_*: exit 1 e so os 2 E0709 do net.fj (a divida conhecida); devolve (problemas, texto da saida)."""
    Codigo, Saida, _ = L3C.Rodar(Forja, ["check", f"{Nome}.fj"], str(Raiz))
    Diagnosticos = L3C.LerDiagnosticos(Saida)
    Dividas = [D for D in Diagnosticos if D.get("codigo") == "E0709" and str(D.get("arquivo", "")).endswith(ARQUIVO_DA_DIVIDA)]
    Problemas = []
    if len(Diagnosticos) != len(Dividas) or len(Dividas) != E0709_DA_DIVIDA:
        Problemas.append(f"{Nome}: {[(D.get('codigo'), Path(str(D.get('arquivo', ''))).name) for D in Diagnosticos]}, esperado so {E0709_DA_DIVIDA} E0709 do {ARQUIVO_DA_DIVIDA}")
    if Codigo != (1 if E0709_DA_DIVIDA else 0):
        Problemas.append(f"{Nome}: check exit {Codigo}, esperado {1 if E0709_DA_DIVIDA else 0} (a divida do net.fj)")
    return Problemas, Saida.replace(str(Raiz).replace("\\", "/"), "")


def ProblemasDoNetAntigo(Forja, Raiz, Nome):
    """O check de um ok_net_* na forma antiga: exit 1 e o 1o diagnostico e E0110 "forma antiga" (o migrar nao analisa quem importa vendor:net: a divida E0709 do net.fj)."""
    Codigo, Saida, _ = L3C.Rodar(Forja, ["check", f"{Nome}.fj"], str(Raiz))
    Diagnosticos = L3C.LerDiagnosticos(Saida)
    if Codigo != 1 or not Diagnosticos or Diagnosticos[0].get("codigo") != "E0110" or not Diagnosticos[0].get("causa", "").startswith("forma antiga:"):
        return [f"{Nome}: exit {Codigo}, 1o diagnostico {[(D.get('codigo'), D.get('causa')) for D in Diagnosticos[:1]]}, esperado E0110 'forma antiga'"]
    return []


def CasosSoCheck(Forja, Raiz):
    """[(nome, problemas)]: o ok_net_novo so tem a divida do net.fj; o antigo e o misto dao E0110."""
    Problemas, _ = DiagnosticosDoNet(Forja, Raiz, SO_CHECK[0])
    Resultados = [(f"check_{SO_CHECK[0]}", Problemas)]
    for Nome in SO_CHECK[1:]:
        Resultados.append((f"check_{Nome}_e0110", ProblemasDoNetAntigo(Forja, Raiz, Nome)))
    return Resultados


def CasosDeErro(Forja, Raiz):
    """[(nome, problemas)] dos err_*: a lista de diagnosticos do .esperado.jsonl."""
    Resultados = []
    for Fonte in sorted(FIXTURES.glob("err_*.fj")):
        _, Saida, _ = L3C.Rodar(Forja, ["check", Fonte.name], str(Raiz))
        Resultados.append((Fonte.stem, L3C.ProblemasDaLista(L3C.EsperadosDe(Fonte), L3C.LerDiagnosticos(Saida))))
    return Resultados


def Verificar(Forja):
    """Roda tudo numa pasta temporaria com a juncao vendor; devolve a quantidade de falhas."""
    with tempfile.TemporaryDirectory(prefix="l3c3_") as Nome:
        Raiz = Path(Nome)
        for Fonte in FIXTURES.glob("*.fj"):
            shutil.copyfile(Fonte, Raiz / Fonte.name)
        L3C.Ligar(AQUI.parent.parent / "vendor", Raiz / "vendor")
        try:
            Resultados = CasosDeExecucao(Forja, Raiz) + CasosSoCheck(Forja, Raiz) + CasosDeErro(Forja, Raiz)
        finally:
            for Sub in list(Raiz.rglob("vendor")):
                L3C.Desligar(Sub)
    for Rotulo, Problemas in Resultados:
        print(f"{'FALHOU' if Problemas else 'ok    '} {Rotulo}")
        for Problema in Problemas:
            print(f"       {Problema}")
    Falhas = sum(1 for _, Problemas in Resultados if Problemas)
    print(f"casos={len(Resultados)} falhas={Falhas}")
    return Falhas


def AutoTeste():
    """Plantio: as fixtures e os .esperado existem; cada err_ tem o .esperado.jsonl; ProblemasDoErro reprova linha errada e diagnostico a mais e aprova o certo."""
    Faltam = [N for Nomes in list(OK_COM_ESPERADO.values()) + list(ANTIGOS_COM_ESPERADO.values()) for N in Nomes if not (FIXTURES / f"{N}.fj").is_file()]
    Faltam += [A for A in OK_COM_ESPERADO if not (FIXTURES / A).is_file()]
    Faltam += [N for N in SO_CHECK if not (FIXTURES / f"{N}.fj").is_file()]
    Erros_ = sorted(FIXTURES.glob("err_*.fj"))
    Faltam += [E.stem for E in Erros_ if not E.with_suffix(".esperado.jsonl").is_file()]
    Bom = {"codigo": "E0501", "linha": 5, "coluna": 49, "causa": "c", "explicacao": "e", "forma_certa": "f"}
    Esperado = {"codigo": "E0501", "linha": 5, "coluna": 49}
    Falhas = []
    if L3C.ProblemasDoErro(Esperado, [Bom]):
        Falhas.append("bom reprovado")
    if not L3C.ProblemasDoErro(Esperado, [dict(Bom, linha=6)]) or not L3C.ProblemasDoErro(Esperado, [Bom, Bom]):
        Falhas.append("ruim aprovado")
    if Faltam or len(Erros_) < MINIMO_ERR:
        Falhas.append(f"fixtures: faltam {Faltam} ou menos de {MINIMO_ERR} err_ ({len(Erros_)})")
    print("selftest:", "ok" if not Falhas else f"FALHOU {Falhas}")
    return 1 if Falhas else 0


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
    return 1 if Verificar(str(Path(Argumentos.forja).resolve())) else 0


if __name__ == "__main__":
    sys.exit(Principal())
