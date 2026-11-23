"""Casos de cli do E0110 (forma de outra linguagem, IA1-L1).

Importado pelo motor verificar_cli.py; as funcoes comuns vem de la.

Regras:
  - a mesma fixture de idioma da o mesmo E0110 (codigo, linha, coluna) no check e no build x64 (M-D5): o idioma e recusado antes de qualquer backend.
  - `forja corrigir --aplicar` troca '&&' por 'and' numa copia e o check do resultado sai 0.
  - explicar E0110 e spec E0110 (secao formato) existem; o ';' e os demais idiomas de tests\\sintaxe\\err_E0110_* tem 1a linha E0110.

Versao: 1.2.0
Changelog: 1.2.0 - 0.21.0 F3: o build do E0110 roda so no x64 (a perna --backend c saiu).
           1.1.0 - O0 P13: CasoIdiomaXorSoComOperando ('a ^ b' = E0110 com '~'; p^.campo, pm^[i], pn^ + 1 e x^ sozinho como antes).
           1.0.0 - IA1-L1: primeira versao (3 casos).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json
import shutil

from verificar_cli import ConferirExecucao, LinhaJson, PastaAqui, Rodar

SINTAXE = PastaAqui.parent / "sintaxe"


def PrimeiroDiagnostico(Texto):
    for Linha in Texto.splitlines():
        try:
            Dados = json.loads(Linha)
        except json.JSONDecodeError:
            continue
        if isinstance(Dados, dict) and "codigo" in Dados:
            return Dados
    return None


def CasoIdiomaMesmoDiagnosticoNosBackends(Forja, PastaSaidas):
    """err_E0110_e_logico: check e build x64 dao o mesmo E0110 (linha, coluna) e a correcao 'and'; exit 1 nos dois."""
    Fonte = str(SINTAXE / "err_E0110_e_logico.fj")
    Check = Rodar(Forja, ["check", Fonte])
    Problemas = ConferirExecucao(Check, 1)
    Base = PrimeiroDiagnostico(Check.stdout)
    if Base is None or Base["codigo"] != "E0110" or [E.get("novo") for E in Base.get("correcao", [])] != ["and"] or Base.get("confianca") != "certa":
        return Problemas + [f"check: sem E0110 com correcao certa 'and': {Check.stdout[:200]!r}"]
    for Backend in ("x64",):
        Saida = PastaSaidas / f"idioma_{Backend}.exe"
        Rodada = Rodar(Forja, ["build", "--backend", Backend, Fonte, "-o", str(Saida)])
        Problemas += [f"build {Backend}: {P}" for P in ConferirExecucao(Rodada, 1)]
        Dados = PrimeiroDiagnostico(Rodada.stdout + Rodada.stderr)
        if Dados is None or [Dados.get(C) for C in ("codigo", "linha", "coluna")] != [Base[C] for C in ("codigo", "linha", "coluna")]:
            Problemas.append(f"build {Backend}: diagnostico diferente do check: {Dados!r:.200}")
        if Saida.exists():
            Problemas.append(f"build {Backend} gerou {Saida.name} apesar do E0110")
    return Problemas


def CasoIdiomaCorrigirAplica(Forja, PastaSaidas):
    """forja corrigir --aplicar numa copia de err_E0110_e_logico.fj troca '&&' por 'and' e o check do resultado sai 0; sem --aplicar nada e gravado."""
    Copia = PastaSaidas / "idioma_corrigir.fj"
    shutil.copyfile(SINTAXE / "err_E0110_e_logico.fj", Copia)
    Original = Copia.read_bytes()
    Problemas = ConferirExecucao(Rodar(Forja, ["corrigir", str(Copia)]), 0)
    if Copia.read_bytes() != Original:
        Problemas.append("corrigir sem --aplicar gravou o arquivo")
    Problemas += [f"corrigir --aplicar: {P}" for P in ConferirExecucao(Rodar(Forja, ["corrigir", str(Copia), "--aplicar"]), 0)]
    Texto = Copia.read_text(encoding="utf-8")
    if "&&" in Texto or " and " not in Texto:
        Problemas.append(f"corrigir --aplicar nao trocou '&&' por 'and': {Texto!r:.200}")
    Problemas += [f"check depois do corrigir: {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Copia)]), 0)]
    return Problemas


def CasoIdiomaXorSoComOperando(Forja, PastaSaidas):
    """O0 P13: 'a ^ b' = E0110 com a troca por '~'; p^.campo, pm^[i] e pn^ + 1 seguem como deref (check exit 0) e 'x^' sozinho segue E0306."""
    Positivo = Rodar(Forja, ["check", str(SINTAXE / "err_E0110_xor_circunflexo.fj")])
    Problemas = ConferirExecucao(Positivo, 1)
    Dados = PrimeiroDiagnostico(Positivo.stdout)
    if Dados is None or Dados["codigo"] != "E0110" or [E.get("novo") for E in Dados.get("correcao", [])] != ["~"] or "a ~ b" not in Dados.get("causa", ""):
        Problemas.append(f"a ^ b: sem E0110 com a troca por '~': {Positivo.stdout[:200]!r}")
    Negativo = PastaSaidas / "xor_deref.fj"
    Negativo.write_text("struct P {\n    v: int = 1\n}\nproc main() {\n    var q = P{}\n    let p = &q\n    let a = p^.v\n    var m: [2]int = {1, 2}\n    let pm = &m\n"
                        "    let c = pm^[1]\n    var n = 4\n    let pn = &n\n    let d = pn^ + 1\n    print(a + c + d)\n}\n", encoding="utf-8")
    Problemas += [f"p^.campo, pm^[i], pn^ + 1: {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Negativo)]), 0)]
    Sozinho = PastaSaidas / "xor_sozinho.fj"
    Sozinho.write_text("proc main() {\n    let x: i32 = 1\n    let y = x^\n}\n", encoding="utf-8")
    Rodada = Rodar(Forja, ["check", str(Sozinho)])
    Dados = PrimeiroDiagnostico(Rodada.stdout)
    if Dados is None or Dados["codigo"] != "E0306":
        Problemas.append(f"x^ sozinho deixou de ser E0306: {Rodada.stdout[:200]!r}")
    return Problemas


def CasoIdiomaCatalogoESpec(Forja, PastaSaidas):
    """explicar E0110 e spec E0110 (secao formato); todo err_E0110_* de tests\\sintaxe tem 1o diagnostico E0110 (a linha esperada vem do .esperado.jsonl)."""
    Explicar = LinhaJson(Rodar(Forja, ["explicar", "E0110"]).stdout)
    Problemas = []
    if Explicar is None or Explicar.get("codigo") != "E0110" or "outra linguagem" not in Explicar.get("explicacao", ""):
        Problemas.append(f"explicar E0110: {Explicar!r:.200}")
    Spec = Rodar(Forja, ["spec", "E0110"])
    Problemas += [f"spec E0110: {P}" for P in ConferirExecucao(Spec, 0)]
    if not Spec.stdout.startswith("formato: ") or "E0110" not in Spec.stdout:
        Problemas.append(f"spec E0110 nao traz a secao formato: {Spec.stdout[:100]!r}")
    Fixtures = sorted(SINTAXE.glob("err_E0110_*.fj"))
    if len(Fixtures) < 14:
        Problemas.append(f"so {len(Fixtures)} fixtures err_E0110_* (minimo 14)")
    for Fixture in Fixtures:
        Esperado = json.loads(Fixture.with_suffix(".esperado.jsonl").read_text(encoding="utf-8").splitlines()[0])
        Dados = PrimeiroDiagnostico(Rodar(Forja, ["check", str(Fixture)]).stdout)
        if Dados is None or (Dados["codigo"], Dados["linha"], Dados["coluna"]) != (Esperado["codigo"], Esperado["linha"], Esperado["coluna"]):
            Problemas.append(f"{Fixture.name}: {Dados and (Dados['codigo'], Dados['linha'], Dados['coluna'])} != {Esperado}")
    return Problemas
