# -*- coding: utf-8 -*-
"""verificar_l3c2.py -- IA1-L3c2/L3f: o compilador aceita os nomes novos do pacote vendor:db (ex-banco) e dos pacotes renomeados e RECUSA os antigos com E0110 (o forja migrar os converte).

Cobre as tres classes novas do L3c2 (alem das de modulo, qualificador, proc e tipo do L3c):
  - pacote: a PASTA do import "vendor:" (banco -> db, banco_servidor -> db_server, compressao -> compression, rede -> net, fisica2d -> physics2d) e o arquivo db.fj / compression.fj / net.fj;
  - membro de enum: DbError.nao_abriu -> not_opened (e os outros 4), por enum e modulo (um enum do usuario com membro igual nao e tocado);
  - param: o rotulo de argumento nomeado dos procs do modulo (caminho -> path, indice -> index, valor -> value), escopo = o modulo inteiro.
Confere, numa pasta temporaria com uma juncao `vendor` para language\\vendor (removida no fim, sem tocar no destino):
  - fixtures novas (fixtures_l3c2): db_novo, db_colisao, compression_novo, physics2d_novo, pasta_novo: `forja check` limpo e `forja run` imprime o .esperado.txt no backend x64 e em --release;
  - fixtures antigas: db_{antigo,misto,misto2}, compression_antigo, physics2d_antigo, pasta_antigo: `forja check` sai 1 com E0110 "forma antiga" primeiro; `forja migrar --aplicar` numa copia
    deixa o fonte que compila e imprime o .esperado.txt; min_antigo, servidor_antigo e rede_antigo: E0110 e, migrados, o check dao os mesmos codigos da forma nova, sem E1001/E1005/E0110
    (o net.fj ja acusa E0709 no repo: nao e do L3c2);
  - err_*: `forja check` sai com a LISTA de diagnosticos do .esperado.jsonl (codigo/linha/coluna), com causa, explicacao e forma_certa.
Uso:
    python -I verificar_l3c2.py [--forja <forja.exe>] [--selftest]
Exit 0 se tudo passa, 1 se algum caso falha, 2 se o forja nao existe.
Depende de: so a biblioteca padrao (cmd mklink /J, do Windows) e verificar_l3c.py (mesma pasta).
Versao: 0.2.1
Changelog: 0.2.1 - 0.21.0 F4 residuo: sai o backend C dos modos de execucao (via verificar_l3c.py 0.2.2).
           0.2.0 - IA1-L3f: a forma antiga e recusada (E0110) e o migrar a converte; sai a comparacao de exe entre as formas (politica D0591); err_ compara a lista inteira.
           0.1.0 - IA1-L3c2: primeira versao.
Verificacao: python -I verificar_l3c2.py --selftest   |   python -I verificar_l3c2.py --forja <forja.exe>
"""

import argparse
import importlib.util
import shutil
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
FIXTURES = AQUI / "fixtures_l3c2"
FORJA_PADRAO = AQUI.parent.parent / "compiler" / "forja.exe"
OK_COM_ESPERADO = {
    "db.esperado.txt": ["db_novo"],
    "db_colisao.esperado.txt": ["db_colisao"],
    "compression.esperado.txt": ["compression_novo"],
    "physics2d.esperado.txt": ["physics2d_novo"],
    "pasta.esperado.txt": ["pasta_novo"],
}
ANTIGOS_COM_ESPERADO = {
    "db.esperado.txt": ["db_antigo", "db_misto", "db_misto2"],
    "compression.esperado.txt": ["compression_antigo"],
    "physics2d.esperado.txt": ["physics2d_antigo"],
    "pasta.esperado.txt": ["pasta_antigo"],
}
SO_CHECK = (("servidor_antigo", "servidor_novo"), ("rede_antigo", "rede_novo"), ("min_antigo", "min_novo"))
SEM_MIGRAR = ("rede_antigo",)
CODIGOS_PROIBIDOS = ("E1001", "E1005", "E0110", "E0201", "E0504")


