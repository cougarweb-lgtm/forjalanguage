# -*- coding: utf-8 -*-
"""verificar_conjunto_pr.py - prova por execucao dos conjuntos P (projeto + 20 perguntas) e R (10 refatoracoes) do IA FIRST (P-IA1-a17).

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao. P: o projeto passa em check e run, cada teste_*.fj passa em `forja test`, 3 a 6 mil linhas, 20 perguntas em 5
        tipos (4 de cada), cada gabarito igual ao refeito por GREP e ao refeito PELO COMPILADOR (consultar/check); o corretor nota 20/20 e 19/20.
        R: a pasta inicial compila e roda, NAO passa nas checagens; a solucao de referencia (fora da celula) passa e imprime o mesmo.
        Montagem deterministica (2 montagens = bytes iguais) e gabaritos fora das pastas que a celula copia. --selftest planta defeitos.

Uso:
  python -I verificar_conjunto_pr.py --forja <forja.exe> [--raiz <pasta com tarefas\\ e referencias\\>] [--sem-montagem]
  python -I verificar_conjunto_pr.py --forja <forja.exe> --selftest
Saida: uma linha JSON por verificacao {conjunto,id,ok,motivos,...} e uma linha final {"resumo":...}. Exit: 0 tudo verde, 1 algo vermelho, 2 uso, 3 interno.
Leitura: tarefas\\P, tarefas\\R, referencias\\forja\\P e \\R. Escrita: so diretorios temporarios. Reversao: apagar este arquivo.
Depende de: Python 3.9+, forja.exe congelado, conjunto_pr.py, verificar_pr_compilador.py e montagem\\montar_pr*.py.
"""
import argparse
import hashlib
import json
import shutil
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

RAIZ = Path(__file__).resolve().parent
sys.path.insert(0, str(RAIZ))
sys.path.insert(0, str(RAIZ / "montagem"))
import conjunto_pr as CP  # noqa: E402  (precisam do sys.path acima)
import montar_pr as MPR  # noqa: E402
import montar_pr_perguntas as MP  # noqa: E402
import verificar_pr_compilador as VC  # noqa: E402

LINHAS_MIN, LINHAS_MAX = 3000, 6000
CLASSES = {"onde": "onde esta", "chamadores_arquivos": "quem chama", "chamadores_procs": "quem chama", "retorno": "que tipo",
           "campos": "que tipo", "membros": "que tipo", "tipo_campo": "que tipo", "quebra": "o que quebra", "teste_nome": "qual teste cobre",
           "teste_contagem": "qual teste cobre", "teste_arquivos": "qual teste cobre", "teste_total": "qual teste cobre"}
NOMES_DE_GABARITO = {"esperado.json", "gabarito.json", "checks.json", "catalogo.json"}


def Resultado(Conj, Id, Motivos, **Extra):
    return dict({"conjunto": Conj, "id": Id, "ok": not Motivos, "motivos": Motivos}, **Extra)


def ContarLinhas(Pasta):
    """{grupo: linhas} dos .fj do projeto; grupo = 1a pasta (motor, demos, vendor) ou 'raiz'."""
    Contagem = {}
    for Rel in CP.ArquivosFj(Pasta):
        Grupo = Rel.split("/")[0] if "/" in Rel else "raiz"
        Contagem[Grupo] = Contagem.get(Grupo, 0) + (Pasta / Rel).read_bytes().count(b"\n")
    return Contagem


def MotivosProjetoP(Forja, Pasta):
    """prog.fj passa em check e roda; cada teste_*.fj passa em forja test; o total fica entre 3.000 e 6.000 linhas."""
    Run, Falha = CP.CompilarERodar(Forja, Pasta)
    if Run is None:
        return [Falha], {}
    Motivos = [] if Run["rc"] == 0 and Run["stdout"].strip() else ["prog.fj rodou com rc=%s e stdout=%r" % (Run["rc"], Run["stdout"][:80])]
    for Teste in sorted(Pasta.glob("teste_*.fj")):
        R = CP.RodarForja(Forja, ["test", Teste.name], Pasta)
        if R["rc"] != 0 or " 0 falharam" not in R["stdout"]:
            Motivos.append("%s: forja test rc=%s: %s" % (Teste.name, R["rc"], R["stdout"][-120:]))
    Contagem = ContarLinhas(Pasta)
    if not LINHAS_MIN <= sum(Contagem.values()) <= LINHAS_MAX:
        Motivos.append("projeto tem %d linhas, esperado %d a %d" % (sum(Contagem.values()), LINHAS_MIN, LINHAS_MAX))
    return Motivos, Contagem


