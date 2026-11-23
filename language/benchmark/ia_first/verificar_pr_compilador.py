# -*- coding: utf-8 -*-
"""verificar_pr_compilador.py - refaz as 20 respostas do conjunto P PELO COMPILADOR (forja consultar e forja check/test), sem olhar o grep (P-IA1-a17).

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao: onde/chamadores/retorno/campos/membros/tipo_campo/teste_* por `forja consultar ... --json`, quebra por
        mutacao de uma copia do projeto + `forja check` em cada raiz (prog.fj e teste_*.fj), teste_total pelo resumo do `forja test`.

O gabarito do P nasce do grep (montagem\\montar_pr_perguntas.py); aqui a mesma pergunta e respondida por outro caminho. Se os dois divergem,
a pergunta esta mal escolhida ou o gabarito errado: o verificar_conjunto_pr.py acusa.
Uso: ResponderPeloCompilador(Item, Pasta, Forja) -> texto da resposta.
Depende de: Python 3.9+ e conjunto_pr.py. Verificacao: python -I verificar_conjunto_pr.py --forja <forja.exe> --selftest
"""
import json
import re
import shutil
import tempfile
from pathlib import Path

from conjunto_pr import Consultar, RelativoA, RodarForja


def RaizesDoProjeto(Pasta):
    """prog.fj primeiro, depois cada teste_*.fj da raiz (so elas tem `main` e podem ser alvo do forja)."""
    return ["prog.fj"] + sorted(P.name for P in Path(Pasta).glob("teste_*.fj"))


def AcharSimbolo(Forja, Pasta, Nome):
    """Entrada de `consultar simbolos` com esse nome; procura em cada raiz ate achar. ValueError se nao achar ou se houver duas."""
    for Raiz in RaizesDoProjeto(Pasta):
        Achados = [S for S in Consultar(Forja, "simbolos", [Raiz], Pasta)["simbolos"] if S["nome"] == Nome and S["categoria"] != "campo"]
        if len(Achados) > 1:
            raise ValueError("simbolo %s: %d declaracoes" % (Nome, len(Achados)))
        if Achados:
            return Achados[0], Raiz
    raise ValueError("simbolo %s nao existe no projeto" % Nome)


def AlvosDeRefs(Forja, Pasta, Nome, Raiz):
    """Alvos de `consultar refs` numa raiz; raiz que nao enxerga o simbolo (fora do seu fechamento de imports) devolve lista vazia."""
    try:
        return Consultar(Forja, "refs", [Nome, Raiz], Pasta)["alvos"]
    except ValueError as Erro:
        if "nao e declaracao do projeto" in str(Erro):
            return []
        raise


def UsosNaRaiz(Forja, Pasta, Simbolo, Raiz):
    """[(arquivo relativo, posicao, em, chamada)] dos usos do simbolo vistos a partir de uma raiz."""
    Alvos = [A for A in AlvosDeRefs(Forja, Pasta, Simbolo["nome"], Raiz) if A["id"] == Simbolo["id"]]
    return [(RelativoA(Pasta, U["arquivo"]), tuple(U["pos"]), U["em"], U["chamada"]) for A in Alvos for U in A["usos"]]


def UsosDe(Forja, Pasta, Simbolo):
    """Todos os usos do simbolo, nas raizes do projeto: [{arquivo (relativo), chamada, em}] sem repetidos."""
    Unicos = {}
    for Raiz in RaizesDoProjeto(Pasta):
        for Uso in UsosNaRaiz(Forja, Pasta, Simbolo, Raiz):
            Unicos.setdefault(Uso[:3], Uso)
    return [{"arquivo": U[0], "chamada": U[3], "em": U[2]} for U in Unicos.values()]


def RespOnde(Item, Pasta, Forja):
    Simbolo, _ = AcharSimbolo(Forja, Pasta, Item["param"]["simbolo"])
    return "%s:%d" % (RelativoA(Pasta, Simbolo["arquivo"]), Simbolo["pos"][0])


def RespChamadoresArquivos(Item, Pasta, Forja):
    Simbolo, _ = AcharSimbolo(Forja, Pasta, Item["param"]["simbolo"])
    return ",".join(sorted({U["arquivo"] for U in UsosDe(Forja, Pasta, Simbolo) if U["chamada"]}))


def RespChamadoresProcs(Item, Pasta, Forja):
    Simbolo, _ = AcharSimbolo(Forja, Pasta, Item["param"]["simbolo"])
    Procs = {U["em"].split(":", 1)[1] for U in UsosDe(Forja, Pasta, Simbolo) if U["chamada"] and ":test:" not in U["em"]}
    return ",".join(sorted(Procs))


def RespRetorno(Item, Pasta, Forja):
    _, Raiz = AcharSimbolo(Forja, Pasta, Item["param"]["simbolo"])
    return Consultar(Forja, "assinatura", [Item["param"]["simbolo"], Raiz], Pasta)["retorno"]


def CamposDe(Item, Pasta, Forja):
    _, Raiz = AcharSimbolo(Forja, Pasta, Item["param"]["simbolo"])
    return Consultar(Forja, "campos", [Item["param"]["simbolo"], Raiz], Pasta)


def RespCampos(Item, Pasta, Forja):
    return ",".join(C["nome"] for C in CamposDe(Item, Pasta, Forja)["campos"])


def RespMembros(Item, Pasta, Forja):
    return ",".join(M["nome"] for M in CamposDe(Item, Pasta, Forja)["membros"])


