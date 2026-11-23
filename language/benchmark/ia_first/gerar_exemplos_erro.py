# gerar_exemplos_erro.py -- gera language\doc\exemplos.txt: o par errado/certo de cada codigo, tirado da bateria (tests\sintaxe e tests\semantica, err_*.fj)
"""O `forja explicar --exemplos` e o `forja spec --exemplo` mostram o que este arquivo traz; ele entra no forja.exe por .incbin (common\\doc_embutida.c).
Para cada codigo: entre as fixtures err_<CODIGO>_*.fj cujo 1o diagnostico e mesmo aquele codigo, fica a que tem conserto verificado (a "correcao" de confianca certa ou
provavel do proprio compilador, aplicada ao fonte, da `check` exit 0), depois a menor, depois o nome. O errado e o trecho do proprio arquivo da fixture (sem os comentarios
do topo; no maximo 9 linhas em volta do erro); o certo e o mesmo trecho do fonte consertado. Nenhum nome sai de fora do arquivo da fixture.
Uso:
    python -I gerar_exemplos_erro.py --forja <forja.exe> [--saida <exemplos.txt>]
    python -I gerar_exemplos_erro.py --forja <forja.exe> --conferir [--saida <exemplos.txt>]     (exit 1 se o arquivo gravado difere do que sai agora)
    python -I gerar_exemplos_erro.py --selftest
Formato (fim de linha CRLF, como a pasta doc): cabecalho; depois, por codigo, "@@ <codigo> <fixture> <linha do erro> <1a linha do trecho>", "= <forma_certa>",
  "< <linha errada>" e (se ha conserto verificado) "> <linha certa>". Deterministico: a mesma bateria e o mesmo binario dao bytes iguais.
Versao: 0.1.0
Changelog: 0.1.0 - P-IA1-b09: primeira versao.
Verificacao: python -I gerar_exemplos_erro.py --selftest
"""

import argparse
import json
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

LANG = Path(r"C:\Users\Cougar\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language")
PASTAS = [LANG / "tests" / "sintaxe", LANG / "tests" / "semantica"]
SAIDA_PADRAO = LANG / "doc" / "exemplos.txt"
PADRAO_NOME = re.compile(r"^err_(E\d{4})_.*\.fj$")
JANELA = 9
ANTES = 4
TEMPO = 60
WORKERS = 12
CABECALHO = ("EXEMPLOS DE ERRO - par errado/certo de cada codigo, tirado da bateria (tests\\sintaxe e tests\\semantica) por tests\\ia_first\\gerar_exemplos_erro.py; "
             "nao edite a mao.\nFormato: @@ codigo fixture linha primeira_linha | = forma_certa | < linha errada | > linha certa (so com conserto verificado).\n")


def Chamar(Forja, Argumentos, Pasta, Entrada=None):
    """Roda o forja na pasta da fixture (os imports sao relativos a ela); devolve (exit, stdout)."""
    Teclado = {"input": Entrada} if Entrada is not None else {"stdin": subprocess.DEVNULL}
    Res = subprocess.run([str(Forja), *Argumentos], cwd=Pasta, capture_output=True, timeout=TEMPO, **Teclado)
    return Res.returncode, Res.stdout.decode("utf-8", errors="replace")


def PrimeiroDiagnostico(Saida):
    """O 1o objeto JSON da saida do check, ou None."""
    for Linha in Saida.splitlines():
        if Linha.startswith("{"):
            return json.loads(Linha)
    return None


def AplicarCorrecao(Fonte, Edicoes):
    """Fonte (bytes) com as edicoes {linha, coluna, coluna_fim, antigo, novo} aplicadas; None se alguma nao bate com o texto. Colunas em bytes, a partir de 1."""
    Linhas = Fonte.split(b"\n")
    for Edicao in sorted(Edicoes, key=lambda E: (E["linha"], E["coluna"]), reverse=True):
        Alvo = Linhas[Edicao["linha"] - 1]
        Ini, Fim = Edicao["coluna"] - 1, Edicao["coluna_fim"] - 1
        if Alvo[Ini:Fim] != Edicao["antigo"].encode("utf-8"):
            return None
        Linhas[Edicao["linha"] - 1] = Alvo[:Ini] + Edicao["novo"].encode("utf-8") + Alvo[Fim:]
    return b"\n".join(Linhas)


def ConsertoVerificado(Forja, Arquivo, Diag, Fonte):
    """O fonte consertado (bytes) se o 1o diagnostico traz correcao certa/provavel e o check do consertado sai 0 mudo; senao None."""
    if Diag.get("confianca") not in ("certa", "provavel") or not Diag.get("correcao"):
        return None
    Novo = AplicarCorrecao(Fonte, Diag["correcao"])
    if Novo is None:
        return None
    Saida, Texto = Chamar(Forja, ["check", "--stdin", Arquivo.name], Arquivo.parent, Novo)
    return Novo if Saida == 0 and not Texto.strip() else None


def Candidato(Forja, Arquivo):
    """Dados da fixture se o 1o diagnostico dela e o codigo do nome; senao None."""
    Codigo = PADRAO_NOME.match(Arquivo.name).group(1)
    Saida, Texto = Chamar(Forja, ["check", Arquivo.name], Arquivo.parent)
    Diag = PrimeiroDiagnostico(Texto)
    if Saida != 1 or Diag is None or Diag.get("codigo") != Codigo or Diag.get("arquivo") != Arquivo.name:
        return None
    Fonte = Arquivo.read_bytes()
    Conserto = ConsertoVerificado(Forja, Arquivo, Diag, Fonte)
    return {"codigo": Codigo, "arquivo": Arquivo, "diag": Diag, "fonte": Fonte, "conserto": Conserto}


