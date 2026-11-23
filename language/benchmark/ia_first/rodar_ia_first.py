# -*- coding: utf-8 -*-
"""rodar_ia_first.py - runner do benchmark IA FIRST (P-IA1-a07, contrato 4): preparar, prompt, retomar, coletar, relatorio.

Versao 0.1.1
Changelog:
  0.1.1 (11/11/2026) P-IA1-a08: --pacote aceita P1 e P2 (material\\forja\\cartao_P1/P2.txt; a fiacao do variante.py esta em celula_ia_first,
        ia.py e coleta_ia_first). Nenhuma mudanca de codigo neste arquivo.
  0.1.0 (11/11/2026) primeira versao. As celulas ficam FORA do CWD do projeto (%TEMP%\\ia_first\\rodadas\\<rodada>\\celulas\\<id>),
        com uma copia fiel do forja.exe congelado (md5 conferido) e o ia.py dentro de cada uma. Quem dispara os subagentes e
        o main (Regra #23); este script nunca chama modelo.

Uso:
  python rodar_ia_first.py preparar --rodada R1 --modelo haiku [--pacote P0|P1|P2|semcartao] [--conjuntos E,L,Cx,Dx,C_forja,D_forja]
        [--linguas forja,c] [--rep-forja 3] [--rep-c 2] [--tarefas E01,E02] [--forja <exe>] [--base <pasta>]
  python rodar_ia_first.py retomar --rodada R1 [--tamanho 12]    celulas sem resultado, em lote, com o prompt de cada uma (JSON)
  python rodar_ia_first.py prompt --rodada R1 --celula <id>      texto do prompt fixo do spawn
  python rodar_ia_first.py coletar --rodada R1 [--celulas id,id] [--forcar]   verifica, checa contaminacao, grava resultado.json
  python rodar_ia_first.py relatorio --rodada R1 [--rodada R2 ...] [--base-rodada B1 ...] [--braco-forja P0] [--json]
  python rodar_ia_first.py toolchains [--forja <exe>]            versoes de gcc, python, node e do forja (nada e instalado)
Saida: JSON (uma linha por item) ou texto. Exit: 0 ok, 2 uso/erro de preparo, 4 celula invalidada duas vezes (PARA).
Reversao: apagar %TEMP%\\ia_first\\rodadas\\<rodada>. Verificacao: python selftest_ia_first.py
Depende de: Python 3.9+ (biblioteca padrao) e os modulos irmaos (ia_comum, celula_ia_first, coleta_ia_first, relatorio_ia_first).
"""
import argparse
import json
import sys
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import celula_ia_first as CEL
import coleta_ia_first
import relatorio_ia_first
from ia_comum import BASE_PADRAO, FORJA_CONGELADA, GravarJson, LerJson

CONJUNTOS_PADRAO = "E,L,Cx,Dx,C_forja,D_forja"


def PastaDaRodada(Base, Rodada):
    return Path(Rodada) if any(C in str(Rodada) for C in "\\/") else Path(Base) / "rodadas" / Rodada


def LinguasUteis(Pedidas, Forja):
    """Separa as linguagens pedidas em (usaveis, puladas com motivo). Obrigatoria ausente e erro."""
    Toolchains = CEL.Toolchains(Forja)
    Usaveis, Puladas = [], []
    for Nome in Pedidas:
        Info = Toolchains.get(Nome)
        if Info is None:
            raise SystemExit("linguagem desconhecida: " + Nome)
        if Info["disponivel"]:
            Usaveis.append(Nome)
        elif Info["obrigatoria"]:
            raise SystemExit("linguagem obrigatoria ausente: %s (%s)" % (Nome, Info["motivo"]))
        else:
            Puladas.append({"lingua": Nome, "motivo": Info["motivo"]})
    return Usaveis, Puladas


def ItensDaRodada(Args, Linguas):
    """Todos os (conjunto, tarefa, lingua, repeticao) a criar, com o filtro de tarefas aplicado."""
    Filtro = {T for T in (Args.tarefas or "").split(",") if T}
    Itens = []
    for Lingua in Linguas:
        Repeticoes = Args.rep_forja if Lingua == "forja" else Args.rep_c
        for Conj in Args.conjuntos.split(","):
            for Id in (I for I in CEL.IdsDoConjunto(Conj, Lingua) if not Filtro or I in Filtro):
                for Rep in range(1, Repeticoes + 1):
                    Itens.append({"conjunto": Conj, "tarefa": Id, "lingua": Lingua, "braco": CEL.Braco(Lingua, Args.pacote),
                                  "repeticao": Rep, "tentativa": 1})
    return Itens


def Preparar(Args):
    Pasta = PastaDaRodada(Args.base, Args.rodada or datetime.now().strftime("R%Y%m%d_%H%M%S"))
    if Pasta.exists():
        raise SystemExit("a rodada ja existe: %s (use outro --rodada)" % Pasta)
    Forja = CEL.CopiarForja(Args.forja, Pasta / "forja" / "forja.exe")
    Usaveis, Puladas = LinguasUteis(Args.linguas.split(","), Forja["caminho"])
    Rodada = {"id": Pasta.name, "pasta": str(Pasta), "modelo": Args.modelo, "pacote": Args.pacote, "forja": Forja,
              "criada": datetime.now().isoformat(timespec="seconds"), "celulas": []}
    for Item in ItensDaRodada(Args, Usaveis):
        Rodada["celulas"].append(CEL.CriarCelula(Rodada, Item))
    GravarJson(Pasta / "rodada.json", Rodada)
    print(json.dumps({"rodada": Rodada["id"], "pasta": str(Pasta), "celulas": len(Rodada["celulas"]), "forja": Forja,
                      "puladas": Puladas}, ensure_ascii=False))
    return 0


