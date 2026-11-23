# verificar_mapa.py -- prova do P-IA1-I3: `forja consultar` parte 2 (callers, callees, imports, api, testes, entrada) e `forja mapa`: M-I1 completo, M-I2 e M-I3
"""Casos (cada um imprime uma linha JSON {id, valor, meta, verde, falhas}; exit 1 se algum nao e verde):
  mi1   M-I1 consultas certas, COMPLETO: os pontos de prova do verificar_consultar.py (tipo, assinatura, campos, refs, escopo, simbolos: 66) mais os pontos novos desta parte
        (callers, callees, imports, api, testes, entrada e o mapa), todos com gabarito escrito a mao sobre tests\\consultar\\proj e proj2. Pede >= 60 pontos no total
  mi2   M-I2 forja mapa: --resumo <= 4.000 tokens (chars/4) para engine\\forja inteiro (um main de teste importa os modulos raiz da pasta; ela nao tem main.fj) e <= 1.500 para cada
        projeto de testes (proj, proj2, std_todas); --modulo X <= 3.000 (todo modulo de engine\\forja e dos projetos); 2 execucoes = bytes iguais; toda declaracao pub do fonte
        aparece no --json e bate com `consultar api`; engine\\forja em <= 1 s (mediana de 5)
  mi3   M-I3 consultas rapidas: as 12 perguntas em engine\\forja <= 300 ms, mediana de 5
  uso   erros de uso das perguntas novas e do mapa (exit 2), projeto com erro (exit 1), pergunta desconhecida lista as 12, 2 execucoes = bytes iguais
Uso:
    python verificar_mapa.py --forja <forja.exe> [--caso mi1,mi2] [--jobs N]
    python verificar_mapa.py --forja <forja_antigo.exe> --esperar-vermelho     (exit 0 so se algum caso fica vermelho: prova que a regua discrimina)
    python verificar_mapa.py --selftest
Versao: 0.1.0
Changelog: 0.1.0 - P-IA1-I3: primeira versao.
Verificacao: python verificar_mapa.py --selftest
"""

import argparse
import importlib.util
import json
import os
import re
import statistics
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LANG = AQUI.parents[1]
PROJ = AQUI / "proj"
PROJ2 = AQUI / "proj2"
STD_TODAS = AQUI / "std_todas"
ENGINE = LANG.parent / "engine"
MINIMO_PONTOS = 60
LIMITE_TOKENS_ENGINE = 4000
LIMITE_TOKENS_PROJETO = 1500
LIMITE_TOKENS_MODULO = 3000
LIMITE_MS_MAPA = 1000
LIMITE_MS_CONSULTA = 300
PERGUNTAS = ["simbolos", "escopo", "refs", "tipo", "assinatura", "campos", "callers", "callees", "imports", "api", "testes", "entrada"]


def CarregarConsultar():
    Spec = importlib.util.spec_from_file_location("verificar_consultar", AQUI / "verificar_consultar.py")
    Modulo = importlib.util.module_from_spec(Spec)
    sys.modules["verificar_consultar"] = Modulo
    Spec.loader.exec_module(Modulo)
    return Modulo


VC = CarregarConsultar()
Caso = VC.Caso
Confere = VC.Confere
Rodar = VC.Rodar


def Tokens(Bytes):
    return len(Bytes) / 4


def Pergunta(Forja, Argumentos, Cwd=PROJ, Env=None):
    """(exit, json ou None, bytes, stderr) de `forja consultar ...` rodado em Cwd."""
    R = Rodar(Forja, ["consultar", *Argumentos], Cwd=str(Cwd), Env=Env)
    return R.returncode, VC.JsonDe(R.stdout), R.stdout, R.stderr.decode("utf-8", "replace")


def Mapa(Forja, Argumentos, Cwd=PROJ, Env=None):
    """(exit, bytes do stdout, stderr) de `forja mapa ...` rodado em Cwd."""
    R = Rodar(Forja, ["mapa", *Argumentos], Cwd=str(Cwd), Env=Env)
    return R.returncode, R.stdout, R.stderr.decode("utf-8", "replace")


def Linhas(Arquivo):
    Dados = Path(Arquivo).read_bytes()
    return Dados.count(b"\n") + (1 if Dados and not Dados.endswith(b"\n") else 0)


def PosDe(Base):
    """Funcao (Arq, Trecho, Ocorrencia, Desloc) -> [linha, coluna] sobre os arquivos de Base."""
    return lambda Arq, Trecho, Ocorrencia=1, Desloc=0: VC.Pos(Arq, Trecho, Ocorrencia, Desloc, Base=Base)


P1 = PosDe(PROJ)
P2 = PosDe(PROJ2)


def Chk(Forja, Rotulo, Argumentos, Esperado, Cwd=PROJ):
    """Roda `consultar <Argumentos>` (exit 0) e confere o subconjunto Esperado da resposta."""
    Cod, Doc, _, Err = Pergunta(Forja, Argumentos, Cwd=Cwd)
    if Cod != 0 or Doc is None:
        return [f"{Rotulo}: exit {Cod} {Err.strip()[:120]}"]
    return Confere(Esperado, Doc, Rotulo)


# ---------- gabarito: callers e callees (projeto proj) ----------

def Chamada(Em, PosNome, Categoria=None):
    Item = {"em": Em, "pos_nome": PosNome}
    if Categoria:
        Item["categoria"] = Categoria
    return Item