def CarregarL3c():
    """O verificar_l3c.py como modulo (python -I nao poe a pasta do script no sys.path)."""
    Spec = importlib.util.spec_from_file_location("verificar_l3c", AQUI / "verificar_l3c.py")
    Modulo = importlib.util.module_from_spec(Spec)
    Spec.loader.exec_module(Modulo)
    return Modulo


L3C = CarregarL3c()


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


def CodigosDoCheck(Forja, Pasta, Arquivo):
    """Os codigos dos diagnosticos do `forja check` de `Arquivo` em `Pasta`, na ordem."""
    _, Saida, _ = L3C.Rodar(Forja, ["check", Arquivo], str(Pasta))
    return [D.get("codigo") for D in L3C.LerDiagnosticos(Saida)]


def ProblemasDoCheckIgual(Forja, Raiz, Antigo, Novo):
    """O antigo da E0110 e, migrado, o check dele tem os mesmos codigos do check do novo; nenhum codigo de nome/modulo/rotulo (o pacote tem de ser achado).
    Quem importa vendor:net nao migra (o net.fj acusa E0709): o antigo so precisa dar E0110."""
    if Antigo in SEM_MIGRAR:
        return L3C.ProblemasDoPrimeiroE0110(Forja, str(Raiz), f"{Antigo}.fj")
    Sub, Problemas = L3C.CopiaMigrada(Forja, str(Raiz), f"{Antigo}.fj")
    if Problemas:
        return Problemas
    DoMigrado, DoNovo = CodigosDoCheck(Forja, Sub, "prog.fj"), CodigosDoCheck(Forja, Raiz, f"{Novo}.fj")
    if DoMigrado != DoNovo:
        Problemas.append(f"check do {Antigo} migrado {DoMigrado} diferente do {Novo} {DoNovo}")
    Achados = [Codigo for Codigo in DoMigrado + DoNovo if Codigo in CODIGOS_PROIBIDOS]
    return Problemas + ([f"diagnosticos de pacote/nome: {Achados}"] if Achados else [])


def CasosSoCheck(Forja, Raiz):
    return [(f"check_{Par[0].split('_')[0]}_migrado", ProblemasDoCheckIgual(Forja, Raiz, *Par)) for Par in SO_CHECK]


def CasosDeErro(Forja, Raiz):
    """[(nome, problemas)] dos err_*: a lista de diagnosticos do .esperado.jsonl."""
    Resultados = []
    for Fonte in sorted(FIXTURES.glob("err_*.fj")):
        _, Saida, _ = L3C.Rodar(Forja, ["check", Fonte.name], str(Raiz))
        Resultados.append((Fonte.stem, L3C.ProblemasDaLista(L3C.EsperadosDe(Fonte), L3C.LerDiagnosticos(Saida))))
    return Resultados


def Verificar(Forja):
    """Roda tudo numa pasta temporaria com a juncao vendor; devolve a quantidade de falhas."""
    with tempfile.TemporaryDirectory(prefix="l3c2_") as Nome:
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
    Faltam += [N for Par in SO_CHECK for N in Par if not (FIXTURES / f"{N}.fj").is_file()]
    Erros_ = sorted(FIXTURES.glob("err_*.fj"))
    Faltam += [E.stem for E in Erros_ if not E.with_suffix(".esperado.jsonl").is_file()]
    Bom = {"codigo": "E0501", "linha": 5, "coluna": 35, "causa": "c", "explicacao": "e", "forma_certa": "f"}
    Esperado = {"codigo": "E0501", "linha": 5, "coluna": 35}
    Falhas = []
    if L3C.ProblemasDoErro(Esperado, [Bom]):
        Falhas.append("bom reprovado")
    if not L3C.ProblemasDoErro(Esperado, [dict(Bom, linha=6)]) or not L3C.ProblemasDoErro(Esperado, [Bom, Bom]):
        Falhas.append("ruim aprovado")
    if Faltam or len(Erros_) < 7:
        Falhas.append(f"fixtures: faltam {Faltam} ou menos de 7 err_ ({len(Erros_)})")
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
