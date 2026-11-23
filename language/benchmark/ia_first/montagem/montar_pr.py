# -*- coding: utf-8 -*-
"""montar_pr.py - monta os conjuntos P (projeto multi-modulo + 20 perguntas) e R (10 refatoracoes) do benchmark IA FIRST (P-IA1-a17, so Forja).

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao. P: copia (montagem\\dados\\P_fontes.txt) de engine\\forja, testes_mundo, language\\vendor e exemplos_libs, com a
        unica reescrita declarada em cada linha, mais prog.fj (cola); as respostas das 20 perguntas saem por GREP (montar_pr_perguntas.py).
        R: 10 pastas iniciais a partir de uma base pequena (motor\\ + R_prog.fj), cada uma com pedido.txt; solucao de referencia = a inicial
        mais as operacoes de R_tarefas.txt. Tudo deterministico (duas montagens = bytes iguais).

Onde cada coisa fica (o agente da celula so recebe tarefas\\):
  tarefas\\P\\P01\\            projeto + pedido.txt + perguntas.txt          referencias\\forja\\P\\P01\\gabarito.json
  tarefas\\R\\R01..R10\\       pasta inicial + pedido.txt                    referencias\\forja\\R\\Rnn\\{esperado.json, checks.json, solucao\\}
  tarefas\\P\\catalogo.json, tarefas\\R\\catalogo.json (nivel do conjunto; o celula_ia_first nao os copia)

Uso: python -I montar_pr.py --forja <forja.exe> [--conjunto P,R]      (ou  montar_conjuntos.py --forja <exe> --conjunto P,R)
Reversao: apagar tarefas\\P, tarefas\\R, referencias\\forja\\P e referencias\\forja\\R e montar de novo.
Verificacao: python -I ..\\verificar_conjunto_pr.py --forja <forja.exe> --selftest
Depende de: Python 3.9+, forja.exe congelado, montar_conjuntos.py e conjunto_pr.py (pasta acima).
"""
import argparse
import json
import shutil
import sys
import tempfile
from pathlib import Path

MONTAGEM = Path(__file__).resolve().parent
RAIZ = MONTAGEM.parent
ENGINE = RAIZ.parents[2]
sys.path.insert(0, str(RAIZ))
sys.path.insert(0, str(MONTAGEM))
import conjunto_pr as CP  # noqa: E402  (precisam do sys.path acima)
import montar_conjuntos as MC  # noqa: E402
import montar_pr_perguntas as MP  # noqa: E402

ARQUIVOS_DA_BASE_R = ("motor/matriz.fj", "motor/camera.fj", "motor/mundo.fj", "motor/mundo_tick.fj")
FORMATOS = {"linha": "arquivo:linha (caminho relativo a este diretorio, com /), por exemplo motor/exemplo.fj:10",
            "conjunto": "nomes separados por virgula, em qualquer ordem (arquivos: caminho relativo a este diretorio, com /)",
            "lista": "nomes separados por virgula, na ordem pedida", "texto": "o texto exato, em uma linha",
            "inteiro": "um numero inteiro"}
PEDIDO_P = """TAREFA P01 - Perguntas sobre um projeto Forja

Este diretorio e um projeto Forja de varios modulos: raiz prog.fj, modulos em motor\\, demos\\ e vendor\\, e arquivos de teste teste_*.fj na raiz.
Responda as 20 perguntas de perguntas.txt sobre o projeto e escreva as respostas em resposta.txt, uma por linha, no formato `Q01: <resposta>` (de Q01 ate Q20), no formato pedido em cada pergunta.
Nao edite os fontes do projeto. Caminhos sao relativos a este diretorio, escritos com `/`.
"""
PEDIDO_R = """TAREFA {id} - {titulo}

Este diretorio e um projeto Forja de varios modulos (raiz prog.fj; modulos em motor\\). Faca a refatoracao abaixo editando os arquivos deste diretorio.

{descricao}

Ao terminar, `check` e `run` de prog.fj tem de passar (exit 0) e o programa tem de imprimir exatamente o mesmo que imprime agora.
"""


def LerLinhasJson(Corpo):
    return [json.loads(L) for L in Corpo.splitlines() if L.strip()]


def Escrever(Caminho, Texto):
    Caminho = Path(Caminho)
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    Caminho.write_text(Texto, encoding="utf-8", newline="\n")


def EscreverJson(Caminho, Dados):
    Escrever(Caminho, json.dumps(Dados, ensure_ascii=False, indent=2) + "\n")


def LinhasDeFontes():
    """[(destino, origem, reescrita)] de dados\\P_fontes.txt."""
    Linhas = (MONTAGEM / "dados" / "P_fontes.txt").read_text(encoding="utf-8").splitlines()
    return [tuple(Parte.strip() for Parte in L.split("|")) for L in Linhas if L.strip() and not L.startswith("#")]


def BytesReescritos(Origem, Reescrita):
    Dados = (ENGINE / Origem).read_bytes()
    if Reescrita == "engine":
        return Dados.replace(b'import "engine:', b'import "motor/')
    if Reescrita.startswith("demo:"):
        return Dados.replace(b"proc main(", b"pub proc " + Reescrita[5:].encode("ascii") + b"(", 1)
    return Dados


def CopiarFontes(Destino, Filtro=None):
    for Alvo, Origem, Reescrita in LinhasDeFontes():
        if Filtro is None or Alvo in Filtro:
            (Destino / Alvo).parent.mkdir(parents=True, exist_ok=True)
            (Destino / Alvo).write_bytes(BytesReescritos(Origem, Reescrita))


