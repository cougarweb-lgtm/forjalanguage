"""Casos de cli da higiene (v1.11 L10a): explicar E0707, E0708, E0709 e E1005.

Importado pelo motor verificar_cli.py; as funcoes comuns vem de la.

Regras:
  - explicar devolve 1 linha JSON com o codigo, o tipo (fluxo nos tres E07xx, modulo no E1005), a explicacao e a forma_certa do contrato.

Versao: 1.4.0
Changelog: 1.4.0 - 0.21.0 F3: CasoImportRenomeadoOuReservado sem a perna --backend c (check e build x64).
           1.3.0 - IA1-b05: CasoCorrigirDiffEAplicar e CasoCorrigirUsoErrado.
           1.2.0 - IA1-b01: CasoExplicarE1006.
           1.1.0 - Linguagem v1.11 L10a-bis: CasoExplicarE0708Sublinhado, CasoExplicarE0709DescarteTry e CasoExplicarE0601DescarteTry.
           1.0.0 - Linguagem v1.11 L10a: CasoExplicarE0707, CasoExplicarE0708, CasoExplicarE0709 e CasoExplicarE1005.
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

from verificar_cli import ConferirExecucao, LinhaJson, Rodar


def ConferirExplicar(Forja, Codigo, Tipo, TrechoExplicacao, TrechoForma):
    Rodada = Rodar(Forja, ["explicar", Codigo])
    Problemas = ConferirExecucao(Rodada, 0)
    Dados = LinhaJson(Rodada.stdout)
    if Dados is None or Dados.get("codigo") != Codigo or Dados.get("tipo") != Tipo:
        return Problemas + [f"explicar {Codigo} sem JSON com codigo e tipo {Tipo}: {Rodada.stdout.strip()[:120]!r}"]
    if TrechoExplicacao not in Dados.get("explicacao", "") or TrechoForma not in Dados.get("forma_certa", ""):
        Problemas.append(f"explicar {Codigo} sem explicacao e forma_certa do contrato: {Rodada.stdout.strip()[:160]!r}")
    return Problemas


def CasoExplicarE0707(Forja, PastaSaidas):
    """explicar E0707: var que nunca muda deve ser let."""
    return ConferirExplicar(Forja, "E0707", "fluxo", "deve ser let", "troque var por let")


def CasoExplicarE0708(Forja, PastaSaidas):
    """explicar E0708: let/var que nunca e lido."""
    return ConferirExplicar(Forja, "E0708", "fluxo", "nunca e lido", "_ = registrar(x) para descartar um retorno")


def CasoExplicarE0709(Forja, PastaSaidas):
    """explicar E0709: o retorno de um proc nao some em silencio."""
    return ConferirExplicar(Forja, "E0709", "fluxo", "nao some em silencio", "_ = puts(")


def CasoExplicarE0708Sublinhado(Forja, PastaSaidas):
    """explicar E0708 (L10a-bis): o nome com sublinhado na frente fica isento."""
    return ConferirExplicar(Forja, "E0708", "fluxo", "(_r, _unused)", "let _r = f() para guardar sem ler")


def CasoExplicarE0709DescarteTry(Forja, PastaSaidas):
    """explicar E0709 (L10a-bis): '_ = try f()' descarta o payload de T ! E."""
    return ConferirExplicar(Forja, "E0709", "fluxo", "'_ = try f()'", "_ = try ler(f)")


def CasoExplicarE0601DescarteTry(Forja, PastaSaidas):
    """explicar E0601 (L10a-bis): a forma certa cita '_ = try f()'."""
    return ConferirExplicar(Forja, "E0601", "resultado", "erro precisa ser tratado", "_ = try f() (repassa o erro e descarta o valor)")


def CasoExplicarE1005(Forja, PastaSaidas):
    """explicar E1005: import cujo apelido nunca aparece."""
    return ConferirExplicar(Forja, "E1005", "modulo", "apelido nunca aparece", "remova a linha do import")


def CasoImportRenomeadoOuReservado(Forja, PastaSaidas):
    """IA1-b01: import "std:proc" = E1001 com std:os na causa e import "x/proc" sem as = E1006 na linha 1; igual no check e no build x64."""
    Casos = (("std_proc", 'import "std:proc"\n', "E1001", "std:os"), ("x_proc", 'import "x/proc"\n', "E1006", "as <nome>"))
    Problemas = []
    for Nome, Fonte, Codigo, Trecho in Casos:
        Arquivo = PastaSaidas / f"import_{Nome}.fj"
        Arquivo.write_text(Fonte + "\nproc main() {\n}\n", encoding="utf-8", newline="\n")
        for Comando in (["check"], ["build", "--backend", "x64", "-o", str(PastaSaidas / f"{Nome}_x.exe")]):
            Rodada = Rodar(Forja, Comando[:1] + [str(Arquivo)] + Comando[1:])
            Dados = LinhaJson(Rodada.stdout)
            Problemas += ConferirExecucao(Rodada, 1)
            if Dados is None or Dados.get("codigo") != Codigo or Dados.get("linha") != 1 or Trecho not in Dados.get("causa", ""):
                Problemas.append(f"{' '.join(Comando)} de {Nome}: esperava {Codigo} na linha 1 com {Trecho!r} na causa: {Rodada.stdout.strip()[:200]!r}")
    return Problemas


def CasoExplicarE1006(Forja, PastaSaidas):
    """explicar E1006 (IA1-b01): import cujo ultimo segmento e palavra reservada, sem 'as'."""
    return ConferirExplicar(Forja, "E1006", "modulo", "palavra reservada", "as <nome>")


def CasoCorrigirDiffEAplicar(Forja, PastaSaidas):
    """IA1-b05: forja corrigir com caminho absoluto: sem --aplicar devolve o diff e nao grava; com --aplicar troca var por let e o check passa."""
    Arquivo = PastaSaidas / "corrigir_var.fj"
    Fonte = "proc main() {\n    var n = 3\n    print(n)\n}\n"
    Arquivo.write_text(Fonte, encoding="utf-8", newline="\n")
    Seco = Rodar(Forja, ["corrigir", str(Arquivo)])
    Dados = LinhaJson(Seco.stdout)
    Problemas = ConferirExecucao(Seco, 0)
    if Dados is None or Dados.get("edicoes") != 1 or "-    var n = 3" not in Dados.get("diff", "") or Dados.get("aplicado") is not False:
        Problemas.append(f"sem --aplicar: esperava 1 edicao e o diff com var: {Seco.stdout.strip()[:200]!r}")
    if Arquivo.read_text(encoding="utf-8") != Fonte:
        Problemas.append("sem --aplicar o arquivo mudou")
    Aplicado = Rodar(Forja, ["corrigir", str(Arquivo), "--aplicar"])
    Dados = LinhaJson(Aplicado.stdout)
    Problemas += ConferirExecucao(Aplicado, 0)
    if Dados is None or Dados.get("aplicado") is not True or Dados.get("erros_depois") != 0 or Arquivo.read_text(encoding="utf-8") != Fonte.replace("var", "let"):
        Problemas.append(f"com --aplicar: esperava aplicado e 0 erros: {Aplicado.stdout.strip()[:200]!r}")
    Problemas += ConferirExecucao(Rodar(Forja, ["check", str(Arquivo)]), 0)
    return Problemas


def CasoCorrigirUsoErrado(Forja, PastaSaidas):
    """IA1-b05: arquivo inexistente, opcao desconhecida e codigo desconhecido = exit 2, nada no stdout."""
    Problemas = []
    for Argumentos in (["corrigir", str(PastaSaidas / "nao_existe.fj")], ["corrigir", "--foo"], ["corrigir", "x.fj", "--codigo", "E9999"]):
        Rodada = Rodar(Forja, Argumentos)
        Problemas += ConferirExecucao(Rodada, 2)
        if Rodada.stdout or not Rodada.stderr.startswith("forja:"):
            Problemas.append(f"{Argumentos}: stdout {Rodada.stdout!r}, stderr {Rodada.stderr[:60]!r}")
    return Problemas