def PontosCallers(Forja):
    M, F = "main.fj", "jogo/fisica.fj"
    T = "main:test:somar usa o padrao"
    Casos = [
        ("callers: somar (2 em main, 1 em test)", ["somar", M], {"id": "main:somar", "total": 3, "chamadores": ["main:main", T],
         "chamadas": [Chamada("main:main", P1(M, "somar(1)", 1)), Chamada("main:main", P1(M, "somar(i", 1)), Chamada(T, P1(M, "somar(1)", 2), "teste")]}),
        ("callers: dividir (main e test)", ["dividir", M], {"total": 2, "chamadores": ["main:main", "main:test:dividir"],
         "chamadas": [Chamada("main:main", P1(M, "dividir(17", 1)), Chamada("main:test:dividir", P1(M, "dividir(7", 1), "teste")]}),
        ("callers: achar (dentro de print)", ["achar", M], {"total": 1, "chamadas": [Chamada("main:main", P1(M, "achar(3)", 1))]}),
        ("callers: mover", ["mover", M], {"total": 1, "chamadas": [Chamada("main:main", P1(M, "mover(&", 1), "proc")]}),
        ("callers: descrever (ninguem chama)", ["descrever", M], {"total": 0, "chamadas": [], "chamadores": []}),
        ("callers: proc de outro modulo (pos = inicio da expressao, pos_nome = o nome)", ["aceleracao", M],
         {"id": "jogo/fisica:aceleracao", "chamadores": ["main:mover"], "chamadas": [{"em": "main:mover", "pos": P1(M, "fis.aceleracao(c", 1, 0), "pos_nome": P1(M, "fis.aceleracao(c", 1, 4)}]}),
        ("callers: cor_de", ["cor_de", M], {"chamadas": [Chamada("main:main", P1(M, "fis.cor_de(", 1, 4))]}),
        ("callers: proximo", ["proximo", M], {"chamadas": [Chamada("main:main", P1(M, "fis.proximo(r)", 1, 4))]}),
        ("callers: proc privado de outro modulo", ["interno", M], {"chamadores": ["jogo/fisica:proximo"], "chamadas": [Chamada("jogo/fisica:proximo", P1(F, "interno(n)", 1))]}),
        ("callers: std", ["arena_create", M], {"id": "std:mem:arena_create", "chamadores": ["main:main"], "total": 1}),
        ("callers: foreign do std", ["forja_arena_criar", M], {"chamadores": ["std:mem:arena_create"], "total": 1}),
        ("callers: por id", ["jogo/fisica:proximo", M], {"id": "jogo/fisica:proximo", "chamadores": ["main:main"]}),
    ]
    return [(R, lambda A=A, E=E, R=R: Chk(Forja, R, ["callers", *A], E)) for R, A, E in Casos]


def Chamado(Nome, Categoria, Id, Chamadas=None, Pos=None):
    Item = {"nome": Nome, "categoria": Categoria, "id": Id}
    if Chamadas:
        Item["chamadas"] = Chamadas
    if Pos:
        Item["pos"] = Pos
    return Item


def PontosCallees(Forja):
    M = "main.fj"
    ChamadosMain = [Chamado("somar", "proc", "main:somar", 2, P1(M, "somar(1)", 1)), Chamado("dividir", "proc", "main:dividir", 1), Chamado("arena_create", "proc", "std:mem:arena_create"),
                    Chamado("arena_destroy", "proc", "std:mem:arena_destroy"), Chamado("cor_de", "proc", "jogo/fisica:cor_de"), Chamado("print", "embutido", None, 5),
                    Chamado("mover", "proc", "main:mover"), Chamado("achar", "proc", "main:achar"), Chamado("proximo", "proc", "jogo/fisica:proximo")]
    Casos = [
        ("callees: main (9 destinos na ordem da 1a chamada, print = embutido x5)", ["main", M], {"id": "main:main", "total": 9, "chamados": ChamadosMain}),
        ("callees: main por id", ["main:main", M], {"total": 9}),
        ("callees: mover", ["mover", M], {"total": 1, "chamados": [Chamado("aceleracao", "proc", "jogo/fisica:aceleracao", 1, P1(M, "fis.aceleracao(c", 1, 4))]}),
        ("callees: descrever (so campos e cases)", ["descrever", M], {"total": 0, "chamados": []}),
        ("callees: proc de outro modulo", ["proximo", M], {"chamados": [Chamado("interno", "proc", "jogo/fisica:interno", 1)]}),
        ("callees: test", ["main:test:somar usa o padrao", M], {"chamados": [Chamado("somar", "proc", "main:somar", 1, P1(M, "somar(1)", 2))]}),
        ("callees: outro test", ["main:test:dividir", M], {"total": 1, "chamados": [Chamado("dividir", "proc", "main:dividir", 1, P1(M, "dividir(7", 1))]}),
        ("callees: proc sem chamada", ["aceleracao", M], {"total": 0}),
        ("callees: std chama foreign", ["arena_create", M], {"chamados": [Chamado("forja_arena_criar", "foreign", "std:mem:forja_arena_criar", 1)]}),
    ]
    return [(R, lambda A=A, E=E, R=R: Chk(Forja, R, ["callees", *A], E)) for R, A, E in Casos]


# ---------- gabarito: imports, api, testes, entrada ----------

def Importa(Modulo, Apelido, Pos):
    return {"modulo": Modulo, "apelido": Apelido, "pos": Pos}


def PontosImports(Forja):
    M, M2 = "main.fj", "main.fj"
    Linhas1, Linhas2 = Linhas(PROJ / "main.fj"), Linhas(PROJ / "jogo" / "fisica.fj")
    ModulosProj = [
        {"modulo": "main", "origem": "raiz", "linhas": Linhas1, "importado_por": [], "links": [],
         "importa": [Importa("std:mem", "mem", P1(M, 'import "std:mem"')), Importa("jogo/fisica", "fis", P1(M, 'import "jogo/fisica"'))]},
        {"modulo": "std:mem", "origem": "std", "arquivo": "std:mem", "importa": [], "importado_por": ["main"], "links": []},
        {"modulo": "jogo/fisica", "origem": "arquivo", "linhas": Linhas2, "importa": [], "importado_por": ["main"], "links": []},
    ]
    ModulosProj2 = [
        {"modulo": "main", "origem": "raiz", "linhas": Linhas(PROJ2 / "main.fj"), "links": [], "importa": [Importa("lib/nativo", "nat", P2(M2, 'import "lib/nativo"')), Importa("lib/util", "util", P2(M2, 'import "lib/util"'))]},
        {"modulo": "lib/nativo", "origem": "arquivo", "importado_por": ["main"], "links": [{"lib": "kernel32", "modulo": "lib/nativo", "pos": P2("lib/nativo.fj", '"kernel32"')}]},
        {"modulo": "lib/util", "origem": "arquivo", "importado_por": ["main"], "links": [], "linhas": Linhas(PROJ2 / "lib" / "util.fj")},
    ]
    Casos = [
        ("imports: proj, modulos na ordem (raiz, std, arquivo), origem, linhas, importa com apelido e pos", PROJ, {"modulos": ModulosProj, "links": []}),
        ("imports: proj2, foreign com link na pos da string e importa de 2 modulos", PROJ2, {"modulos": ModulosProj2, "links": [{"lib": "kernel32", "modulo": "lib/nativo"}]}),
    ]
    Pontos = [(R, lambda C=C, E=E, R=R: Chk(Forja, R, ["imports", "main.fj"], E, Cwd=C)) for R, C, E in Casos]
    Pontos.append(("imports: o std embutido tem link vazio e arquivo std:mem", lambda: Chk(Forja, "imports std", ["imports", "main.fj"], {"modulos": [{}, {"modulo": "std:mem", "links": []}, {}]})))
    return Pontos


