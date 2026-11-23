# verificar_spec_b09.py -- prova do P-IA1-b09: forja spec, explicar --lista/--exemplos e o campo spec em todo diagnostico
"""Casos (cada um imprime uma linha JSON {id, valor, meta, verde}; exit 1 se algum nao e verde):
  M-U1   `forja spec --cartao` == language\\doc\\cartao.txt byte a byte (e `forja spec` == spec.txt, `--secao migracao` == migracao.txt)
  M-U2   `forja explicar --lista --json` traz todo codigo do mapa_spec.tsv, e o `spec` de cada um e uma secao que existe em `forja spec --json`
  json   `forja spec --json` e {versao, secoes:[{id,titulo,texto}]} valido, com a versao de `--versao`
  diag   todo diagnostico de toda fixture err_* (tests\\sintaxe e tests\\semantica): 1a vez do codigo com `spec` ("<secao do mapa>: ..."), depois `spec_ref`
  exem   `explicar --exemplos` devolve, por codigo, linhas "errado" que existem no arquivo da fixture; exemplos.txt gravado == o gerado agora
Uso:
    python -I verificar_spec_b09.py --forja <forja.exe>
    python -I verificar_spec_b09.py --forja <forja_antigo.exe> --esperar-vermelho     (exit 0 so se algum caso fica vermelho: prova que a regua discrimina)
    python -I verificar_spec_b09.py --selftest
Versao: 0.1.0
Changelog: 0.1.0 - P-IA1-b09: primeira versao.
Verificacao: python -I verificar_spec_b09.py --selftest
"""

import argparse
import json
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

LANG = Path(r"C:\Users\USUARIO\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language")
DOC = LANG / "doc"
PASTAS = [LANG / "tests" / "sintaxe", LANG / "tests" / "semantica"]
GERADOR = LANG / "tests" / "ia_first" / "gerar_exemplos_erro.py"
PADRAO_NOME = re.compile(r"^err_(E\d{4})_.*\.fj$")
TEMPO = 120
WORKERS = 12


def Rodar(Forja, Argumentos, Pasta=None):
    """CompletedProcess com stdout em bytes."""
    return subprocess.run([str(Forja), *Argumentos], capture_output=True, timeout=TEMPO, cwd=Pasta, stdin=subprocess.DEVNULL)


