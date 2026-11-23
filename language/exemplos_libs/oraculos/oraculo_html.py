#!/usr/bin/env python3
r"""html.py - oraculo do pacote vendor:html (lexbor 3.0.0), exemplo html_basico.fj.

Fontes independentes (nao o lexbor):
  (a) arvore HTML: html5lib 1.1 (Python), treebuilder dom; a arvore vira o mesmo texto de html_basico.fj
      (| + recuo de 2 espacos por nivel; elemento <nome> / <svg nome> / <math nome>; atributos na ordem do documento;
      texto "..."; comentario <!-- -->; doctype <!DOCTYPE nome>; documento #document).
  (b) seletores CSS: bs4 + soupsieve 4.1.2 sobre o parse html5lib; o no e dado pelo indice em pre-ordem (documento = 0).
  (c) folha CSS: tinycss2 1.5.1 (regras de topo, at-rules, declaracoes, !important); especificidade calculada aqui
      (A ids, B classes/atributos/pseudo-classes, C tipos e pseudo-elementos; :not conta o argumento; primeiro seletor do grupo,
      como o lexbor guarda). tinycss2 nao calcula especificidade: esse campo e conferido so pela regra acima.
Os textos de entrada sao lidos dos proprios literais de html_basico.fj (mesma entrada nos dois lados).

Modos:
  python oraculos\html.py [--forja <forja.exe>] [--gerar]
    --gerar   escreve exemplos_libs\html_basico.esperado a partir do oraculo (so depois de ele bater com o forja)
    sem --gerar: roda o exemplo e compara; imprime ok N/M por secao e as divergencias explicadas.
Versao: 0.1.1 (html5lib 1.1, soupsieve 4.1.2, tinycss2 1.5.1, bs4 4.15.0)
Changelog: 0.1.1 - nomes em CamelCase (PY-R01); lambda morta removida.
           0.1.0 - primeira versao.
"""
import argparse
import ast
import pathlib
import re
import subprocess
import sys

import bs4
import html5lib
import soupsieve  # noqa: F401  (o bs4 usa o seletor do soupsieve no .select)
import tinycss2

PASTA_EXEMPLOS = pathlib.Path(__file__).resolve().parent.parent
FORJA_PADRAO = PASTA_EXEMPLOS.parent / "compiler" / "forja.exe"
EXEMPLO = PASTA_EXEMPLOS / "html_basico.fj"
ESPERADO = PASTA_EXEMPLOS / "html_basico.esperado"
RECUOS = ["  " * n for n in range(12)]
NS_SVG = "http://www.w3.org/2000/svg"
NS_MATH = "http://www.w3.org/1998/Math/MathML"
# Divergencias EXPLICADAS (nao sao falha): linha esperada pela CSS -> (linha que o lexbor 3.0.0 devolve, causa).
EXPLICADAS = {
    "regra 5 ul li:first-child spec=0,1,2 decls=1": (
        "regra 5 ul li:first-child spec=0,0,2 decls=1",
        "lexbor 3.0.0 nao soma a especificidade de pseudo-classe sem argumento (:first-child, :hover, :link); "
        "a CSS exige B+1. Medido com lexbor_build\\teste_spec.c: p:first-child -> 0,0,1, p:not(.a) -> 0,1,1."),
}


def LerLiteraisFj():
    """Le DOCUMENTOS, SELETORES e FOLHA do proprio exemplo (mesma entrada nos dois lados)."""
    Fonte = EXEMPLO.read_text(encoding="utf-8")
    Documentos = ast.literal_eval("[" + re.search(r"DOCUMENTOS[^{]*\{(.*?)\n\}", Fonte, re.S).group(1) + "]")
    Seletores = ast.literal_eval("[" + re.search(r"SELETORES[^{]*\{(.*?)\}", Fonte, re.S).group(1) + "]")
    Folha = ast.literal_eval(re.search(r"const FOLHA: str = (\".*\")", Fonte).group(1))
    return Documentos, Seletores, Folha


def PrefixoNs(No):
    if No.namespaceURI == NS_SVG:
        return "svg "
    if No.namespaceURI == NS_MATH:
        return "math "
    return ""


class NoTexto:
    """Texto vizinho ja unido: o html5lib guarda cada caractere de entidade num no, o lexbor junta."""

    # lint-ok: PY-R01 nodeType e childNodes sao atributos do protocolo DOM (minidom), nao nomes nossos
    nodeType = 3
    # lint-ok: PY-R01 childNodes e atributo do protocolo DOM (minidom)
    childNodes = ()

    def __init__(self, Dado):
        self.data = Dado