def Item(Nome, Categoria, Assinatura, Comentario=None, **Mais):
    return {"nome": Nome, "categoria": Categoria, "assinatura": Assinatura, "comentario": Comentario, **Mais}


def PontosApi(Forja):
    F, N, U = "jogo/fisica.fj", "lib/nativo.fj", "lib/util.fj"
    Cabecalho = "fisica.fj - modulo da fixture de 'forja consultar' (tests\\consultar): constantes, enums, struct, union e procs publicos."
    Fisica = [Item("GRAVIDADE", "const", "const GRAVIDADE: f32", Cabecalho, pos=P1(F, "pub const GRAVIDADE")), Item("DT", "const", "const DT: f32"), Item("Cor", "enum", "enum Cor"),
              Item("Nivel", "enum", "enum Nivel"), Item("Corpo", "struct", "struct Corpo"), Item("Forma", "union", "union Forma"),
              Item("aceleracao", "proc", "proc aceleracao(c: ^Corpo, forca: f32 = 1.0) -> f32", pos=P1(F, "pub proc aceleracao")),
              Item("cor_de", "proc", "proc cor_de(c: Corpo) -> Cor"), Item("nivel_de", "proc", "proc nivel_de(c: Corpo) -> Nivel"), Item("proximo", "proc", "proc proximo(n: i32) -> i32")]
    Util = [Item("LIMITE", "const", "const LIMITE: i32", "util.fj - modulo da fixture: const, procs, tipos, test e um proc privado."), Item("dobro", "proc", "proc dobro(n: i32) -> i32", "Dobra o valor.", id="lib/util:dobro"),
            Item("Par", "struct", "struct Par"), Item("Modo", "enum", "enum Modo")]
    Nativo = [Item("GetTickCount", "foreign", "proc GetTickCount() -> u32", "Milissegundos desde o boot (32 bits).", pos=P2(N, "proc GetTickCount")),
              Item("tempo_ms", "proc", "proc tempo_ms() -> u32", "Tempo em milissegundos.\nSegunda linha do comentario.", pos=P2(N, "pub proc tempo_ms"))]
    Raiz2 = [Item("somar_dll", "proc", "proc somar_dll(a: i32, b: i32) -> i32", "Exportado para a DLL.", exportado=True, pos=P2("main.fj", "pub export proc"))]
    Casos = [
        ("api: jogo/fisica (so pub, na ordem do fonte, interno fica fora)", PROJ, "jogo/fisica", {"modulo": "jogo/fisica", "total": 10, "api": Fisica}),
        ("api: main de proj nao tem pub", PROJ, "main", {"total": 0, "api": []}),
        ("api: lib/util (privado fora; comentario de 1 linha; tipos)", PROJ2, "lib/util", {"total": 4, "api": Util}),
        ("api: lib/nativo (foreign indentado com comentario; comentario de 2 linhas)", PROJ2, "lib/nativo", {"total": 2, "api": Nativo}),
        ("api: raiz com pub export", PROJ2, "main", {"total": 1, "api": Raiz2}),
    ]
    Pontos = [(R, lambda C=C, M=M, E=E, R=R: Chk(Forja, R, ["api", M, "main.fj"], E, Cwd=C)) for R, C, M, E in Casos]
    Pontos.append(("api: std:mem traz arena_create com o padrao", lambda: Chk(Forja, "api std", ["api", "std:mem", "main.fj"], {"modulo": "std:mem"})))
    Pontos.append(("api: arquivo do modulo termina no fonte", lambda: FalhasDeArquivoDaApi(Forja)))
    return Pontos


def FalhasDeArquivoDaApi(Forja):
    Cod, Doc, _, _ = Pergunta(Forja, ["api", "lib/util", "main.fj"], Cwd=PROJ2)
    if Cod != 0 or Doc is None:
        return [f"api arquivo: exit {Cod}"]
    return [] if VC.Norm(Doc["arquivo"]).endswith("proj2/lib/util.fj") else [f"api arquivo: {Doc['arquivo']}"]


def PontosTestesEEntrada(Forja):
    M = "main.fj"
    Casos = [
        ("testes: proj (titulo, id, modulo, arquivo:linha)", PROJ, ["testes"], {"total": 2, "testes": [
            {"nome": "somar usa o padrao", "id": "main:test:somar usa o padrao", "modulo": "main", "pos": P1(M, 'test "somar usa')},
            {"nome": "dividir", "id": "main:test:dividir", "pos": P1(M, 'test "dividir"')}]}),
        ("testes: proj2 (modulos importados vem antes da raiz)", PROJ2, ["testes"], {"total": 2, "testes": [
            {"nome": "dobro de 2", "id": "lib/util:test:dobro de 2", "modulo": "lib/util", "pos": P2("lib/util.fj", 'test "dobro')},
            {"nome": "main usa dobro", "id": "main:test:main usa dobro", "modulo": "main", "pos": P2(M, 'test "main usa')}]}),
        ("entrada: proj tem so main", PROJ, ["entrada"], {"total": 1, "entradas": [{"nome": "main", "entrada": "main", "id": "main:main", "pos": P1(M, "proc main()"), "assinatura": "proc main()"}]}),
        ("entrada: proj2 tem pub export e main", PROJ2, ["entrada"], {"total": 2, "entradas": [
            {"nome": "somar_dll", "entrada": "export", "id": "main:somar_dll", "pos": P2(M, "pub export proc"), "assinatura": "proc somar_dll(a: i32, b: i32) -> i32"},
            {"nome": "main", "entrada": "main", "pos": P2(M, "proc main()")}]}),
    ]
    return [(R, lambda C=C, A=A, E=E, R=R: Chk(Forja, R, [*A, "main.fj"], E, Cwd=C)) for R, C, A, E in Casos]


# ---------- gabarito: mapa --json e --resumo ----------

