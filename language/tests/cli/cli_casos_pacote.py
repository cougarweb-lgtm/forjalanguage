"""Casos de cli do modulo de pasta (Linguagem v1.13 L1b): as ferramentas (ast, consultar, mapa, fmt, editar, migrar) mostram e usam o ARQUIVO certo das declaracoes de um pacote, mover_decl de
ou para um pacote e recusado e o cache do build (.entradas) ve .fj que entra ou sai da pasta.

Importado pelo motor verificar_cli.py; as fixtures moram em tests\\cli\\pacote\\ (proj: main.fj + os pacotes geo e util + os modulos de arquivo calc e outro; migrar: raiz com forma antiga +
pacote pk). Cada caso trabalha numa copia temporaria.

Regras:
  - ast --projeto: cada declaracao de um pacote de 2+ arquivos leva "arquivo" e a linha LOCAL a esse arquivo; a raiz e os modulos de um arquivo so ficam como antes (sem "arquivo" por declaracao);
  - consultar (simbolos, api, escopo) e mapa (--json e --resumo): "arquivos" do modulo, "arquivo" por item, linhas e bytes somados nos arquivos do pacote, o escopo so enxerga os imports do arquivo do ponto;
  - fmt de uma pasta de pacote: cada arquivo e formatado; editar dentro do pacote cai no arquivo certo e o import de um arquivo some com a declaracao que o usava;
  - mover_decl de ou para um pacote (inclusive entre os arquivos do mesmo pacote) = exit 1, motivo "pacote", 0 arquivo alterado; mover entre modulos de arquivo num projeto COM pacotes segue valendo;
  - migrar de uma raiz que importa um pacote: troca a raiz, o pacote fica intacto e o texto migrado compila;
  - build --projeto: .fj acrescentado ou removido na pasta de um pacote invalida o cache ("fonte mudou: <pasta>").

Versao: 1.0.0
Changelog: 1.0.0 - Linguagem v1.13 L1b: primeira versao (8 casos).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import hashlib
import json
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

from verificar_cli import ConferirExecucao, PastaAqui, Rodar, TimeoutSegundos

FIXTURES = PastaAqui / "pacote"
PROJ = FIXTURES / "proj"
MIGRAR = FIXTURES / "migrar"


def CopiaDe(Fixture):
    """(raiz temporaria, pasta copiada da fixture)."""
    Raiz = Path(tempfile.mkdtemp(prefix="cli_pacote_"))
    Pasta = Raiz / Fixture.name
    shutil.copytree(Fixture, Pasta)
    return Raiz, Pasta


def HashPasta(Pasta):
    return {A.relative_to(Pasta).as_posix(): hashlib.sha256(A.read_bytes()).hexdigest() for A in sorted(Pasta.rglob("*")) if A.is_file()}


def JsonDe(Rodada, Problemas):
    try:
        return json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        Problemas.append(f"stdout nao e um documento JSON: {Rodada.stdout[:100]!r}")
        return None


def LinhaDe(Arquivo, Inicio):
    """Linha (a partir de 1) da 1a linha de 'Arquivo' que comeca com 'Inicio'."""
    for Numero, Linha in enumerate(Arquivo.read_text(encoding="utf-8").split("\n"), start=1):
        if Linha.startswith(Inicio):
            return Numero
    raise ValueError(f"{Arquivo.name} nao tem linha que comece com {Inicio!r}")


def NomeDe(Caminho):
    return str(Caminho).replace("\\", "/").rsplit("/", 1)[-1]


def DeclsDoModulo(Documento, Modulo):
    return {D["id"]: D for M in Documento["modulos"] if M["modulo"] == Modulo for D in M["declaracoes"]}


def CasoPacoteAst(Forja, PastaSaidas):
    """ast --projeto: cada declaracao do pacote geo/util leva o arquivo dela e a linha local; a raiz e calc, que tem um arquivo so, nao levam "arquivo" por declaracao."""
    Problemas = []
    Raiz, Pasta = CopiaDe(PROJ)
    try:
        Rodada = Rodar(Forja, ["ast", "--projeto", str(Pasta / "main.fj")])
        Problemas += ConferirExecucao(Rodada, 0)
        Doc = JsonDe(Rodada, Problemas)
        if Doc is None:
            return Problemas
        Esperado = {("geo", "geo:total"): ("geo/a.fj", "pub proc total"), ("geo", "geo:Ponto"): ("geo/b.fj", "pub struct Ponto"), ("geo", "geo:soma"): ("geo/b.fj", "pub proc soma("),
                    ("geo", "geo:soma_dobrada"): ("geo/b.fj", "pub proc soma_dobrada"), ("util", "util:dobro"): ("util/c.fj", "pub proc dobro"), ("util", "util:somar"): ("util/d.fj", "proc somar")}
        for (Modulo, Id), (Arquivo, Inicio) in Esperado.items():
            Decl = DeclsDoModulo(Doc, Modulo).get(Id)
            if Decl is None:
                Problemas.append(f"{Id} nao saiu no ast")
                continue
            if NomeDe(Decl.get("arquivo", "")) != NomeDe(Arquivo) or not Decl.get("arquivo", "").endswith(Arquivo):
                Problemas.append(f"{Id}: arquivo {Decl.get('arquivo')!r}, esperado ...{Arquivo}")
            Linha = LinhaDe(Pasta / Arquivo, Inicio)
            if Decl["pos"][0] != Linha:
                Problemas.append(f"{Id}: pos {Decl['pos']}, a linha local em {Arquivo} e {Linha}")
        Modulos = {M["modulo"]: M for M in Doc["modulos"]}
        if not Modulos["geo"]["arquivo"].endswith("geo/a.fj"):
            Problemas.append(f"o arquivo do modulo geo deveria ser o 1o do pacote (geo/a.fj): {Modulos['geo']['arquivo']}")
        for Modulo in ("calc", "outro", "main"):
            if any("arquivo" in D for D in Modulos[Modulo]["declaracoes"]):
                Problemas.append(f"o modulo {Modulo} (um arquivo so) ganhou \"arquivo\" por declaracao")
        Sem = Rodar(Forja, ["ast", "--projeto", "--sem-pos", str(Pasta / "main.fj")])
        if '"arquivo": "' in Sem.stdout:
            Problemas.append("--sem-pos ainda traz \"arquivo\" por declaracao")
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def CasoPacoteConsultar(Forja, PastaSaidas):
    """consultar simbolos/api/escopo num pacote: o arquivo e a linha local de cada simbolo, o comentario colado e os imports do arquivo do ponto."""
    Problemas = []
    Raiz, Pasta = CopiaDe(PROJ)
    try:
        Principal = str(Pasta / "main.fj")
        Simbolos = JsonDe(Rodar(Forja, ["consultar", "simbolos", Principal, "--modulo", "geo", "--json"], Pasta=Pasta), Problemas)
        if Simbolos is not None:
            Achados = {S["id"]: (NomeDe(S["arquivo"]), S["pos"][0]) for S in Simbolos["simbolos"]}
            Esperado = {"geo:total": ("a.fj", LinhaDe(Pasta / "geo" / "a.fj", "pub proc total")), "geo:Ponto": ("b.fj", LinhaDe(Pasta / "geo" / "b.fj", "pub struct Ponto")),
                        "geo:soma": ("b.fj", LinhaDe(Pasta / "geo" / "b.fj", "pub proc soma("))}
            for Id, Valor in Esperado.items():
                if Achados.get(Id) != Valor:
                    Problemas.append(f"simbolos {Id}: {Achados.get(Id)}, esperado {Valor}")
        Api = JsonDe(Rodar(Forja, ["consultar", "api", "geo", Principal], Pasta=Pasta), Problemas)
        if Api is not None:
            Arquivos = [NomeDe(A) for A in Api.get("arquivos", [])]
            Itens = {I["nome"]: I for I in Api["api"]}
            if Arquivos != ["a.fj", "b.fj"]:
                Problemas.append(f"api geo: \"arquivos\" = {Arquivos}, esperado ['a.fj', 'b.fj']")
            if NomeDe(Itens["soma"].get("arquivo", "")) != "b.fj" or Itens["soma"]["comentario"] != "soma as duas coordenadas":
                Problemas.append(f"api geo: soma = {Itens['soma']}")
            if Itens["total"]["comentario"].startswith("fim do a.fj"):
                Problemas.append("o comentario sem quebra de linha no fim de a.fj grudou na declaracao de outro arquivo")
        EmB = JsonDe(Rodar(Forja, ["consultar", "escopo", "geo/b.fj:11:5", Principal, "--json"], Pasta=Pasta), Problemas)
        EmA = JsonDe(Rodar(Forja, ["consultar", "escopo", "geo/a.fj:3:5", Principal, "--json"], Pasta=Pasta), Problemas)
        if EmB is not None and EmA is not None:
            if EmB["modulo"] != "geo" or EmA["modulo"] != "geo":
                Problemas.append(f"escopo: modulo {EmA['modulo']!r} e {EmB['modulo']!r}, esperado geo nos dois")
            if [I["modulo"] for I in EmB["importados"]] != ["util"] or EmA["importados"] != []:
                Problemas.append(f"escopo: so b.fj importa util; importados de b.fj = {EmB['importados']}, de a.fj = {EmA['importados']}")
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def LinhasDe(Arquivo):
    Texto = Arquivo.read_bytes()
    return Texto.count(b"\n") + (1 if Texto and not Texto.endswith(b"\n") else 0)


def CasoPacoteMapa(Forja, PastaSaidas):
    """mapa --json e --resumo: o pacote lista os arquivos, soma linhas e bytes de todos eles e a linha do teste diz de que arquivo e."""
    Problemas = []
    Raiz, Pasta = CopiaDe(PROJ)
    try:
        Arquivos = sorted(Pasta.rglob("*.fj"))
        Mapa = JsonDe(Rodar(Forja, ["mapa", str(Pasta / "main.fj"), "--json"], Pasta=Pasta), Problemas)
        if Mapa is not None:
            Tamanho = Mapa["tamanho"]
            if Tamanho["bytes"] != sum(A.stat().st_size for A in Arquivos) or Tamanho["linhas"] != sum(LinhasDe(A) for A in Arquivos):
                Problemas.append(f"tamanho {Tamanho}: esperado {sum(LinhasDe(A) for A in Arquivos)} linhas e {sum(A.stat().st_size for A in Arquivos)} bytes (todos os arquivos)")
            Geo = next(M for M in Mapa["modulos"] if M["modulo"] == "geo")
            if [NomeDe(A) for A in Geo.get("arquivos", [])] != ["a.fj", "b.fj"] or Geo["linhas"] != LinhasDe(Pasta / "geo" / "a.fj") + LinhasDe(Pasta / "geo" / "b.fj"):
                Problemas.append(f"modulo geo no mapa: {Geo}")
        Resumo = Rodar(Forja, ["mapa", str(Pasta / "main.fj"), "--resumo", "--modulo", "geo"], Pasta=Pasta)
        Problemas += ConferirExecucao(Resumo, 0)
        Esperada = f"b.fj:L{LinhaDe(Pasta / 'geo' / 'b.fj', 'test ')} soma do pacote"
        if Esperada not in Resumo.stdout:
            Problemas.append(f"--resumo --modulo geo sem a linha {Esperada!r}: {Resumo.stdout!r}")
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def CasoPacoteFmt(Forja, PastaSaidas):
    """fmt de uma pasta de pacote formata cada arquivo (dry-run lista os 2 e nao grava; --aplicar grava e a 2a passada acha 0)."""
    Problemas = []
    Raiz, Pasta = CopiaDe(PROJ)
    try:
        Originais = {N: (Pasta / "geo" / N).read_text(encoding="utf-8") for N in ("a.fj", "b.fj")}
        for Nome, Texto in Originais.items():
            (Pasta / "geo" / Nome).write_text(re.sub(r"(?m)^    ", "  ", Texto), encoding="utf-8", newline="\n")
        Ruins = HashPasta(Pasta / "geo")
        Seco = JsonDe(Rodar(Forja, ["fmt", str(Pasta / "geo"), "--json"]), Problemas)
        if Seco is not None and ([NomeDe(A["arquivo"]) for A in Seco["arquivos"]] != ["a.fj", "b.fj"] or Seco["totais"]["mudariam"] != 2):
            Problemas.append(f"dry-run: {Seco['arquivos'] if Seco else None}")
        if HashPasta(Pasta / "geo") != Ruins:
            Problemas.append("o dry-run gravou")
        Aplicado = Rodar(Forja, ["fmt", str(Pasta / "geo"), "--aplicar", "--json"])
        Problemas += ConferirExecucao(Aplicado, 0)
        for Nome, Texto in Originais.items():
            Esperado = Texto if Texto.endswith("\n") else Texto + "\n"
            if (Pasta / "geo" / Nome).read_text(encoding="utf-8") != Esperado:
                Problemas.append(f"geo/{Nome} formatado difere do original canonico")
        Segunda = JsonDe(Rodar(Forja, ["fmt", str(Pasta / "geo"), "--json"]), Problemas)
        if Segunda is not None and Segunda["totais"]["mudariam"] != 0:
            Problemas.append("a 2a passada ainda muda arquivo")
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def Editar(Forja, Pasta, Raiz, Operacoes, Aplicar=True):
    """editar <pasta> --ops (com --aplicar por padrao): devolve a rodada e o documento JSON (None se a resposta nao for JSON, como nas recusas de operacao)."""
    Ops = Raiz / "ops.json"
    Ops.write_text(json.dumps(Operacoes), encoding="utf-8")
    Rodada = Rodar(Forja, ["editar", str(Pasta), "--ops", str(Ops)] + (["--aplicar"] if Aplicar else []))
    try:
        return Rodada, json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        return Rodada, None


def CasoPacoteEditar(Forja, PastaSaidas):
    """editar dentro de um pacote: inserir depois de Ponto, remover a que usava o import (o import do arquivo sai), trocar_corpo e renomear caem nos arquivos certos; no_fim sem "arquivo" e recusado."""
    Problemas = []
    Raiz, Pasta = CopiaDe(PROJ)
    try:
        Antes = HashPasta(Pasta)
        Recusa, _ = Editar(Forja, Pasta, Raiz, [{"op": "inserir_decl", "modulo": "geo", "no_fim": True, "texto": "proc nova() -> int {\n    return 1\n}"}])
        Problemas += ConferirExecucao(Recusa, 1)
        if "pasta com 2 arquivos" not in Recusa.stderr or HashPasta(Pasta) != Antes:
            Problemas.append(f"no_fim em pacote sem \"arquivo\": esperava recusa com 'pasta com 2 arquivos' e 0 arquivo alterado: {Recusa.stderr[:200]!r}")
        Operacoes = [{"op": "inserir_decl", "depois_de": "Ponto", "texto": "proc dobro_do_dobro(n: int) -> int {\n    return n * 4\n}"}, {"op": "remover_decl", "id": "soma_dobrada"},
                     {"op": "trocar_corpo", "id": "geo:total", "texto": "return 5"}]
        Rodada, Doc = Editar(Forja, Pasta, Raiz, Operacoes)
        Problemas += ConferirExecucao(Rodada, 0)
        Depois = HashPasta(Pasta)
        Mudaram = sorted(N for N in Depois if Depois[N] != Antes.get(N))
        if Mudaram != ["geo/a.fj", "geo/b.fj"]:
            Problemas.append(f"arquivos alterados: {Mudaram}, esperado so geo/a.fj e geo/b.fj")
        A, B = (Pasta / "geo" / "a.fj").read_text(encoding="utf-8"), (Pasta / "geo" / "b.fj").read_text(encoding="utf-8")
        if "return 5" not in A or "soma_dobrada" in B or 'import "util"' in B or "dobro_do_dobro" not in B or "dobro_do_dobro" in A:
            Problemas.append(f"conteudo depois do editar fora do esperado:\n--- a.fj\n{A}\n--- b.fj\n{B}")
        Problemas += [f"check depois do editar: {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Pasta / "main.fj")]), 0)]
        Rodada, Doc = Editar(Forja, Pasta, Raiz, [{"op": "renomear", "id": "geo:soma", "novo": "somar_pontos"}])
        Problemas += ConferirExecucao(Rodada, 0)
        A, B = (Pasta / "geo" / "a.fj").read_text(encoding="utf-8"), (Pasta / "geo" / "b.fj").read_text(encoding="utf-8")
        if "somar_pontos" not in B or "pub proc soma(" in B:
            Problemas.append("renomear: a declaracao em b.fj nao mudou")
        Problemas += [f"check depois do renomear: {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Pasta / "main.fj")]), 0)]
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def CasoPacoteMover(Forja, PastaSaidas):
    """mover_decl de ou para um pacote (e entre os arquivos do mesmo pacote) e recusado sem gravar; entre modulos de arquivo, num projeto com pacotes, segue valendo."""
    Problemas = []
    Raiz, Pasta = CopiaDe(PROJ)
    try:
        Antes = HashPasta(Pasta)
        for Operacao in ({"op": "mover_decl", "id": "soma", "para": "util"}, {"op": "mover_decl", "id": "soma", "para": "geo/a.fj"}, {"op": "mover_decl", "id": "dobro", "para": "outro"}):
            Rodada, _ = Editar(Forja, Pasta, Raiz, [Operacao])
            Problemas += [f"{Operacao['id']} -> {Operacao['para']}: {P}" for P in ConferirExecucao(Rodada, 1)]
            if "pacote" not in Rodada.stderr or HashPasta(Pasta) != Antes:
                Problemas.append(f"{Operacao['id']} -> {Operacao['para']}: esperava recusa com 'pacote' e 0 arquivo alterado: {Rodada.stderr[:200]!r}")
        Saida = Rodar(Forja, ["run", str(Pasta / "main.fj")]).stdout
        Rodada, Doc = Editar(Forja, Pasta, Raiz, [{"op": "mover_decl", "id": "triplo", "para": "outro"}])
        Problemas += ConferirExecucao(Rodada, 0)
        Mudaram = sorted(N for N, H in HashPasta(Pasta).items() if H != Antes.get(N))
        if Mudaram != ["calc.fj", "main.fj", "outro.fj"] or "triplo" not in (Pasta / "outro.fj").read_text(encoding="utf-8"):
            Problemas.append(f"mover triplo calc -> outro: arquivos alterados {Mudaram}, esperado calc.fj, main.fj e outro.fj")
        Problemas += [f"check depois de mover: {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Pasta / "main.fj")]), 0)]
        if Rodar(Forja, ["run", str(Pasta / "main.fj")]).stdout != Saida:
            Problemas.append("o programa imprime outra coisa depois do mover")
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def CasoPacoteMigrar(Forja, PastaSaidas):
    """migrar de uma raiz que importa um pacote: troca a forma antiga da raiz, deixa o pacote intacto, o texto migrado compila com o pacote e a 2a passada acha 0 trocas."""
    Problemas = []
    Raiz, Pasta = CopiaDe(MIGRAR)
    try:
        Pacote = HashPasta(Pasta / "pk")
        Seco = JsonDe(Rodar(Forja, ["migrar", str(Pasta / "raiz.fj"), "--json"]), Problemas)
        if Seco is not None and (Seco["arquivos"][0]["estado"] != "ok" or Seco["total_trocas"] != 1 or Seco["arquivos_alterados"] != 0):
            Problemas.append(f"dry-run fora do esperado: {Seco['arquivos'][0]['estado']}, {Seco['total_trocas']} trocas")
        Problemas += [f"antes de migrar o check deveria falhar (E0110): {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Pasta / "raiz.fj")]), 1)]
        Aplicado = JsonDe(Rodar(Forja, ["migrar", str(Pasta / "raiz.fj"), "--aplicar", "--json"]), Problemas)
        if Aplicado is not None and Aplicado["arquivos_alterados"] != 1:
            Problemas.append(f"--aplicar: {Aplicado['arquivos_alterados']} alterados, esperado 1")
        if "alloc = heap" not in (Pasta / "raiz.fj").read_text(encoding="utf-8") or HashPasta(Pasta / "pk") != Pacote:
            Problemas.append("a raiz nao foi migrada ou o pacote foi alterado")
        Problemas += [f"check depois de migrar: {P}" for P in ConferirExecucao(Rodar(Forja, ["check", str(Pasta / "raiz.fj")]), 0)]
        Segunda = JsonDe(Rodar(Forja, ["migrar", str(Pasta / "raiz.fj"), "--json"]), Problemas)
        if Segunda is not None and Segunda["total_trocas"] != 0:
            Problemas.append("a 2a passada achou trocas (nao e idempotente)")
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas


def CasoPacoteCachePasta(Forja, PastaSaidas):
    """build --projeto: o .entradas guarda a pasta do pacote e o cache ve .fj acrescentado (o build muda: E0202 de nome repetido) e removido (volta a compilar)."""
    Problemas = []
    Raiz = Path(tempfile.mkdtemp(prefix="cli_pacote_cache_"))
    try:
        (Raiz / "pk").mkdir()
        (Raiz / "app.fj").write_text('import "pk"\n\nproc main() {\n    print(pk.um())\n}\n', encoding="utf-8", newline="\n")
        (Raiz / "pk" / "a.fj").write_text("pub proc um() -> int {\n    return 1\n}\n", encoding="utf-8", newline="\n")
        (Raiz / "forja.proj").write_text("saida = bin\nproduto app exe app.fj\n", encoding="utf-8", newline="\n")
        Manifesto = str(Raiz / "forja.proj")
        Primeira = Rodar(Forja, ["build", "--projeto", Manifesto])
        Problemas += ConferirExecucao(Primeira, 0)
        Entradas = list((Raiz / ".forja_cache").rglob("app.entradas"))
        if len(Entradas) != 1 or "\npasta\t" not in Entradas[0].read_text(encoding="utf-8"):
            Problemas.append("o .entradas nao tem a linha 'pasta' do pacote pk")
        Plano = Rodar(Forja, ["build", "--projeto", Manifesto, "--plano"])
        if "1 reusados" not in Plano.stdout + Plano.stderr:
            Problemas.append(f"sem mudar nada o plano deveria reusar: {Plano.stdout + Plano.stderr!r}")
        (Raiz / "pk" / "b.fj").write_text("pub proc um() -> int {\n    return 2\n}\n", encoding="utf-8", newline="\n")
        Plano = Rodar(Forja, ["build", "--projeto", Manifesto, "--plano"])
        if "fonte mudou: pk" not in Plano.stdout + Plano.stderr:
            Problemas.append(f"com b.fj novo na pasta o plano deveria recompilar (fonte mudou: pk): {Plano.stdout + Plano.stderr!r}")
        Repetido = Rodar(Forja, ["build", "--projeto", Manifesto])
        if Repetido.returncode == 0 or "E0202" not in Repetido.stdout + Repetido.stderr:
            Problemas.append(f"com b.fj novo o build deveria falhar com E0202 (nome repetido no pacote): exit {Repetido.returncode}")
        (Raiz / "pk" / "b.fj").unlink()
        Volta = Rodar(Forja, ["build", "--projeto", Manifesto])
        Problemas += ConferirExecucao(Volta, 0)
        if "recompilado (fonte mudou: pk)" not in Volta.stdout + Volta.stderr:
            Problemas.append(f"com b.fj removido o build deveria recompilar: {Volta.stdout + Volta.stderr!r}")
        Execucao = subprocess.run([str(Raiz / "bin" / "app.exe")], capture_output=True, timeout=TimeoutSegundos)
        if Execucao.stdout.replace(b"\r\n", b"\n") != b"1\n":
            Problemas.append(f"app.exe: stdout {Execucao.stdout!r}, esperado b'1\\n'")
    finally:
        shutil.rmtree(Raiz, ignore_errors=True)
    return Problemas
