# verificar_forma_b07.py -- mede a forma por ocorrencia do E0101 e do E0301 nos err_* (contrato IA1, P-IA1-b07, M-D3 parcial)
"""Roda `forja check` em todos os err_*.fj (sintaxe, semantica, dourado, regressao) e olha as ocorrencias de E0101 e E0301:
  M-D3 catalogo     <= 40% das ocorrencias com a explicacao OU a forma_certa igual ao texto do catalogo (`forja explicar`), por codigo
  M-D3 identicas    <= 40% das ocorrencias no maior grupo de textos identicos entre si (explicacao + forma_certa), por codigo
  M-D3 nomes        0 ocorrencia cuja explicacao/forma_certa cita entre aspas um nome que nao esta no arquivo, na causa nem na lista de palavras da Forja (ex.: Grifo)
  catalogo limpo    o texto do catalogo do E0101 e do E0301 nao traz exemplo com nome inventado (lista NOMES_PROIBIDOS_CATALOGO)
  campos            toda ocorrencia traz explicacao e forma_certa nao vazias
Uso:
    python -I verificar_forma_b07.py --forja <forja.exe>          (exit 0 so se tudo verde; 1 = alguma vermelha; 2 = uso)
    python -I verificar_forma_b07.py --selftest                   (prova o discriminador: texto identico, texto do catalogo e nome inventado ficam vermelhos)
Contra o compilador do b06 (sem forma por ocorrencia) este verificador sai vermelho.
Versao: 0.1.0
Changelog: 0.1.0 - P-IA1-b07: primeira versao (reaproveita Fixtures, Executar e LinhasJson do sondas_compilador.py).
Verificacao: python -I verificar_forma_b07.py --selftest
"""

import argparse
import collections
import json
import re
import sys
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sondas_compilador as SC  # noqa: E402  Fixtures, Executar, LinhasJson (mesma pasta)

CODIGOS = ("E0101", "E0301")
LIMITE_PCT = 40.0
NOMES_PROIBIDOS_CATALOGO = ("Grifo", "n as f32", "let x", "1.5 as i32", "x: f32", "(n as")
# palavras que a Forja define (palavras-chave, tipos, embutidos) e que a forma pode citar sem estarem no arquivo
PALAVRAS_DA_FORJA = set("""as or_else try print let var return int bool str cstr rawptr f32 f64 i8 i16 i32 i64 u8 u16 u32 u64 uint true false none and or not if else for in while
switch case default break continue defer proc struct enum union unit pub export import foreign link test expect const new make delete append len assert panic format std
cond msg""".split())  # cond e msg: parametros dos embutidos assert(cond, msg) e panic(msg), que a forma cita ao dizer o argumento da chamada
ASPAS = re.compile(r"'([^']*)'")
IDENTIFICADOR = re.compile(r"[A-Za-z_]\w*")


def Percentual(Parte, Total):
    return round(100.0 * Parte / Total, 1) if Total else 0.0


def Catalogo(Forja, Codigo):
    """(explicacao, forma_certa) do `forja explicar <Codigo>`."""
    Objetos, _ = SC.LinhasJson(SC.Executar([str(Forja), "explicar", Codigo], Path(Forja).parent)[1])
    return (Objetos[0].get("explicacao"), Objetos[0].get("forma_certa")) if Objetos else (None, None)


def Ocorrencias(Forja):
    """[(arquivo .fj, texto do arquivo, diagnostico)] de todo E0101/E0301 dos err_*."""
    with ThreadPoolExecutor(max_workers=8) as Pool:
        Rodadas = list(Pool.map(partial(RodarCheck, str(Forja)), SC.Fixtures("err_")))
    return [O for Rodada in Rodadas for O in Rodada]


def RodarCheck(Forja, Arquivo):
    _, Saida, Erro = SC.Executar([Forja, "check", Arquivo.name], Arquivo.parent)
    Objetos, _ = SC.LinhasJson(Saida + "\n" + Erro)
    Texto = TextoComIrmaos(Arquivo)
    return [(Arquivo, Texto, D) for D in Objetos if D.get("codigo") in CODIGOS]


def TextoComIrmaos(Arquivo):
    """O fonte do caso mais o dos .fj da mesma pasta que ele importa pelo nome (casos multi-modulo), para saber se um nome existe no projeto do caso."""
    Texto = Arquivo.read_text(encoding="utf-8", errors="replace")
    for Importado in re.findall(r'import\s+"([^"]+)"', Texto):
        Irmao = Arquivo.parent / (Importado if Importado.endswith(".fj") else Importado + ".fj")
        if Irmao.is_file():
            Texto += "\n" + Irmao.read_text(encoding="utf-8", errors="replace")
    return Texto


def NomesInventados(Texto, Diag):
    """Palavras entre aspas da explicacao/forma_certa que nao estao no arquivo, na causa nem na lista da Forja."""
    Conhecido = set(IDENTIFICADOR.findall(Texto)) | set(IDENTIFICADOR.findall(Diag.get("causa", ""))) | PALAVRAS_DA_FORJA
    Achados = []
    for Campo in ("explicacao", "forma_certa"):
        for Trecho in ASPAS.findall(Diag.get(Campo, "")):
            Achados += [P for P in IDENTIFICADOR.findall(Trecho) if P not in Conhecido]
    return Achados


def MedirCodigo(Diags, TextoCatalogo):
    """(pct igual ao catalogo, pct do maior grupo identico) de uma lista de diagnosticos do mesmo codigo."""
    Total = len(Diags)
    IguaisAoCatalogo = sum(1 for D in Diags if (D.get("explicacao"), D.get("forma_certa")) != (None, None)
                           and (D.get("explicacao") == TextoCatalogo[0] or D.get("forma_certa") == TextoCatalogo[1]))
    Grupos = collections.Counter((D.get("explicacao"), D.get("forma_certa")) for D in Diags)
    return Percentual(IguaisAoCatalogo, Total), Percentual(max(Grupos.values()) if Grupos else 0, Total)