def DocDoMapa(Forja, Cwd, Extra=()):
    Cod, Bruto, Err = Mapa(Forja, [*Extra, "--json", "main.fj"], Cwd=Cwd)
    return VC.JsonDe(Bruto) if Cod == 0 else None


def FalhasDoMapaJson(Forja, Rotulo, Cwd, Esperado):
    Doc = DocDoMapa(Forja, Cwd)
    return Confere(Esperado, Doc, Rotulo) if Doc is not None else [f"{Rotulo}: mapa --json falhou"]


def FalhasDeEquivalencia(Forja, Cwd):
    """Cada secao do mapa --json e a mesma linha que a pergunta correspondente devolve."""
    Doc = DocDoMapa(Forja, Cwd)
    if Doc is None:
        return [f"{Cwd.name}: mapa --json falhou"]
    Falhas = []
    Imports = Pergunta(Forja, ["imports", "main.fj"], Cwd=Cwd)[1] or {}
    Testes = Pergunta(Forja, ["testes", "main.fj"], Cwd=Cwd)[1] or {}
    Entrada = Pergunta(Forja, ["entrada", "main.fj"], Cwd=Cwd)[1] or {}
    Falhas += [] if Doc["modulos"] == Imports.get("modulos") else [f"{Cwd.name}: modulos diferem de consultar imports"]
    Falhas += [] if Doc["dependencias"]["links"] == Imports.get("links") else [f"{Cwd.name}: links diferem de consultar imports"]
    Falhas += [] if Doc["testes"] == Testes.get("testes") else [f"{Cwd.name}: testes diferem de consultar testes"]
    Falhas += [] if Doc["entrada"] == Entrada.get("entradas") else [f"{Cwd.name}: entrada difere de consultar entrada"]
    for Bloco in Doc["api"]:
        Api = Pergunta(Forja, ["api", Bloco["modulo"], "main.fj"], Cwd=Cwd)[1] or {}
        Falhas += [] if Bloco["api"] == Api.get("api") else [f"{Cwd.name}: api de {Bloco['modulo']} difere de consultar api"]
    return Falhas


def Membro(Nome, Valor=None):
    return {"nome": Nome, "valor": Valor}


def PontosMapaJson(Forja):
    L1, L2 = Linhas(PROJ / "main.fj"), Linhas(PROJ / "jogo" / "fisica.fj")
    L2a = sum(Linhas(A) for A in (PROJ2 / "main.fj", PROJ2 / "lib" / "nativo.fj", PROJ2 / "lib" / "util.fj"))
    return [
        ("mapa json: tamanho de proj2 (3 modulos, 9 simbolos, 7 pub, 2 tipos, 2 testes, sem std)", lambda: FalhasDoMapaJson(
            Forja, "tamanho proj2", PROJ2, {"pergunta": "mapa", "tamanho": {"modulos": 3, "externos": 0, "linhas": L2a, "simbolos": 9, "publicos": 7, "tipos": 2, "testes": 2}})),
        ("mapa json: tamanho de proj (o std conta so em externos)", lambda: FalhasDoMapaJson(
            Forja, "tamanho proj", PROJ, {"tamanho": {"modulos": 2, "externos": 1, "linhas": L1 + L2, "simbolos": 19, "publicos": 10, "tipos": 6, "testes": 2}})),
        ("mapa json: tipos de proj2 com campos e membros", lambda: FalhasDoMapaJson(
            Forja, "tipos proj2", PROJ2, {"tipos": [{"nome": "Par", "categoria": "struct", "modulo": "lib/util", "publico": True, "campos": [{"nome": "a", "tipo": "i32"}, {"nome": "b", "tipo": "i32"}]},
                                                  {"nome": "Modo", "categoria": "enum", "membros": [Membro("rapido", 1), Membro("lento", 5)]}]})),
        ("mapa json: tipos de proj (struct, enum, union nesta ordem de modulos)", lambda: FalhasDoMapaJson(
            Forja, "tipos proj", PROJ, {"tipos": [{"nome": "Cor", "membros": [Membro("vermelho"), Membro("azul")]}, {"nome": "Nivel", "membros": [Membro("baixo", -1), Membro("medio", 0), Membro("alto", 1)]},
                                               {"nome": "Corpo", "campos": [{"nome": "massa", "tipo": "f32"}, {"nome": "cor", "tipo": "Cor"}, {"nome": "vida", "tipo": "i32"}]},
                                               {"nome": "Forma", "categoria": "union", "variantes": [{"tipo": "Corpo"}, {"tipo": "i32"}]},
                                               {"nome": "Erro", "modulo": "main", "publico": False}, {"nome": "Mundo", "campos": [{"nome": "corpos", "tipo": "[4]Corpo"}, {"nome": "quadros", "tipo": "i32"}]}]})),
        ("mapa json: dependencias de proj (std:mem) e proj2 (link kernel32)", lambda: FalhasDoMapaJson(
            Forja, "dep proj", PROJ, {"dependencias": {"externas": ["std:mem"], "links": []}}) + FalhasDoMapaJson(
            Forja, "dep proj2", PROJ2, {"dependencias": {"externas": [], "links": [{"lib": "kernel32"}]}})),
        ("mapa json: entrada e testes de proj2", lambda: FalhasDoMapaJson(
            Forja, "entrada proj2", PROJ2, {"entrada": [{"entrada": "export"}, {"entrada": "main"}], "testes": [{"nome": "dobro de 2"}, {"nome": "main usa dobro"}]})),
        ("mapa json: as secoes batem com consultar imports, testes, entrada e api (proj)", lambda: FalhasDeEquivalencia(Forja, PROJ)),
        ("mapa json: as secoes batem com consultar (proj2)", lambda: FalhasDeEquivalencia(Forja, PROJ2)),
        ("mapa json --modulo jogo/fisica: so o modulo (api, 4 tipos, sem teste nem entrada)", lambda: FalhasDoModuloJson(Forja)),
        ("mapa json --modulo std:mem: o std so aparece quando pedido", lambda: FalhasDoStdJson(Forja)),
    ]


def FalhasDoModuloJson(Forja):
    Doc = DocDoMapa(Forja, PROJ, ("--modulo", "jogo/fisica"))
    if Doc is None:
        return ["--modulo jogo/fisica: mapa falhou"]
    Falhas = Confere({"testes": [], "entrada": []}, Doc, "modulo jogo/fisica")
    Falhas += [] if [B["modulo"] for B in Doc["api"]] == ["jogo/fisica"] and len(Doc["tipos"]) == 4 else [f"api/tipos de jogo/fisica: {[B['modulo'] for B in Doc['api']]} {len(Doc['tipos'])} tipos"]
    return Falhas


