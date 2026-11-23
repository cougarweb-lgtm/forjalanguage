# gerar_migracao_doc.py -- gera a secao "RENOMEADOS" de language\doc\migracao.txt a partir da tabela unica de renomeados (compiler\common\renomeados.c)
"""A tabela de nomes em portugues que viram ingles (Emenda 4, A32) vive so em C (compiler\\common\\renomeados.c). Este programa le as entradas de la e acrescenta ao fim de
migracao.txt a secao "RENOMEADOS (Emenda 4)", uma linha por entrada, com tabulacao: classe, escopo, antigo, novo, nota. O texto da migracao.txt que vem antes da secao nao
e tocado; rodar de novo troca so a secao (idempotente). A migracao.txt entra no forja.exe por .incbin (common\\doc_embutida.c).
Uso:
    python -I gerar_migracao_doc.py                 grava a migracao.txt (sem BOM, fim de linha LF, como o arquivo ja era)
    python -I gerar_migracao_doc.py --verificar     imprime "igual" (exit 0) ou "diferente" (exit 1) comparando o arquivo gravado com o que sai agora
    python -I gerar_migracao_doc.py --secao         imprime so a secao gerada (o que o teste do cli compara com `forja spec --secao migracao`)
    python -I gerar_migracao_doc.py --selftest
    --tabela <renomeados.c> e --doc <migracao.txt> trocam os arquivos (o selftest usa copias em pasta temporaria).
Formato esperado do renomeados.c: `{REN_X, "escopo", "antigo", "novo", "nota", recusar},` (uma por linha; o ultimo campo, true|false, e opcional e nao vai para a doc) e `[REN_X] = "nome",` por classe; erro de formato = exit 2.
Versao: 0.4.0
Changelog: 0.4.0 - IA1-L3f: o cabecalho da secao diz que a forma antiga e E0110 (recusar = true) e que o forja migrar a troca.
           0.3.0 - IA1-L3d1: o cabecalho da secao cita o comando `forja migrar`.
           0.2.0 - IA1-L3c: aceita o campo recusar; o cabecalho da secao diz "dupla aceitacao".
           0.1.0 - IA1-L3a: primeira versao.
Verificacao: python -I gerar_migracao_doc.py --selftest && python -I gerar_migracao_doc.py --verificar
"""

import argparse
import re
import sys
import tempfile
from pathlib import Path

LANG = Path(__file__).resolve().parent.parent.parent
TABELA_PADRAO = LANG / "compiler" / "common" / "renomeados.c"
DOC_PADRAO = LANG / "doc" / "migracao.txt"
MARCA = "RENOMEADOS (Emenda 4)"
ENTRADA = re.compile(r'^\s*\{(REN_\w+),\s*"([^"\\\t]*)",\s*"([^"\\\t]*)",\s*"([^"\\\t]*)",\s*"([^"\\\t]*)"(?:,\s*(?:true|false))?\},\s*$', re.M)
CLASSE = re.compile(r'^\s*\[(REN_\w+)\]\s*=\s*"([^"\\\t]*)",\s*$', re.M)
LINHAS_ENTRADA = re.compile(r"^\s*\{REN_", re.M)
CABECALHO = [
    MARCA + " - nomes em portugues da superficie que viram ingles (IDIOMA UNICO = INGLES). Gerado por tests\\ia_first\\gerar_migracao_doc.py a partir de",
    "compiler\\common\\renomeados.c (tabela unica); nao edite a mao. Estado (passo L3f): so a forma da coluna novo compila; a da coluna antigo e E0110 \"forma antiga; rode forja migrar\"",
    "(com a troca sugerida) e o comando `forja migrar <arquivo.fj|pasta> [--aplicar] [--comentarios]` troca as formas antigas pelas novas no projeto todo (--dry-run e o padrao).",
    "classe\tescopo\tantigo\tnovo\tnota",
]


class ErroFormato(Exception):
    """A tabela em C nao esta no formato que o gerador le."""


def LerTabela(Caminho):
    """As entradas de renomeados.c como lista de (classe_texto, escopo, antigo, novo, nota), na ordem da tabela; ErroFormato se uma linha de entrada nao casa."""
    Texto = Caminho.read_text(encoding="utf-8")
    Classes = dict(CLASSE.findall(Texto))
    Achadas = ENTRADA.findall(Texto)
    if not Achadas or len(Achadas) != len(LINHAS_ENTRADA.findall(Texto)):
        raise ErroFormato(f"{Caminho.name}: {len(LINHAS_ENTRADA.findall(Texto))} linhas de entrada, {len(Achadas)} no formato esperado")
    Linhas = []
    for Classe, Escopo, Antigo, Novo, Nota in Achadas:
        if Classe not in Classes:
            raise ErroFormato(f"{Caminho.name}: classe {Classe} sem linha em CLASSE_TEXTO")
        if not Escopo or not Antigo or not Novo or not Nota:
            raise ErroFormato(f"{Caminho.name}: entrada {Antigo!r} com campo vazio")
        Linhas.append((Classes[Classe], Escopo, Antigo, Novo, Nota))
    return Linhas


