# -*- coding: utf-8 -*-
"""notas.py - as 4 notas MEDIDAS do IA1 (ENTENDER, ESCREVER, DEPURAR, GERAL) e a lista de metas vermelhas (P-IA1-a07, contrato 1).

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao. nota_eixo = 10 x metas_verdes / metas_do_eixo, arredondada para 0,5. O total do eixo e
        FIXO (a tabela abaixo = o contrato): meta sem linha de medida, ou com verde null (pendente), nao conta como verde.
        GERAL = G-INDICE, M-V1, M-V2 mais 3 metas "ENTENDER, ESCREVER e DEPURAR em 10". M-D10 e M-D11 contam em DEPURAR e em ESCREVER.

Uso:
  python notas.py --mecanicas <arq.jsonl> --bench <arq.jsonl> [--json]
  (mecanicas = saida de metricas_mecanicas.py --json; bench = saida de rodar_ia_first.py relatorio --json; qualquer um pode faltar)
Saida: texto, ou um JSON {notas:{eixo:{nota,verdes,total,vermelhas,pendentes}}} com --json. Exit 0; 2 uso.
Verificacao: python selftest_ia_first.py. Reversao: apagar este arquivo. Depende de: Python 3.9+ (biblioteca padrao).
"""
import argparse
import json
import sys
from pathlib import Path

EIXOS = {
    "ENTENDER": ["M-U1", "M-U2", "M-U3", "M-U4", "M-U5", "B-U6", "M-A1", "M-A2", "M-A3", "M-A4", "M-I1", "M-I2", "M-I3", "B-I4"],
    "ESCREVER": ["B-W1", "B-W2", "B-W3", "B-W4", "B-W7", "B-W5", "M-W6", "M-E1", "M-E2", "B-E5", "M-D10", "M-D11"],
    "DEPURAR": ["M-D1", "M-D2", "M-D3", "M-D4", "M-D5", "M-D6", "M-D7", "M-D8", "B-D9", "M-D10", "M-D11", "M-D12", "M-D13"],
    "GERAL": ["G-INDICE", "M-V1", "M-V2"],
}
EIXOS_EXIGIDOS_NO_GERAL = ("ENTENDER", "ESCREVER", "DEPURAR")


def LerLinhas(Caminho):
    """{id: linha} das linhas de metrica do arquivo (ignora as que nao tem id, como a de grupos)."""
    Linhas = {}
    if not Caminho:
        return Linhas
    for Texto in Path(Caminho).read_text(encoding="utf-8").splitlines():
        try:
            Objeto = json.loads(Texto)
        except ValueError:
            continue
        if isinstance(Objeto, dict) and "id" in Objeto and "verde" in Objeto:
            Linhas[Objeto["id"]] = Objeto
    return Linhas


def ArredondarMeio(Valor):
    return round(Valor * 2) / 2


def NotaDoEixo(Ids, Linhas):
    Verdes = [I for I in Ids if Linhas.get(I, {}).get("verde") is True]
    Vermelhas = [I for I in Ids if Linhas.get(I, {}).get("verde") is False]
    Pendentes = [I for I in Ids if I not in Verdes and I not in Vermelhas]
    return {"nota": ArredondarMeio(10.0 * len(Verdes) / len(Ids)), "verdes": len(Verdes), "total": len(Ids),
            "vermelhas": Vermelhas, "pendentes": Pendentes}


def NotaGeral(Linhas, Notas):
    """GERAL: as metas proprias mais uma meta por eixo que precisa estar em 10 (vira verde so com todas as metas dele verdes)."""
    Proprias = NotaDoEixo(EIXOS["GERAL"], Linhas)
    Exigidos = [E for E in EIXOS_EXIGIDOS_NO_GERAL if Notas[E]["nota"] < 10.0]
    Total = Proprias["total"] + len(EIXOS_EXIGIDOS_NO_GERAL)
    Verdes = Proprias["verdes"] + len(EIXOS_EXIGIDOS_NO_GERAL) - len(Exigidos)
    return dict(Proprias, nota=ArredondarMeio(10.0 * Verdes / Total), verdes=Verdes, total=Total,
                vermelhas=Proprias["vermelhas"] + ["%s=10" % E for E in Exigidos], pendentes=Proprias["pendentes"])


def CalcularNotas(Mecanicas, Bench):
    Linhas = dict(Mecanicas)
    Linhas.update(Bench)
    Notas = {Eixo: NotaDoEixo(Ids, Linhas) for Eixo, Ids in EIXOS.items() if Eixo != "GERAL"}
    Notas["GERAL"] = NotaGeral(Linhas, Notas)
    return {"notas": Notas}


def ImprimirTexto(Resultado):
    for Eixo, Dados in Resultado["notas"].items():
        print("%-9s nota %4.1f  (%d/%d metas verdes)" % (Eixo, Dados["nota"], Dados["verdes"], Dados["total"]))
        print("          vermelhas: %s" % (", ".join(Dados["vermelhas"]) or "nenhuma"))
        print("          pendentes: %s" % (", ".join(Dados["pendentes"]) or "nenhuma"))


def Principal():
    Analisador = argparse.ArgumentParser(description="As 4 notas medidas do IA1")
    Analisador.add_argument("--mecanicas")
    Analisador.add_argument("--bench")
    Analisador.add_argument("--json", action="store_true")
    Args = Analisador.parse_args()
    if not (Args.mecanicas or Args.bench):
        print("uso: notas.py --mecanicas <arq> --bench <arq> [--json]", file=sys.stderr)
        return 2
    Resultado = CalcularNotas(LerLinhas(Args.mecanicas), LerLinhas(Args.bench))
    if Args.json:
        print(json.dumps(Resultado, ensure_ascii=False))
    else:
        ImprimirTexto(Resultado)
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