def FalhasDoStdJson(Forja):
    Padrao = DocDoMapa(Forja, PROJ) or {}
    Std = DocDoMapa(Forja, PROJ, ("--modulo", "std:mem")) or {}
    Falhas = [] if all(B["modulo"] != "std:mem" for B in Padrao.get("api", [])) else ["mapa sem --modulo traz a api do std"]
    Falhas += [] if [B["modulo"] for B in Std.get("api", [])] == ["std:mem"] else ["mapa --modulo std:mem nao traz so a api do std:mem"]
    return Falhas


RESUMO_PROJ2 = """mapa main: 3 modulos, 52 linhas, 7 pub, 2 tipos, 2 testes (Nl = linhas; * = pub)
entrada: export main:somar_dll, main:main
modulos:
main 17l raiz -> lib/nativo lib/util
  pub: somar_dll()

lib/nativo 12l
  pub: GetTickCount() tempo_ms()

lib/util 23l
  tipos: struct Par*; enum Modo*
  pub: LIMITE dobro()
testes: main 1, lib/util 1
links: kernel32
"""

MODULO_UTIL = """modulo lib/util (arquivo, 23l)
importado por: main
tipos:
  struct Par* {a: i32, b: i32}
  enum Modo* {rapido, lento}
api:
  const LIMITE: i32  // util.fj - modulo da fixture: const, procs, tipos, test e um proc privado.
  proc dobro(n: i32) -> i32  // Dobra o valor.
testes:
  L{linha} dobro de 2
"""


def TextoDoMapa(Forja, Cwd, Argumentos):
    Cod, Bruto, Err = Mapa(Forja, [*Argumentos, "main.fj"], Cwd=Cwd)
    return Bruto.decode("utf-8", "replace") if Cod == 0 else f"<exit {Cod}: {Err.strip()[:100]}>"


def FalhasDeTexto(Rotulo, Real, Esperado):
    return [] if Real == Esperado else [f"{Rotulo}: veio\n{Real}\nesperava\n{Esperado}"]


def FalhasDeTrechos(Rotulo, Real, Trechos):
    return [f"{Rotulo}: falta '{T}' em\n{Real}" for T in Trechos if T not in Real]


def PontosMapaResumo(Forja):
    LinhaTeste = P2("lib/util.fj", 'test "dobro')[0]
    return [
        ("mapa resumo: texto exato de proj2", lambda: FalhasDeTexto("resumo proj2", TextoDoMapa(Forja, PROJ2, ["--resumo"]), RESUMO_PROJ2)),
        ("mapa resumo: sem opcao = --resumo", lambda: FalhasDeTexto("padrao", TextoDoMapa(Forja, PROJ2, []), RESUMO_PROJ2)),
        ("mapa resumo --modulo lib/util: texto exato (tipos com campos, api com assinatura e comentario, testes com a linha)", lambda: FalhasDeTexto(
            "modulo util", TextoDoMapa(Forja, PROJ2, ["--resumo", "--modulo", "lib/util"]), MODULO_UTIL.replace("{linha}", str(LinhaTeste)))),
        ("mapa resumo de proj: entrada, std e testes", lambda: FalhasDeTrechos("resumo proj", TextoDoMapa(Forja, PROJ, ["--resumo"]), [
            "mapa main: 2 modulos,", ", 1 std (Nl = linhas; * = pub)", "entrada: main:main", "main " + str(Linhas(PROJ / "main.fj")) + "l raiz -> std:mem jogo/fisica",
            "  tipos: struct Mundo; enum Erro", "  tipos: struct Corpo*; enum Cor* Nivel*; union Forma*", "  pub: GRAVIDADE DT aceleracao() cor_de() nivel_de() proximo()", "\nstd: mem\n", "testes: main 2"])),
        ("mapa resumo --modulo jogo/fisica: assinaturas e campos", lambda: FalhasDeTrechos("modulo fisica", TextoDoMapa(Forja, PROJ, ["--resumo", "--modulo", "jogo/fisica"]), [
            "modulo jogo/fisica (arquivo,", "  struct Corpo* {massa: f32, cor: Cor, vida: i32}", "  enum Nivel* {baixo, medio, alto}", "  union Forma* {Corpo, i32}",
            "  proc aceleracao(c: ^Corpo, forca: f32 = 1.0) -> f32", "  const GRAVIDADE: f32", "importado por: main"])),
        ("mapa resumo --limite 2 corta cada lista e conta o resto com +K", lambda: FalhasDeTrechos("limite", TextoDoMapa(Forja, PROJ, ["--resumo", "--limite", "2"]), [
            "  tipos: struct Corpo*; enum Cor* +2\n", "  pub: GRAVIDADE DT +4\n"])),
        ("mapa resumo --modulo main (raiz): testes com a linha do fonte", lambda: FalhasDeTrechos("modulo main", TextoDoMapa(Forja, PROJ, ["--resumo", "--modulo", "main"]), [
            "modulo main (raiz,", f"  L{P1('main.fj', 'test \"somar usa')[0]} somar usa o padrao\n", f"  L{P1('main.fj', 'test \"dividir')[0]} dividir\n", "  struct Mundo {corpos: [4]Corpo, quadros: i32}"])),
    ]


def PontosDoMi1(Forja):
    Pontos = list(VC.PontosDeM_I1(Forja))
    for Parte in (PontosCallers, PontosCallees, PontosImports, PontosApi, PontosTestesEEntrada, PontosMapaJson, PontosMapaResumo):
        Pontos += Parte(Forja)
    return Pontos


def CasoMI1(Forja, Jobs):
    Pontos = PontosDoMi1(Forja)
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Resultados = list(Pool.map(lambda P: P[1](), Pontos))
    Falhas = [F for R in Resultados for F in R]
    Bons = sum(1 for R in Resultados if not R)
    if len(Pontos) < MINIMO_PONTOS:
        Falhas.append(f"so {len(Pontos)} pontos de prova, o minimo e {MINIMO_PONTOS}")
    return Caso("mi1", f"{Bons}/{len(Pontos)} pontos", f"{len(Pontos)}/{len(Pontos)} (>= {MINIMO_PONTOS})", Falhas)


