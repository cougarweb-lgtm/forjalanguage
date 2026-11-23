"""censo_e0913.py - censo c2 do M6e (P93a): candidatos a E0913 nos .fj do repositorio.

Procura, por proc, um emprestimo de elemento de uma colecao (`let p = &l[i]`, `var p = &l[i]`, `l[..]`, `get_ptr(&l, ...)`,
`for &x in l`) seguido de uma mutacao da MESMA colecao (append/insert/remove/delete/clear/resize). Marca se o nome
emprestado (p) aparece depois da mutacao (usado_depois = sim: E0913 de verdade; nao: so risco). Heuristica textual
por proc (o E0913 real e por fluxo do fonte); serve para decidir o PARA do c2 e a lista de conserto do P93g.
Achados em pastas congeladas (tests\\dourado, examples, benchmark\\corpus\\dourado e gerado, tests\\lowering) saem marcados.

Versao: 0.1.0
Changelog:
  0.1.0 - M6e P93a: primeira versao (emprestimo x mutacao por proc, usado depois, congelados).
Uso: python tests\\nativo\\m6e\\censo_e0913.py [--so-usado-depois] [--raiz <pasta_language>]
Depende de: stdlib do Python.
"""

import argparse
import re
import sys
from pathlib import Path

PastaPadrao = Path(__file__).resolve().parents[3]
PastasVarridas = ["std", "tests", "examples", "exemplos_libs", "vendor", "benchmark"]
PastasCongeladas = ["tests/dourado", "examples", "benchmark/corpus/dourado", "benchmark/corpus/gerado", "tests/lowering"]
RegexProc = re.compile(r"^\s*(export\s+)?proc\s")
RegexEmprestimoElemento = re.compile(r"\b(?:let|var)\s+(\w+)\s*=\s*&\s*(\w+)\s*\[")
RegexEmprestimoFatia = re.compile(r"\b(?:let|var)\s+(\w+)\s*=\s*(\w+)\s*\[\s*\.\.")
RegexEmprestimoGetPtr = re.compile(r"\b(?:let|var)\s+(\w+)\s*=\s*get_ptr\(\s*&\s*(\w+)")
RegexLacoEmprestimo = re.compile(r"\bfor\s+(?:\w+\s*,\s*)?&\s*\w+\s+in\s+(\w+)")
RegexMutacao = re.compile(r"\b(append|insert|remove|delete|clear|resize)\(\s*&?\s*(\w+)")


def ListarProcs(Linhas):
    """Divide o arquivo em blocos (lista de (numero_da_linha, texto)) a cada linha de proc."""
    Blocos, Atual = [], []
    for Numero, Texto in enumerate(Linhas, 1):
        if RegexProc.match(Texto) and Atual:
            Blocos.append(Atual)
            Atual = []
        Atual.append((Numero, Texto))
    if Atual:
        Blocos.append(Atual)
    return Blocos


def EmprestimosDaLinha(Numero, Texto):
    """Devolve (nome_emprestado, colecao, numero) para cada forma de emprestimo da linha."""
    Achados = []
    for Regex in (RegexEmprestimoElemento, RegexEmprestimoFatia, RegexEmprestimoGetPtr):
        Casou = Regex.search(Texto)
        if Casou:
            Achados.append((Casou.group(1), Casou.group(2), Numero))
    Laco = RegexLacoEmprestimo.search(Texto)
    if Laco:
        Achados.append(("", Laco.group(1), Numero))
    return Achados


def AchadosDoProc(Bloco):
    """Pares (emprestimo, mutacao) do mesmo proc: mutacao da colecao emprestada depois do emprestimo."""
    Emprestimos = []
    for Numero, Texto in Bloco:
        Emprestimos.extend(EmprestimosDaLinha(Numero, Texto))
    Achados = []
    for Nome, Colecao, Linha in Emprestimos:
        for Numero, Texto in Bloco:
            Mut = RegexMutacao.search(Texto)
            if Numero > Linha and Mut and Mut.group(2) == Colecao:
                Achados.append((Nome, Colecao, Linha, Numero, UsadoDepois(Bloco, Nome, Numero)))
                break
    return Achados


def UsadoDepois(Bloco, Nome, LinhaMutacao):
    """O nome emprestado aparece depois da mutacao? (laco `for &x` nao tem nome: conta como risco)."""
    if not Nome:
        return "laco"
    Padrao = re.compile(r"\b" + re.escape(Nome) + r"\b")
    for Numero, Texto in Bloco:
        if Numero > LinhaMutacao and Padrao.search(Texto):
            return "sim"
    return "nao"


def EhCongelado(Relativo):
    return any(Relativo.startswith(Pasta + "/") or Relativo == Pasta for Pasta in PastasCongeladas)


def VarrerArquivo(Caminho, Raiz):
    Relativo = Caminho.relative_to(Raiz).as_posix()
    Linhas = Caminho.read_text(encoding="utf-8", errors="replace").splitlines()
    Saida = []
    for Bloco in ListarProcs(Linhas):
        for Nome, Colecao, Linha, Mut, Usado in AchadosDoProc(Bloco):
            Congelado = "CONGELADO" if EhCongelado(Relativo) else "livre"
            Saida.append(f"{Relativo} | {Colecao} | emprestimo {Linha} | mutacao {Mut} | p={Nome or '-'} | usado_depois={Usado} | {Congelado}")
    return Saida


def Principal(Argumentos):
    Parser = argparse.ArgumentParser(description="censo c2 do M6e: candidatos a E0913")
    Parser.add_argument("--so-usado-depois", action="store_true")
    Parser.add_argument("--raiz", default=str(PastaPadrao))
    Opcoes = Parser.parse_args(Argumentos)
    Raiz = Path(Opcoes.raiz)
    Linhas = []
    for Pasta in PastasVarridas:
        for Arquivo in sorted((Raiz / Pasta).rglob("*.fj")):
            if "__pycache__" in Arquivo.parts:
                continue
            Linhas.extend(VarrerArquivo(Arquivo, Raiz))
    if Opcoes.so_usado_depois:
        Linhas = [L for L in Linhas if "usado_depois=sim" in L or "usado_depois=laco" in L]
    print("\n".join(Linhas))
    print(f"total: {len(Linhas)}")
    return 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