def JsonDe(Bytes):
    """O objeto JSON da saida inteira, ou None."""
    try:
        return json.loads(Bytes.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return None


def CodigosDoMapa():
    """{codigo: secao} do mapa_spec.tsv."""
    Linhas = (DOC / "mapa_spec.tsv").read_text(encoding="utf-8").splitlines()[1:]
    return dict(Linha.split("\t") for Linha in Linhas if Linha.strip())


# ---------- julgamentos puros (o selftest planta falhas nelas) ----------

def JulgarBytes(Obtido, Esperado, Nome):
    return [] if Obtido == Esperado else [f"{Nome}: {len(Obtido)} bytes diferem dos {len(Esperado)} do arquivo"]


def JulgarSpecJson(Dados, Versao):
    """{versao, secoes:[{id,titulo,texto}]} com a versao dada e ao menos uma secao completa."""
    if not isinstance(Dados, dict) or Dados.get("versao") != Versao:
        return [f"spec --json sem a versao {Versao!r}"]
    Secoes = Dados.get("secoes")
    if not isinstance(Secoes, list) or not Secoes:
        return ["spec --json sem secoes"]
    return [f"secao incompleta: {S!r}"[:80] for S in Secoes if not (isinstance(S, dict) and S.get("id") and S.get("titulo") and S.get("texto"))]


def JulgarLista(Lista, Mapa, Secoes):
    """Todo codigo do mapa na lista, com titulo, etapa e a secao do mapa (que existe em Secoes)."""
    if not isinstance(Lista, list):
        return ["explicar --lista --json nao e uma lista"]
    Por = {Item.get("codigo"): Item for Item in Lista if isinstance(Item, dict)}
    Falhas = [f"{C}: fora da lista" for C in Mapa if C not in Por]
    for Codigo, Item in Por.items():
        if Codigo in Mapa and (Item.get("spec") != Mapa[Codigo] or Item.get("spec") not in Secoes or not Item.get("titulo") or not Item.get("etapa")):
            Falhas.append(f"{Codigo}: spec={Item.get('spec')!r} (mapa {Mapa[Codigo]!r})")
    return Falhas


def JulgarDiagnosticos(Diags, Mapa, Secoes):
    """1a vez de cada codigo: spec "<secao do mapa>: ..." com secao existente; depois: spec_ref == "forja explicar <codigo>"."""
    Falhas, Vistos = [], set()
    for Diag in Diags:
        Codigo = Diag.get("codigo")
        if Codigo in Vistos:
            if Diag.get("spec_ref") != f"forja explicar {Codigo}" or "spec" in Diag:
                Falhas.append(f"{Codigo}: repetido sem spec_ref")
            continue
        Vistos.add(Codigo)
        Texto = Diag.get("spec")
        Secao = Texto.split(": ", 1)[0] if isinstance(Texto, str) else None
        if Secao is None or Secao not in Secoes or Mapa.get(Codigo) != Secao or len(Texto) <= len(Secao) + 2:
            Falhas.append(f"{Codigo}: spec={str(Texto)[:40]!r} (mapa {Mapa.get(Codigo)!r})")
    return Falhas


def JulgarExemplo(Linha):
    """Um item de `explicar --exemplos`: fixture existente do codigo e linhas erradas que estao nela."""
    Falhas = []
    Caminho = LANG / Linha["fixture"]
    if not Caminho.is_file() or not Caminho.name.startswith(f"err_{Linha['codigo']}_"):
        return [f"{Linha['codigo']}: fixture {Linha['fixture']!r} nao e do codigo"]
    Fonte = {L.rstrip("\r") for L in Caminho.read_text(encoding="utf-8").split("\n")}
    Falhas += [f"{Linha['codigo']}: linha errada fora da fixture: {L!r}" for L in Linha.get("errado", []) if L not in Fonte]
    if not Linha.get("errado") or not Linha.get("forma_certa"):
        Falhas.append(f"{Linha['codigo']}: sem errado ou sem forma_certa")
    return Falhas


# ---------- casos contra o binario ----------

def CasoMU1(Forja):
    Falhas = []
    for Argumentos, Arquivo in ((["spec", "--cartao"], "cartao.txt"), (["spec"], "spec.txt"), (["spec", "--secao", "migracao"], "migracao.txt")):
        Rodada = Rodar(Forja, Argumentos)
        Falhas += JulgarBytes(Rodada.stdout, (DOC / Arquivo).read_bytes(), f"forja {' '.join(Argumentos)} (exit {Rodada.returncode})")
    return "M-U1", Falhas


def SecoesDoSpec(Forja):
    Dados = JsonDe(Rodar(Forja, ["spec", "--json"]).stdout)
    return {S["id"] for S in Dados["secoes"]} if isinstance(Dados, dict) and "secoes" in Dados else set()


def CasoMU2(Forja):
    Lista = JsonDe(Rodar(Forja, ["explicar", "--lista", "--json"]).stdout)
    return "M-U2", JulgarLista(Lista, CodigosDoMapa(), SecoesDoSpec(Forja))


def CasoJson(Forja):
    Versao = Rodar(Forja, ["--versao"]).stdout.decode("utf-8", errors="replace").strip()
    return "json", JulgarSpecJson(JsonDe(Rodar(Forja, ["spec", "--json"]).stdout), Versao)


def DiagnosticosDe(Forja, Fixture):
    """Os diagnosticos (objetos JSON de uma linha) do `check` da fixture."""
    Saida = Rodar(Forja, ["check", Fixture.name], Fixture.parent).stdout.decode("utf-8", errors="replace")
    return [json.loads(L) for L in Saida.splitlines() if L.startswith("{")]


def CasoDiag(Forja):
    Mapa, Secoes = CodigosDoMapa(), SecoesDoSpec(Forja)
    Fixtures = sorted(F for Pasta in PASTAS for F in Pasta.glob("err_*.fj") if PADRAO_NOME.match(F.name))
    with ThreadPoolExecutor(max_workers=WORKERS) as Pool:
        Todos = list(Pool.map(lambda F: DiagnosticosDe(Forja, F), Fixtures))
    Falhas = [f"{F.name}: {M}" for F, Diags in zip(Fixtures, Todos) for M in (JulgarDiagnosticos(Diags, Mapa, Secoes) if Diags else ["sem diagnostico"])]
    return f"diag({len(Fixtures)} fixtures)", Falhas[:20]


def CasoExemplos(Forja):
    Rodada = Rodar(Forja, ["explicar", "--exemplos"])
    Falhas = [] if Rodada.returncode == 0 else [f"explicar --exemplos exit {Rodada.returncode}"]
    Itens = [json.loads(L) for L in Rodada.stdout.decode("utf-8", errors="replace").splitlines() if L.strip()]
    Com = [E for Item in Itens for E in Item.get("exemplos", [])]
    if len(Com) < 50:
        Falhas.append(f"so {len(Com)} exemplos (esperado >= 50)")
    Falhas += [M for E in Com for M in JulgarExemplo(E)]
    Conferir = subprocess.run([sys.executable, "-I", str(GERADOR), "--forja", str(Forja), "--conferir"], capture_output=True, timeout=TEMPO * 5)
    if Conferir.returncode != 0:
        Falhas.append("exemplos.txt gravado difere do gerado agora (gerar_exemplos_erro.py --conferir)")
    return "exem", Falhas[:20]


CASOS = [CasoMU1, CasoMU2, CasoJson, CasoDiag, CasoExemplos]


def RodarCasos(Forja):
    """Roda todos os casos; devolve quantos ficaram vermelhos."""
    Vermelhos = 0
    for Caso in CASOS:
        try:
            Id, Falhas = Caso(Forja)
        except Exception as Erro:  # um caso que quebra e vermelho, nao derruba os outros
            Id, Falhas = Caso.__name__, [f"excecao: {type(Erro).__name__}: {Erro}"]
        Vermelhos += bool(Falhas)
        print(json.dumps({"id": Id, "valor": "ok" if not Falhas else f"{len(Falhas)} falhas", "meta": "ok", "verde": not Falhas, "falhas": Falhas}, ensure_ascii=False))
    return Vermelhos


def Selftest():
    """Cada julgamento aceita o certo e recusa o errado plantado."""
    Mapa = {"E0001": "formato", "E0301": "declarar"}
    Bom = {"codigo": "E0301", "spec": "declarar: Variavel: let"}
    Casos = {
        "bytes igual": not JulgarBytes(b"a", b"a", "x"),
        "bytes difere": bool(JulgarBytes(b"a", b"b", "x")),
        "json ok": not JulgarSpecJson({"versao": "1", "secoes": [{"id": "a", "titulo": "A", "texto": "t"}]}, "1"),
        "json versao errada": bool(JulgarSpecJson({"versao": "2", "secoes": [{"id": "a", "titulo": "A", "texto": "t"}]}, "1")),
        "json secao sem texto": bool(JulgarSpecJson({"versao": "1", "secoes": [{"id": "a", "titulo": "A"}]}, "1")),
        "lista ok": not JulgarLista([{"codigo": Cod, "titulo": "t", "etapa": "e", "spec": Sec} for Cod, Sec in Mapa.items()], Mapa, {"formato", "declarar"}),
        "lista sem codigo": bool(JulgarLista([{"codigo": "E0001", "titulo": "t", "etapa": "e", "spec": "formato"}], Mapa, {"formato", "declarar"})),
        "lista secao inexistente": bool(JulgarLista([{"codigo": "E0001", "titulo": "t", "etapa": "e", "spec": "formato"}, {"codigo": "E0301", "titulo": "t", "etapa": "e", "spec": "declarar"}], Mapa, {"formato"})),
        "diag ok": not JulgarDiagnosticos([Bom, {"codigo": "E0301", "spec_ref": "forja explicar E0301"}], Mapa, {"declarar"}),
        "diag sem spec": bool(JulgarDiagnosticos([{"codigo": "E0301"}], Mapa, {"declarar"})),
        "diag secao errada": bool(JulgarDiagnosticos([{"codigo": "E0301", "spec": "formato: x y"}], Mapa, {"declarar", "formato"})),
        "diag repetido sem ref": bool(JulgarDiagnosticos([Bom, Bom], Mapa, {"declarar"})),
    }
    Falhas = [Nome for Nome, Ok in Casos.items() if not Ok]
    print(json.dumps({"id": "selftest", "valor": f"{len(Casos) - len(Falhas)}/{len(Casos)}", "meta": f"{len(Casos)}/{len(Casos)}", "verde": not Falhas, "falhas": Falhas}))
    return 1 if Falhas else 0


def Principal():
    Args = argparse.ArgumentParser(description="Verifica o P-IA1-b09 (forja spec, explicar --lista/--exemplos, campo spec)")
    Args.add_argument("--forja", type=Path)
    Args.add_argument("--esperar-vermelho", action="store_true")
    Args.add_argument("--selftest", action="store_true")
    Opcoes = Args.parse_args()
    if Opcoes.selftest:
        return Selftest()
    if Opcoes.forja is None:
        raise SystemExit("falta --forja <forja.exe>")
    Vermelhos = RodarCasos(Opcoes.forja)
    if Opcoes.esperar_vermelho:
        return 0 if Vermelhos else 1
    return 1 if Vermelhos else 0


if __name__ == "__main__":
    sys.exit(Principal())
