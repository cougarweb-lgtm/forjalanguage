"""Casos de cli do layout (v1.11 L9, A12): explicar E0328 e E0329, e o check que pega static_assert falso.

Importado pelo motor verificar_cli.py; as funcoes comuns vem de la.

Regras:
  - explicar devolve 1 linha JSON com o codigo, tipo "layout", explicacao e forma_certa do contrato;
  - check de um static_assert falso sai 1 com 1 linha E0329, a msg na causa, na posicao do static_assert (o check baixa a IR so para isso).

Versao: 1.0.1
Changelog: 1.0.1 - IA1-b08: o exemplo do catalogo de E0328 deixou o nome inventado (offset_of(S.campo) no lugar de offset_of(Vertice.uv)).
           1.0.0 - Linguagem v1.11 L9: CasoExplicarE0328, CasoExplicarE0329 e CasoStaticAssertFalso (fixture layout_static_assert_falso.fj).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

from verificar_cli import ConferirExecucao, LinhaJson, Rodar


def ConferirExplicar(Forja, Codigo, TrechoExplicacao, TrechoForma):
    Rodada = Rodar(Forja, ["explicar", Codigo])
    Problemas = ConferirExecucao(Rodada, 0)
    Dados = LinhaJson(Rodada.stdout)
    if Dados is None or Dados.get("codigo") != Codigo or Dados.get("tipo") != "layout":
        return Problemas + [f"explicar {Codigo} sem JSON com codigo e tipo layout: {Rodada.stdout.strip()[:120]!r}"]
    if TrechoExplicacao not in Dados.get("explicacao", "") or TrechoForma not in Dados.get("forma_certa", ""):
        Problemas.append(f"explicar {Codigo} sem explicacao e forma_certa do contrato: {Rodada.stdout.strip()[:160]!r}")
    return Problemas


def CasoExplicarE0328(Forja, PastaSaidas):
    """explicar E0328: size_of/align_of/offset_of, nao valem no [N]T, opaco e proc sem tamanho."""
    return ConferirExplicar(Forja, "E0328", "nao valem no tamanho de [N]T", "offset_of(S.campo)")


def CasoExplicarE0329(Forja, PastaSaidas):
    """explicar E0329: static_assert confere condicao constante; a msg vai na causa."""
    return ConferirExplicar(Forja, "E0329", "confere na compilacao uma condicao constante", "assert(x > 0) para valor de execucao")


def CasoStaticAssertFalso(Forja, PastaSaidas):
    """check de static_assert(size_of(Vertice) == 32, msg = ...) com Vertice de 12 bytes: E0329 em 7:1 com a msg na causa."""
    Rodada = Rodar(Forja, ["check", "layout_static_assert_falso.fj"])
    Problemas = ConferirExecucao(Rodada, 1)
    Dados = LinhaJson(Rodada.stdout)
    if Dados is None or Dados.get("codigo") != "E0329":
        return Problemas + [f"check nao devolveu 1 linha JSON E0329: {Rodada.stdout.strip()[:120]!r}"]
    if "Vertice mudou: confira o shader" not in Dados.get("causa", ""):
        Problemas.append(f"causa sem a msg do static_assert: {Dados.get('causa')!r}")
    if (Dados.get("linha"), Dados.get("coluna")) != (7, 1):
        Problemas.append(f"posicao {Dados.get('linha')}:{Dados.get('coluna')}, esperado 7:1")
    return Problemas