def MontarProjetoP(Destino):
    CopiarFontes(Destino)
    shutil.copyfile(MONTAGEM / "dados" / "P_prog.fj", Destino / "prog.fj")


def TextoDasPerguntas(Itens):
    Blocos = ["%s - %s\n    Formato da resposta: %s\n" % (I["id"], I["pergunta"], FORMATOS[I["formato"]]) for I in Itens]
    return "PERGUNTAS SOBRE O PROJETO (responda em resposta.txt, `Qnn: <resposta>`)\n\n" + "\n".join(Blocos)


def ItemDeP(Id, Tipo, Corpo, Pasta):
    """Uma pergunta do P_perguntas.txt com a resposta derivada por grep no projeto montado em Pasta."""
    Sec = MC.LerSecoes(Corpo)
    Item = {"id": Id, "tipo": Tipo, "pergunta": Sec["pergunta"][0], "formato": Sec["formato"][0], "param": json.loads(Sec["param"][0])}
    Item["resposta"], Declaracao = MP.Derivar(Item, Pasta)
    if Declaracao:
        Item["declaracao"] = Declaracao
    return Item


def ItensDeP():
    with tempfile.TemporaryDirectory(prefix="mp_proj_") as Dir:
        MontarProjetoP(Path(Dir))
        return [ItemDeP(Id, Tipo, Corpo, Dir) for Id, Tipo, Corpo in MC.LerBlocos("P_perguntas.txt")]


def MontarP():
    Pasta = RAIZ / "tarefas" / "P" / "P01"
    Itens = ItensDeP()
    MontarProjetoP(Pasta)
    Escrever(Pasta / "pedido.txt", PEDIDO_P)
    Escrever(Pasta / "perguntas.txt", TextoDasPerguntas(Itens))
    EscreverJson(RAIZ / "referencias" / "forja" / "P" / "P01" / "gabarito.json", {"versao": "0.1.0", "perguntas": Itens})
    EscreverJson(RAIZ / "tarefas" / "P" / "catalogo.json", {"conjunto": "P", "linguas": ["forja"], "tarefas": [
        {"id": "P01", "titulo": "20 perguntas sobre um projeto Forja multi-modulo", "perguntas": len(Itens)}]})


# ---------------------------------------------------------------- conjunto R

def MontarBaseR(Destino):
    CopiarFontes(Destino, Filtro=set(ARQUIVOS_DA_BASE_R))
    shutil.copyfile(MONTAGEM / "dados" / "R_prog.fj", Destino / "prog.fj")


def AplicarSecao(Pasta, Secoes, Nome):
    """Aplica as operacoes (JSON, uma por linha) da secao @<Nome>; secao ausente = nada a fazer."""
    CP.AplicarOps(Pasta, LerLinhasJson(Secoes.get(Nome, ("", ""))[1]))


def MontarTarefaR(Id, Titulo, Secoes, Forja):
    Tarefa = RAIZ / "tarefas" / "R" / Id
    Ref = RAIZ / "referencias" / "forja" / "R" / Id
    MontarBaseR(Tarefa)
    AplicarSecao(Tarefa, Secoes, "inicial")
    shutil.copytree(Tarefa, Ref / "solucao")
    AplicarSecao(Ref / "solucao", Secoes, "solucao")
    Run, Falha = CP.CompilarERodar(Forja, Tarefa)
    if Run is None:
        raise ValueError("%s: a pasta inicial nao roda: %s" % (Id, Falha))
    EscreverJson(Ref / "esperado.json", {"stdout": Run["stdout"], "exit": Run["rc"]})
    EscreverJson(Ref / "checks.json", {"checks": LerLinhasJson(Secoes["checks"][1])})
    Escrever(Tarefa / "pedido.txt", PEDIDO_R.format(id=Id, titulo=Titulo, descricao=Secoes["pedido"][1].strip()))
    return {"id": Id, "titulo": Titulo, "classe": Secoes["classe"][0]}


def MontarR(Forja):
    Itens = []
    for Id, Titulo, Corpo in MC.LerBlocos("R_tarefas.txt"):
        Itens.append(MontarTarefaR(Id, Titulo, MC.LerSecoes(Corpo), Forja))
    EscreverJson(RAIZ / "tarefas" / "R" / "catalogo.json", {"conjunto": "R", "linguas": ["forja"], "tarefas": Itens})


def LimparConjunto(Conj):
    shutil.rmtree(RAIZ / "tarefas" / Conj, ignore_errors=True)
    shutil.rmtree(RAIZ / "referencias" / "forja" / Conj, ignore_errors=True)


def Montar(Forja, Conjuntos):
    """Monta P e/ou R (apaga antes so as proprias pastas). Chamado tambem por montar_conjuntos.py."""
    for Conj in Conjuntos:
        LimparConjunto(Conj)
        MontarP() if Conj == "P" else MontarR(Path(Forja))
        print("montado", Conj)


def Principal():
    Parser = argparse.ArgumentParser(description="Monta os conjuntos P e R do benchmark IA FIRST")
    Parser.add_argument("--forja", required=True, help="forja.exe da copia congelada")
    Parser.add_argument("--conjunto", default="P,R")
    Args = Parser.parse_args()
    Montar(Path(Args.forja).resolve(), [C for C in Args.conjunto.split(",") if C in ("P", "R")])
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
