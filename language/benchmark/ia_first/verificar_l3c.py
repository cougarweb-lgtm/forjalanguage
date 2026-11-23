# -*- coding: utf-8 -*-
"""verificar_l3c.py -- IA1-L3c/L3f: as formas novas em ingles valem e as antigas dao E0110 "forma antiga; rode forja migrar" (tabela compiler\\common\\renomeados.c, recusar = true).

Confere:
  - ok_l3c_* (tests\\semantica, as formas novas e a colisao com nome do usuario): `forja check` limpo e `forja run` imprime o .esperado.txt no backend x64 e em --release;
  - cada forma antiga -- alloc, scope, to, file, misto, banco (open/exec/DbError via vendor:banco): `forja check` sai 1 e o 1o diagnostico e E0110 "forma antiga"; `forja migrar --aplicar`
    numa copia deixa o fonte que compila e imprime a MESMA saida da forma nova;
  - err_*_l3c_* (tests\\semantica): `forja check` sai com a LISTA de diagnosticos do .esperado.jsonl (codigo/linha/coluna, na ordem), com causa, explicacao e forma_certa.
O banco roda numa pasta temporaria com uma juncao `vendor` para language\\vendor (a juncao e removida no fim, sem tocar no destino).
Uso:
    python -I verificar_l3c.py [--forja <forja.exe>] [--selftest]
Exit 0 se tudo passa, 1 se algum caso falha, 2 se o forja nao existe.
Depende de: so a biblioteca padrao (e cmd mklink /J, do Windows).
Versao: 0.2.2
Changelog: 0.2.2 - 0.21.0 F4 residuo: sai o --backend c da MODOS_DE_EXECUCAO (backend aposentado, X0003); o oraculo e o .esperado.txt.
           0.2.1 - E4: SEMANTICA aponta para tests\\semantica (a pasta ia_first foi movida para benchmark).
           0.2.0 - IA1-L3f: a forma antiga e recusada (E0110) e o forja migrar a converte; sai a comparacao de exe entre forma nova e antiga (politica D0591); err_ compara a lista inteira.
           0.1.0 - IA1-L3c: primeira versao.
Verificacao: python -I verificar_l3c.py --selftest   |   python -I verificar_l3c.py --forja <forja.exe>
"""

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LINGUAGEM = AQUI.parent.parent
FORJA_PADRAO = LINGUAGEM / "compiler" / "forja.exe"
SEMANTICA = LINGUAGEM / "tests" / "semantica"
FIXTURES_BANCO = AQUI / "fixtures_l3c"
MODOS_DE_EXECUCAO = (["--backend", "x64"], ["--release"])
MODOS_DE_BUILD = ([], ["--release"])
PARES_SEMANTICA = ("alloc", "scope", "to", "file")
TIMEOUT = 180


def Rodar(Forja, Argumentos, Pasta=None):
    """Roda o forja e devolve (exit, stdout, stderr)."""
    Rodada = subprocess.run([str(Forja)] + Argumentos, capture_output=True, text=True, encoding="utf-8", timeout=TIMEOUT, cwd=Pasta)
    return Rodada.returncode, Rodada.stdout, Rodada.stderr


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


def ProblemasDoErro(Esperado, Diagnosticos):
    """Problemas do check de um err_ (vazia = passou): exatamente 1 diagnostico, no lugar certo, com causa, explicacao e forma_certa."""
    if len(Diagnosticos) != 1:
        return [f"{len(Diagnosticos)} diagnosticos, esperado 1: {[D.get('codigo') for D in Diagnosticos]}"]
    Obtido = Diagnosticos[0]
    Problemas = [f"{Campo} {Obtido.get(Campo)!r}, esperado {Esperado[Campo]!r}" for Campo in ("codigo", "linha", "coluna") if Obtido.get(Campo) != Esperado[Campo]]
    Problemas += [f"{Campo} vazio" for Campo in ("causa", "explicacao", "forma_certa") if not Obtido.get(Campo)]
    return Problemas


