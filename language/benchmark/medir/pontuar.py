# -*- coding: utf-8 -*-
"""Pontua os candidatos do benchmark do ADR-01 pelos pesos gravados em pesos.json.

Versao: 1.0.0
Changelog: 1.0.0 - primeira versao (notas por melhor/valor, soma ponderada, sensibilidade sem velocidade).
Verificacao: python pontuar.py   (le ../resultados/medicao_oficial.json e entradas_pontuacao.json)
"""
import json
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
CANDIDATOS = ("c", "cpp", "py")


def Carregar(Caminho: Path) -> dict:
    with open(Caminho, encoding="utf-8") as Arquivo:
        return json.load(Arquivo)


def NotaMenorMelhor(Valores: dict) -> dict:
    """melhor/valor; se todos forem zero, todos ficam com 1.0."""
    Melhor = min(Valores.values())
    return {Nome: (1.0 if Valor == 0 else Melhor / Valor) for Nome, Valor in Valores.items()}


def NotaMaiorMelhor(Valores: dict) -> dict:
    Melhor = max(Valores.values())
    return {Nome: Valor / Melhor for Nome, Valor in Valores.items()}


def MedianaDoBuild200k(Medicao: dict, Campo: str) -> dict:
    """Mediana do campo (total de tempos ou pico de memoria) do build no corpus de 200 mil linhas."""
    Saida = {}
    for Linha in Medicao["resumo"]:
        if Linha["comando"] == "build" and Linha["corpus"] == 200000:
            if Campo == "total_ms":
                Saida[Linha["candidato"]] = Linha["tempos_ms"]["total_ms"]["mediana"]
            else:
                Saida[Linha["candidato"]] = Linha["pico_mem_mb"]["mediana"]
    return Saida


def Media(Notas: list) -> dict:
    return {Nome: sum(Nota[Nome] for Nota in Notas) / len(Notas) for Nome in CANDIDATOS}


def MontarNotas(Entradas: dict, Medicao: dict, Pesos: dict) -> dict:
    """Nota 0..1 de cada candidato por criterio."""
    Cand = Entradas["candidatos"]
    Coluna = lambda Chave: {Nome: Cand[Nome][Chave] for Nome in CANDIDATOS}
    return {
        "agente_ia": Media([NotaMenorMelhor(Coluna("tokens_ate_suite_verde")),
                            NotaMaiorMelhor(Coluna("testes_de_primeira"))]),
        "velocidade_frontend_200k": NotaMenorMelhor(MedianaDoBuild200k(Medicao, "total_ms")),
        "manutencao_sloc": NotaMenorMelhor(Coluna("sloc")),
        "memoria_pico": NotaMenorMelhor(MedianaDoBuild200k(Medicao, "pico")),
        "distribuicao_toolchain": Media([NotaMenorMelhor(Coluna("tamanho_entregavel_mb")),
                                         NotaMenorMelhor(Coluna("toolchain_novo_mb")),
                                         NotaMenorMelhor(Coluna("build_limpo_s"))]),
        "self_hosting_e_llvm": NotasFixas(Pesos["pesos"]["self_hosting_e_llvm"]["notas_fixas"]),
    }


def NotasFixas(Fixas: dict) -> dict:
    """pesos.json chama o Python de 'python'; os resultados chamam de 'py'."""
    return {"c": Fixas["c"], "cpp": Fixas["cpp"], "py": Fixas["python"]}


def Pontuar(Notas: dict, Pesos: dict, Ignorar: tuple = ()) -> dict:
    Total = {Nome: 0.0 for Nome in CANDIDATOS}
    for Criterio, PorCandidato in Notas.items():
        if Criterio in Ignorar:
            continue
        for Nome in CANDIDATOS:
            Total[Nome] += Pesos["pesos"][Criterio]["peso"] * PorCandidato[Nome]
    return Total


def Imprimir(Notas: dict, Pesos: dict, Cenarios: dict) -> None:
    print(f"{'criterio':28s} {'peso':>5s}  " + "  ".join(f"{Nome:>6s}" for Nome in CANDIDATOS))
    for Criterio, PorCandidato in Notas.items():
        Peso = Pesos["pesos"][Criterio]["peso"]
        print(f"{Criterio:28s} {Peso:5d}  " + "  ".join(f"{PorCandidato[Nome]:6.3f}" for Nome in CANDIDATOS))
    for Titulo, Total in Cenarios.items():
        print(f"{Titulo:28s} {'':5s}  " + "  ".join(f"{Total[Nome]:6.1f}" for Nome in CANDIDATOS))


def Principal() -> int:
    Pesos = Carregar(Path(__file__).parent / "pesos.json")
    Entradas = Carregar(RAIZ / "resultados" / "entradas_pontuacao.json")
    Medicao = Carregar(RAIZ / "resultados" / "medicao_oficial.json")
    Notas = MontarNotas(Entradas, Medicao, Pesos)
    Cenarios = {
        "PONTUACAO (0-100)": Pontuar(Notas, Pesos),
        "sem self_hosting (sens.)": Pontuar(Notas, Pesos, ("self_hosting_e_llvm",)),
        "sem velocidade (sens.)": Pontuar(Notas, Pesos, ("velocidade_frontend_200k",)),
    }
    Imprimir(Notas, Pesos, Cenarios)
    Saida = RAIZ / "resultados" / "pontuacao.json"
    Saida.write_text(json.dumps({"notas": Notas, "cenarios": Cenarios}, indent=2), encoding="utf-8")
    print(f"gravado em {Saida}")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