def MotivosPergunta(Forja, Pasta, Item):
    """Gabarito == resposta refeita por grep == resposta refeita pelo compilador (consultar/check)."""
    Motivos = []
    try:
        Grep, _ = MP.Derivar(Item, Pasta)
        Compilador = VC.ResponderPeloCompilador(Item, Pasta, Forja)
    except (ValueError, KeyError, StopIteration, OSError) as Erro:
        return ["%s: %s" % (type(Erro).__name__, Erro)]
    if CP.NormalizarResposta(Item["formato"], Grep) != CP.NormalizarResposta(Item["formato"], Item["resposta"]):
        Motivos.append("gabarito %r difere do grep %r" % (Item["resposta"], Grep))
    if CP.NormalizarResposta(Item["formato"], Compilador) != CP.NormalizarResposta(Item["formato"], Item["resposta"]):
        Motivos.append("gabarito %r difere do compilador %r" % (Item["resposta"], Compilador))
    return Motivos


def MotivosDistribuicao(Itens):
    Contagem = {}
    for Item in Itens:
        Contagem[CLASSES[Item["tipo"]]] = Contagem.get(CLASSES[Item["tipo"]], 0) + 1
    Motivos = [] if len(Itens) == 20 else ["%d perguntas, esperado 20" % len(Itens)]
    if sorted(Contagem.values()) != [4, 4, 4, 4, 4]:
        Motivos.append("distribuicao por tipo %s, esperado 4 de cada um dos 5 tipos" % Contagem)
    return Motivos


def MotivosCorretor(Gabarito):
    """O corretor da celula nota 20/20 com as respostas do gabarito (escritas de forma frouxa) e 19/20 com uma trocada."""
    Linhas = ["%s: %s" % (I["id"], I["resposta"].replace(",", ", ")) for I in Gabarito["perguntas"]]
    Cheio = CP.CorrigirP(Gabarito, "\n".join(Linhas))
    Trocado = CP.CorrigirP(Gabarito, "\n".join(["Q01: errado"] + Linhas[1:]))
    Vazio = CP.CorrigirP(Gabarito, "")
    Motivos = [] if Cheio["acertos"] == 20 else ["corretor nao deu 20/20 ao gabarito: %s" % Cheio]
    Motivos += [] if Trocado["acertos"] == 19 else ["corretor nao deu 19/20 com 1 trocada: %s" % Trocado]
    return Motivos + ([] if Vazio["acertos"] == 0 else ["resposta vazia acertou %d" % Vazio["acertos"]])


def VerificarP(Forja, Raiz):
    Pasta = Raiz / "tarefas" / "P" / "P01"
    Gabarito = CP.LerJson(Raiz / "referencias" / "forja" / "P" / "P01" / "gabarito.json")
    Motivos, Contagem = MotivosProjetoP(Forja, Pasta)
    Resultados = [Resultado("P", "projeto", Motivos, linhas=sum(Contagem.values()), por_modulo=Contagem)]
    Resultados.append(Resultado("P", "perguntas", MotivosDistribuicao(Gabarito["perguntas"]) + MotivosCorretor(Gabarito)))
    for Arq in ("pedido.txt", "perguntas.txt"):
        Resultados.append(Resultado("P", Arq, [] if (Pasta / Arq).is_file() else [Arq + " nao existe"]))
    with ThreadPoolExecutor(max_workers=4) as Pool:
        Motivos = list(Pool.map(lambda I: MotivosPergunta(Forja, Pasta, I), Gabarito["perguntas"]))
    return Resultados + [Resultado("P", I["id"], M, tipo=I["tipo"]) for I, M in zip(Gabarito["perguntas"], Motivos)]