def ProblemasDeExecucao(Forja, Pasta, Arquivo, Esperado):
    """check limpo e, em cada modo, o run imprime `Esperado` (lista de linhas); Pasta e o cwd (onde ficam o fonte e, se precisar, o vendor)."""
    Codigo, Saida, Erro = Rodar(Forja, ["check", Arquivo], Pasta)
    if Codigo != 0 or Saida.strip() or Erro.strip():
        return [f"check exit {Codigo}: {(Saida + Erro)[:200]!r}"]
    Problemas = []
    for Modo in MODOS_DE_EXECUCAO:
        Codigo, Saida, Erro = Rodar(Forja, ["run"] + Modo + [Arquivo], Pasta)
        if Codigo != 0 or Saida.splitlines() != Esperado:
            Problemas.append(f"run {' '.join(Modo)}: exit {Codigo}, saida {Saida.splitlines()} (esperado {Esperado}) {Erro[:120]!r}")
    return Problemas


def ProblemasDaLista(Esperados, Diagnosticos):
    """Problemas do check de um err_ com a LISTA do .esperado.jsonl (vazia = passou): mesma quantidade, codigo/linha/coluna na ordem, causa/explicacao/forma_certa nao vazias."""
    if len(Diagnosticos) != len(Esperados):
        return [f"{len(Diagnosticos)} diagnosticos, esperado {len(Esperados)}: {[(D.get('codigo'), D.get('linha'), D.get('coluna')) for D in Diagnosticos]}"]
    Problemas = []
    for Posicao, (Esperado, Obtido) in enumerate(zip(Esperados, Diagnosticos)):
        Problemas += [f"#{Posicao} {Campo} {Obtido.get(Campo)!r}, esperado {Esperado[Campo]!r}" for Campo in ("codigo", "linha", "coluna") if Obtido.get(Campo) != Esperado[Campo]]
        Problemas += [f"#{Posicao} {Campo} vazio" for Campo in ("causa", "explicacao", "forma_certa") if not Obtido.get(Campo)]
    return Problemas


def EsperadosDe(Fonte):
    """Todas as linhas do .esperado.jsonl de `Fonte` como lista de dicts."""
    return [json.loads(Linha) for Linha in Fonte.with_suffix(".esperado.jsonl").read_text(encoding="utf-8").splitlines() if Linha.strip()]


def ProblemasDoPrimeiroE0110(Forja, Pasta, Arquivo):
    """`forja check` de `Arquivo` (em `Pasta`) sai 1 e o 1o diagnostico e E0110 "forma antiga"."""
    Codigo, Saida, _ = Rodar(Forja, ["check", Arquivo], Pasta)
    Diagnosticos = LerDiagnosticos(Saida)
    if Codigo != 1 or not Diagnosticos or Diagnosticos[0].get("codigo") != "E0110" or not Diagnosticos[0].get("causa", "").startswith("forma antiga:"):
        return [f"check da forma antiga: exit {Codigo}, 1o diagnostico {[(D.get('codigo'), D.get('causa')) for D in Diagnosticos[:1]]}, esperado E0110 'forma antiga'"]
    return []


def CopiaMigrada(Forja, Pasta, Arquivo):
    """A forma antiga e recusada e o migrar a converte: `forja check` de `Arquivo` (em `Pasta`) sai 1 e o 1o diagnostico e E0110 "forma antiga"; `forja migrar --aplicar` numa copia
    (prog.fj em Pasta/migrado, com a juncao vendor se Pasta tem uma) a converte. Devolve (pasta da copia, problemas)."""
    Sub = Path(Pasta) / "migrado"
    Problemas = ProblemasDoPrimeiroE0110(Forja, Pasta, Arquivo)
    if Problemas:
        return Sub, Problemas
    Sub.mkdir(exist_ok=True)
    shutil.copyfile(Path(Pasta) / Arquivo, Sub / "prog.fj")
    if (Path(Pasta) / "vendor").exists():
        Ligar(Path(Pasta) / "vendor", Sub / "vendor")
    Codigo, Saida, Erro = Rodar(Forja, ["migrar", "prog.fj", "--aplicar"], str(Sub))
    return Sub, ([f"migrar --aplicar: exit {Codigo} {(Saida + Erro)[:200]!r}"] if Codigo != 0 else [])


