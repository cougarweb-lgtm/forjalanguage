# verificar_sugestoes_l2.py -- confere o pacote do erro (contrato IA1, secao 1.1) nos err_* do lote 2 de sugestoes (P-IA1-b06)
"""Roda `forja check` nos err_* dos codigos do lote 2 (E0502, E0310, E0311, E0201, E0102, E1003, E0202, E0307) e confere, sem modelo:
  intacto       o 1o diagnostico tem o mesmo codigo, arquivo, linha e coluna do .esperado.jsonl
  M-D1          >= 90% das ocorrencias CORRIGIVEIS (a forma da lista D-IA1-6, ver Elegivel) trazem "sugestao"
  M-D11         campos do pacote (trecho, causa, correcao, confianca, sugestao, relacionados, spec ou spec_ref, id) em todo diagnostico que tem
                sugestao; "antigo" de cada edicao e o texto da faixa de bytes
  nunca_certa   (R-IA1-5, M-D10) E0201/E1003 com parecido, E0102, E0311 e E0202 nunca saem com confianca "certa"
  M-D13         aplicar so as correcoes "certa" numa copia (com as pastas irmas): o original some e o numero de erros nao sobe
  M-D10         (reportado) quantos casos com correcao certa chegam a exit 0 depois de aplicada
  M-D5          `build` nos 2 backends (c e x64) imprime a mesma saida byte a byte
  M-D6          2 execucoes do mesmo check dao a mesma saida byte a byte
Uso:
    python -I verificar_sugestoes_l2.py --forja <forja.exe> [--lang <pasta language>]
    python -I verificar_sugestoes_l2.py --selftest
Saida: uma linha por metrica {id, valor, verde}; exit 0 so se todas verdes (1 = alguma vermelha, 2 = uso).
Versao: 0.1.1
Changelog: 0.1.1 - M6e P93g: E0913 entra no lote 2 (Elegivel sempre; nunca certa).
           0.1.0 - P-IA1-b06: primeira versao (reaproveita o leitor, o conferidor de pacote e a paridade do verificar_sugestoes.py).
Verificacao: python -I verificar_sugestoes_l2.py --selftest
"""

import argparse
import json
import re
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import verificar_sugestoes as VS  # noqa: E402  leitor de diagnosticos, ConferirPacote, AplicarCertas, paridade (mesma pasta)

CODIGOS_DO_LOTE_2 = {"E0502", "E0310", "E0311", "E0201", "E0102", "E1003", "E0202", "E0307", "E0913"}
NUNCA_CERTA = {"E0201", "E0102", "E0311", "E0202", "E0913"}
LET_VAR_SIMPLES = re.compile(r"^\s*(let|var)\s+\w+\s*=\s*[\w.]+\s*$")
MINIMO_M_D1 = 0.90


def Elegivel(Diag, Linha):
    """A ocorrencia tem a forma que a lista corrigivel (D-IA1-6) promete corrigir: esta tem de trazer sugestao."""
    Codigo, Causa = Diag.get("codigo"), Diag.get("causa", "")
    Parecidos = Diag.get("parecidos", [])
    if Codigo == "E0502":
        return True
    if Codigo == "E0310":
        return Causa.startswith("append recebe o endereco")
    if Codigo == "E0311":
        return LET_VAR_SIMPLES.match(Linha) is not None
    if Codigo == "E0201":
        return len(Parecidos) == 1
    if Codigo == "E1003":
        return " e do modulo " in Causa or len(Parecidos) == 1
    if Codigo == "E0102":
        return "esta na caixa errada" in Causa
    if Codigo == "E0202":
        return "simbolo C" in Causa or ("reservado pela linguagem" in Causa and Linha.startswith((" ", "\t")))
    if Codigo == "E0307":
        return Causa.startswith("str nao se soma")
    if Codigo == "E0913":
        return True  # M6e P93g: dica em todo caso (&l[i], l[..], get_ptr, for &x); provavel com edicao quando o indice nao muda
    return False


def ProblemasDeConfianca(Diag):
    """R-IA1-5: confianca certa so onde a correcao e unica (parecido, caixa, emprestimo e alias nunca)."""
    Itens = [Diag] + list(Diag.get("alternativas", []))
    Certa = any(I.get("confianca") == "certa" for I in Itens if I.get("sugestao"))
    if Certa and (Diag.get("codigo") in NUNCA_CERTA or (Diag.get("codigo") == "E1003" and Diag.get("parecidos"))):
        return ["%s com confianca certa" % Diag["codigo"]]
    return []


def ListarLote2(Lang):
    """(fj, primeiro esperado) dos err_* cujo 1o esperado e de um codigo do lote 2."""
    Lote = []
    for Fj in sorted((Lang / "tests").glob("*/err_*.fj")):
        Esp = Fj.with_suffix(".esperado.jsonl")
        Primeiro = VS.PrimeiroEsperado(Esp) if Esp.exists() else None
        if Primeiro is not None and Primeiro["codigo"] in CODIGOS_DO_LOTE_2:
            Lote.append((Fj, Primeiro))
    return Lote