def Retomar(Args):
    Rodada = LerJson(PastaDaRodada(Args.base, Args.rodada) / "rodada.json")
    Faltam = coleta_ia_first.Pendentes(Rodada)
    Sem = [E for E in Faltam if not (Path(E["pasta"]) / "idas.jsonl").is_file() and not (Path(E["pasta"]) / "resposta.txt").is_file()]
    Lote = [{"id": E["id"], "pasta": E["pasta"], "batimento": E["pasta"].replace("\\", "/") + "/batimento.log",
             "prompt": CEL.TextoDoPrompt(Rodada, E)} for E in Sem[:Args.tamanho]]
    print(json.dumps({"pendentes": len(Faltam), "sem_atividade": len(Sem), "lote": Lote}, ensure_ascii=False))
    return 0


def Prompt(Args):
    Rodada = LerJson(PastaDaRodada(Args.base, Args.rodada) / "rodada.json")
    Entrada = next((E for E in Rodada["celulas"] if E["id"] == Args.celula and E["pasta"].endswith(Args.celula)), None)
    if Entrada is None:
        raise SystemExit("celula nao existe: " + str(Args.celula))
    print(CEL.TextoDoPrompt(Rodada, Entrada))
    return 0


def Coletar(Args):
    Filtro = [C for C in (Args.celulas or "").split(",") if C]
    Resultados = coleta_ia_first.Coletar(PastaDaRodada(Args.base, Args.rodada), Filtro, Args.forcar)
    for Res in Resultados:
        print(json.dumps({"id": Res["id"], "valida": Res["valida"], "pass1": Res["pass1"], "idas_ate_verde": Res["idas_ate_verde"],
                          "verde_final": Res["verde_final"], "motivos_invalidez": Res["motivos_invalidez"][:3],
                          "PARA": Res.get("PARA", False)}, ensure_ascii=False))
    return 4 if any(R.get("PARA") for R in Resultados) else 0


def Relatorio(Args):
    Pastas = [PastaDaRodada(Args.base, R) for R in Args.rodada_lista]
    Base = [PastaDaRodada(Args.base, R) for R in Args.base_rodada]
    Linhas = relatorio_ia_first.Relatorio(Pastas, Base, Args.braco_forja, Args.braco_base)
    for Linha in Linhas:
        print(json.dumps(Linha, ensure_ascii=False) if Args.json else TextoDaLinha(Linha))
    return 0


def TextoDaLinha(Linha):
    if "id" in Linha:
        Estado = {True: "VERDE", False: "VERMELHO", None: "pendente"}[Linha["verde"]]
        return "%-9s %-9s %s" % (Linha["id"], Estado, json.dumps(Linha["valor"], ensure_ascii=False)[:160])
    return "celulas validas=%d invalidas=%d PARA=%s" % (Linha["celulas_validas"], Linha["celulas_invalidas"], Linha["PARA"])


def Toolchains(Args):
    for Nome, Info in CEL.Toolchains(Args.forja).items():
        print(json.dumps(dict(Info, lingua=Nome), ensure_ascii=False))
    return 0


def Argumentos():
    Analisador = argparse.ArgumentParser(description="Runner do benchmark IA FIRST")
    Analisador.add_argument("comando", choices=["preparar", "retomar", "prompt", "coletar", "relatorio", "toolchains"])
    Analisador.add_argument("--rodada", action="append", default=None)
    Analisador.add_argument("--modelo", default="haiku", choices=["haiku", "sonnet"])
    Analisador.add_argument("--pacote", default="P0")
    Analisador.add_argument("--conjuntos", default=CONJUNTOS_PADRAO)
    Analisador.add_argument("--linguas", default="forja,c")
    Analisador.add_argument("--rep-forja", type=int, default=1)
    Analisador.add_argument("--rep-c", type=int, default=1)
    Analisador.add_argument("--tarefas")
    Analisador.add_argument("--forja", default=str(FORJA_CONGELADA))
    Analisador.add_argument("--base", default=str(BASE_PADRAO))
    Analisador.add_argument("--tamanho", type=int, default=12)
    Analisador.add_argument("--celula")
    Analisador.add_argument("--celulas")
    Analisador.add_argument("--forcar", action="store_true")
    Analisador.add_argument("--base-rodada", action="append", default=[])
    Analisador.add_argument("--braco-forja", default="P0")
    Analisador.add_argument("--braco-base", default=None)  # braco da Forja nas rodadas --base-rodada (padrao: o mesmo de --braco-forja)
    Analisador.add_argument("--json", action="store_true")
    Args = Analisador.parse_args()
    Args.rodada_lista = Args.rodada or []
    Args.rodada = Args.rodada_lista[0] if Args.rodada_lista else None
    return Args


COMANDOS = {"preparar": Preparar, "retomar": Retomar, "prompt": Prompt, "coletar": Coletar, "relatorio": Relatorio, "toolchains": Toolchains}


def Principal():
    Args = Argumentos()
    if Args.comando in ("retomar", "prompt", "coletar") and not Args.rodada:
        print("uso: informe --rodada", file=sys.stderr)
        return 2
    return COMANDOS[Args.comando](Args)


if __name__ == "__main__":
    try:
        sys.exit(Principal())
    except SystemExit:
        raise
    except Exception as Erro:  # sem traceback: uma linha e exit 3
        print("erro interno: %s: %s" % (type(Erro).__name__, Erro), file=sys.stderr)
        sys.exit(3)