def ProblemasDaFormaAntiga(Forja, Pasta, Arquivo, Esperado):
    """CopiaMigrada sem problemas e o fonte migrado compila e imprime `Esperado` nos 3 modos."""
    Sub, Problemas = CopiaMigrada(Forja, Pasta, Arquivo)
    return Problemas or ProblemasDeExecucao(Forja, str(Sub), "prog.fj", Esperado)


def Ligar(Origem, Destino):
    """Juncao Destino -> Origem (cmd mklink /J, sem administrador)."""
    if not Destino.exists():
        subprocess.run(["cmd", "/c", "mklink", "/J", str(Destino), str(Origem)], capture_output=True, check=True)


def Desligar(Destino):
    """Remove a juncao sem tocar no destino (rmdir em juncao so tira o link)."""
    if Destino.exists():
        subprocess.run(["cmd", "/c", "rmdir", str(Destino)], capture_output=True)


def EsperadoDe(Caminho):
    return Caminho.with_suffix(".esperado.txt").read_text(encoding="utf-8").splitlines()


def CasosDeSemantica(Forja):
    """[(nome, problemas)] dos ok_l3c_* (execucao) e das formas antigas (E0110 + migrar): alloc, scope, to e file repetem a saida da forma nova; o misto tem a sua."""
    Resultados = []
    for Fonte in sorted(SEMANTICA.glob("ok_l3c_*.fj")):
        Resultados.append((Fonte.stem, ProblemasDeExecucao(Forja, str(SEMANTICA), Fonte.name, EsperadoDe(Fonte))))
    for Par in PARES_SEMANTICA:
        Antigo = SEMANTICA / f"err_E0110_l3c_{Par}_antigo.fj"
        Resultados.append((f"antigo_{Par}_migrado", ProblemasDaFormaAntiga(Forja, str(SEMANTICA), Antigo.name, EsperadoDe(SEMANTICA / f"ok_l3c_{Par}_novo.fj"))))
    Misto = SEMANTICA / "err_E0110_l3c_misto.fj"
    EsperadoMisto = (FIXTURES_BANCO / "misto_semantica.esperado.txt").read_text(encoding="utf-8").splitlines()
    Resultados.append(("misto_migrado", ProblemasDaFormaAntiga(Forja, str(SEMANTICA), Misto.name, EsperadoMisto)))
    return Resultados


def CasosDeBanco(Forja, Pasta):
    """[(nome, problemas)] do vendor:banco: novo, antigo e misto dao E0110 (o import vendor:banco/banco) e o migrar os converte para o banco.esperado.txt."""
    Raiz = Pasta / "banco"
    Raiz.mkdir(parents=True, exist_ok=True)
    Ligar(LINGUAGEM / "vendor", Raiz / "vendor")
    Esperado = (FIXTURES_BANCO / "banco.esperado.txt").read_text(encoding="utf-8").splitlines()
    Resultados = []
    try:
        for Nome in ("banco_novo", "banco_antigo", "banco_misto"):  # os tres ainda importam vendor:banco/banco (a pasta so mudou no L3c2): todos sao forma antiga
            shutil.copyfile(FIXTURES_BANCO / f"{Nome}.fj", Raiz / f"{Nome}.fj")
            Resultados.append((Nome + "_migrado", ProblemasDaFormaAntiga(Forja, str(Raiz), f"{Nome}.fj", Esperado)))
    finally:
        for Sub in (Raiz / "migrado", Raiz):
            Desligar(Sub / "vendor")
    return Resultados