def MotivosTarefaR(Forja, Raiz, Id):
    """Inicial: compila, roda e imprime o esperado, mas reprova nas checagens. Solucao: passa em tudo."""
    Ref = Raiz / "referencias" / "forja" / "R" / Id
    Esperado, Checks = CP.LerJson(Ref / "esperado.json"), CP.LerJson(Ref / "checks.json")["checks"]
    Motivos = []
    Inicial = CP.MotivosR(Forja, Raiz / "tarefas" / "R" / Id, Esperado, Checks)
    if not Inicial:
        Motivos.append("a pasta inicial passa no verificador sem mudar nada")
    Run, Falha = CP.CompilarERodar(Forja, Raiz / "tarefas" / "R" / Id)
    if Run is None or Run["stdout"] != Esperado["stdout"]:
        Motivos.append("a pasta inicial nao imprime o esperado: %s" % (Falha or "stdout difere"))
    Solucao = CP.MotivosR(Forja, Ref / "solucao", Esperado, Checks)
    Motivos += ["solucao de referencia reprovada: " + M for M in Solucao]
    if not (Raiz / "tarefas" / "R" / Id / "pedido.txt").is_file():
        Motivos.append("pedido.txt nao existe")
    return Motivos


def VerificarR(Forja, Raiz):
    Ids = [T["id"] for T in CP.LerJson(Raiz / "tarefas" / "R" / "catalogo.json")["tarefas"]]
    with ThreadPoolExecutor(max_workers=4) as Pool:
        Motivos = list(Pool.map(lambda Id: MotivosTarefaR(Forja, Raiz, Id), Ids))
    Total = [Resultado("R", "catalogo", [] if len(Ids) == 10 else ["%d tarefas, esperado 10" % len(Ids)])]
    return Total + [Resultado("R", Id, M) for Id, M in zip(Ids, Motivos)]


def MotivosVisibilidade(Raiz):
    """Nada de gabarito, checagem, catalogo ou solucao dentro de uma pasta de tarefa (a celula copia a pasta inteira)."""
    Caminhos = [(Conj, A.relative_to(Raiz / "tarefas" / Conj).parts) for Conj in ("P", "R") for A in (Raiz / "tarefas" / Conj).rglob("*")]
    Achados = ["%s/%s" % (Conj, "/".join(Dentro)) for Conj, Dentro in Caminhos
               if len(Dentro) > 1 and (Dentro[-1] in NOMES_DE_GABARITO or "solucao" in Dentro)]
    return ["gabarito visivel para o agente: " + A for A in Achados[:5]]


def MotivosIntegracao():
    """celula_ia_first enxerga P (1 tarefa) e R (10 tarefas) so em Forja, com a pasta de origem certa."""
    import celula_ia_first as CEL
    Motivos = []
    for Conj, Ids in (("P", ["P01"]), ("R", ["R%02d" % N for N in range(1, 11)])):
        if CEL.IdsDoConjunto(Conj, "forja") != Ids:
            Motivos.append("IdsDoConjunto(%s) = %s" % (Conj, CEL.IdsDoConjunto(Conj, "forja")))
        if CEL.IdsDoConjunto(Conj, "c"):
            Motivos.append("%s apareceu para a linguagem c" % Conj)
    return Motivos


def HashDaArvore(Raiz):
    Resumo = hashlib.sha256()
    Pastas = [Raiz / Base / Conj for Base in ("tarefas", "referencias/forja") for Conj in ("P", "R")]
    for Arq in (A for Pasta in Pastas for A in sorted(Pasta.rglob("*")) if A.is_file()):
        Resumo.update(Arq.relative_to(Raiz).as_posix().encode() + Arq.read_bytes())
    return Resumo.hexdigest()


def MontarEmRaiz(Forja, Raiz):
    """Monta P e R numa raiz temporaria (redireciona o RAIZ do montar_pr) e devolve o hash da arvore."""
    Antes = MPR.RAIZ
    MPR.RAIZ = Raiz
    try:
        MPR.Montar(Forja, ["P", "R"])
    finally:
        MPR.RAIZ = Antes
    return HashDaArvore(Raiz)


def MotivosDeterminismo(Forja):
    with tempfile.TemporaryDirectory(prefix="pr_det_") as Dir:
        Hashes = [MontarEmRaiz(Forja, Path(Dir) / ("m%d" % N)) for N in (1, 2)]
    return [] if Hashes[0] == Hashes[1] else ["2 montagens deram hashes diferentes: %s %s" % tuple(H[:12] for H in Hashes)]


def VerificarTudo(Forja, Raiz, ComMontagem):
    Resultados = VerificarP(Forja, Raiz) + VerificarR(Forja, Raiz)
    Resultados.append(Resultado("PR", "visibilidade", MotivosVisibilidade(Raiz)))
    if Raiz == RAIZ:
        Resultados.append(Resultado("PR", "integracao_celula", MotivosIntegracao()))
    if ComMontagem:
        Resultados.append(Resultado("PR", "montagem_deterministica", MotivosDeterminismo(Forja)))
    return Resultados


