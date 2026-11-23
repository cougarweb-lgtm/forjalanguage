# verificar_relacionados_b14.py -- mede o campo "relacionados" do pacote do erro (contrato IA1, P-IA1-b14, M-D11 campo 4)
"""Roda `forja check --max-erros 100` em todos os err_* de tests\\sintaxe e tests\\semantica e olha as ocorrencias dos 10 codigos que tem relacionado:
  taxa por codigo    quantas ocorrencias do codigo trazem "relacionados" nao vazio (verde so com 100% em todos os 10) e a lista das que nao trazem
  esquema            todo relacionado tem papel (declaracao|tipo|assinatura|parecido|uso_anterior), arquivo, linha >= 1, coluna >= 1, texto (<= 120 bytes) e nota;
                     no maximo 5 por diagnostico; os outros codigos podem ficar com a lista vazia
  pontos de prova    PONTOS: relacionados com a linha CERTA, escritos a mao (o papel, o arquivo, a linha e o trecho do texto)
  tamanho            com --antes <forja.exe>: caracteres a mais por diagnostico (media) contra o compilador anterior, nos 10 codigos e no total
Uso:
    python -I verificar_relacionados_b14.py --forja <forja.exe> [--antes <forja.exe do b13>]     (exit 0 so se tudo verde; 1 = alguma vermelha; 2 = uso)
    python -I verificar_relacionados_b14.py --selftest                                          (prova o discriminador: lista vazia, esquema ruim e linha errada ficam vermelhos)
Contra o compilador do b13 (relacionados sempre vazio ou ausente) este verificador sai vermelho.
Versao: 0.1.1
Changelog: 0.1.1 - IA1-L3b (V-06 = B): a fixture do E0502 passa a ser o posicional DEPOIS de nomeado (soma(a = 1, 2)); soma(1, 2) agora compila.
           0.1.0 - P-IA1-b14: primeira versao.
Verificacao: python -I verificar_relacionados_b14.py --selftest
"""

import argparse
import json
import sys
import tempfile
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sondas_compilador as SC  # noqa: E402  Executar, LinhasJson, LANG (mesma pasta)

CODIGOS = ("E0201", "E0301", "E0310", "E0311", "E0502", "E0503", "E0504", "E1003", "E0202", "E0607")
PAPEIS = ("declaracao", "tipo", "assinatura", "parecido", "uso_anterior")
CHAVES = ("papel", "arquivo", "linha", "coluna", "texto", "nota")
MAXIMO_POR_DIAGNOSTICO = 5
TEXTO_MAXIMO = 120
PASTAS = ("sintaxe", "semantica")
FIXTURE_E0502 = "proc soma(a: i32, b: i32) -> i32 {\n    return a + b\n}\n\nproc main() {\n    print(soma(a = 1, 2))\n}\n"