def CasosDeErro(Forja):
    """[(nome, problemas)] dos err_*_l3c_*: a lista de diagnosticos do .esperado.jsonl."""
    Resultados = []
    for Fonte in sorted(SEMANTICA.glob("err_*_l3c_*.fj")):
        _, Saida, _ = Rodar(Forja, ["check", Fonte.name], str(SEMANTICA))
        Resultados.append((Fonte.stem, ProblemasDaLista(EsperadosDe(Fonte), LerDiagnosticos(Saida))))
    return Resultados


def Verificar(Forja):
    """Roda tudo; devolve a quantidade de falhas."""
    with tempfile.TemporaryDirectory(prefix="l3c_") as Nome:
        Pasta = Path(Nome)
        Resultados = CasosDeSemantica(Forja) + CasosDeBanco(Forja, Pasta) + CasosDeErro(Forja)
        for Sub in Pasta.rglob("vendor"):
            Desligar(Sub)
    for Nome, Problemas in Resultados:
        print(f"{'FALHOU' if Problemas else 'ok    '} {Nome}")
        for Problema in Problemas:
            print(f"       {Problema}")
    Falhas = sum(1 for _, Problemas in Resultados if Problemas)
    print(f"casos={len(Resultados)} falhas={Falhas}")
    return Falhas


def AutoTeste():
    """Plantio: ProblemasDoErro reprova codigo/linha/coluna errados, diagnostico a mais, a menos e campo vazio; aprova o certo; as fixtures existem nos pares."""
    Esperado = {"codigo": "E0501", "linha": 3, "coluna": 33}
    Bom = {"codigo": "E0501", "linha": 3, "coluna": 33, "causa": "c", "explicacao": "e", "forma_certa": "f"}
    Casos = [("bom", [Bom], 0), ("codigo", [dict(Bom, codigo="E0502")], 1), ("linha", [dict(Bom, linha=4)], 1), ("coluna", [dict(Bom, coluna=1)], 1),
             ("extra", [Bom, Bom], 1), ("vazio", [], 1), ("sem_forma", [dict(Bom, forma_certa="")], 1)]
    Erros = [Nome for Nome, Lista, Falha in Casos if bool(ProblemasDoErro(Esperado, Lista)) != bool(Falha)]
    Lista = [Bom, dict(Bom, codigo="E0101", linha=4, coluna=30)]
    Esperados = [{"codigo": "E0501", "linha": 3, "coluna": 33}, {"codigo": "E0101", "linha": 4, "coluna": 30}]
    if ProblemasDaLista(Esperados, Lista) or not ProblemasDaLista(Esperados, Lista[:1]) or not ProblemasDaLista(Esperados, Lista[::-1]):
        Erros.append("ProblemasDaLista nao reprova lista curta ou fora de ordem")
    Faltam = [f"ok_l3c_{P}_novo" for P in PARES_SEMANTICA if not (SEMANTICA / f"ok_l3c_{P}_novo.fj").is_file()]
    Faltam += [f"err_E0110_l3c_{P}_antigo" for P in PARES_SEMANTICA if not (SEMANTICA / f"err_E0110_l3c_{P}_antigo.fj").is_file()]
    Faltam += [N for N in ("banco_novo", "banco_antigo", "banco_misto") if not (FIXTURES_BANCO / f"{N}.fj").is_file()]
    Faltam += [N for N in ("misto_semantica.esperado.txt", "banco.esperado.txt") if not (FIXTURES_BANCO / N).is_file()]
    if Faltam or len(list(SEMANTICA.glob("err_*_l3c_*.fj"))) < 8:
        Erros.append(f"fixtures: faltam {Faltam} ou menos de 8 err_")
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
    return 1 if Verificar(str(Path(Argumentos.forja).resolve())) else 0


if __name__ == "__main__":
    sys.exit(Principal())
