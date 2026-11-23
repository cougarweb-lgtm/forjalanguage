"""Casos de cli do forja spec e do forja explicar --lista / --exemplos (IA1-b09), mais o campo spec de todo diagnostico.

Importado pelo motor verificar_cli.py; as funcoes comuns vem de la.

Regras:
  - spec --cartao e spec (sem opcao) devolvem os bytes de language\\doc\\cartao.txt e spec.txt (o texto vai embutido no forja.exe).
  - spec --json: {versao, secoes: [{id, titulo, texto}]} com a versao de --versao; --secao filtra; secao desconhecida, opcao desconhecida e codigo sem exemplo = exit 2.
  - explicar --lista [--json] lista todos os codigos (codigo, titulo, etapa, spec); explicar [CODIGO] --exemplos devolve {codigo, exemplos: [...]}.
  - todo diagnostico leva "spec" na 1a vez do codigo e "spec_ref" nas seguintes.
  - a secao RENOMEADOS da migracao.txt sai da tabela de renomeados (common\\renomeados.c) por benchmark\\ia_first\\gerar_migracao_doc.py: arquivo e binario batem com a tabela (IA1-L3a).

Versao: 1.2.1
Changelog: 1.2.1 - E4: GERADOR_MIGRACAO aponta para benchmark\\ia_first (pasta movida).
           1.2.0 - IA1-L3d1: CasoSpecCitaMigrar (a secao cli do cartao e da spec e a mensagem de uso citam o forja migrar): 9 casos.
           1.1.0 - IA1-L3a: CasoMigracaoRenomeados (gerar_migracao_doc --verificar = igual; forja spec --secao migracao termina na secao gerada da tabela): 8 casos.
           1.0.0 - IA1-b09: primeira versao (7 casos).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json
import subprocess
import sys

from verificar_cli import ConferirExecucao, LinhaJson, PastaAqui, Rodar

DOC = PastaAqui.parent.parent / "doc"
GERADOR_MIGRACAO = PastaAqui.parent.parent / "benchmark" / "ia_first" / "gerar_migracao_doc.py"
TEMAS = "formato declarar proc fluxo erro memoria colecoes texto foreign modulos testes saida cli".split()


def RodarBytes(Forja, Argumentos):
    """Roda o forja e devolve o CompletedProcess com a saida em bytes (para comparar byte a byte)."""
    return subprocess.run([str(Forja)] + Argumentos, capture_output=True, timeout=60, cwd=PastaAqui)


def CasoSpecBytes(Forja, PastaSaidas):
    """spec --cartao == cartao.txt e spec == spec.txt, byte a byte (M-U1); migracao == migracao.txt."""
    Problemas = []
    for Argumentos, Arquivo in ((["spec", "--cartao"], "cartao.txt"), (["spec"], "spec.txt"), (["spec", "--secao", "migracao"], "migracao.txt")):
        Rodada = RodarBytes(Forja, Argumentos)
        if Rodada.returncode != 0 or Rodada.stdout != (DOC / Arquivo).read_bytes():
            Problemas.append(f"forja {' '.join(Argumentos)} difere de {Arquivo} (exit {Rodada.returncode}, {len(Rodada.stdout)} bytes)")
    return Problemas


def CasoMigracaoRenomeados(Forja, PastaSaidas):
    """gerar_migracao_doc --verificar diz 'igual' (migracao.txt gravada == tabela) e `forja spec --secao migracao` (o binario) termina com a secao gerada da tabela."""
    Problemas = []
    Base = [sys.executable, "-I", str(GERADOR_MIGRACAO)]
    Verificar = subprocess.run(Base + ["--verificar"], capture_output=True, timeout=60, text=True)
    if Verificar.returncode != 0 or Verificar.stdout.strip() != "igual":
        Problemas.append(f"gerar_migracao_doc --verificar: exit {Verificar.returncode}, saida {Verificar.stdout.strip()!r} (tabela e migracao.txt divergem: rode o gerador)")
    Secao = subprocess.run(Base + ["--secao"], capture_output=True, timeout=60, text=True)
    Embutida = RodarBytes(Forja, ["spec", "--secao", "migracao"])
    if Secao.returncode != 0 or not Secao.stdout or not Embutida.stdout.endswith(Secao.stdout.encode("utf-8")):
        Problemas.append("forja spec --secao migracao nao termina na secao RENOMEADOS gerada da tabela (reconstrua o forja.exe depois de regerar a migracao.txt)")
    return Problemas


def CasoSpecCitaMigrar(Forja, PastaSaidas):
    """IA1-L3d1: a secao cli do cartao e da spec (o que o forja devolve) cita `migrar`, e a mensagem de uso do forja lista `forja migrar`."""
    Problemas = []
    for Argumentos in (["spec", "--cartao"], ["spec", "--secao", "cli"]):
        Rodada = RodarBytes(Forja, Argumentos)
        if Rodada.returncode != 0 or b"migrar" not in Rodada.stdout:
            Problemas.append(f"forja {' '.join(Argumentos)} nao cita migrar (exit {Rodada.returncode})")
    Uso = Rodar(Forja, [])
    if "forja migrar" not in Uso.stderr:
        Problemas.append("a mensagem de uso do forja nao lista 'forja migrar'")
    return Problemas


def CasoSpecJson(Forja, PastaSaidas):
    """spec --json: versao igual a --versao, as 13 secoes por tema (+ migracao) com id, titulo e texto; --cartao --json e --secao filtram."""
    Versao = Rodar(Forja, ["--versao"]).stdout.strip()
    Rodada = Rodar(Forja, ["spec", "--json"])
    Problemas = ConferirExecucao(Rodada, 0)
    Dados = LinhaJson(Rodada.stdout)
    if Dados is None or Dados.get("versao") != Versao:
        return Problemas + [f"spec --json sem JSON de uma linha com a versao {Versao!r}: {Rodada.stdout.strip()[:100]!r}"]
    Ids = [Item.get("id") for Item in Dados.get("secoes", [])]
    if Ids != TEMAS + ["migracao"] or any(not Item.get("titulo") or not Item.get("texto") for Item in Dados["secoes"]):
        Problemas.append(f"spec --json com secoes {Ids!r}")
    Cartao = LinhaJson(Rodar(Forja, ["spec", "--cartao", "--json"]).stdout)
    TemasCartao = TEMAS[:-1] + ["alvos", "cli"]  # 0.47.0: o cartao ganhou a secao alvos antes de cli; a spec segue com 13
    if Cartao is None or [Item["id"] for Item in Cartao["secoes"]] != TemasCartao:
        Problemas.append("spec --cartao --json sem as 14 secoes por tema (13 + alvos)")
    Uma = LinhaJson(Rodar(Forja, ["spec", "--secao", "proc", "--json"]).stdout)
    if Uma is None or [Item["id"] for Item in Uma["secoes"]] != ["proc"]:
        Problemas.append("spec --secao proc --json sem so a secao proc")
    return Problemas


def CasoSpecSecaoEUso(Forja, PastaSaidas):
    """--secao proc imprime a secao; secao desconhecida, opcao desconhecida e codigo desconhecido dao exit 2."""
    Rodada = Rodar(Forja, ["spec", "--secao", "proc"])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Rodada.stdout.startswith("== proc ==") or "== fluxo ==" in Rodada.stdout:
        Problemas.append(f"spec --secao proc nao imprime so a secao proc: {Rodada.stdout[:60]!r}")
    for Argumentos in (["spec", "--secao", "nao_existe"], ["spec", "--foo"], ["spec", "E9999"], ["spec", "--secao"], ["spec", "--exemplo", "E0707", "--cartao"]):
        Problemas += [f"{' '.join(Argumentos)}: {P}" for P in ConferirExecucao(Rodar(Forja, Argumentos), 2)]
    return Problemas


def CasoSpecCodigoEExemplo(Forja, PastaSaidas):
    """spec CODIGO traz o trecho da secao do mapa; spec --exemplo E0707 --json traz errado e certo da bateria; codigo sem exemplo = exit 2."""
    Rodada = Rodar(Forja, ["spec", "E0502"])
    Problemas = ConferirExecucao(Rodada, 0)
    if not Rodada.stdout.startswith("proc: ") or "E0502" not in Rodada.stdout:
        Problemas.append(f"spec E0502: {Rodada.stdout[:100]!r}")
    Exemplo = LinhaJson(Rodar(Forja, ["spec", "--exemplo", "E0707", "--json"]).stdout)
    if Exemplo is None or Exemplo.get("codigo") != "E0707" or not Exemplo.get("errado") or not Exemplo.get("certo"):
        Problemas.append(f"spec --exemplo E0707 --json sem errado e certo: {Exemplo!r}")
    elif not any("var " in Linha for Linha in Exemplo["errado"]) or any("var " in Linha for Linha in Exemplo["certo"]):
        Problemas.append("o par do E0707 nao troca var por let")
    Problemas += [f"spec --exemplo R0001: {P}" for P in ConferirExecucao(Rodar(Forja, ["spec", "--exemplo", "R0001"]), 2)]
    return Problemas


def CasoExplicarLista(Forja, PastaSaidas):
    """explicar --lista --json: array com codigo, titulo, etapa e spec de todo codigo; sem --json uma linha por codigo; combinar com --exemplos = exit 2."""
    Rodada = Rodar(Forja, ["explicar", "--lista", "--json"])
    Problemas = ConferirExecucao(Rodada, 0)
    try:
        Lista = json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        Lista = None
    if not isinstance(Lista, list) or not Lista or any(sorted(Item) != ["codigo", "etapa", "spec", "titulo"] for Item in Lista):
        return Problemas + [f"explicar --lista --json sem a lista de 4 campos: {Rodada.stdout[:100]!r}"]
    if any(Item["spec"] not in TEMAS or not Item["titulo"] for Item in Lista):
        Problemas.append("codigo da lista sem secao existente ou sem titulo")
    Texto = Rodar(Forja, ["explicar", "--lista"])
    if len(Texto.stdout.splitlines()) != len(Lista) or Texto.stdout.splitlines()[0].split("\t")[0] != Lista[0]["codigo"]:
        Problemas.append("explicar --lista (texto) nao tem uma linha por codigo")
    Problemas += [f"--lista --exemplos: {P}" for P in ConferirExecucao(Rodar(Forja, ["explicar", "--lista", "--exemplos"]), 2)]
    return Problemas


def CasoExplicarExemplos(Forja, PastaSaidas):
    """explicar E0707 --exemplos: 1 exemplo da bateria; R0001 (sem fixture err_): lista vazia; sem CODIGO: 1 linha por codigo; codigo desconhecido = exit 2."""
    Um = LinhaJson(Rodar(Forja, ["explicar", "E0707", "--exemplos"]).stdout)
    Problemas = []
    if Um is None or Um.get("codigo") != "E0707" or len(Um.get("exemplos", [])) != 1 or not Um["exemplos"][0].get("fixture", "").startswith("tests/"):
        Problemas.append(f"explicar E0707 --exemplos: {Um!r}")
    Vazio = LinhaJson(Rodar(Forja, ["explicar", "R0001", "--exemplos"]).stdout)
    if Vazio is None or Vazio.get("exemplos") != []:
        Problemas.append(f"explicar R0001 --exemplos deveria dar lista vazia: {Vazio!r}")
    Todos = Rodar(Forja, ["explicar", "--exemplos"]).stdout.splitlines()
    Catalogo = json.loads(Rodar(Forja, ["explicar", "--lista", "--json"]).stdout)
    if len(Todos) != len(Catalogo) or sum(1 for Linha in Todos if json.loads(Linha)["exemplos"]) < 50:
        Problemas.append(f"explicar --exemplos: {len(Todos)} linhas para {len(Catalogo)} codigos")
    Problemas += [f"explicar E9999 --exemplos: {P}" for P in ConferirExecucao(Rodar(Forja, ["explicar", "E9999", "--exemplos"]), 2)]
    return Problemas


def CasoDiagnosticoComSpec(Forja, PastaSaidas):
    """multi_erros.fj (E0301, E0201, E0301): a 1a vez de cada codigo traz spec (secao: trecho), a 2a do E0301 traz spec_ref."""
    Rodada = Rodar(Forja, ["check", "multi_erros.fj"])
    Problemas = ConferirExecucao(Rodada, 1)
    Linhas = [json.loads(Linha) for Linha in Rodada.stdout.splitlines() if Linha.strip()]
    Campos = [("spec" in Item, "spec_ref" in Item) for Item in Linhas]
    if len(Linhas) != 3 or Campos != [(True, False), (True, False), (False, True)]:
        return Problemas + [f"multi_erros.fj: campos spec/spec_ref {Campos!r}"]
    if not Linhas[0]["spec"].startswith("declarar: ") or not Linhas[1]["spec"].startswith("declarar: ") or Linhas[2]["spec_ref"] != "forja explicar E0301":
        Problemas.append(f"spec dos diagnosticos: {[Item.get('spec', Item.get('spec_ref'))[:40] for Item in Linhas]!r}")
    Resumo = Rodar(Forja, ["check", "--resumo", "multi_erros.fj"])
    if "spec" in Resumo.stdout:
        Problemas.append("--resumo nao leva o spec")
    return Problemas
