# -*- coding: utf-8 -*-
"""montar_pr_perguntas.py - deriva por GREP (texto do fonte) a resposta de cada uma das 20 perguntas do conjunto P (P-IA1-a17).

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao: 11 tipos de pergunta (onde, chamadores_arquivos, chamadores_procs, retorno, campos, membros, tipo_campo,
        quebra, teste_nome, teste_contagem, teste_arquivos, teste_total), cada um por regex/varredura de linhas no fonte sem comentarios.

A resposta daqui e o GABARITO; o verificar_conjunto_pr.py a refaz por outro caminho (forja consultar e execucao do forja check) e as duas tem de bater.
Uso: Derivar(Item, Pasta) com Item = {"tipo", "param"} e Pasta = o projeto montado. Devolve (resposta, declaracao ou None).
Verificacao: python -I ..\\verificar_conjunto_pr.py --forja <forja.exe> --selftest
Depende de: Python 3.9+ e conjunto_pr.py (pasta acima).
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from conjunto_pr import LerFontes  # noqa: E402  (precisa do sys.path acima)

PREFIXO_DECL = r"^[ \t]*(?:pub[ \t]+)?(?:export[ \t]+)?(?:proc|struct|enum|union|const|unit)[ \t]+"
INICIO_DE_ITEM = re.compile(r'^(?:pub[ \t]+)?(?:export[ \t]+)?proc[ \t]+(\w+)|^test[ \t]+"(.*)"[ \t]*\{')


def AcharDeclaracao(Fontes, Nome):
    """(arquivo, linha de 1) da unica declaracao de Nome no projeto; ValueError se houver 0 ou mais de 1."""
    Regex = re.compile(PREFIXO_DECL + re.escape(Nome) + r"\b")
    Achados = [(Rel, N) for Rel, Texto in Fontes.items() for N, Linha in enumerate(Texto.split("\n"), 1) if Regex.match(Linha)]
    if len(Achados) != 1:
        raise ValueError("declaracao de %s: %d achados, esperado 1: %s" % (Nome, len(Achados), Achados))
    return Achados[0]


def LinhasDoBloco(Fontes, Arquivo, Inicio):
    """[(n, texto)] da linha Inicio ate a primeira linha que e so '}' na coluna 0 (inclusive)."""
    Linhas = Fontes[Arquivo].split("\n")
    Saida = []
    for N in range(Inicio, len(Linhas) + 1):
        Saida.append((N, Linhas[N - 1]))
        if N > Inicio and Linhas[N - 1].rstrip() == "}":
            return Saida
    raise ValueError("%s:%d: bloco sem fechamento" % (Arquivo, Inicio))


def CamposDoBloco(Fontes, Arquivo, Inicio):
    """[(nome, tipo, linha)] dos campos de uma struct (linhas 'nome: tipo [= padrao]')."""
    Campos = []
    for N, Texto in LinhasDoBloco(Fontes, Arquivo, Inicio)[1:-1]:
        Achado = re.match(r"^[ \t]+(\w+)[ \t]*:[ \t]*(.*?)(?:[ \t]*=.*)?[ \t]*$", Texto)
        if Achado:
            Campos.append((Achado.group(1), Achado.group(2).strip(), N))
    return Campos


def ChamadasDe(Fontes, Nome):
    """[(arquivo, linha)] das linhas que chamam Nome( fora da propria declaracao."""
    Decl = re.compile(PREFIXO_DECL + re.escape(Nome) + r"\b")
    Chamada = re.compile(r"\b" + re.escape(Nome) + r"[ \t]*\(")
    return [(Rel, N) for Rel, Texto in Fontes.items() for N, L in enumerate(Texto.split("\n"), 1) if Chamada.search(L) and not Decl.match(L)]


def ItemQueContem(Brutos, Arquivo, Linha):
    """('proc', nome) ou ('test', titulo) do ultimo proc/test de topo que comeca antes da Linha (cabecalhos lidos do fonte bruto)."""
    Linhas = Brutos[Arquivo].split("\n")
    for N in range(Linha, 0, -1):
        Achado = INICIO_DE_ITEM.match(Linhas[N - 1])
        if Achado:
            return ("proc", Achado.group(1)) if Achado.group(1) else ("test", Achado.group(2))
    return (None, None)


def Declaracao(Param, Fontes):
    """Declaracao a mutar/medir: o campo (se Param tem 'campo') ou o proprio simbolo. Devolve {arquivo, linha, nome}."""
    Arquivo, Linha = AcharDeclaracao(Fontes, Param["simbolo"])
    if "campo" not in Param:
        return {"arquivo": Arquivo, "linha": Linha, "nome": Param["simbolo"]}
    for Nome, _, N in CamposDoBloco(Fontes, Arquivo, Linha):
        if Nome == Param["campo"]:
            return {"arquivo": Arquivo, "linha": N, "nome": Nome}
    raise ValueError("campo %s nao existe em %s" % (Param["campo"], Param["simbolo"]))


def RespOnde(Param, Fontes, Brutos):
    Arquivo, Linha = AcharDeclaracao(Fontes, Param["simbolo"])
    return "%s:%d" % (Arquivo, Linha)


def RespChamadoresArquivos(Param, Fontes, Brutos):
    return ",".join(sorted({Rel for Rel, _ in ChamadasDe(Fontes, Param["simbolo"])}))


def RespChamadoresProcs(Param, Fontes, Brutos):
    Itens = {ItemQueContem(Brutos, Rel, N) for Rel, N in ChamadasDe(Fontes, Param["simbolo"])}
    return ",".join(sorted(Nome for Tipo, Nome in Itens if Tipo == "proc"))


def RespRetorno(Param, Fontes, Brutos):
    Arquivo, Linha = AcharDeclaracao(Fontes, Param["simbolo"])
    Achado = re.search(r"\)[ \t]*->[ \t]*(.*?)[ \t]*\{[ \t]*$", Fontes[Arquivo].split("\n")[Linha - 1])
    if not Achado:
        raise ValueError("%s: assinatura sem retorno em linha unica" % Param["simbolo"])
    return Achado.group(1)


def RespCampos(Param, Fontes, Brutos):
    Arquivo, Linha = AcharDeclaracao(Fontes, Param["simbolo"])
    return ",".join(Nome for Nome, _, _ in CamposDoBloco(Fontes, Arquivo, Linha))


def RespMembros(Param, Fontes, Brutos):
    Arquivo, Linha = AcharDeclaracao(Fontes, Param["simbolo"])
    Nomes = [re.match(r"^[ \t]+(\w+)", T).group(1) for _, T in LinhasDoBloco(Fontes, Arquivo, Linha)[1:-1] if re.match(r"^[ \t]+\w+", T)]
    return ",".join(Nomes)


def RespTipoCampo(Param, Fontes, Brutos):
    Arquivo, Linha = AcharDeclaracao(Fontes, Param["simbolo"])
    for Nome, Tipo, _ in CamposDoBloco(Fontes, Arquivo, Linha):
        if Nome == Param["campo"]:
            return Tipo
    raise ValueError("campo %s nao existe em %s" % (Param["campo"], Param["simbolo"]))


def RespQuebra(Param, Fontes, Brutos):
    Decl = Declaracao(Param, Fontes)
    Palavra = re.compile(r"\b" + re.escape(Decl["nome"]) + r"\b")
    return ",".join(sorted(Rel for Rel, Texto in Fontes.items() if Rel != Decl["arquivo"] and Palavra.search(Texto)))


def BlocosTestQueChamam(Param, Fontes, Brutos):
    """{(arquivo, titulo)} dos blocos test que chamam o simbolo."""
    Itens = {(Rel, ItemQueContem(Brutos, Rel, N)) for Rel, N in ChamadasDe(Fontes, Param["simbolo"])}
    return {(Rel, Nome) for Rel, (Tipo, Nome) in Itens if Tipo == "test"}


def RespTesteNome(Param, Fontes, Brutos):
    Blocos = BlocosTestQueChamam(Param, Fontes, Brutos)
    if len(Blocos) != 1:
        raise ValueError("%s: %d blocos test chamam, esperado 1" % (Param["simbolo"], len(Blocos)))
    return next(iter(Blocos))[1]


def RespTesteContagem(Param, Fontes, Brutos):
    return str(len(BlocosTestQueChamam(Param, Fontes, Brutos)))


def RespTesteArquivos(Param, Fontes, Brutos):
    return ",".join(sorted({Rel for Rel, _ in BlocosTestQueChamam(Param, Fontes, Brutos)}))


def RespTesteTotal(Param, Fontes, Brutos):
    return str(len(re.findall(r'^test[ \t]+"', Brutos[Param["arquivo"]], flags=re.M)))


DERIVADORES = {"onde": RespOnde, "chamadores_arquivos": RespChamadoresArquivos, "chamadores_procs": RespChamadoresProcs,
               "retorno": RespRetorno, "campos": RespCampos, "membros": RespMembros, "tipo_campo": RespTipoCampo, "quebra": RespQuebra,
               "teste_nome": RespTesteNome, "teste_contagem": RespTesteContagem, "teste_arquivos": RespTesteArquivos,
               "teste_total": RespTesteTotal}


def Derivar(Item, Pasta):
    """(resposta, declaracao) de uma pergunta do P, por grep no projeto montado em Pasta. A declaracao so existe no tipo quebra."""
    Fontes, Brutos = LerFontes(Pasta), LerFontes(Pasta, SemComentario=False)
    Resposta = DERIVADORES[Item["tipo"]](Item["param"], Fontes, Brutos)
    return Resposta, (Declaracao(Item["param"], Fontes) if Item["tipo"] == "quebra" else None)
