# verificar_sugestoes.py -- confere o pacote do erro (contrato IA1, secao 1.1) nos err_* do lote 1 de sugestoes (P-IA1-b04)
"""Roda `forja check` nos err_*.fj do lote (E0001 do ';', E0707, E0708 e E0709) e confere, sem modelo:
  intacto   o 1o diagnostico tem o mesmo codigo, arquivo, linha e coluna do .esperado.jsonl
  M-D11     campos do pacote: trecho (com a linha do erro e o marcador ^~~~ na coluna certa), causa, correcao, confianca, sugestao,
            relacionados, spec ou spec_ref, id; "antigo" de cada edicao e o texto que esta de fato na faixa (coluna em BYTES)
  M-D1      100% das ocorrencias trazem "sugestao"
  M-D13     aplicar so as correcoes "certa" numa copia: o diagnostico original some (mesmo codigo e linha) e o numero de erros nao sobe
  M-D10     (reportado) quantos casos com correcao certa chegam a exit 0 depois de aplicada
  M-D5      `build` nos 2 backends (c e x64) imprime a mesma saida byte a byte
  M-D6      2 execucoes do mesmo check dao a mesma saida byte a byte
Uso:
    python -I verificar_sugestoes.py --forja <forja.exe> [--lang <pasta language>] [--json]
    python -I verificar_sugestoes.py --selftest
Saida: uma linha por metrica {id, valor, verde}; exit 0 so se todas verdes (1 = alguma vermelha, 2 = uso).
Versao: 0.2.1
Changelog: 0.2.1 - P72: a edicao cujo "arquivo" e outro que o do diagnostico (modulo importado) so confere os campos; o texto da faixa e das linhas e do arquivo raiz.
           0.2.0 - P-IA1-b14: "relacionados" deixa de ter de ser [] (o b14 o preenche): basta ser lista de ate 5; o conteudo e medido em verificar_relacionados_b14.py.
           0.1.0 - P-IA1-b04: primeira versao.
Verificacao: python -I verificar_sugestoes.py --selftest
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

CODIGOS_DO_LOTE = {"E0707", "E0708", "E0709"}
CONFIANCAS = {"certa", "provavel", "so_dica"}
CAMPOS_DO_PACOTE = ("trecho", "causa", "correcao", "confianca", "sugestao", "relacionados", "id")
CAMPOS_DA_EDICAO = ("arquivo", "linha", "coluna", "coluna_fim", "antigo", "novo")
MARCADOR = re.compile(r"^ *\| ([ \t]*)\^~*$")
TEMPO_MAX = 60


def LerDiagnosticos(Saida):
    """Linhas JSON da saida; texto que nao e JSON vira um diagnostico {'invalido': linha} (a metrica acusa)."""
    Lista = []
    for Linha in Saida.splitlines():
        if not Linha.strip():
            continue
        try:
            Lista.append(json.loads(Linha))
        except ValueError:
            Lista.append({"invalido": Linha})
    return Lista


def RodarForja(Forja, Argumentos, Pasta):
    """(exit, stdout) de uma execucao do forja em Pasta (cwd), sem stdin."""
    Proc = subprocess.run([str(Forja), *Argumentos], capture_output=True, cwd=Pasta, stdin=subprocess.DEVNULL, timeout=TEMPO_MAX)
    return Proc.returncode, Proc.stdout.decode("utf-8", errors="replace")


def TextoDaLinha(Linhas, Numero):
    """Linha (1-based) sem o CR final; None se nao existe."""
    if Numero < 1 or Numero > len(Linhas):
        return None
    return Linhas[Numero - 1].rstrip(b"\r")


def ProblemasDoMarcador(Diag, Linhas):
    """O trecho tem a linha do erro e, logo depois, o marcador ^ na coluna do diagnostico (contada em caracteres)."""
    Trecho = Diag.get("trecho")
    if not isinstance(Trecho, list) or not all(isinstance(T, str) for T in Trecho):
        return ["trecho nao e lista de textos"]
    Marcas = [K for K, T in enumerate(Trecho) if MARCADOR.match(T)]
    if len(Marcas) != 1 or Marcas[0] == 0:
        return ["trecho sem exatamente um marcador depois da linha do erro"]
    Texto = TextoDaLinha(Linhas, Diag["linha"])
    Depois = Trecho[Marcas[0] - 1].split(" | ", 1)
    if Texto is None or len(Depois) != 2 or Depois[1].encode("utf-8") != Texto[:200]:
        return ["a linha antes do marcador nao e a linha do erro"]
    Recuo = MARCADOR.match(Trecho[Marcas[0]]).group(1)
    Esperado = len(Texto[: Diag["coluna"] - 1].decode("utf-8", errors="replace"))
    return [] if len(Recuo) == Esperado else ["marcador na coluna %d, esperado %d" % (len(Recuo), Esperado)]


def ProblemasDaEdicao(Edicao, Linhas):
    """Edicao com todos os campos e 'antigo' igual ao texto da faixa [coluna, coluna_fim) da linha (bytes)."""
    Faltam = [C for C in CAMPOS_DA_EDICAO if C not in Edicao]
    if Faltam:
        return ["edicao sem " + ",".join(Faltam)]
    Texto = TextoDaLinha(Linhas, Edicao["linha"])
    if Texto is None:
        return ["edicao numa linha que nao existe"]
    Faixa = Texto[Edicao["coluna"] - 1 : Edicao["coluna_fim"] - 1]
    return [] if Faixa == Edicao["antigo"].encode("utf-8") else ["antigo %r nao e o texto da faixa %r" % (Edicao["antigo"], Faixa)]


def ProblemasDasCorrecoes(Diag, Linhas):
    Problemas = []
    Correcao = Diag.get("correcao")
    if not isinstance(Correcao, list):
        return ["correcao nao e lista"]
    for Edicao in Correcao:
        if Path(str(Edicao.get("arquivo", ""))).name != Path(str(Diag.get("arquivo", ""))).name:
            Problemas += [("edicao sem " + C) for C in CAMPOS_DA_EDICAO if C not in Edicao]  # P72: edicao num modulo importado (pub da declaracao): as linhas do arquivo raiz nao a conferem
            continue
        Problemas += ProblemasDaEdicao(Edicao, Linhas)
    if Diag.get("confianca") == "certa" and not Correcao:
        Problemas.append("confianca certa sem edicao")
    for Alt in Diag.get("alternativas", []):
        if Alt.get("confianca") not in CONFIANCAS or not Alt.get("sugestao") or not isinstance(Alt.get("correcao"), list):
            Problemas.append("alternativa incompleta")
    return Problemas


def ConferirPacote(Diag, Linhas):
    """Lista de problemas do pacote de UM diagnostico (vazia = M-D11 verde)."""
    if "invalido" in Diag:
        return ["linha que nao e JSON"]
    Problemas = ["falta " + C for C in CAMPOS_DO_PACOTE if C not in Diag]
    if "spec" not in Diag and "spec_ref" not in Diag:
        Problemas.append("falta spec ou spec_ref")
    if Problemas:
        return Problemas
    if Diag["confianca"] not in CONFIANCAS:
        Problemas.append("confianca invalida")
    if not Diag["sugestao"] or not isinstance(Diag["relacionados"], list) or len(Diag["relacionados"]) > 5 or not Diag["id"].startswith(Diag["codigo"] + "@"):
        Problemas.append("sugestao vazia, relacionados que nao e lista de ate 5 ou id fora do formato")  # b14: o campo e preenchido (verificar_relacionados_b14.py mede o conteudo)
    return Problemas + ProblemasDoMarcador(Diag, Linhas) + ProblemasDasCorrecoes(Diag, Linhas)


def AplicarCertas(Conteudo, Diags):
    """Aplica as edicoes das correcoes 'certa' ao texto (bytes), de tras para frente. Devolve (novo texto, quantas edicoes)."""
    Linhas = Conteudo.split(b"\n")
    Edicoes = [E for D in Diags if D.get("confianca") == "certa" for E in D["correcao"]]
    for E in sorted(Edicoes, key=lambda X: (X["linha"], X["coluna"]), reverse=True):
        Linha = Linhas[E["linha"] - 1]
        Linhas[E["linha"] - 1] = Linha[: E["coluna"] - 1] + E["novo"].encode("utf-8") + Linha[E["coluna_fim"] - 1 :]
    return b"\n".join(Linhas), len(Edicoes)


def PrimeiroEsperado(Esperado):
    Linhas = [L for L in Esperado.read_text(encoding="utf-8").splitlines() if L.strip()]
    return json.loads(Linhas[0]) if Linhas else None


def EhDoLote(Fj, Esperado):
    """err_* do lote: E0707/E0708/E0709 ou E0001 em cima de um ';'."""
    if Esperado is None:
        return False
    if Esperado["codigo"] in CODIGOS_DO_LOTE:
        return True
    if Esperado["codigo"] not in ("E0001", "E0110"):  # IA1-L1: o ';' passou de E0001 para E0110
        return False
    Linha = TextoDaLinha(Fj.read_bytes().split(b"\n"), Esperado["linha"])
    return Linha is not None and Linha[Esperado["coluna"] - 1 : Esperado["coluna"]] == b";"


def ListarLote(Lang):
    Lote = []
    for Fj in sorted((Lang / "tests").glob("*/err_*.fj")):
        Esp = Fj.with_suffix(".esperado.jsonl")
        if Esp.exists() and EhDoLote(Fj, PrimeiroEsperado(Esp)):
            Lote.append((Fj, PrimeiroEsperado(Esp)))
    return Lote


def ConferirUmCaso(Forja, Fj, Esperado):
    """Resultado de um err_*: dict com os achados de cada metrica (listas de problemas vazias = verde)."""
    Pasta = Fj.parent
    Codigo, Saida = RodarForja(Forja, ["check", Fj.name], Pasta)
    Diags = LerDiagnosticos(Saida)
    Conteudo = Fj.read_bytes()
    Linhas = Conteudo.split(b"\n")
    Primeiro = Diags[0] if Diags else {}
    Achados = {"arquivo": Fj.name, "intacto": [], "pacote": [], "sem_sugestao": 0, "piora": [], "exit0": None, "paridade": [], "determinismo": []}
    Chave = ("codigo", "arquivo", "linha", "coluna")
    if Codigo != 1 or [Primeiro.get(C) for C in Chave] != [Esperado[C] for C in Chave]:
        Achados["intacto"].append("1o diagnostico %r, esperado %r" % ([Primeiro.get(C) for C in Chave], [Esperado[C] for C in Chave]))
    for Diag in Diags:
        Achados["pacote"] += ConferirPacote(Diag, Linhas)
        Achados["sem_sugestao"] += 0 if Diag.get("sugestao") else 1
    Achados["determinismo"] += [] if RodarForja(Forja, ["check", Fj.name], Pasta)[1] == Saida else ["2 execucoes diferem"]
    Achados.update(ConferirAplicacao(Forja, Fj, Conteudo, Diags))
    Achados["paridade"] += ConferirParidade(Forja, Fj)
    return Achados


def ConferirAplicacao(Forja, Fj, Conteudo, Diags):
    """M-D13 e M-D10: aplica as 'certa' numa copia e re-checa. Sem correcao certa: nada a conferir (exit0 None)."""
    Novo, Quantas = AplicarCertas(Conteudo, Diags)
    if Quantas == 0:
        return {"piora": [], "exit0": None}
    with tempfile.TemporaryDirectory(prefix="verif_sugestoes_") as Tmp:
        Copia = Path(Tmp) / Fj.name
        Copia.write_bytes(Novo)
        Codigo, Saida = RodarForja(Forja, ["check", Copia.name], Tmp)
    Depois = LerDiagnosticos(Saida)
    Piora = []
    if len(Depois) > len(Diags):
        Piora.append("erros subiram de %d para %d" % (len(Diags), len(Depois)))
    Original = (Diags[0]["codigo"], Diags[0]["linha"])
    if any((D.get("codigo"), D.get("linha")) == Original for D in Depois):
        Piora.append("o diagnostico original continua depois da correcao")
    return {"piora": Piora, "exit0": Codigo == 0}


def ConferirParidade(Forja, Fj):
    """build nos 2 backends: mesmo stdout byte a byte (o diagnostico da semantica nao depende do backend)."""
    with tempfile.TemporaryDirectory(prefix="verif_sugestoes_") as Tmp:
        Saidas = []
        for Backend in ("c", "x64"):
            Saidas.append(RodarForja(Forja, ["build", Fj.name, "--backend", Backend, "-o", str(Path(Tmp) / "saida.exe")], Fj.parent))
    return [] if Saidas[0] == Saidas[1] else ["build c e x64 diferem"]


def Resumir(Achados):
    """Metricas do lote a partir dos achados de cada caso: lista de (id, valor, verde)."""
    N = len(Achados)
    Ocorrencias = N
    Com = sum(1 for A in Achados if A["sem_sugestao"] == 0)
    Certos = [A for A in Achados if A["exit0"] is not None]
    Verdes = sum(1 for A in Certos if A["exit0"])
    Falhas = lambda Chave: [A["arquivo"] + ": " + P for A in Achados for P in A[Chave]]  # noqa: E731
    return [
        ("intacto", {"casos": N, "problemas": Falhas("intacto")}, not Falhas("intacto")),
        ("M-D1", {"com_sugestao": Com, "casos": Ocorrencias}, Com == Ocorrencias and N > 0),
        ("M-D11", {"problemas": Falhas("pacote")}, not Falhas("pacote") and N > 0),
        ("M-D13", {"piora": Falhas("piora")}, not Falhas("piora")),
        ("M-D10", {"exit0_apos_certas": Verdes, "casos_com_certa": len(Certos)}, True),
        ("M-D5", {"problemas": Falhas("paridade")}, not Falhas("paridade")),
        ("M-D6", {"problemas": Falhas("determinismo")}, not Falhas("determinismo")),
    ]


def Selftest():
    """O pacote bom passa; cada defeito plantado e acusado."""
    Fonte = b"proc main() {\n    var n = 3\n    print(n)\n}\n"
    Bom = {"codigo": "E0707", "linha": 2, "coluna": 9, "trecho": ["1 | proc main() {", "2 |     var n = 3", "  |         ^", "3 |     print(n)"],
           "causa": "x", "correcao": [{"arquivo": "a.fj", "linha": 2, "coluna": 5, "coluna_fim": 8, "antigo": "var", "novo": "let"}],
           "confianca": "certa", "sugestao": "troque", "relacionados": [], "spec_ref": "forja explicar E0707", "id": "E0707@a.fj:2:9"}
    Linhas = Fonte.split(b"\n")
    Plantas = {
        "antigo errado": lambda D: D["correcao"][0].update(antigo="let"),
        "marcador na coluna errada": lambda D: D["trecho"].__setitem__(2, "  |        ^"),
        "sem trecho": lambda D: D.pop("trecho"),
        "certa sem edicao": lambda D: D.update(correcao=[]),
        "sem spec": lambda D: D.pop("spec_ref"),
    }
    Falhou = 0
    if ConferirPacote(Bom, Linhas):
        print("selftest: o pacote bom foi acusado: %r" % ConferirPacote(Bom, Linhas))
        Falhou += 1
    for Nome, Planta in Plantas.items():
        Copia = json.loads(json.dumps(Bom))
        Planta(Copia)
        if not ConferirPacote(Copia, Linhas):
            print("selftest: planta nao acusada: " + Nome)
            Falhou += 1
    Novo, Quantas = AplicarCertas(Fonte, [Bom])
    if Quantas != 1 or Novo != Fonte.replace(b"var", b"let"):
        print("selftest: AplicarCertas errou")
        Falhou += 1
    print("selftest: %s" % ("ok" if Falhou == 0 else "%d falha(s)" % Falhou))
    return Falhou


def Principal():
    Parser = argparse.ArgumentParser(description="Confere o pacote do erro nos err_* do lote 1 de sugestoes (P-IA1-b04).")
    Parser.add_argument("--forja")
    Parser.add_argument("--lang", default=str(Path(__file__).resolve().parents[2]))
    Parser.add_argument("--json", action="store_true")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    if Args.selftest:
        return 1 if Selftest() else 0
    if not Args.forja:
        Parser.error("--forja e obrigatorio")
    Lote = ListarLote(Path(Args.lang))
    Achados = [ConferirUmCaso(Path(Args.forja), Fj, Esp) for Fj, Esp in Lote]
    Metricas = Resumir(Achados)
    for Id, Valor, Verde in Metricas:
        print(json.dumps({"id": Id, "valor": Valor, "verde": Verde}, ensure_ascii=False))
    print("casos=%d falhas=%d" % (len(Lote), sum(1 for _, _, V in Metricas if not V)))
    return 0 if all(V for _, _, V in Metricas) else 1


if __name__ == "__main__":
    sys.exit(Principal())