def Resumir(Achados, Catalogos):
    """Linhas (id, valor, verde) a partir das ocorrencias [(arquivo, texto, diag)] e dos catalogos {codigo: (explicacao, forma)}."""
    Linhas = []
    for Codigo in CODIGOS:
        Diags = [D for _, _, D in Achados if D["codigo"] == Codigo]
        PctCatalogo, PctGrupo = MedirCodigo(Diags, Catalogos.get(Codigo, (None, None)))
        Linhas.append(("M-D3 %s catalogo" % Codigo, {"ocorrencias": len(Diags), "pct_igual_ao_catalogo": PctCatalogo}, bool(Diags) and PctCatalogo <= LIMITE_PCT))
        Linhas.append(("M-D3 %s identicas" % Codigo, {"ocorrencias": len(Diags), "pct_maior_grupo_identico": PctGrupo}, bool(Diags) and PctGrupo <= LIMITE_PCT))
    Inventados = [(A.name, D.get("linha"), NomesInventados(T, D)) for A, T, D in Achados if NomesInventados(T, D)]
    Linhas.append(("M-D3 nomes", {"ocorrencias_com_nome_inventado": len(Inventados), "exemplos": Inventados[:3]}, not Inventados))
    Sujos = [C for C, Textos in Catalogos.items() for T in Textos if T and any(N in T for N in NOMES_PROIBIDOS_CATALOGO)]
    Linhas.append(("catalogo limpo", {"codigos_com_exemplo_inventado": sorted(set(Sujos))}, not Sujos))
    Vazios = [(A.name, D.get("linha")) for A, _, D in Achados if not D.get("explicacao") or not D.get("forma_certa")]
    Linhas.append(("campos", {"sem_explicacao_ou_forma": len(Vazios)}, not Vazios))
    return Linhas


def Diag(Codigo, Explicacao, Forma, Causa="c"):
    return {"codigo": Codigo, "linha": 1, "causa": Causa, "explicacao": Explicacao, "forma_certa": Forma}


def Selftest():
    """O discriminador acusa o que deve: tudo igual ao catalogo, tudo identico, nome inventado, catalogo com exemplo; e aceita texto variado."""
    Catalogos = {"E0101": ("expl catalogo", "forma catalogo"), "E0301": ("expl cat 2", "forma cat 2")}
    Fonte = "proc main() {\n    let total = 1\n}\n"
    Arq = Path("a.fj")

    def Cenario(Fabrica, Cats=Catalogos):
        Achados = [(Arq, Fonte, Fabrica(C, I)) for C in CODIGOS for I in range(10)]
        return {Id: V for Id, _, V in Resumir(Achados, Cats)}

    Bom = Cenario(lambda C, I: Diag(C, "na linha %d 'total' pede" % I, "troque 'total' %d" % I))
    Catalogal = Cenario(lambda C, I: Diag(C, Catalogos[C][0], Catalogos[C][1]))
    Identico = Cenario(lambda C, I: Diag(C, "mesma explicacao", "mesma forma"))
    Inventado = Cenario(lambda C, I: Diag(C, "na linha %d 'Grifo' pede" % I, "troque 'Grifo' %d" % I))
    Sujo = Cenario(lambda C, I: Diag(C, "na linha %d 'total'" % I, "forma %d" % I), {"E0101": ("a", "use (n as f32)"), "E0301": ("b", "c")})
    Vazio = Cenario(lambda C, I: Diag(C, "", "forma %d" % I))
    Provas = [
        ("bom passa tudo", all(Bom.values())),
        ("catalogo acusado", not Catalogal["M-D3 E0101 catalogo"] and not Catalogal["M-D3 E0301 catalogo"]),
        ("identico acusado", not Identico["M-D3 E0101 identicas"] and not Identico["M-D3 E0301 identicas"]),
        ("nome inventado acusado", not Inventado["M-D3 nomes"]),
        ("exemplo no catalogo acusado", not Sujo["catalogo limpo"]),
        ("campo vazio acusado", not Vazio["campos"]),
        ("sem ocorrencias nao fica verde", not Resumir([], Catalogos)[0][2]),
    ]
    Falhas = [Nome for Nome, Passou in Provas if not Passou]
    for Nome in Falhas:
        print("selftest: falhou: " + Nome)
    print("selftest: %s" % ("ok" if not Falhas else "%d falha(s)" % len(Falhas)))
    return len(Falhas)


def Principal():
    Parser = argparse.ArgumentParser(description="Forma por ocorrencia do E0101 e do E0301 nos err_* (P-IA1-b07, M-D3 parcial).")
    Parser.add_argument("--forja")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    if Args.selftest:
        return 1 if Selftest() else 0
    if not Args.forja or not Path(Args.forja).is_file():
        Parser.error("--forja <forja.exe> e obrigatorio")
    Forja = Path(Args.forja).resolve()
    Achados = Ocorrencias(Forja)
    Linhas = Resumir(Achados, {C: Catalogo(Forja, C) for C in CODIGOS})
    for Id, Valor, Verde in Linhas:
        print(json.dumps({"id": Id, "valor": Valor, "verde": Verde}, ensure_ascii=False))
    print("ocorrencias=%d falhas=%d" % (len(Achados), sum(1 for _, _, V in Linhas if not V)))
    return 0 if all(V for _, _, V in Linhas) else 1


if __name__ == "__main__":
    sys.exit(Principal())
