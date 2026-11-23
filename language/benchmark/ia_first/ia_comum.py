# -*- coding: utf-8 -*-
"""ia_comum.py - utilidades compartilhadas pelos scripts do benchmark IA FIRST (P-IA1-a07).

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao: json/jsonl, caminhos padrao, mediana, percentil, bootstrap (10.000 reamostras, semente
        fixa) para o IC95 da media e da diferenca entre dois grupos de tarefas.

Reversao: apagar este arquivo e os scripts que o importam. Depende de: Python 3.9+ (biblioteca padrao).
"""
import json
import os
import random
import statistics
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent
PASTA_TEMPORARIA = Path(os.environ.get("TEMP") or tempfile.gettempdir())
BASE_PADRAO = PASTA_TEMPORARIA / "ia_first"
FORJA_CONGELADA = PASTA_TEMPORARIA / "forja_ia1_base" / "forja.exe"
LIMITE_IDAS = 5
TOKENS_REF = 150000
REAMOSTRAS = 10000
SEMENTE = 20261111


def LerJson(Caminho):
    with open(Caminho, encoding="utf-8") as Arq:
        return json.load(Arq)


def GravarJson(Caminho, Dados):
    Caminho = Path(Caminho)
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    Caminho.write_text(json.dumps(Dados, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")


def LerJsonl(Caminho):
    """Lista de objetos de um .jsonl; arquivo ausente = lista vazia; linha invalida e ignorada."""
    Caminho = Path(Caminho)
    if not Caminho.is_file():
        return []
    Itens = []
    for Linha in Caminho.read_text(encoding="utf-8", errors="replace").splitlines():
        try:
            Itens.append(json.loads(Linha))
        except ValueError:
            continue
    return Itens


def Mediana(Valores):
    return statistics.median(Valores) if Valores else None


def Media(Valores):
    return sum(Valores) / len(Valores) if Valores else None


def Percentil(Valores, P):
    """Percentil pelo metodo do posto mais proximo (P de 0 a 100)."""
    if not Valores:
        return None
    Ordem = sorted(Valores)
    Posto = max(1, -(-len(Ordem) * P // 100))
    return Ordem[int(Posto) - 1]


def IcMedia(PorTarefa):
    """IC95 por bootstrap da media, reamostrando TAREFAS. PorTarefa: dict tarefa -> valor."""
    Valores = list(PorTarefa.values())
    if len(Valores) < 2:
        return None
    Sorteio = random.Random(SEMENTE)
    Medias = sorted(Media([Sorteio.choice(Valores) for _ in Valores]) for _ in range(REAMOSTRAS))
    return [Medias[int(REAMOSTRAS * 0.025)], Medias[int(REAMOSTRAS * 0.975) - 1]]


def IcDiferenca(A, B):
    """IC95 por bootstrap de media(A) - media(B) nas tarefas comuns. A e B: dict tarefa -> valor."""
    Comuns = sorted(set(A) & set(B))
    if len(Comuns) < 2:
        return None
    Dif = [A[T] - B[T] for T in Comuns]
    Sorteio = random.Random(SEMENTE)
    Medias = sorted(Media([Sorteio.choice(Dif) for _ in Dif]) for _ in range(REAMOSTRAS))
    return [Medias[int(REAMOSTRAS * 0.025)], Medias[int(REAMOSTRAS * 0.975) - 1]]


def Metrica(Id, Valor, Meta, Verde, **Extra):
    """Linha padrao de metrica: {id, valor, meta, verde}; verde None = pendente (nunca verde de mentira)."""
    Linha = {"id": Id, "valor": Valor, "meta": Meta, "verde": Verde}
    Linha.update(Extra)
    return Linha