# (nome, fixture, codigo, linha do erro, papel, arquivo termina com, linha do relacionado, trecho que o texto (ou a nota) traz). Escritos a mao olhando o .fj.
PONTOS = [
    ("E0201 parecido local", "semantica/err_E0201_parecido_local.fj", "E0201", 3, "parecido", "err_E0201_parecido_local.fj", 2, "let vida = 3"),
    ("E0201 parecido membro", "semantica/err_E0201_parecido_membro.fj", "E0201", 4, "parecido", "err_E0201_parecido_membro.fj", 1, "azul"),
    ("E0201 enum do membro", "semantica/err_E0201_membro_inexistente.fj", "E0201", 4, "tipo", "err_E0201_membro_inexistente.fj", 1, "enum Cor"),
    ("E0201 struct do campo", "semantica/err_E0201_offset_campo_parecido.fj", "E0201", 7, "tipo", "err_E0201_offset_campo_parecido.fj", 1, "posicao, cor"),
    ("E0504 parametros com nome e tipo", "semantica/err_E0504_parecido_param.fj", "E0504", 6, "parecido", "err_E0504_parecido_param.fj", 1, "a: i32, valor: i32"),
    ("E0502 assinatura do proc", None, "E0502", 6, "assinatura", "soma.fj", 1, "proc soma(a: i32, b: i32) -> i32"),
    ("E0503 campo sem valor", "semantica/err_E0503_campo_ponteiro_sem_valor.fj", "E0503", 6, "declaracao", "err_E0503_campo_ponteiro_sem_valor.fj", 2, "dono: ^No"),
    ("E1003 declaracao sem pub", "semantica/err_E1003_nao_pub.fj", "E1003", 4, "assinatura", "geo.fj", 18, "sem pub"),
    ("E1003 parecido no modulo", "semantica/err_E1003_parecido_pub.fj", "E1003", 4, "parecido", "geo.fj", 26, "pub proc soma(a: int, b: int) -> int"),
    ("E0202 primeira declaracao", "semantica/err_E0202_variavel_repetida.fj", "E0202", 3, "uso_anterior", "err_E0202_variavel_repetida.fj", 2, "let a = 1"),
    ("E0607 enum com os membros", "semantica/err_E0607_switch_enum_incompleto.fj", "E0607", 5, "tipo", "err_E0607_switch_enum_incompleto.fj", 1, "cima, baixo, lado"),
    ("E0301 operando da esquerda", "semantica/err_E0301_comparar_u32_i32.fj", "E0301", 4, "declaracao", "err_E0301_comparar_u32_i32.fj", 2, "let a: u32 = 1"),
    ("E0301 operando da direita", "semantica/err_E0301_comparar_u32_i32.fj", "E0301", 4, "declaracao", "err_E0301_comparar_u32_i32.fj", 3, "let b: i32 = 2"),
]


def FixturesDoContrato():
    """err_*.fj de tests\\sintaxe e tests\\semantica, em ordem."""
    return [P for Pasta in PASTAS for P in sorted((SC.LANG / "tests" / Pasta).glob("err_*.fj"))]


def Diagnosticos(Forja, Arquivo):
    """[(arquivo, linha JSON crua, objeto)] de `forja check --max-erros 100` no arquivo."""
    _, Saida, _ = SC.Executar([str(Forja), "check", "--max-erros", "100", Arquivo.name], Arquivo.parent)
    Achados = []
    for Linha in Saida.splitlines():
        if not Linha.lstrip().startswith("{"):
            continue
        try:
            Achados.append((Arquivo.name, Linha, json.loads(Linha)))
        except ValueError:
            continue
    return Achados


def Ocorrencias(Forja):
    with ThreadPoolExecutor(max_workers=SC.TRABALHADORES) as Pool:
        Rodadas = list(Pool.map(partial(Diagnosticos, Forja), FixturesDoContrato()))
    return [A for Rodada in Rodadas for A in Rodada]


def TaxaPorCodigo(Achados):
    """{codigo: (total, com relacionados, [arquivo:linha sem])} dos 10 codigos."""
    Total, Com, Sem = Counter(), Counter(), defaultdict(list)
    for Arquivo, _, D in Achados:
        Codigo = D.get("codigo")
        if Codigo not in CODIGOS:
            continue
        Total[Codigo] += 1
        if D.get("relacionados"):
            Com[Codigo] += 1
        else:
            Sem[Codigo].append("%s:%s" % (Arquivo, D.get("linha")))
    return {C: (Total[C], Com[C], Sem[C]) for C in CODIGOS}


def RelacionadoRuim(R):
    """Motivo pelo qual um relacionado fere o esquema, ou None."""
    if not isinstance(R, dict) or any(C not in R for C in CHAVES):
        return "faltam chaves"
    if R["papel"] not in PAPEIS:
        return "papel %r" % R["papel"]
    if not (isinstance(R["linha"], int) and R["linha"] >= 1 and isinstance(R["coluna"], int) and R["coluna"] >= 1):
        return "linha/coluna"
    if not R["arquivo"] or not R["texto"] or not R["nota"]:
        return "campo vazio"
    if len(R["texto"].encode("utf-8")) > TEXTO_MAXIMO:
        return "texto com mais de %d bytes" % TEXTO_MAXIMO
    return None