def Janela(Linhas, LinhaErro):
    """(primeira, ultima) linhas (1-based, inclusive) do trecho: sem os comentarios do topo, ate JANELA linhas em volta do erro."""
    Primeira = 1
    while Primeira < len(Linhas) and Linhas[Primeira - 1].startswith(b"//"):
        Primeira += 1
    Ultima = len(Linhas)
    while Ultima > Primeira and not Linhas[Ultima - 1].strip():
        Ultima -= 1
    if Ultima - Primeira + 1 > JANELA:
        Primeira = max(Primeira, min(LinhaErro - ANTES, Ultima - JANELA + 1))
        Ultima = Primeira + JANELA - 1
    return Primeira, Ultima


def LinhasDe(Fonte):
    """Linhas do fonte sem o fim de linha (LF ou CRLF)."""
    Linhas = Fonte.split(b"\n")
    if Linhas and Linhas[-1] == b"":
        Linhas.pop()
    return [L[:-1] if L.endswith(b"\r") else L for L in Linhas]


def Entrada(Cand):
    """Linhas do arquivo de saida para um codigo."""
    Linhas = LinhasDe(Cand["fonte"])
    Primeira, Ultima = Janela(Linhas, Cand["diag"]["linha"])
    Forma = " ".join(Cand["diag"]["forma_certa"].split())
    Caminho = Cand["arquivo"].relative_to(LANG).as_posix()
    Saida = [f"@@ {Cand['codigo']} {Caminho} {Cand['diag']['linha']} {Primeira}", f"= {Forma}"]
    Saida += ["< " + L.decode("utf-8") for L in Linhas[Primeira - 1:Ultima]]
    if Cand["conserto"] is not None:
        Certas = LinhasDe(Cand["conserto"])
        Saida += ["> " + L.decode("utf-8") for L in Certas[Primeira - 1:Ultima]]
    return Saida


def Chave(Cand):
    """Ordem de preferencia: com conserto verificado, menor fonte, nome."""
    return (Cand["conserto"] is None, len(Cand["fonte"]), Cand["arquivo"].name)


def Melhores(Candidatos):
    """Um candidato por codigo (o de menor Chave), na ordem dos codigos."""
    Por = {}
    for Cand in Candidatos:
        if Cand is not None and (Cand["codigo"] not in Por or Chave(Cand) < Chave(Por[Cand["codigo"]])):
            Por[Cand["codigo"]] = Cand
    return [Por[Codigo] for Codigo in sorted(Por)]


def Gerar(Forja):
    """O texto do exemplos.txt (str, fim de linha LF; Gravar poe o CRLF)."""
    Arquivos = sorted(A for Pasta in PASTAS for A in Pasta.glob("err_*.fj") if PADRAO_NOME.match(A.name))
    with ThreadPoolExecutor(max_workers=WORKERS) as Pool:
        Candidatos = list(Pool.map(lambda A: Candidato(Forja, A), Arquivos))
    Linhas = [Linha for Cand in Melhores(Candidatos) for Linha in Entrada(Cand)]
    return CABECALHO + "\n".join(Linhas) + "\n"


def Gravar(Texto, Saida):
    Saida.write_bytes(Texto.replace("\n", "\r\n").encode("utf-8"))


def Conferir(Texto, Saida):
    """Exit 0 se o arquivo gravado e igual ao gerado agora."""
    Gravado = Saida.read_bytes() if Saida.is_file() else b""
    Igual = Gravado == Texto.replace("\n", "\r\n").encode("utf-8")
    print(json.dumps({"id": "exemplos", "valor": "igual" if Igual else "diferente", "meta": "igual", "verde": Igual}))
    return 0 if Igual else 1


def Selftest():
    """Aplica uma correcao conhecida, recusa uma que nao bate e recorta a janela em volta do erro."""
    Edicao = {"linha": 2, "coluna": 5, "coluna_fim": 8, "antigo": "var", "novo": "let"}
    Falhas = []
    if AplicarCorrecao(b"a\n    var n = 1\n", [Edicao]) != b"a\n    let n = 1\n":
        Falhas.append("correcao certa")
    if AplicarCorrecao(b"a\n    let n = 1\n", [Edicao]) is not None:
        Falhas.append("correcao que nao bate")
    Linhas = [b"// c"] + [b"l%d" % K for K in range(2, 31)]
    if Janela(Linhas, 20) != (16, 24) or Janela(Linhas, 2) != (2, 10) or Janela(Linhas[:5], 3) != (2, 5):
        Falhas.append("janela")
    print(json.dumps({"id": "selftest", "valor": f"{3 - len(Falhas)}/3", "meta": "3/3", "verde": not Falhas, "falhas": Falhas}))
    return 1 if Falhas else 0


def Principal():
    Args = argparse.ArgumentParser(description="Gera language\\doc\\exemplos.txt a partir da bateria de erros")
    Args.add_argument("--forja", type=Path)
    Args.add_argument("--saida", type=Path, default=SAIDA_PADRAO)
    Args.add_argument("--conferir", action="store_true")
    Args.add_argument("--selftest", action="store_true")
    Opcoes = Args.parse_args()
    if Opcoes.selftest:
        return Selftest()
    if Opcoes.forja is None:
        raise SystemExit("falta --forja <forja.exe>")
    Texto = Gerar(Opcoes.forja)
    if Opcoes.conferir:
        return Conferir(Texto, Opcoes.saida)
    Gravar(Texto, Opcoes.saida)
    print(json.dumps({"id": "exemplos", "gravado": str(Opcoes.saida), "bytes": Opcoes.saida.stat().st_size}))
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