def EmitirResultados(Todos):
    for Item in Todos:
        print(json.dumps(Item, ensure_ascii=False))
    Falhas = [T for T in Todos if not T["ok"]]
    print(json.dumps({"resumo": {"verificacoes": len(Todos), "verdes": len(Todos) - len(Falhas), "vermelhas": len(Falhas)}}))
    return 0 if Todos and not Falhas else 1


# ---------------------------------------------------------------- selftest

def PlantarDefeitos(Raiz):
    """Gabarito trocado (Q01), solucao quebrada (R03), checagem frouxa (R05) e gabarito vazado numa pasta de tarefa (R06)."""
    Gab = Raiz / "referencias" / "forja" / "P" / "P01" / "gabarito.json"
    Dados = CP.LerJson(Gab)
    Dados["perguntas"][0]["resposta"] = "motor/mundo.fj:1"
    Gab.write_text(json.dumps(Dados, ensure_ascii=False), encoding="utf-8")
    Prog = Raiz / "referencias" / "forja" / "R" / "R03" / "solucao" / "prog.fj"
    Prog.write_text(Prog.read_text(encoding="utf-8") + "\nproc quebrado( {\n", encoding="utf-8")
    Checks = Raiz / "referencias" / "forja" / "R" / "R05" / "checks.json"
    Checks.write_text(json.dumps({"checks": []}), encoding="utf-8")
    shutil.copyfile(Gab, Raiz / "tarefas" / "R" / "R06" / "gabarito.json")


def CopiarParaSelftest(Raiz, Destino):
    for Base in ("tarefas", "referencias/forja"):
        for Conj in ("P", "R"):
            shutil.copytree(Raiz / Base / Conj, Destino / Base / Conj)


def Selftest(Forja, Raiz):
    """Controle positivo (copia fiel verde) e negativo (4 defeitos plantados, cada um acusado onde deve)."""
    with tempfile.TemporaryDirectory(prefix="pr_self_") as Dir:
        Copia = Path(Dir) / "raiz"
        CopiarParaSelftest(Raiz, Copia)
        Limpo = VerificarTudo(Forja, Copia, False)
        PlantarDefeitos(Copia)
        Sujo = VerificarTudo(Forja, Copia, False)
    Acusados = {(T["conjunto"], T["id"]) for T in Sujo if not T["ok"]}
    Esperados = {("P", "Q01"), ("R", "R03"), ("R", "R05"), ("PR", "visibilidade")}
    Faltou = sorted(Esperados - Acusados)
    VerdeLimpo = all(T["ok"] for T in Limpo) and bool(Limpo)
    print(json.dumps({"selftest": {"controle_positivo_verde": VerdeLimpo, "acusados": sorted(map(list, Acusados)), "nao_acusados": Faltou}}))
    return 0 if VerdeLimpo and not Faltou and ("P", "Q02") not in Acusados else 1


def AcharForja(Arg):
    import os
    Caminho = Arg or os.environ.get("IA_FIRST_FORJA")
    if not Caminho or not Path(Caminho).is_file():
        print("uso: informe --forja <forja.exe> ou IA_FIRST_FORJA", file=sys.stderr)
        sys.exit(2)
    return Path(Caminho).resolve()


def Principal():
    Parser = argparse.ArgumentParser(description="Prova por execucao dos conjuntos P e R do IA FIRST")
    Parser.add_argument("--forja")
    Parser.add_argument("--raiz", default=str(RAIZ))
    Parser.add_argument("--sem-montagem", action="store_true", help="nao refaz as 2 montagens do teste de determinismo")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    Forja, Raiz = AcharForja(Args.forja), Path(Args.raiz).resolve()
    if Args.selftest:
        return Selftest(Forja, Raiz)
    return EmitirResultados(VerificarTudo(Forja, Raiz, not Args.sem_montagem))


if __name__ == "__main__":
    try:
        sys.exit(Principal())
    except Exception as Erro:  # sem traceback do verificador: erro interno vira exit 3
        print("erro interno: %s %s" % (type(Erro).__name__, Erro), file=sys.stderr)
        sys.exit(3)
