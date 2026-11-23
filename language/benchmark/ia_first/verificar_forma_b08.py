# verificar_forma_b08.py -- mede a forma por ocorrencia de TODO codigo dos err_* (contrato IA1, P-IA1-b08, M-D3 total)
"""Roda `forja check` em todos os err_*.fj (sintaxe, semantica, dourado, regressao) e olha todas as ocorrencias:
  M-D3 total        <= 40% das ocorrencias com a forma_certa igual a do catalogo (`forja explicar`)
  M-D3 por codigo   nenhum codigo com mais de 40% das ocorrencias com a explicacao OU a forma_certa igual a do catalogo
  M-D3 identicas    nenhum codigo com 5+ ocorrencias com mais de 40% no maior grupo de textos identicos entre si (explicacao + forma_certa)
  M-D3 nomes        0 ocorrencia cuja explicacao/forma_certa cita entre aspas um nome que nao esta no arquivo, na causa nem na lista de palavras da Forja (ex.: Grifo)
  catalogo limpo    nenhum texto do catalogo (de qualquer codigo) cita tipo de exemplo inventado (lista NOMES_PROIBIDOS_NO_CATALOGO)
  campos            toda ocorrencia traz explicacao e forma_certa nao vazias
  lote 2            E0308 E0306 E0307 E0503 E0504 E0310 E0311 E0601 E0607: todas as ocorrencias com texto proprio (0% iguais ao catalogo)
  E0311 sugestao    toda ocorrencia de E0311 dos err_* traz "sugestao"
  E0306 texto       `print("a" + 1)` diz os tipos reais (cstr com int) e o caminho do f"...", sem falar de aritmetica de ponteiro
Uso:
    python -I verificar_forma_b08.py --forja <forja.exe>          (exit 0 so se tudo verde; 1 = alguma vermelha; 2 = uso)
    python -I verificar_forma_b08.py --selftest                   (prova o discriminador: catalogo, grupo identico, nome inventado, campo vazio e catalogo sujo ficam vermelhos)
Contra o compilador do b07 (so E0101/E0301 com forma por ocorrencia) este verificador sai vermelho.
Versao: 0.1.0
Changelog: 0.1.0 - P-IA1-b08: primeira versao (a medida mora em sondas_compilador.ResumirForma, a mesma da M-D3 do metricas_mecanicas).
Verificacao: python -I verificar_forma_b08.py --selftest
"""

import argparse
import json
import re
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sondas_compilador as SC  # noqa: E402  Fixtures, Executar, LinhasJson, ResumirForma (mesma pasta)

LOTE_2 = ("E0308", "E0306", "E0307", "E0503", "E0504", "E0310", "E0311", "E0601", "E0607")
NOMES_PROIBIDOS_NO_CATALOGO = re.compile(r"\b(Grifo|Bala|Vertice|Circulo|Retangulo|Forma|Particula|Janela|Bando|ErroCarga|ErroIo|Cor)\b")
DIAG_C = SC.LANG / "compiler" / "common" / "diag.c"
FONTE_TEXTO = 'proc main() {\n    let c: cstr = "x"\n    print("a" + 1)\n    print(c + 1)\n}\n'


def CodigosDoCatalogo():
    """Todo codigo do catalogo (linhas {"E0001", "tipo", ...} do diag.c)."""
    return sorted(set(re.findall(r'\{"([EXRB]\d{4})", "', DIAG_C.read_text(encoding="utf-8"))))


def RodarCheck(Forja, Arquivo):
    _, Saida, Erro = SC.Executar([Forja, "check", Arquivo.name], Arquivo.parent)
    Objetos, _ = SC.LinhasJson(Saida + "\n" + Erro)
    Texto = SC.TextoComIrmaos(Arquivo)
    return [(Texto, D) for D in Objetos if D.get("codigo")]


def Ocorrencias(Forja):
    """[(texto do .fj com os irmaos, diagnostico)] de toda ocorrencia dos err_*."""
    with ThreadPoolExecutor(max_workers=8) as Pool:
        Rodadas = list(Pool.map(partial(RodarCheck, str(Forja)), SC.Fixtures("err_")))
    return [O for Rodada in Rodadas for O in Rodada]


def CatalogoSujo(Textos):
    """Codigos do catalogo {codigo: (explicacao, forma)} cujo texto cita tipo de exemplo inventado."""
    return sorted(C for C, Par in Textos.items() for T in Par if T and NOMES_PROIBIDOS_NO_CATALOGO.search(T))