def MontarSecao(Linhas):
    """O texto da secao: a marca, o cabecalho e uma linha por entrada, terminado em LF."""
    return "\n".join(CABECALHO + ["\t".join(Linha) for Linha in Linhas]) + "\n"


def MontarDoc(Atual, Secao):
    """A migracao.txt com a secao no fim: o que veio antes da marca fica como esta (uma linha em branco antes da secao)."""
    Fim = Atual.find("\n" + MARCA)
    Base = Atual if Fim < 0 else Atual[:Fim + 1]
    return Base.rstrip("\n") + "\n\n" + Secao


def Gerar(Tabela, Doc):
    """O novo conteudo da migracao.txt para a tabela e o doc dados."""
    return MontarDoc(Doc.read_text(encoding="utf-8"), MontarSecao(LerTabela(Tabela)))


def Verificar(Tabela, Doc):
    """True se o arquivo gravado e igual ao que sai agora."""
    return Doc.read_bytes() == Gerar(Tabela, Doc).encode("utf-8")


def Selftest():
    """Gera, confere, muda uma entrada sem regerar (tem de dar diferente), regera (igual) e checa a idempotencia e o texto antigo intacto."""
    with tempfile.TemporaryDirectory() as Pasta:
        Tabela, Doc = Path(Pasta) / "renomeados.c", Path(Pasta) / "migracao.txt"
        Tabela.write_text('[REN_ROTULO] = "rotulo",\n    {REN_ROTULO, "e", "a", "b", "n"},\n', encoding="utf-8", newline="\n")
        Antigo = "TEXTO ANTIGO\nlinha 2\n"
        Doc.write_text(Antigo, encoding="utf-8", newline="\n")
        Doc.write_bytes(Gerar(Tabela, Doc).encode("utf-8"))
        Primeiro = Doc.read_bytes()
        Resultados = [("igual apos gerar", Verificar(Tabela, Doc)), ("texto antigo intacto", Primeiro.decode().startswith(Antigo + "\n" + MARCA))]
        Tabela.write_text(Tabela.read_text().replace('"b"', '"c"'), encoding="utf-8", newline="\n")
        Resultados.append(("diferente apos mudar a tabela", not Verificar(Tabela, Doc)))
        Doc.write_bytes(Gerar(Tabela, Doc).encode("utf-8"))
        Resultados.append(("igual apos regerar", Verificar(Tabela, Doc) and Doc.read_bytes().count(MARCA.encode()) == 1))
        Tabela.write_text(Tabela.read_text() + '    {REN_ROTULO, "e", "a", "b"},\n', encoding="utf-8", newline="\n")
        try:
            LerTabela(Tabela)
            Resultados.append(("linha de entrada malformada recusada", False))
        except ErroFormato:
            Resultados.append(("linha de entrada malformada recusada", True))
    for Nome, Ok in Resultados:
        print(("ok    " if Ok else "FALHA ") + Nome)
    return all(Ok for _, Ok in Resultados)


def Principal():
    Parser = argparse.ArgumentParser(description="Gera a secao RENOMEADOS da migracao.txt a partir de common\\renomeados.c.")
    Parser.add_argument("--tabela", type=Path, default=TABELA_PADRAO)
    Parser.add_argument("--doc", type=Path, default=DOC_PADRAO)
    Grupo = Parser.add_mutually_exclusive_group()
    Grupo.add_argument("--verificar", action="store_true")
    Grupo.add_argument("--secao", action="store_true")
    Grupo.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    if Args.selftest:
        return 0 if Selftest() else 1
    try:
        if Args.secao:
            sys.stdout.write(MontarSecao(LerTabela(Args.tabela)))
            return 0
        if Args.verificar:
            Igual = Verificar(Args.tabela, Args.doc)
            print("igual" if Igual else "diferente")
            return 0 if Igual else 1
        Args.doc.write_bytes(Gerar(Args.tabela, Args.doc).encode("utf-8"))
    except ErroFormato as Erro:
        print(f"erro: {Erro}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