def FerimentosDeEsquema(Achados):
    """["arquivo:linha motivo"] de todo diagnostico (qualquer codigo) cujos relacionados ferem o esquema ou passam de 5."""
    Ruins = []
    for Arquivo, _, D in Achados:
        Lista = D.get("relacionados", [])
        Onde = "%s:%s" % (Arquivo, D.get("linha"))
        if not isinstance(Lista, list) or len(Lista) > MAXIMO_POR_DIAGNOSTICO:
            Ruins.append("%s mais de %d ou nao e lista" % (Onde, MAXIMO_POR_DIAGNOSTICO))
            continue
        Ruins += ["%s %s" % (Onde, M) for M in (RelacionadoRuim(R) for R in Lista) if M]
    return Ruins


def PontoConfere(Diags, Ponto):
    """True se algum diagnostico do codigo na linha do erro tem o relacionado do ponto."""
    _, _, Codigo, LinhaErro, Papel, Arquivo, Linha, Trecho = Ponto
    for D in Diags:
        if D.get("codigo") != Codigo or D.get("linha") != LinhaErro:
            continue
        for R in D.get("relacionados", []):
            Bate = R.get("papel") == Papel and str(R.get("arquivo", "")).endswith(Arquivo) and R.get("linha") == Linha
            if Bate and (Trecho in R.get("texto", "") or Trecho in R.get("nota", "")):
                return True
    return False


def LinhasDosPontos(Forja):
    """Uma linha (id, valor, verde) por ponto de prova. O ponto sem fixture usa o proc escrito num diretorio temporario."""
    Linhas = []
    with tempfile.TemporaryDirectory(prefix="b14_e0502_") as Tmp:
        (Path(Tmp) / "soma.fj").write_text(FIXTURE_E0502, encoding="utf-8")
        for Ponto in PONTOS:
            Arquivo = Path(Tmp) / "soma.fj" if Ponto[1] is None else SC.LANG / "tests" / Ponto[1]
            Diags = [D for _, _, D in Diagnosticos(Forja, Arquivo)]
            Linhas.append(("ponto %s" % Ponto[0], {"linha_certa": Ponto[6], "papel": Ponto[4]}, PontoConfere(Diags, Ponto)))
    return Linhas


def MediaDeCaracteres(Achados, Filtro):
    Tamanhos = [len(L) for _, L, D in Achados if Filtro(D)]
    return (sum(Tamanhos) / len(Tamanhos), len(Tamanhos)) if Tamanhos else (0.0, 0)


def LinhaDoTamanho(Antes, Depois):
    """Chars a mais por diagnostico (media) nos 10 codigos e em todos; as listas de diagnosticos tem de ter o mesmo tamanho (o 1o erro de cada err_* nao muda)."""
    Dos10 = lambda D: D.get("codigo") in CODIGOS  # noqa: E731
    Todos = lambda D: True  # noqa: E731
    A10, N10 = MediaDeCaracteres(Antes, Dos10)
    D10, M10 = MediaDeCaracteres(Depois, Dos10)
    AT, NT = MediaDeCaracteres(Antes, Todos)
    DT, MT = MediaDeCaracteres(Depois, Todos)
    Valor = {"diagnosticos_dos_10_codigos": N10, "media_antes_10": round(A10, 1), "media_depois_10": round(D10, 1), "a_mais_por_diag_10": round(D10 - A10, 1),
             "diagnosticos_total": NT, "media_antes_total": round(AT, 1), "media_depois_total": round(DT, 1), "a_mais_por_diag_total": round(DT - AT, 1)}
    return ("tamanho", Valor, N10 == M10 and NT == MT and N10 > 0)


def LinhasDaTaxa(Achados):
    Taxa = TaxaPorCodigo(Achados)
    Linhas = [("M-D11 campo 4 %s" % C, {"com": Com, "total": Total, "sem": Sem[:8]}, Total > 0 and Com == Total) for C, (Total, Com, Sem) in Taxa.items()]
    Ruins = FerimentosDeEsquema(Achados)
    return Linhas + [("esquema", {"ferimentos": len(Ruins), "exemplos": Ruins[:5]}, not Ruins)]