def LinhasDoLote2(Achados, Catalogos):
    """Uma linha por codigo do lote 2: ocorrencias e % iguais ao catalogo (verde = tem ocorrencia e 0% iguais)."""
    Linhas = []
    for Codigo in LOTE_2:
        Diags = [D for _, D in Achados if D["codigo"] == Codigo]
        Iguais = SC.IguaisAoCatalogo(Diags, Catalogos.get(Codigo, (None, None)))
        Linhas.append(("lote 2 %s" % Codigo, {"ocorrencias": len(Diags), "iguais_ao_catalogo": Iguais}, bool(Diags) and Iguais == 0))
    return Linhas


def ResumirLinhas(Achados, Catalogos, Sujo):
    """Linhas (id, valor, verde) da M-D3 a partir de [(texto, diag)], dos catalogos e dos codigos sujos."""
    R = SC.ResumirForma(Achados, Catalogos)
    Limite = SC.LIMITE_FORMA_PCT
    Linhas = [
        ("M-D3 total", {"ocorrencias": R["ocorrencias"], "codigos": R["codigos"], "pct_forma_igual_ao_catalogo": R["pct_forma_igual_ao_catalogo"]},
         R["ocorrencias"] > 0 and R["pct_forma_igual_ao_catalogo"] <= Limite),
        ("M-D3 por codigo", {"pior": R["pior_codigo_igual_ao_catalogo"]}, R["ocorrencias"] > 0 and R["pior_codigo_igual_ao_catalogo"][1] <= Limite),
        ("M-D3 identicas", {"pior": R["pior_grupo_identico"]}, R["ocorrencias"] > 0 and R["pior_grupo_identico"][1] <= Limite),
        ("M-D3 nomes", {"ocorrencias_com_nome_fora_do_arquivo": R["com_nome_fora_do_arquivo"], "exemplos": R["exemplos_de_nome_fora"]}, R["com_nome_fora_do_arquivo"] == 0),
        ("catalogo limpo", {"codigos_com_exemplo_inventado": Sujo}, not Sujo),
        ("campos", {"sem_explicacao_ou_forma": R["sem_explicacao_ou_forma"]}, R["sem_explicacao_ou_forma"] == 0),
    ]
    return Linhas + LinhasDoLote2(Achados, Catalogos)


def LinhaDaSugestaoE0311(Achados):
    Diags = [D for _, D in Achados if D["codigo"] == "E0311"]
    Sem = [D.get("linha") for D in Diags if not D.get("sugestao")]
    return ("E0311 sugestao", {"ocorrencias": len(Diags), "sem_sugestao": len(Sem)}, bool(Diags) and not Sem)


def TextoDoE0306Certo(Diags):
    """As duas contas de texto (literal + int, cstr + int) dizem os tipos reais e o f"..." e nao falam de aritmetica de ponteiro."""
    Seis = [D for D in Diags if D.get("codigo") == "E0306"]
    return len(Seis) >= 1 and all("junta" in D.get("explicacao", "") and "cstr com int" in D.get("explicacao", "") and 'f"' in D.get("forma_certa", "")
                                  and "aritmetica" not in D.get("forma_certa", "") for D in Seis)


def LinhaDoE0306Texto(Forja):
    with tempfile.TemporaryDirectory(prefix="b08_e0306_") as Tmp:
        Arquivo = Path(Tmp) / "texto_em_conta.fj"
        Arquivo.write_text(FONTE_TEXTO, encoding="utf-8")
        _, Saida, Erro = SC.Executar([str(Forja), "check", "--max-erros", "5", Arquivo.name], Tmp)
    Diags, _ = SC.LinhasJson(Saida + "\n" + Erro)
    return ("E0306 texto", {"diagnosticos": len([D for D in Diags if D.get("codigo") == "E0306"])}, TextoDoE0306Certo(Diags))


def Diag(Codigo, Explicacao, Forma, Causa="c", Sugestao=None):
    D = {"codigo": Codigo, "linha": 1, "causa": Causa, "explicacao": Explicacao, "forma_certa": Forma}
    if Sugestao:
        D["sugestao"] = Sugestao
    return D