# ---------- engine\forja: um main de teste que importa os modulos raiz ----------

def ModulosDaEngine():
    return sorted(A.stem for A in (ENGINE / "forja").glob("*.fj"))


def ImportsDe(Modulo):
    return set(re.findall(r'^import "engine:(\w+)"', (ENGINE / "forja" / f"{Modulo}.fj").read_text(encoding="utf-8"), re.M))


def UsoDoModulo(Modulo, Apelido, Indice):
    """Uma linha `let _uN = ...` que usa algo pub do modulo (import sem uso e E1005): const, 1o membro de enum ou proc sem parametro."""
    Texto = (ENGINE / "forja" / f"{Modulo}.fj").read_text(encoding="utf-8")
    Const = re.search(r"^pub const (\w+)", Texto, re.M)
    Enum = re.search(r"^pub enum (\w+) \{\s*(\w+)", Texto, re.M)
    Proc = re.search(r"^\s*(?:pub )?proc (\w+)\(\) -> ", Texto, re.M)
    if Const:
        return f"    let _u{Indice} = {Apelido}.{Const.group(1)}"
    if Enum:
        return f"    let _u{Indice} = {Apelido}.{Enum.group(1)}.{Enum.group(2)}"
    if Proc:
        return f"    let _u{Indice} = {Apelido}.{Proc.group(1)}()"
    raise ValueError(f"engine:{Modulo} nao tem const, enum nem proc sem parametro para o main de teste usar")


def TextoMainDaEngine():
    Modulos = ModulosDaEngine()
    Usados = set().union(*(ImportsDe(M) for M in Modulos))
    Raizes = [M for M in Modulos if M not in Usados]
    Imports = [f'import "engine:{M}" as m{I}' for I, M in enumerate(Raizes)]
    Usos = [UsoDoModulo(M, f"m{I}", I) for I, M in enumerate(Raizes)]
    return "\n".join(Imports) + "\n\nproc main() {\n" + "\n".join(Usos) + "\n}\n"


class Engine:
    """Pasta temporaria com o main de teste de engine\\forja (a pasta nao tem main.fj: e uma biblioteca de modulos)."""

    def __enter__(self):
        self.PastaTemporaria = tempfile.TemporaryDirectory(prefix="forja_mapa_engine_")
        self.Pasta = Path(self.PastaTemporaria.name)
        (self.Pasta / "main.fj").write_text(TextoMainDaEngine(), encoding="utf-8", newline="\n")
        self.Env = dict(os.environ, FORJA_ENGINE=str(ENGINE))
        return self

    def __exit__(self, *Args):
        self.PastaTemporaria.cleanup()


# ---------- M-I2 ----------

def PubsDoFonte(Texto):
    """Nomes pub declarados no fonte: no topo (`pub [export] proc|struct|enum|union|const|unit nome`) e dentro de `pub foreign { ... }`."""
    Nomes, EmForeign = [], False
    for Linha in Texto.splitlines():
        if Linha.startswith("pub foreign"):
            EmForeign = True
            continue
        if EmForeign and Linha.startswith("}"):
            EmForeign = False
            continue
        Achou = re.match(r"\s+(?:proc|opaque)\s+(\w+)", Linha) if EmForeign else re.match(r"pub\s+(?:export\s+)?(?:proc|struct|enum|union|const|unit)\s+(\w+)", Linha)
        if Achou:
            Nomes.append(Achou.group(1))
    return sorted(Nomes)


def ApiDoMapa(Doc):
    return {B["modulo"]: sorted(I["nome"] for I in B["api"]) for B in Doc["api"]}


def FalhasDePubs(Forja, Cwd, Env, Arquivos):
    """Toda declaracao pub do fonte aparece no mapa --json e bate com `consultar api` do modulo. Arquivos = {caminho do modulo: arquivo}."""
    Cod, Bruto, Err = Mapa(Forja, ["--json", "main.fj"], Cwd=Cwd, Env=Env)
    Doc = VC.JsonDe(Bruto)
    if Cod != 0 or Doc is None:
        return [f"{Cwd.name}: mapa --json exit {Cod} {Err.strip()[:100]}"], 0
    Api, Falhas, Total = ApiDoMapa(Doc), [], 0
    for Modulo, Arquivo in Arquivos.items():
        Previstos = PubsDoFonte(Arquivo.read_text(encoding="utf-8"))
        Total += len(Previstos)
        Falhas += [] if Api.get(Modulo, []) == Previstos else [f"{Modulo}: mapa {sorted(set(Api.get(Modulo, [])) ^ set(Previstos))[:5]} divergem do fonte"]
        _, Doc2, _, _ = Pergunta(Forja, ["api", Modulo, "main.fj"], Cwd=Cwd, Env=Env)
        Bloco = next((B["api"] for B in Doc["api"] if B["modulo"] == Modulo), [])
        Falhas += [] if (Doc2 or {}).get("api") == Bloco else [f"{Modulo}: mapa e consultar api diferem"]
    return Falhas, Total


def FalhasDeTamanho(Rotulo, Bytes, Limite):
    return [] if Tokens(Bytes) <= Limite else [f"{Rotulo}: {Tokens(Bytes):.0f} tokens (limite {Limite})"]


def FalhasDeModulos(Forja, Cwd, Env, Modulos):
    Falhas, Maior = [], (0, "")
    for Modulo in Modulos:
        Cod, Bruto, Err = Mapa(Forja, ["--resumo", "--modulo", Modulo, "main.fj"], Cwd=Cwd, Env=Env)
        if Cod != 0:
            Falhas.append(f"--modulo {Modulo}: exit {Cod} {Err.strip()[:80]}")
            continue
        Maior = max(Maior, (len(Bruto), Modulo))
        Falhas += FalhasDeTamanho(f"--modulo {Modulo}", Bruto, LIMITE_TOKENS_MODULO)
    return Falhas, Maior


def MedianaMs(Forja, Argumentos, Env, Cwd):
    Tempos = []
    for _ in range(5):
        T0 = time.perf_counter()
        R = Rodar(Forja, Argumentos, Cwd=str(Cwd), Env=Env)
        Tempos.append((time.perf_counter() - T0) * 1000)
        if R.returncode != 0:
            return None
    return statistics.median(Tempos)