def Selftest():
    """O discriminador acusa o que deve: lista vazia, relacionado mal formado, mais de 5, texto longo e linha errada ficam vermelhos; o bom fica verde."""
    Bom = {"papel": "tipo", "arquivo": "a.fj", "linha": 1, "coluna": 6, "texto": "enum Cor { verde }", "nota": "enum 'Cor': verde"}
    Fabrica = lambda Codigo, Lista, Linha=3: ("a.fj", json.dumps({"codigo": Codigo, "linha": Linha, "relacionados": Lista}), {"codigo": Codigo, "linha": Linha, "relacionados": Lista})  # noqa: E731
    Cheio = [Fabrica(C, [Bom]) for C in CODIGOS]
    Vazio = Cheio[:-1] + [Fabrica("E0607", [])]
    Ponto = ("p", "x", "E0201", 3, "tipo", "a.fj", 1, "enum Cor")
    Provas = [
        ("100% verde", all(Com == Total and Total > 0 for Total, Com, _ in TaxaPorCodigo(Cheio).values())),
        ("lista vazia acusada", TaxaPorCodigo(Vazio)["E0607"][2] == ["a.fj:3"] and TaxaPorCodigo(Vazio)["E0607"][1] == 0),
        ("bom sem ferimento", FerimentosDeEsquema(Cheio) == []),
        ("papel invalido acusado", FerimentosDeEsquema([Fabrica("E0201", [dict(Bom, papel="coisa")])]) != []),
        ("chave faltando acusada", FerimentosDeEsquema([Fabrica("E0201", [{"papel": "tipo"}])]) != []),
        ("seis relacionados acusados", FerimentosDeEsquema([Fabrica("E0201", [Bom] * 6)]) != []),
        ("texto de 121 bytes acusado", FerimentosDeEsquema([Fabrica("E0201", [dict(Bom, texto="y" * 121)])]) != []),
        ("linha 0 acusada", FerimentosDeEsquema([Fabrica("E0201", [dict(Bom, linha=0)])]) != []),
        ("ponto certo confere", PontoConfere([{"codigo": "E0201", "linha": 3, "relacionados": [Bom]}], Ponto)),
        ("ponto com linha errada acusado", not PontoConfere([{"codigo": "E0201", "linha": 3, "relacionados": [dict(Bom, linha=2)]}], Ponto)),
        ("ponto com papel errado acusado", not PontoConfere([{"codigo": "E0201", "linha": 3, "relacionados": [dict(Bom, papel="parecido")]}], Ponto)),
        ("ponto sem relacionados acusado", not PontoConfere([{"codigo": "E0201", "linha": 3}], Ponto)),
        ("tamanho exige as mesmas contagens", not LinhaDoTamanho(Cheio, Cheio[:-1])[2] and LinhaDoTamanho(Cheio, Cheio)[2]),
    ]
    Falhas = [Nome for Nome, Passou in Provas if not Passou]
    for Nome in Falhas:
        print("selftest: falhou: " + Nome)
    print("selftest: %s (%d provas)" % ("ok" if not Falhas else "%d falha(s)" % len(Falhas), len(Provas)))
    return len(Falhas)


def Principal():
    Parser = argparse.ArgumentParser(description="Relacionados do pacote do erro (P-IA1-b14, M-D11 campo 4).")
    Parser.add_argument("--forja")
    Parser.add_argument("--antes", help="forja.exe anterior (b13), para medir o tamanho a mais por diagnostico")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    if Args.selftest:
        return 1 if Selftest() else 0
    if not Args.forja or not Path(Args.forja).is_file() or (Args.antes and not Path(Args.antes).is_file()):
        Parser.error("--forja <forja.exe> e obrigatorio (e --antes, se dado, tem de existir)")
    Achados = Ocorrencias(Path(Args.forja).resolve())
    Linhas = LinhasDaTaxa(Achados) + LinhasDosPontos(Path(Args.forja).resolve())
    if Args.antes:
        Linhas.append(LinhaDoTamanho(Ocorrencias(Path(Args.antes).resolve()), Achados))
    for Id, Valor, Verde in Linhas:
        print(json.dumps({"id": Id, "valor": Valor, "verde": Verde}, ensure_ascii=False))
    print("ocorrencias=%d falhas=%d" % (len(Achados), sum(1 for _, _, V in Linhas if not V)))
    return 0 if all(V for _, _, V in Linhas) else 1


if __name__ == "__main__":
    sys.exit(Principal())