def Selftest():
    """O discriminador acusa o que deve e aceita texto variado; os dois codigos de teste cobrem o grupo identico (5+ ocorrencias) e a ocorrencia unica."""
    Catalogos = {"E0310": ("expl cat", "forma cat"), "E0201": ("expl cat 2", "forma cat 2")}
    Fonte = "proc main() {\n    let total = 1\n}\n"

    def Cenario(Fabrica, Sujo=()):
        Achados = [(Fonte, Fabrica(C, I)) for C in Catalogos for I in range(6)]
        return {Id: V for Id, _, V in ResumirLinhas(Achados, Catalogos, list(Sujo))}

    Bom = Cenario(lambda C, I: Diag(C, "na linha %d 'total' pede" % I, "troque 'total' %d" % I))
    Catalogal = Cenario(lambda C, I: Diag(C, Catalogos[C][0], Catalogos[C][1]))
    Identico = Cenario(lambda C, I: Diag(C, "mesma explicacao", "mesma forma"))
    Inventado = Cenario(lambda C, I: Diag(C, "na linha %d 'Grifo' pede" % I, "troque 'Grifo' %d" % I))
    Vazio = Cenario(lambda C, I: Diag(C, "", "forma %d" % I))
    Sujo = CatalogoSujo({"E0310": ("let g = new(Grifo, em = heap)", "x"), "E0201": ("ok", "ok")})
    Limpo = CatalogoSujo({"E0310": ("let p = new(T, em = heap)", "x")})
    Provas = [
        ("bom passa a M-D3", all(V for Id, V in Bom.items() if Id.startswith(("M-D3", "campos", "catalogo")))),
        ("catalogo acusado (total e por codigo)", not Catalogal["M-D3 total"] and not Catalogal["M-D3 por codigo"]),
        ("grupo identico acusado", not Identico["M-D3 identicas"]),
        ("nome inventado acusado", not Inventado["M-D3 nomes"]),
        ("campo vazio acusado", not Vazio["campos"]),
        ("exemplo inventado no catalogo acusado", Sujo == ["E0310"] and not Cenario(lambda C, I: Diag(C, "a %d" % I, "b %d" % I), Sujo)["catalogo limpo"]),
        ("catalogo limpo aceito", Limpo == []),
        ("sem ocorrencias nao fica verde", not ResumirLinhas([], Catalogos, [])[0][2]),
        ("E0311 sem sugestao acusado", not LinhaDaSugestaoE0311([(Fonte, Diag("E0311", "a", "b"))])[2] and LinhaDaSugestaoE0311([(Fonte, Diag("E0311", "a", "b", Sugestao="s"))])[2]),
        ("E0306 de texto: forma errada acusada", not TextoDoE0306Certo([Diag("E0306", "ponteiro nao tem conta", "use fatia")])
         and TextoDoE0306Certo([Diag("E0306", "a conta junta cstr com int", 'troque por f"..."')])),
    ]
    Falhas = [Nome for Nome, Passou in Provas if not Passou]
    for Nome in Falhas:
        print("selftest: falhou: " + Nome)
    print("selftest: %s (%d provas)" % ("ok" if not Falhas else "%d falha(s)" % len(Falhas), len(Provas)))
    return len(Falhas)


def Principal():
    Parser = argparse.ArgumentParser(description="Forma por ocorrencia de todo codigo dos err_* (P-IA1-b08, M-D3 total).")
    Parser.add_argument("--forja")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    if Args.selftest:
        return 1 if Selftest() else 0
    if not Args.forja or not Path(Args.forja).is_file():
        Parser.error("--forja <forja.exe> e obrigatorio")
    Forja = Path(Args.forja).resolve()
    Achados = Ocorrencias(Forja)
    Catalogos = SC.TextosDoCatalogo(Forja, {D["codigo"] for _, D in Achados} | set(CodigosDoCatalogo()))
    Linhas = ResumirLinhas(Achados, Catalogos, CatalogoSujo(Catalogos)) + [LinhaDaSugestaoE0311(Achados), LinhaDoE0306Texto(Forja)]
    for Id, Valor, Verde in Linhas:
        print(json.dumps({"id": Id, "valor": Valor, "verde": Verde}, ensure_ascii=False))
    print("ocorrencias=%d falhas=%d" % (len(Achados), sum(1 for _, _, V in Linhas if not V)))
    return 0 if all(V for _, _, V in Linhas) else 1


if __name__ == "__main__":
    sys.exit(Principal())