def RespTipoCampo(Item, Pasta, Forja):
    Campos = CamposDe(Item, Pasta, Forja)["campos"]
    return next(C["tipo"] for C in Campos if C["nome"] == Item["param"]["campo"])


def NomesDeTestes(Item, Pasta, Forja):
    """{(arquivo, titulo)} dos blocos test que chamam o simbolo (campo `em` = <modulo>:test:<titulo>)."""
    Simbolo, _ = AcharSimbolo(Forja, Pasta, Item["param"]["simbolo"])
    return {(U["arquivo"], U["em"].split(":test:", 1)[1]) for U in UsosDe(Forja, Pasta, Simbolo) if U["chamada"] and ":test:" in U["em"]}


def RespTesteNome(Item, Pasta, Forja):
    Nomes = NomesDeTestes(Item, Pasta, Forja)
    if len(Nomes) != 1:
        raise ValueError("%d blocos test chamam %s, esperado 1" % (len(Nomes), Item["param"]["simbolo"]))
    return next(iter(Nomes))[1]


def RespTesteContagem(Item, Pasta, Forja):
    return str(len(NomesDeTestes(Item, Pasta, Forja)))


def RespTesteArquivos(Item, Pasta, Forja):
    return ",".join(sorted({Arquivo for Arquivo, _ in NomesDeTestes(Item, Pasta, Forja)}))


def RespTesteTotal(Item, Pasta, Forja):
    R = RodarForja(Forja, ["test", Item["param"]["arquivo"]], Pasta)
    Achado = re.search(r"(\d+) testes, (\d+) falharam", R["stdout"])
    if R["rc"] != 0 or not Achado:
        raise ValueError("forja test %s: rc=%s %s" % (Item["param"]["arquivo"], R["rc"], R["stdout"][-200:]))
    return Achado.group(1)


def DeclaracaoPeloCompilador(Item, Pasta, Forja):
    """(arquivo relativo, linha, nome) do que a pergunta quebra renomearia: o campo (consultar campos) ou o proprio simbolo."""
    Simbolo, Raiz = AcharSimbolo(Forja, Pasta, Item["param"]["simbolo"])
    Arquivo = RelativoA(Pasta, Simbolo["arquivo"])
    if "campo" not in Item["param"]:
        return Arquivo, Simbolo["pos"][0], Simbolo["nome"]
    Campos = Consultar(Forja, "campos", [Item["param"]["simbolo"], Raiz], Pasta)["campos"]
    Campo = next(C for C in Campos if C["nome"] == Item["param"]["campo"])
    return Arquivo, Campo["pos"][0], Campo["nome"]


def MutarNome(Pasta, Arquivo, Linha, Nome):
    """Troca a 1a ocorrencia de Nome na linha da declaracao por Nome_x (o resto do projeto fica como esta)."""
    Alvo = Path(Pasta) / Arquivo
    Linhas = Alvo.read_text(encoding="utf-8").replace("\r\n", "\n").split("\n")
    Novo, Vezes = re.subn(r"\b" + re.escape(Nome) + r"\b", Nome + "_x", Linhas[Linha - 1], count=1)
    if not Vezes:
        raise ValueError("%s:%d nao tem %s" % (Arquivo, Linha, Nome))
    Linhas[Linha - 1] = Novo
    Alvo.write_text("\n".join(Linhas), encoding="utf-8", newline="\n")


def ArquivosDoDiagnostico(Pasta, Saida):
    """Arquivos (relativos) citados nas linhas JSON de diagnostico de uma saida do forja."""
    Linhas = [L for L in Saida.split("\n") if L.startswith("{") and '"arquivo"' in L]
    return {RelativoA(Pasta, json.loads(L)["arquivo"]) for L in Linhas}


def ArquivosComErro(Forja, Pasta):
    """Conjunto dos arquivos (relativos) que aparecem em algum diagnostico de `forja check` de qualquer raiz do projeto."""
    Arquivos = set()
    for Raiz in RaizesDoProjeto(Pasta):
        R = RodarForja(Forja, ["check", "--max-erros", "100", Raiz], Pasta)
        Arquivos |= ArquivosDoDiagnostico(Pasta, R["stdout"] + "\n" + R["stderr"])
    return Arquivos


def RespQuebra(Item, Pasta, Forja):
    Arquivo, Linha, Nome = DeclaracaoPeloCompilador(Item, Pasta, Forja)
    with tempfile.TemporaryDirectory(prefix="pr_quebra_") as Tmp:
        Copia = Path(Tmp) / "p"
        shutil.copytree(Pasta, Copia)
        MutarNome(Copia, Arquivo, Linha, Nome)
        return ",".join(sorted(ArquivosComErro(Forja, Copia) - {Arquivo}))


RESPONDEDORES = {"onde": RespOnde, "chamadores_arquivos": RespChamadoresArquivos, "chamadores_procs": RespChamadoresProcs,
                 "retorno": RespRetorno, "campos": RespCampos, "membros": RespMembros, "tipo_campo": RespTipoCampo, "quebra": RespQuebra,
                 "teste_nome": RespTesteNome, "teste_contagem": RespTesteContagem, "teste_arquivos": RespTesteArquivos,
                 "teste_total": RespTesteTotal}


def ResponderPeloCompilador(Item, Pasta, Forja):
    return RESPONDEDORES[Item["tipo"]](Item, Path(Pasta), Forja)