def MesclaTextos(Filhos):
    """Une textos adjacentes (o lexbor entrega um no de texto por bloco; o html5lib divide por entidade)."""
    Saida = []
    for Filho in Filhos:
        if Filho.nodeType == 3 and Saida and Saida[-1].nodeType == 3:
            Saida[-1] = NoTexto(Saida[-1].data + Filho.data)
        else:
            Saida.append(Filho)
    return Saida


def NormalizaCss(Linha):
    """Forma comum de comparacao: tira aspas e o comentario /**/ que o tinycss2 põe, e troca 2n+1 por odd (o lexbor canoniza)."""
    Linha = Linha.replace('"', "").replace("'", "").replace("/**/", "")
    return Linha.replace("2n+1", "odd")


def LinhasArvore(No, Prof, Saida):
    Recuo = RECUOS[Prof]
    Tipo = No.nodeType
    if Tipo == 9:
        Saida.append(f"| {Recuo}#document")
    elif Tipo == 10:
        Saida.append(f"| {Recuo}<!DOCTYPE {No.name}>")
    elif Tipo == 8:
        Saida.append(f"| {Recuo}<!-- {No.data} -->")
    elif Tipo == 3:
        Saida.append(f"| {Recuo}\"{No.data}\"")
    elif Tipo == 1:
        Saida.append(f"| {Recuo}<{PrefixoNs(No)}{No.localName}>")
        for Nome, Valor in No.attributes.items():
            Saida.append(f"| {RECUOS[Prof + 1]}{Nome}=\"{Valor}\"")
    for Filho in MesclaTextos(No.childNodes):
        LinhasArvore(Filho, Prof + 1, Saida)


def EsperadoArvores(Documentos):
    Saida = []
    for i, Fonte in enumerate(Documentos):
        Saida.append(f"== arvore {i}")
        Arvore = html5lib.parse(Fonte, treebuilder="dom")
        LinhasArvore(Arvore, 0, Saida)
    return Saida


def EspecificidadePrimeiro(Seletor):
    """A,B,C do primeiro seletor complexo do grupo (conta de simples: ver a docstring do modulo)."""
    Primeiro = Seletor.split(",")[0]
    Limpo = re.sub(r"\[[^\]]*\]", "[]", Primeiro)
    Limpo = re.sub(r":not\(", "(", Limpo)
    A = len(re.findall(r"#[\w-]+", Limpo))
    B = len(re.findall(r"\.[\w-]+", Limpo)) + Limpo.count("[]")
    B += len(re.findall(r"(?<!:):(?!:)[\w-]+", Limpo))
    PseudoElem = len(re.findall(r"::[\w-]+", Limpo))
    SemPseudo = re.sub(r"::?[\w-]+(\([^)]*\))?", " ", Limpo)
    SemPseudo = re.sub(r"[#.][\w-]+", " ", SemPseudo)
    SemPseudo = re.sub(r"[\[\]()]", " ", SemPseudo)
    Tipos = re.findall(r"(?<![\w-])[a-zA-Z][\w-]*", SemPseudo)
    C = len(Tipos) + PseudoElem
    return A, B, C


def EsperadoSeletores(Documentos, Seletores):
    Soup = bs4.BeautifulSoup(Documentos[0], "html5lib")
    Descendentes = list(Soup.descendants)
    Indice = {id(No): i + 1 for i, No in enumerate(Descendentes)}
    Saida = ["== seletores"]
    for Sel in Seletores:
        Achados = Soup.select(Sel)
        if not Achados:
            Saida.append(f"{Sel} -")
        for No in Achados:
            Saida.append(f"{Sel} {Indice[id(No)]}")
    return Saida


def TextoSeletor(Prelude):
    return tinycss2.serialize(Prelude).strip()


def EsperadoCss(Folha):
    Regras = tinycss2.parse_stylesheet(Folha, skip_comments=True, skip_whitespace=True)
    Estilo = [R for R in Regras if R.type == "qualified-rule"]
    AtRules = sum(1 for R in Regras if R.type == "at-rule")
    Saida = ["== css", f"regras={len(Estilo)} atrules={AtRules}"]
    for Indice, Regra in enumerate(Estilo):
        Sel = TextoSeletor(Regra.prelude)
        A, B, C = EspecificidadePrimeiro(Sel)
        Decls = [D for D in tinycss2.parse_declaration_list(Regra.content, skip_comments=True, skip_whitespace=True)
                 if D.type == "declaration"]
        Saida.append(f"regra {Indice} {Sel} spec={A},{B},{C} decls={len(Decls)}")
        for D in Decls:
            Valor = tinycss2.serialize(D.value).strip()
            Imp = "true" if D.important else "false"
            Saida.append(f"  {D.name}={Valor} imp={Imp}")
    return Saida