def CasoMI2(Forja, Jobs):
    Falhas, Medidas = [], {}
    with Engine() as E:
        Cod, Resumo, Err = Mapa(Forja, ["--resumo", "main.fj"], Cwd=E.Pasta, Env=E.Env)
        if Cod != 0:
            return Caso("mi2", "quebrou", "verde", [f"mapa --resumo em engine\\forja: exit {Cod} {Err.strip()[:200]}"])
        Medidas["engine_resumo_tokens"] = round(Tokens(Resumo))
        Falhas += FalhasDeTamanho("engine\\forja --resumo", Resumo, LIMITE_TOKENS_ENGINE)
        Falhas += [] if Mapa(Forja, ["--resumo", "main.fj"], Cwd=E.Pasta, Env=E.Env)[1] == Resumo else ["engine --resumo: 2 execucoes com bytes diferentes"]
        Json1, Json2 = Mapa(Forja, ["--json", "main.fj"], Cwd=E.Pasta, Env=E.Env)[1], Mapa(Forja, ["--json", "main.fj"], Cwd=E.Pasta, Env=E.Env)[1]
        Falhas += [] if Json1 and Json1 == Json2 else ["engine --json: 2 execucoes com bytes diferentes"]
        Arquivos = {f"engine:{M}": ENGINE / "forja" / f"{M}.fj" for M in ModulosDaEngine()}
        FalhasPubs, Pubs = FalhasDePubs(Forja, E.Pasta, E.Env, Arquivos)
        Falhas += FalhasPubs
        Medidas["engine_pubs_conferidas"] = Pubs
        FalhasMod, Maior = FalhasDeModulos(Forja, E.Pasta, E.Env, list(Arquivos))
        Falhas += FalhasMod
        Medidas["engine_maior_modulo_tokens"] = [round(Maior[0] / 4), Maior[1]]
        for Nome, Args in (("resumo", ["mapa", "--resumo", "main.fj"]), ("json", ["mapa", "--json", "main.fj"])):
            Ms = MedianaMs(Forja, Args, E.Env, E.Pasta)
            Medidas[f"engine_{Nome}_ms"] = None if Ms is None else round(Ms)
            if Ms is None or Ms > LIMITE_MS_MAPA:
                Falhas.append(f"engine mapa --{Nome}: {Medidas[f'engine_{Nome}_ms']} ms (limite {LIMITE_MS_MAPA})")
    Falhas += FalhasDosProjetos(Forja, Medidas)
    return Caso("mi2", json.dumps(Medidas, ensure_ascii=False), f"resumo <= {LIMITE_TOKENS_ENGINE} (engine) e <= {LIMITE_TOKENS_PROJETO} (projetos), --modulo <= {LIMITE_TOKENS_MODULO}, pub = api, <= {LIMITE_MS_MAPA} ms", Falhas)


def FalhasDosProjetos(Forja, Medidas):
    Falhas = []
    Fontes = {PROJ: {"main": PROJ / "main.fj", "jogo/fisica": PROJ / "jogo" / "fisica.fj"},
              PROJ2: {"main": PROJ2 / "main.fj", "lib/nativo": PROJ2 / "lib" / "nativo.fj", "lib/util": PROJ2 / "lib" / "util.fj"}, STD_TODAS: {"main": STD_TODAS / "main.fj"}}
    for Pasta, Arquivos in Fontes.items():
        Cod, Resumo, Err = Mapa(Forja, ["--resumo", "main.fj"], Cwd=Pasta)
        Medidas[f"{Pasta.name}_tokens"] = round(Tokens(Resumo))
        Falhas += [f"{Pasta.name}: mapa --resumo exit {Cod} {Err.strip()[:100]}"] if Cod != 0 else FalhasDeTamanho(f"{Pasta.name} --resumo", Resumo, LIMITE_TOKENS_PROJETO)
        Falhas += [] if Mapa(Forja, ["--resumo", "main.fj"], Cwd=Pasta)[1] == Resumo else [f"{Pasta.name}: 2 execucoes com bytes diferentes"]
        FalhasPubs, _ = FalhasDePubs(Forja, Pasta, None, Arquivos)
        Falhas += FalhasPubs
        FalhasMod, _ = FalhasDeModulos(Forja, Pasta, None, list(Arquivos))
        Falhas += FalhasMod
    return Falhas


# ---------- M-I3 ----------

def PerguntasDaEngine(Pasta):
    Linha, Coluna = VC.Pos("main.fj", ".ErroRender", 1, 1, Base=Pasta)
    Posicao = f"main.fj:{Linha}:{Coluna}"
    return {"simbolos": ["simbolos", "main.fj"], "escopo": ["escopo", Posicao, "main.fj"], "refs": ["refs", "ErroRender", "main.fj"], "tipo": ["tipo", Posicao, "main.fj"],
            "assinatura": ["assinatura", "render_iniciar", "main.fj"], "campos": ["campos", "Render", "main.fj"], "callers": ["callers", "render_iniciar", "main.fj"],
            "callees": ["callees", "render_iniciar", "main.fj"], "imports": ["imports", "main.fj"], "api": ["api", "engine:render", "main.fj"], "testes": ["testes", "main.fj"],
            "entrada": ["entrada", "main.fj"]}


def CasoMI3(Forja):
    Falhas, Medidas = [], {}
    with Engine() as E:
        Perguntas = PerguntasDaEngine(E.Pasta)
        for Nome, Args in Perguntas.items():
            Ms = MedianaMs(Forja, ["consultar", *Args], E.Env, E.Pasta)
            Medidas[Nome] = None if Ms is None else round(Ms)
            if Ms is None or Ms > LIMITE_MS_CONSULTA:
                Falhas.append(f"{Nome}: {Medidas[Nome]} ms (limite {LIMITE_MS_CONSULTA}; None = a consulta falhou)")
    return Caso("mi3", f"{Medidas} ms (mediana de 5)", f"cada uma <= {LIMITE_MS_CONSULTA} ms", Falhas)


# ---------- erros de uso ----------