def CopiarCaso(Fj, Copia):
    """Copia o .fj e as pastas irmas (mods/...) para a pasta da copia: o import do caso continua resolvendo."""
    shutil.copyfile(Fj, Copia)
    for Irmao in Fj.parent.iterdir():
        if Irmao.is_dir() and Irmao.name != "__pycache__":
            shutil.copytree(Irmao, Copia.parent / Irmao.name, dirs_exist_ok=True)


def ConferirAplicacao(Forja, Fj, Conteudo, Diags):
    """M-D13 e M-D10 como no lote 1, mas na copia com as pastas irmas. Sem correcao certa: nada a conferir (exit0 None)."""
    Novo, Quantas = VS.AplicarCertas(Conteudo, Diags)
    if Quantas == 0:
        return {"piora": [], "exit0": None}
    with tempfile.TemporaryDirectory(prefix="verif_sug_l2_") as Tmp:
        Copia = Path(Tmp) / Fj.name
        CopiarCaso(Fj, Copia)
        Copia.write_bytes(Novo)
        Codigo, Saida = VS.RodarForja(Forja, ["check", Copia.name], Tmp)
    Depois = VS.LerDiagnosticos(Saida)
    Piora = []
    if len(Depois) > len(Diags):
        Piora.append("erros subiram de %d para %d" % (len(Diags), len(Depois)))
    Original = (Diags[0]["codigo"], Diags[0]["linha"])
    if any((D.get("codigo"), D.get("linha")) == Original for D in Depois):
        Piora.append("o diagnostico original continua depois da correcao")
    return {"piora": Piora, "exit0": Codigo == 0}


def ConferirUmCaso(Forja, Fj, Esperado):
    """Achados de um err_*: listas de problemas vazias = verde; elegivel e com_sugestao falam do 1o diagnostico."""
    Codigo, Saida = VS.RodarForja(Forja, ["check", Fj.name], Fj.parent)
    Diags = VS.LerDiagnosticos(Saida)
    Conteudo = Fj.read_bytes()
    Linhas = Conteudo.split(b"\n")
    Primeiro = Diags[0] if Diags else {}
    Chave = ("codigo", "arquivo", "linha", "coluna")
    Achados = {"arquivo": Fj.name, "intacto": [], "pacote": [], "confianca": [], "elegivel": False, "com_sugestao": False, "determinismo": []}
    if Codigo != 1 or [Primeiro.get(C) for C in Chave] != [Esperado[C] for C in Chave]:
        Achados["intacto"].append("1o diagnostico %r, esperado %r" % ([Primeiro.get(C) for C in Chave], [Esperado[C] for C in Chave]))
        return dict(Achados, piora=[], exit0=None, paridade=[])
    Linha = (VS.TextoDaLinha(Linhas, Primeiro["linha"]) or b"").decode("utf-8", errors="replace")
    Achados["elegivel"] = Elegivel(Primeiro, Linha)
    Achados["com_sugestao"] = bool(Primeiro.get("sugestao"))
    for Diag in Diags:
        if Diag.get("sugestao"):
            Achados["pacote"] += VS.ConferirPacote(Diag, Linhas)
            Achados["confianca"] += ProblemasDeConfianca(Diag)
    Achados["determinismo"] += [] if VS.RodarForja(Forja, ["check", Fj.name], Fj.parent)[1] == Saida else ["2 execucoes diferem"]
    Achados.update(ConferirAplicacao(Forja, Fj, Conteudo, Diags))
    Achados["paridade"] = VS.ConferirParidade(Forja, Fj)
    return Achados


def Resumir(Achados):
    """Metricas do lote a partir dos achados de cada caso: lista de (id, valor, verde)."""
    Elegiveis = [A for A in Achados if A["elegivel"]]
    Com = sum(1 for A in Elegiveis if A["com_sugestao"])
    Taxa = Com / len(Elegiveis) if Elegiveis else 0.0
    Certos = [A for A in Achados if A["exit0"] is not None]
    Falhas = lambda Chave: [A["arquivo"] + ": " + P for A in Achados for P in A[Chave]]  # noqa: E731
    return [
        ("intacto", {"casos": len(Achados), "problemas": Falhas("intacto")}, not Falhas("intacto")),
        ("M-D1", {"com_sugestao": Com, "ocorrencias_corrigiveis": len(Elegiveis), "taxa": round(Taxa, 3),
                  "sem": [A["arquivo"] for A in Elegiveis if not A["com_sugestao"]]}, Taxa >= MINIMO_M_D1 and bool(Elegiveis)),
        ("M-D11", {"problemas": Falhas("pacote")}, not Falhas("pacote")),
        ("nunca_certa", {"problemas": Falhas("confianca")}, not Falhas("confianca")),
        ("M-D13", {"piora": Falhas("piora")}, not Falhas("piora")),
        ("M-D10", {"exit0_apos_certas": sum(1 for A in Certos if A["exit0"]), "casos_com_certa": len(Certos)}, True),
        ("M-D5", {"problemas": Falhas("paridade")}, not Falhas("paridade")),
        ("M-D6", {"problemas": Falhas("determinismo")}, not Falhas("determinismo")),
    ]