def EsperadoCompleto():
    Documentos, Seletores, Folha = LerLiteraisFj()
    return EsperadoArvores(Documentos) + EsperadoSeletores(Documentos, Seletores) + EsperadoCss(Folha)


def Secoes(Linhas):
    """Divide a saida em blocos pelos cabecalhos '== ...'."""
    Atual = None
    Mapa = {}
    for Linha in Linhas:
        if Linha.startswith("== "):
            Atual = Linha
            Mapa.setdefault(Atual, [])
        else:
            Mapa.setdefault(Atual, []).append(Linha)
    return Mapa


def Compara(Esp, Obt):
    SecEsp, SecObt = Secoes(Esp), Secoes(Obt)
    Resumo = {"arvore": [0, 0], "seletor": [0, 0], "css": [0, 0, 0]}
    Divergencias = []
    for Cab in SecEsp:
        if Cab.startswith("== arvore"):
            Resumo["arvore"][1] += 1
            if SecEsp[Cab] == SecObt.get(Cab):
                Resumo["arvore"][0] += 1
            else:
                Divergencias.append(Cab)
    EsSel, ObSel = SecEsp.get("== seletores", []), SecObt.get("== seletores", [])
    for Linha in EsSel:
        Resumo["seletor"][1] += 1
        if Linha in ObSel:
            Resumo["seletor"][0] += 1
        else:
            Divergencias.append(f"seletor: {Linha}")
    for Linha in ObSel:
        if Linha not in EsSel:
            Divergencias.append(f"seletor a mais no forja: {Linha}")
    EsCss, ObCss = SecEsp.get("== css", []), SecObt.get("== css", [])
    ObCssNorm = [NormalizaCss(L) for L in ObCss]
    for Linha in EsCss:
        Resumo["css"][1] += 1
        if NormalizaCss(Linha) in ObCssNorm:
            Resumo["css"][0] += 1
        elif Linha in EXPLICADAS and EXPLICADAS[Linha][0] in ObCss:
            Resumo["css"][2] += 1
        else:
            Divergencias.append(f"css: {Linha}")
    return Resumo, Divergencias


def RodaForja(Forja):
    R = subprocess.run([str(Forja), "run", str(EXEMPLO)], capture_output=True, cwd=str(PASTA_EXEMPLOS))
    if R.returncode != 0:
        raise SystemExit(f"forja run falhou (rc={R.returncode}): {R.stderr.decode('utf-8', 'replace')[:600]}")
    return R.stdout.decode("utf-8").replace("\r\n", "\n").splitlines()


def Main():
    Parser = argparse.ArgumentParser(description="oraculo do vendor html (lexbor)")
    Parser.add_argument("--forja", default=str(FORJA_PADRAO))
    Parser.add_argument("--gerar", action="store_true")
    Args = Parser.parse_args()
    Esp = EsperadoCompleto()
    Obt = RodaForja(Args.forja)
    Resumo, Divergencias = Compara(Esp, Obt)
    if Args.gerar:
        # esperado = saida do lexbor (forma canonica dele), so gravada se o oraculo independente aceitar tudo
        if Divergencias:
            print("nao gravado: ha divergencias", *Divergencias[:10], sep="\n")
            return 1
        ESPERADO.write_text("\n".join(Obt) + "\n", encoding="utf-8", newline="\r\n")  # CRLF: como os outros .esperado da casa
        print(f"gravado {ESPERADO} ({len(Obt)} linhas)")
        return 0
    print(f"arvores ok {Resumo['arvore'][0]}/{Resumo['arvore'][1]}")
    print(f"seletores ok {Resumo['seletor'][0]}/{Resumo['seletor'][1]}")
    print(f"css ok {Resumo['css'][0]}/{Resumo['css'][1]} (explicadas: {Resumo['css'][2]})")
    for Linha in Divergencias[:40]:
        print("DIVERGE", Linha)
    return 0 if not Divergencias else 1


if __name__ == "__main__":
    sys.exit(Main())