def FalhasDeUso(Forja):
    Falhas = []
    Consultas = [("callers sem nome", ["callers"], 2), ("callers de struct", ["callers", "Mundo", "main.fj"], 2), ("callers inexistente", ["callers", "nao_existe", "main.fj"], 2),
                 ("callees de struct", ["callees", "Mundo", "main.fj"], 2), ("api sem modulo", ["api"], 2), ("api de modulo inexistente", ["api", "nao/existe", "main.fj"], 2),
                 ("imports com argumento demais", ["imports", "main.fj", "sobra"], 2), ("--modulo em imports", ["imports", "--modulo", "main"], 2), ("testes com opcao desconhecida", ["testes", "--x"], 2),
                 ("entrada de arquivo inexistente", ["entrada", "nao_existe.fj"], 2)]
    for Rotulo, Args, Esperado in Consultas:
        Cod = Pergunta(Forja, Args)[0]
        Falhas += [] if Cod == Esperado else [f"{Rotulo}: exit {Cod}, esperado {Esperado}"]
    Err = Pergunta(Forja, ["foo"])[3]
    Falhas += [] if all(P in Err for P in PERGUNTAS) else [f"pergunta desconhecida nao lista as {len(PERGUNTAS)} perguntas"]
    Mapas = [("opcao desconhecida", ["--x"], 2), ("--json com --resumo", ["--json", "--resumo"], 2), ("--limite sem valor", ["--limite"], 2), ("--limite zero", ["--limite", "0"], 2),
             ("--limite texto", ["--limite", "abc"], 2), ("--limite com --json", ["--json", "--limite", "3"], 2), ("--modulo sem valor", ["--modulo"], 2),
             ("--modulo inexistente", ["--modulo", "nao/existe"], 2), ("2 alvos", ["main.fj", "main.fj"], 2), ("arquivo inexistente", ["nao_existe.fj"], 2), ("pasta sem main.fj", [str(AQUI)], 2)]
    for Rotulo, Args, Esperado in Mapas:
        Cod = Mapa(Forja, Args)[0]
        Falhas += [] if Cod == Esperado else [f"mapa {Rotulo}: exit {Cod}, esperado {Esperado}"]
    Falhas += FalhasDeProjetoComErro(Forja)
    A, B = Mapa(Forja, ["--json", "main.fj"])[1], Mapa(Forja, ["--json", "main.fj"])[1]
    Falhas += [] if A and A == B else ["2 execucoes de mapa --json nao deram bytes iguais"]
    return Falhas


def FalhasDeProjetoComErro(Forja):
    with tempfile.TemporaryDirectory(prefix="forja_mapa_uso_") as Pasta:
        Arq = Path(Pasta) / "main.fj"
        Arq.write_text("proc main() {\n    let x: i32 = nao_existe\n}\n", encoding="utf-8")
        Falhas = []
        for Rotulo, Cod, Bruto in (("mapa", *Mapa(Forja, [str(Arq)], Cwd=Pasta)[:2]), ("consultar imports", *Pergunta(Forja, ["imports", str(Arq)], Cwd=Pasta)[::2][:2])):
            Falhas += [] if Cod == 1 and b"E0201" in Bruto else [f"{Rotulo} em projeto com erro: exit {Cod}, esperado 1 com o diagnostico do check"]
        return Falhas


def CasoUso(Forja):
    Falhas = FalhasDeUso(Forja)
    return Caso("uso", "ok" if not Falhas else f"{len(Falhas)} falhas", "ok", Falhas)


# ---------- orquestracao ----------

def RodarCasos(Forja, Quais, Jobs):
    Tabela = {"mi1": lambda: CasoMI1(Forja, Jobs), "mi2": lambda: CasoMI2(Forja, Jobs), "mi3": lambda: CasoMI3(Forja), "uso": lambda: CasoUso(Forja)}
    Saidas = []
    for Id in Quais:
        try:
            Saidas.append(Tabela[Id]())
        except Exception as Erro:  # noqa: BLE001 - qualquer quebra do caso e um caso vermelho com o motivo
            Saidas.append(Caso(Id, "quebrou", "verde", [f"{type(Erro).__name__}: {Erro}"]))
    return Saidas


# ---------- selftest ----------

def Selftest():
    Erros = []

    def Exigir(Nome, Condicao):
        if not Condicao:
            Erros.append(Nome)

    Exigir("PubsDoFonte topo", PubsDoFonte("pub proc a()\nproc b()\npub export proc c()\npub const X: i32 = 1\npub struct S {\n}\n") == ["S", "X", "a", "c"])
    Exigir("PubsDoFonte foreign", PubsDoFonte("pub foreign {\n    link \"x\"\n    proc f()\n}\nforeign {\n    proc g()\n}\n") == ["f"])
    Exigir("Tokens", Tokens(b"x" * 400) == 100)
    Exigir("Linhas", Linhas(PROJ / "main.fj") > 50)
    Exigir("P1 acha a linha do import", P1("main.fj", 'import "std:mem"') == [2, 1])
    Exigir("UsoDoModulo const", UsoDoModulo("acoes", "m0", 0).startswith("    let _u0 = m0."))
    Exigir("main da engine importa os modulos raiz", TextoMainDaEngine().count("import ") >= 5)
    Exigir("pontos novos bastam", len(PontosCallers(None)) + len(PontosCallees(None)) + len(PontosImports(None)) + len(PontosApi(None)) + len(PontosTestesEEntrada(None)) >= 30)
    Exigir("Caso vermelho", not Caso("x", "", "", ["f"])["verde"] and Caso("x", "", "", [])["verde"])
    for E in Erros:
        print(json.dumps({"selftest": E, "verde": False}))
    print(json.dumps({"selftest": "verificar_mapa", "verde": not Erros, "falhas": Erros}))
    return 0 if not Erros else 1


def Principal():
    Args = argparse.ArgumentParser(description="P-IA1-I3: forja consultar parte 2 e forja mapa")
    Args.add_argument("--forja")
    Args.add_argument("--caso", default="mi1,mi2,mi3,uso")
    Args.add_argument("--jobs", type=int, default=8)
    Args.add_argument("--esperar-vermelho", action="store_true")
    Args.add_argument("--selftest", action="store_true")
    Dados = Args.parse_args()
    if Dados.selftest:
        return Selftest()
    if not Dados.forja:
        Args.error("--forja e obrigatorio (ou --selftest)")
    Saidas = RodarCasos(Path(Dados.forja), [C for C in Dados.caso.split(",") if C], Dados.jobs)
    for S in Saidas:
        print(json.dumps(S, ensure_ascii=False))
    AlgumVermelho = any(not S["verde"] for S in Saidas)
    if Dados.esperar_vermelho:
        return 0 if AlgumVermelho else 1
    return 1 if AlgumVermelho else 0


if __name__ == "__main__":
    sys.exit(Principal())