def Selftest():
    """Elegivel e nunca_certa acusam o que devem (nao precisa de forja.exe); o pacote bom e as plantas ficam com o selftest do verificar_sugestoes."""
    Falhou = 0
    Provas = [
        ("E0502 sempre", Elegivel({"codigo": "E0502"}, "f(1, 2)")),
        ("E0310 append", Elegivel({"codigo": "E0310", "causa": "append recebe o endereco da lista: x"}, "append(l)")),
        ("E0310 delete nao", not Elegivel({"codigo": "E0310", "causa": "delete pede ^T"}, "delete(x)")),
        ("E0311 let simples", Elegivel({"codigo": "E0311"}, "    let b = a")),
        ("E0311 return nao", not Elegivel({"codigo": "E0311"}, "    return b.nomes")),
        ("E0201 parecido unico", Elegivel({"codigo": "E0201", "parecidos": ["vida"]}, "x")),
        ("E0201 dois parecidos nao", not Elegivel({"codigo": "E0201", "parecidos": ["a", "b"]}, "x")),
        ("E1003 qualificar", Elegivel({"codigo": "E1003", "causa": "'soma' e do modulo \"m\": importe-o"}, "x")),
        ("E0102 caixa", Elegivel({"codigo": "E0102", "causa": "o nome 'X' esta na caixa errada (r); use 'x'"}, "x")),
        ("E0102 descarte nao", not Elegivel({"codigo": "E0102", "causa": "'_' so descarta"}, "x")),
        ("E0202 alias", Elegivel({"codigo": "E0202", "causa": "o simbolo C \"labs\" ja tem alias"}, "    proc a")),
        ("E0202 reservado em foreign", Elegivel({"codigo": "E0202", "causa": "o nome 'get' e reservado pela linguagem"}, "    proc get()")),
        ("E0202 reservado fora de foreign nao", not Elegivel({"codigo": "E0202", "causa": "o nome 'get' e reservado pela linguagem"}, "proc get()")),
        ("E0307 soma", Elegivel({"codigo": "E0307", "causa": "str nao se soma nem faz conta"}, "x")),
        ("certa em parecido acusada", bool(ProblemasDeConfianca({"codigo": "E0201", "sugestao": "x", "confianca": "certa"}))),
        ("certa em E1003 qualificar aceita", not ProblemasDeConfianca({"codigo": "E1003", "sugestao": "x", "confianca": "certa"})),
        ("certa em E1003 com parecido acusada", bool(ProblemasDeConfianca({"codigo": "E1003", "sugestao": "x", "confianca": "certa", "parecidos": ["a"]}))),
        ("certa na alternativa de E0311 acusada", bool(ProblemasDeConfianca({"codigo": "E0311", "sugestao": "x", "confianca": "provavel",
                                                                              "alternativas": [{"sugestao": "y", "confianca": "certa", "correcao": []}]}))),
    ]
    for Nome, Passou in Provas:
        if not Passou:
            print("selftest: falhou: " + Nome)
            Falhou += 1
    print("selftest: %s" % ("ok" if Falhou == 0 else "%d falha(s)" % Falhou))
    return Falhou


def Principal():
    Parser = argparse.ArgumentParser(description="Confere o pacote do erro nos err_* do lote 2 de sugestoes (P-IA1-b06).")
    Parser.add_argument("--forja")
    Parser.add_argument("--lang", default=str(Path(__file__).resolve().parents[2]))
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    if Args.selftest:
        return 1 if Selftest() else 0
    if not Args.forja:
        Parser.error("--forja e obrigatorio")
    Lote = ListarLote2(Path(Args.lang))
    Achados = [ConferirUmCaso(Path(Args.forja).resolve(), Fj, Esp) for Fj, Esp in Lote]
    Metricas = Resumir(Achados)
    for Id, Valor, Verde in Metricas:
        print(json.dumps({"id": Id, "valor": Valor, "verde": Verde}, ensure_ascii=False))
    print("casos=%d falhas=%d" % (len(Lote), sum(1 for _, _, V in Metricas if not V)))
    return 0 if all(V for _, _, V in Metricas) else 1


if __name__ == "__main__":
    sys.exit(Principal())
