# verificar_consultar.py -- prova do P-IA1-I2: `forja consultar` (simbolos, escopo, refs, tipo, assinatura, campos): M-I1, M-U4, M-I3 e o confronto M-A3 x refs
"""Casos (cada um imprime uma linha JSON {id, valor, meta, verde, falhas}; exit 1 se algum nao e verde):
  mi1   M-I1 consultas certas: pontos de prova com gabarito escrito a mao sobre tests\\consultar\\proj (main + jogo/fisica + std:mem): tipo em arq:L:C, assinatura, campos,
        refs, escopo e simbolos (a posicao de cada ponto sai de um trecho do proprio fonte, nao de numeros soltos). Pede >= 60 pontos
  mu4   M-U4 navegacao: todo simbolo declarado (regex do fonte) em std\\*.fj e em examples\\*.fj aparece em `consultar simbolos` exatamente 1 vez, e nada alem disso aparece
        (o cg_search de .fj entra com o adaptador do CodeGraph, passo a18)
  mi3   M-I3 consultas rapidas: as 6 perguntas em engine\\forja (projeto de teste que importa engine:render) <= 300 ms, mediana de 5
  ma3x  confronto M-A3 x refs: para cada alvo que o `forja ast --projeto` liga (global, campo, membro, parametro, local), `consultar refs` devolve os MESMOS usos nas MESMAS posicoes
  uso   erros de uso: pergunta desconhecida (exit 2 com a lista), argumento que falta, posicao invalida, nome inexistente (exit 2), projeto com erro (exit 1), 2 execucoes = bytes iguais
Uso:
    python verificar_consultar.py --forja <forja.exe> [--caso mi1,mu4] [--jobs N]
    python verificar_consultar.py --forja <forja_antigo.exe> --esperar-vermelho     (exit 0 so se algum caso fica vermelho: prova que a regua discrimina)
    python verificar_consultar.py --selftest
Versao: 0.1.1
Changelog: 0.1.1 - IA1-S2b2a: mu4 junta std\\<m>.fj com os pedacos std\\<m>\\*.fj e confere que cada pedaco esta em modulos_std.c.
           0.1.0 - P-IA1-I2: primeira versao.
Verificacao: python verificar_consultar.py --selftest
"""

import argparse
import importlib.util
import json
import os
import re
import statistics
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LANG = AQUI.parents[1]
PROJ = AQUI / "proj"
ENGINE = LANG.parent / "engine"
MINIMO_PONTOS_M_I1 = 60
LIMITE_MS_M_I3 = 300
LIMITE_FALHAS = 8
EMBUTIDOS_SEM_DECL = ("print", "len", "assert", "panic", "expect")


def Rodar(Forja, Argumentos, Cwd=None, Env=None):
    return subprocess.run([str(Forja), *Argumentos], capture_output=True, timeout=180, stdin=subprocess.DEVNULL, cwd=Cwd, env=Env)


def JsonDe(Bytes):
    try:
        return json.loads(Bytes.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return None


def Norm(Caminho):
    return str(Caminho).replace("\\", "/").lower()


def Caso(Id, Valor, Meta, Falhas):
    return {"id": Id, "valor": Valor, "meta": Meta, "verde": not Falhas, "falhas": Falhas[:LIMITE_FALHAS]}


def Consultar(Forja, Argumentos, Cwd=PROJ):
    """(exit, json ou None, bytes do stdout, stderr) de `forja consultar ...` rodado em Cwd."""
    R = Rodar(Forja, ["consultar", *Argumentos], Cwd=str(Cwd))
    return R.returncode, JsonDe(R.stdout), R.stdout, R.stderr.decode("utf-8", "replace")


# ---------- posicoes a partir do proprio fonte ----------

def Pos(Arq, Trecho, Ocorrencia=1, Desloc=0, Base=PROJ):
    """[linha, coluna] (a partir de 1, em bytes) do caractere Desloc do trecho, na Ocorrencia-esima vez que ele aparece no arquivo."""
    Dados = (Base / Arq).read_bytes()
    Alvo, Indice = Trecho.encode("utf-8"), -1
    for _ in range(Ocorrencia):
        Indice = Dados.find(Alvo, Indice + 1)
        if Indice < 0:
            raise ValueError(f"{Arq}: '{Trecho}' nao tem {Ocorrencia} ocorrencias")
    Indice += Desloc
    Linha = Dados.count(b"\n", 0, Indice) + 1
    return [Linha, Indice - (Dados.rfind(b"\n", 0, Indice) + 1) + 1]


def Texto(Arq, Trecho, Ocorrencia=1, Desloc=0):
    L, C = Pos(Arq, Trecho, Ocorrencia, Desloc)
    return f"{Arq}:{L}:{C}"


def Absoluto(Arq):
    return str((PROJ / Arq).resolve()).replace("\\", "/")


def Confere(Esperado, Real, Caminho="$"):
    """Falhas de Real contra Esperado: dict = subconjunto das chaves; lista = mesmo tamanho e item a item; o resto, igualdade."""
    if isinstance(Esperado, dict):
        if not isinstance(Real, dict):
            return [f"{Caminho}: esperava objeto, veio {Real!r}"]
        return [F for K, V in Esperado.items() for F in (Confere(V, Real[K], f"{Caminho}.{K}") if K in Real else [f"{Caminho}: falta '{K}'"])]
    if isinstance(Esperado, list):
        if not isinstance(Real, list) or len(Real) != len(Esperado):
            return [f"{Caminho}: esperava {Esperado!r}, veio {Real!r}"]
        return [F for I, (E, R) in enumerate(zip(Esperado, Real)) for F in Confere(E, R, f"{Caminho}[{I}]")]
    return [] if Esperado == Real else [f"{Caminho}: esperava {Esperado!r}, veio {Real!r}"]


# ---------- gabarito do M-I1 ----------

def GabaritoTipo():
    """(rotulo, posicao 'arq:L:C', subconjunto esperado da resposta)."""
    M, F = "main.fj", "jogo/fisica.fj"
    return [
        ("tipo: nome a", Texto(M, "a + b", 1, 0), {"no": "nome", "tipo": "i32", "texto": "a"}),
        ("tipo: soma", Texto(M, "a + b", 1, 2), {"no": "binario", "tipo": "i32", "texto": "a + b"}),
        ("tipo: nome b", Texto(M, "a + b", 1, 4), {"no": "nome", "tipo": "i32"}),
        ("tipo: nome declarado por let", Texto(M, "total = a + b", 1, 0), {"no": "let", "tipo": "i32", "texto": "total"}),
        ("tipo: int da divisao", Texto(M, "a / b, a % b", 1, 0), {"no": "nome", "tipo": "int"}),
        ("tipo: chamada qualificada", Texto(M, "fis.aceleracao(c", 1, 0), {"no": "chamada", "tipo": "f32"}),
        ("tipo: constante qualificada", Texto(M, "fis.GRAVIDADE", 1, 6), {"no": "nome", "tipo": "f32"}),
        ("tipo: literal com tipo do contexto", Texto(M, "somar(1)", 1, 6), {"no": "int", "tipo": "i32", "texto": "1"}),
        ("tipo: conversao", Texto(M, "fis.proximo(r) as int", 1, 16), {"no": "conversao", "tipo": "int"}),
        ("tipo: endereco", Texto(M, "&mundo.corpos[0]", 1, 0), {"no": "unario", "tipo": "^Corpo"}),
        ("tipo: variavel local", Texto(M, "&mundo.corpos[0]", 1, 1), {"no": "nome", "tipo": "Mundo"}),
        ("tipo: campo array", Texto(M, "corpos[0]", 1, 2), {"no": "campo", "tipo": "[4]Corpo"}),
        ("tipo: indice", Texto(M, "corpos[0]", 1, 6), {"no": "indice", "tipo": "Corpo"}),
        ("tipo: nome de campo declarado", Texto(M, "corpos: [4]", 1, 0), {"no": "campo", "tipo": "[4]Corpo"}),
        ("tipo: parametro declarado", Texto(M, "proc somar(a", 1, 11), {"no": "param", "tipo": "i32"}),
        ("tipo: chamada com erro", Texto(M, "achar(3) or_else 0", 1, 0), {"no": "chamada", "tipo": "i32 ! Erro"}),
        ("tipo: or_else", Texto(M, "achar(3) or_else 0", 1, 10), {"no": "or_else", "tipo": "i32"}),
        ("tipo: ligacao do switch", Texto(M, "return k.vida", 1, 7), {"no": "nome", "tipo": "Corpo"}),
        ("tipo: campo de struct", Texto(M, "return k.vida", 1, 9), {"no": "campo", "tipo": "i32"}),
        ("tipo: outro modulo", Texto(F, "c.massa", 1, 3), {"no": "campo", "tipo": "f32", "texto": "c.massa"}),
        ("tipo: literal real", Texto(M, "dt = 0.5", 1, 5), {"no": "real", "tipo": "f32"}),
        ("tipo: fora de expressao", Texto(M, "// main.fj", 1, 3), {"no": None, "tipo": None}),
    ]


def GabaritoAssinatura():
    """(rotulo, argumentos de `consultar assinatura`, subconjunto esperado)."""
    P = lambda N, T, D=None: {"nome": N, "tipo": T, "padrao": D}  # noqa: E731
    return [
        ("assinatura: somar", ["somar"], {"params": [P("a", "i32"), P("b", "i32", "2")], "retorno": "i32", "erro": None, "retornos": [],
                                           "texto": "proc somar(a: i32, b: i32 = 2) -> i32", "categoria": "proc", "publico": False}),
        ("assinatura: retorno multiplo", ["dividir"], {"params": [P("a", "int"), P("b", "int")], "retornos": [{"nome": "q", "tipo": "int"}, {"nome": "r", "tipo": "int"}]}),
        ("assinatura: erro", ["achar"], {"retorno": "i32 ! Erro", "erro": "Erro"}),
        ("assinatura: ponteiro", ["mover"], {"params": [P("c", "^Corpo"), P("dt", "f32")], "retorno": "f32"}),
        ("assinatura: outro modulo", ["aceleracao"], {"modulo": "jogo/fisica", "publico": True, "params": [P("c", "^Corpo"), P("forca", "f32", "1.0")], "retorno": "f32"}),
        ("assinatura: std", ["arena_create"], {"modulo": "std:mem", "publico": True, "params": [P("block_size", "int", "65536")], "retorno": "Arena"}),
        ("assinatura: main", ["main"], {"params": [], "retorno": None, "entrada": "main"}),
        ("assinatura: foreign", ["forja_arena_criar"], {"categoria": "foreign", "externo": True, "params": [P("bloco", "int")], "retorno": "u32"}),
        ("assinatura: por id", ["jogo/fisica:cor_de"], {"id": "jogo/fisica:cor_de", "retorno": "Cor"}),
    ]


def GabaritoCampos():
    """(rotulo, argumentos de `consultar campos`, subconjunto esperado)."""
    C = lambda N, T, D=None: {"nome": N, "tipo": T, "padrao": D}  # noqa: E731
    return [
        ("campos: Mundo", ["Mundo"], {"categoria": "struct", "campos": [C("corpos", "[4]Corpo"), C("quadros", "i32", "0")]}),
        ("campos: Corpo", ["Corpo"], {"campos": [C("massa", "f32", "1.0"), C("cor", "Cor", "Cor.vermelho"), C("vida", "i32", "3")]}),
        ("campos: enum simples", ["Cor"], {"categoria": "enum", "membros": [{"nome": "vermelho", "valor": None}, {"nome": "azul", "valor": None}]}),
        ("campos: enum com valores", ["Nivel"], {"membros": [{"nome": "baixo", "valor": -1}, {"nome": "medio", "valor": 0}, {"nome": "alto", "valor": 1}]}),
        ("campos: union", ["Forma"], {"categoria": "union", "campos": [C(None, "Corpo"), C(None, "i32")]}),
        ("campos: std", ["Arena"], {"modulo": "std:mem", "campos": [C("id", "u32", "0")]}),
        ("campos: enum de erro", ["Erro"], {"membros": [{"nome": "cheio"}, {"nome": "vazio"}]}),
    ]


def UsosEsperados(*Posicoes):
    return sorted([Arq, *Pos_] for Arq, Pos_ in Posicoes)


def GabaritoRefs():
    """(rotulo, argumento de `consultar refs`, [(arquivo, [linha, coluna])] esperados, categoria do alvo)."""
    M, F = "main.fj", "jogo/fisica.fj"
    P = lambda Arq, *A: (Arq, Pos(Arq, *A))  # noqa: E731
    return [
        ("refs: proc local", "somar", [P(M, "somar(1)", 1), P(M, "somar(i", 1), P(M, "somar(1)", 2)], "proc"),
        ("refs: enum", "Erro", [P(M, "Erro.vazio", 1)], "enum"),
        ("refs: struct (literal)", "Mundo", [P(M, "Mundo{}", 1)], "struct"),
        ("refs: struct de outro modulo", "Corpo", [P(M, "fis.Corpo", 1), P(M, "fis.Corpo", 2), P(M, "case fis.Corpo", 1), P(F, "    Corpo,", 1, 4),
                                                     P(F, "c: ^Corpo", 1, 4), P(F, "c: Corpo) -> Cor", 1, 3), P(F, "c: Corpo) -> Nivel", 1, 3)], "struct"),
        ("refs: campo", "Corpo.vida", [P(M, "k.vida", 1), P(F, "c.vida", 1)], "campo"),
        ("refs: membro de enum", "Cor.azul", [P(M, "case fis.Cor.azul", 1)], "membro"),
        ("refs: parametro", "somar.b", [P(M, "a + b", 1, 4)], "param"),
        ("refs: local de let multiplo", "main.q", [P(M, "q + resto", 1)], "local"),
        ("refs: embutido", "print", [P(M, "print(1)", 1), P(M, "print(2)", 1), P(M, "print(f", 1), P(M, "print(achar", 1), P(M, "print(q", 1)], "embutido"),
        ("refs: constante de outro modulo", "GRAVIDADE", [P(M, "fis.GRAVIDADE", 1)], "const"),
        ("refs: proc de outro modulo", "proximo", [P(M, "fis.proximo", 1)], "proc"),
        ("refs: proc interno", "interno", [P(F, "interno(n)", 1)], "proc"),
        ("refs: local de test", "main:test:dividir.q", [P(M, "expect(q == 3)", 1, 7)], "local"),
    ]


def FalhasDeRefs(Forja, Rotulo, Arg, Esperados, Categoria):
    Cod, Doc, _, Err = Consultar(Forja, ["refs", Arg, "main.fj"])
    if Cod != 0 or Doc is None or len(Doc.get("alvos", [])) != 1:
        return [f"{Rotulo}: exit {Cod}, {Err.strip()[:120]}"]
    Alvo = Doc["alvos"][0]
    Reais = sorted([Norm(U["arquivo"]), *U["pos"]] for U in Alvo["usos"])
    Previstos = sorted([Norm(Absoluto(A)) if A != "main.fj" else "main.fj", *P] for A, P in Esperados)
    Falhas = [] if Reais == Previstos else [f"{Rotulo}: usos {Reais}, esperados {Previstos}"]
    Falhas += [] if Alvo["categoria"] == Categoria and Alvo["total"] == len(Esperados) else [f"{Rotulo}: categoria/total {Alvo['categoria']}/{Alvo['total']}"]
    return Falhas


def NomesDe(Lista, Chave="nome"):
    return [I[Chave] for I in Lista]


def FalhasDeEscopo(Forja, Rotulo, Posicao, Esperado, Alvo="main.fj", Globais=(), SemGlobais=()):
    Cod, Doc, _, Err = Consultar(Forja, ["escopo", Posicao, Alvo])
    if Cod != 0 or Doc is None:
        return [f"{Rotulo}: exit {Cod}, {Err.strip()[:120]}"]
    Falhas = Confere(Esperado, Doc, Rotulo)
    Nomes = {G["qualificado"] or G["nome"] for G in Doc["globais"]}
    Falhas += [f"{Rotulo}: global '{N}' deveria aparecer" for N in Globais if N not in Nomes]
    Falhas += [f"{Rotulo}: global '{N}' nao deveria aparecer" for N in SemGlobais if N in Nomes]
    return Falhas


def Loc(Nome, Cat, Tipo=None):
    Item = {"nome": Nome, "categoria": Cat}
    if Tipo:
        Item["tipo"] = Tipo
    return Item


def PontosDeEscopo(Forja):
    M, F = "main.fj", "jogo/fisica.fj"
    Dentro = ["somar", "Mundo", "fis.aceleracao", "fis.cor_de", "mem.arena_create", "mem.Arena", "fis.Corpo"]
    Pontos = [
        ("escopo: params e let", Texto(M, "return total", 1, 7), {"em": "main:somar", "locais": [Loc("a", "param", "i32"), Loc("b", "param", "i32"), Loc("total", "let", "i32")]}, M, Dentro, ["interno", "fis.interno"]),
        ("escopo: let ainda nao declarado", Texto(M, "let total", 1, 0), {"locais": [Loc("a", "param"), Loc("b", "param")]}, M, (), ()),
        ("escopo: dentro do inicializador", Texto(M, "let s = somar", 1, 8), {"locais": [Loc("mundo", "var"), Loc("r", "let"), Loc("i", "for")]}, M, (), ()),
        ("escopo: dentro do for", Texto(M, "mundo.quadros += s", 1, 0), {"locais": [Loc("mundo", "var"), Loc("r", "let"), Loc("i", "for"), Loc("s", "let", "i32")]}, M, (), ()),
        ("escopo: depois do for", Texto(M, "let q, resto", 1, 0), {"locais": [Loc("mundo", "var"), Loc("r", "let")]}, M, (), ()),
        ("escopo: ligacao do switch", Texto(M, "return k.vida", 1, 0), {"em": "main:descrever", "locais": [Loc("f", "param"), Loc("k", "switch", "Corpo")]}, M, (), ()),
        ("escopo: imports", Texto(M, "let r = somar(1)", 1, 0), {"importados": [{"apelido": "mem", "modulo": "std:mem"}, {"apelido": "fis", "modulo": "jogo/fisica"}]}, M, (), ["Corpo", "aceleracao"]),
        ("escopo: outro modulo", Texto(F, "return razao", 1, 7), {"em": "jogo/fisica:aceleracao", "modulo": "jogo/fisica", "importados": [], "locais": [Loc("c", "param"), Loc("forca", "param"), Loc("razao", "let", "f32")]}, M, ["interno", "Corpo"], ["somar", "fis.Corpo"]),
        ("escopo: dentro de um test", Texto(M, "expect(q == 3)", 1, 0), {"em": "main:test:dividir", "locais": [Loc("q", "let"), Loc("r", "let")]}, M, (), ()),
    ]
    return Pontos


def PontosDeSimbolos(Forja):
    """Falhas de `simbolos` (4 consultas) como uma lista de (rotulo, falhas)."""
    Saida = []
    Cod, Doc, _, _ = Consultar(Forja, ["simbolos", "main.fj"])
    Principal = [S for S in (Doc or {}).get("simbolos", []) if S["modulo"] == "main"]
    Esperado = [("Erro", "enum"), ("Mundo", "struct"), ("somar", "proc"), ("dividir", "proc"), ("achar", "proc"), ("mover", "proc"), ("descrever", "proc"), ("main", "proc")]
    Saida.append(("simbolos: modulo main", [] if Cod == 0 and [(S["nome"], S["categoria"]) for S in Principal] == Esperado else [f"main: {[(S['nome'], S['categoria']) for S in Principal]}"]))
    Ordem = [] if Cod != 0 else [S["modulo"] for S in Doc["simbolos"]]
    Saida.append(("simbolos: modulos em ordem de dependencia", [] if Ordem and Ordem[-1] == "main" and Ordem[0] != "main" else [f"ordem {Ordem[:3]}..{Ordem[-1:]}"]))
    Saida.append(("simbolos: entrada", [] if [S["nome"] for S in (Doc or {}).get("simbolos", []) if S["entrada"]] == ["main"] else ["so main e entrada"]))
    Cod, Doc, _, _ = Consultar(Forja, ["simbolos", "main.fj", "--modulo", "jogo/fisica"])
    Reais = [(S["nome"], S["categoria"], S["publico"]) for S in (Doc or {}).get("simbolos", [])]
    Previstos = [("GRAVIDADE", "const", True), ("DT", "const", True), ("Cor", "enum", True), ("Nivel", "enum", True), ("Corpo", "struct", True), ("Forma", "union", True),
                 ("aceleracao", "proc", True), ("cor_de", "proc", True), ("nivel_de", "proc", True), ("interno", "proc", False), ("proximo", "proc", True)]
    Saida.append(("simbolos: --modulo jogo/fisica", [] if Cod == 0 and Reais == Previstos else [f"{Reais}"]))
    Cod, Doc, _, _ = Consultar(Forja, ["simbolos", "main.fj", "--modulo", "std:mem"])
    Cats = {S["nome"]: S["categoria"] for S in (Doc or {}).get("simbolos", [])}
    Saida.append(("simbolos: --modulo std:mem", [] if Cats.get("arena_create") == "proc" and Cats.get("forja_arena_criar") == "foreign" and Cats.get("Arena") == "struct" else [str(Cats)[:200]]))
    Cod, _, _, _ = Consultar(Forja, ["simbolos", "main.fj", "--modulo", "nao/existe"])
    Saida.append(("simbolos: modulo inexistente = exit 2", [] if Cod == 2 else [f"exit {Cod}"]))
    return Saida


def PontosDeM_I1(Forja):
    """Lista de (rotulo, funcao() -> falhas) com todos os pontos de prova."""
    Pontos = []
    for Rotulo, Posicao, Esperado in GabaritoTipo():
        Pontos.append((Rotulo, lambda P=Posicao, E=Esperado, R=Rotulo: Confere(E, (Consultar(Forja, ["tipo", P, "main.fj"])[1] or {}), R)))
    for Rotulo, Args, Esperado in GabaritoAssinatura():
        Pontos.append((Rotulo, lambda A=Args, E=Esperado, R=Rotulo: Confere(E, (Consultar(Forja, ["assinatura", *A, "main.fj"])[1] or {}), R)))
    for Rotulo, Args, Esperado in GabaritoCampos():
        Pontos.append((Rotulo, lambda A=Args, E=Esperado, R=Rotulo: Confere(E, (Consultar(Forja, ["campos", *A, "main.fj"])[1] or {}), R)))
    for Rotulo, Arg, Usos, Cat in GabaritoRefs():
        Pontos.append((Rotulo, lambda R=Rotulo, A=Arg, U=Usos, C=Cat: FalhasDeRefs(Forja, R, A, U, C)))
    for Rotulo, Posicao, Esperado, Alvo, Com, Sem in PontosDeEscopo(Forja):
        Pontos.append((Rotulo, lambda R=Rotulo, P=Posicao, E=Esperado, A=Alvo, C=Com, S=Sem: FalhasDeEscopo(Forja, R, P, E, A, C, S)))
    for Rotulo, Falhas in PontosDeSimbolos(Forja):
        Pontos.append((Rotulo, lambda F=Falhas: F))
    return Pontos


def CasoMI1(Forja, Jobs):
    Pontos = PontosDeM_I1(Forja)
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Resultados = list(Pool.map(lambda P: P[1](), Pontos))
    Falhas = [F for R in Resultados for F in R]
    Bons = sum(1 for R in Resultados if not R)
    if len(Pontos) < MINIMO_PONTOS_M_I1:
        Falhas.append(f"so {len(Pontos)} pontos de prova, o minimo e {MINIMO_PONTOS_M_I1}")
    return Caso("mi1", f"{Bons}/{len(Pontos)} pontos", f"{len(Pontos)}/{len(Pontos)} (>= {MINIMO_PONTOS_M_I1})", Falhas)


# ---------- M-U4 ----------

DECL_TOPO = re.compile(r"^(?:pub\s+)?(?:export\s+)?(proc|struct|enum|union|const|unit)\s+([A-Za-z_][A-Za-z0-9_]*)", re.M)
DECL_FOREIGN = re.compile(r"^[ \t]+proc\s+([A-Za-z_][A-Za-z0-9_]*)", re.M)


def SimbolosDoFonte(Texto_):
    """Nomes declarados no fonte (topo e dentro de foreign), sem comentarios de linha."""
    Limpo = "\n".join(L.split("//")[0] if not L.lstrip().startswith("//") else "" for L in Texto_.splitlines())
    Nomes = [M.group(2) for M in DECL_TOPO.finditer(Limpo)]
    EmForeign = [M.group(1) for M in DECL_FOREIGN.finditer(Limpo) if "foreign" in Limpo]
    return sorted(Nomes + EmForeign)


def FontesDoModulo(Arquivo):
    """std\\<m>.fj mais os pedacos std\\<m>\\*.fj (S2b2a: o modulo std:<m> e a concatenacao deles)."""
    return [Arquivo] + sorted(P for P in (Arquivo.parent / Arquivo.stem).glob("*.fj") if P.name != "testes.fj")  # z5: testes.fj da subpasta e dos testes da std, nao do modulo


def FalhasDePedacos(Arquivo):
    """Todo pedaco std\\<m>\\*.fj tem de aparecer em frontend\\modulos_std.c (a ordem la e a do modulo)."""
    Tabela = (LANG / "compiler" / "frontend" / "modulos_std.c").read_text(encoding="utf-8")
    return [f"std:{Arquivo.stem}: o pedaco {P.name} nao esta em modulos_std.c" for P in FontesDoModulo(Arquivo)[1:] if f"/{Arquivo.stem}/{P.name}" not in Tabela]


def FalhasDeModulo(Forja, Raiz, Modulo, Arquivo):
    Cod, Doc, _, Err = Consultar(Forja, ["simbolos", str(Raiz), "--modulo", Modulo], Cwd=str(Raiz.parent))
    if Cod != 0 or Doc is None:
        return [f"{Modulo}: exit {Cod} {Err.strip()[:100]}"]
    Reais = sorted(S["nome"] for S in Doc["simbolos"])
    Previstos = sorted(N for F in FontesDoModulo(Arquivo) for N in SimbolosDoFonte(F.read_text(encoding="utf-8")))
    return [] if Reais == Previstos else [f"{Modulo}: so no indice {sorted(set(Reais) - set(Previstos))}, so no fonte {sorted(set(Previstos) - set(Reais))}"]


def CasoMU4(Forja, Jobs):
    Falhas, Total = [], 0
    Raiz = AQUI / "std_todas" / "main.fj"
    for A in sorted((LANG / "std").glob("*.fj")):
        Total += 1
        Falhas += FalhasDePedacos(A) + FalhasDeModulo(Forja, Raiz, f"std:{A.stem}", A)
    for A in sorted((LANG.parent / "examples").glob("*.fj")):
        if Rodar(Forja, ["check", str(A)]).returncode != 0:
            continue
        Total += 1
        Cod, Doc, _, _ = Consultar(Forja, ["simbolos", str(A), "--modulo", A.stem], Cwd=str(A.parent))
        Reais = sorted(S["nome"] for S in (Doc or {}).get("simbolos", []))
        Previstos = SimbolosDoFonte(A.read_text(encoding="utf-8"))
        Falhas += [] if Cod == 0 and Reais == Previstos else [f"{A.name}: indice {Reais} fonte {Previstos}"]
    return Caso("mu4", f"{Total - len(Falhas)}/{Total} modulos (std + examples)", f"{Total}/{Total}", Falhas + ([] if Total else ["nenhum modulo verificado"]))


# ---------- M-I3 ----------

def MainDoEngine(Pasta):
    Raiz = Pasta / "main.fj"
    Raiz.write_text('import "engine:render" as rd\n\nproc main() {\n    let e = rd.ErroRender.sem_iluminacao\n    switch e {\n    default:\n        print(1)\n    }\n}\n', encoding="utf-8")
    return Raiz


def MedianaMs(Forja, Argumentos, Env, Cwd):
    Tempos = []
    for _ in range(5):
        T0 = time.perf_counter()
        R = Rodar(Forja, ["consultar", *Argumentos], Cwd=Cwd, Env=Env)
        Tempos.append((time.perf_counter() - T0) * 1000)
        if R.returncode != 0:
            return None
    return statistics.median(Tempos)


def CasoMI3(Forja):
    import tempfile
    Env = dict(os.environ, FORJA_ENGINE=str(ENGINE))
    Falhas, Medidas = [], {}
    with tempfile.TemporaryDirectory(prefix="forja_mi3_") as Pasta:
        Raiz = MainDoEngine(Path(Pasta))
        Perguntas = {"simbolos": ["simbolos", "main.fj"], "escopo": ["escopo", "main.fj:4:9", "main.fj"], "refs": ["refs", "ErroRender", "main.fj"],
                     "tipo": ["tipo", "main.fj:4:9", "main.fj"], "assinatura": ["assinatura", "render_iniciar", "main.fj"], "campos": ["campos", "Render", "main.fj"]}
        for Nome, Args in Perguntas.items():
            Ms = MedianaMs(Forja, Args, Env, str(Raiz.parent))
            Medidas[Nome] = None if Ms is None else round(Ms)
            if Ms is None or Ms > LIMITE_MS_M_I3:
                Falhas.append(f"{Nome}: {Medidas[Nome]} ms (limite {LIMITE_MS_M_I3}; None = a consulta falhou)")
    return Caso("mi3", f"{Medidas} ms (mediana de 5)", f"cada uma <= {LIMITE_MS_M_I3} ms", Falhas)


# ---------- confronto M-A3 x refs ----------

def VerificarAst():
    Spec = importlib.util.spec_from_file_location("verificar_ast", LANG / "tests" / "ast" / "verificar_ast.py")
    Modulo = importlib.util.module_from_spec(Spec)
    sys.modules["verificar_ast"] = Modulo
    Spec.loader.exec_module(Modulo)
    return Modulo


def UsosDaAst(Doc, Ast):
    """{ref: [(arquivo, (linha, coluna), nome)]} dos nos com 'ref' (embutido fica de fora) e {id: (no, pai)} do documento."""
    Usos = {}
    for Modulo in Doc.get("modulos", [Doc]):
        for No, _, _ in Ast.PercorrerNos(Modulo["declaracoes"]):
            Embutido = No.get("ref_tipo") == "embutido" and No.get("no") == "chamada"  # o alvo de um embutido e o proprio nome (sem ':' no id)
            if "ref" in No or Embutido:
                Nome = No.get("nome") or No.get("tipo") or No.get("membro")
                Usos.setdefault(No["nome"] if Embutido else No["ref"], []).append((Norm(Modulo["arquivo"]), tuple(No["pos"]), Nome))
    return Usos, Ast.IndiceDeIds(Doc)


def ArgDoAlvo(Ref, Nome, Indice):
    """Argumento de `consultar refs` que alcanca o alvo 'Ref' do AST, e a posicao [linha, coluna] da declaracao do local (ou None)."""
    Base = re.split(r"/(?:corpo|params|campos|membros)\[", Ref, maxsplit=1)[0]  # o caminho do modulo tambem pode ter '/'
    if Base == Ref:
        return Ref, None
    Final = Ref.split("/")[-1]
    if Final.startswith(("campos[", "membros[", "params[")):
        return f"{Base}.{Indice[Ref][0]['nome']}", None
    return f"{Base}.{Nome}", Indice[Ref][0]["pos"]


def FalhasDoAlvo(Forja, Arq, Ref, Usos, Indice):
    Esperado = {(A, P) for A, P, _ in Usos}
    Nomes = {N for _, _, N in Usos if N}
    Real = set()
    for Nome in sorted(Nomes) if ArgDoAlvo(Ref, "x", Indice)[1] is not None else [None]:
        Arg, PosDecl = ArgDoAlvo(Ref, Nome, Indice)
        Cod, Doc, _, Err = Consultar(Forja, ["refs", Arg, str(Arq)], Cwd=str(Arq.parent))
        if Cod != 0 or Doc is None:
            return [f"{Arq.name}: refs {Arg}: exit {Cod} {Err.strip()[:100]}"]
        for Alvo in Doc["alvos"]:
            if PosDecl is None and Alvo["id"] != Ref or PosDecl is not None and Alvo.get("pos") != PosDecl:
                continue
            Real |= {(Norm(U["arquivo"]), tuple(U["pos"])) for U in Alvo["usos"]}
    if Real == Esperado:
        return []
    return [f"{Arq.name}: {Ref}: ast {sorted(Esperado - Real)[:3]} a mais, refs {sorted(Real - Esperado)[:3]} a mais"]


def FalhasDoArquivo(Forja, Ast, Arq):
    R = Rodar(Forja, ["ast", "--projeto", str(Arq)])
    Doc = JsonDe(R.stdout) if R.returncode == 0 else None
    if Doc is None:
        return [], 0
    Usos, Indice = UsosDaAst(Doc, Ast)
    Falhas = []
    for Ref, Lista in sorted(Usos.items()):
        Falhas += FalhasDoAlvo(Forja, Arq, Ref, Lista, Indice)
    return Falhas, sum(len(L) for L in Usos.values())


def CasoMA3X(Forja, Jobs):
    Ast = VerificarAst()
    Arquivos = [A for A in Ast.ListarFixtures() if Rodar(Forja, ["check", str(A)]).returncode == 0] + [PROJ / "main.fj"]
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Resultados = list(Pool.map(lambda A: FalhasDoArquivo(Forja, Ast, A), Arquivos))
    Falhas = [F for Fs, _ in Resultados for F in Fs]
    Usos = sum(N for _, N in Resultados)
    return Caso("ma3x", f"{Usos - min(len(Falhas), Usos)}/{Usos} usos em {len(Arquivos)} arquivos", f"{Usos}/{Usos}", Falhas + ([] if Usos else ["nenhum uso comparado"]))


# ---------- erros de uso ----------

def FalhasDeUso(Forja):
    Falhas = []
    Casos = [("pergunta desconhecida", ["foo"], 2), ("sem pergunta", [], 2), ("falta o argumento", ["refs"], 2), ("posicao mal formada", ["tipo", "main.fj:x:1", "main.fj"], 2),
             ("posicao fora do arquivo", ["tipo", "main.fj:9999:1", "main.fj"], 2), ("nome inexistente", ["refs", "nao_existe", "main.fj"], 2),
             ("assinatura de struct", ["assinatura", "Mundo", "main.fj"], 2), ("campos de proc", ["campos", "somar", "main.fj"], 2), ("opcao desconhecida", ["simbolos", "--x"], 2),
             ("--modulo fora de simbolos", ["refs", "somar", "--modulo", "main"], 2), ("arquivo inexistente", ["simbolos", "nao_existe.fj"], 2)]
    for Rotulo, Args, Esperado in Casos:
        Cod = Consultar(Forja, Args)[0]
        Falhas += [] if Cod == Esperado else [f"{Rotulo}: exit {Cod}, esperado {Esperado}"]
    Cod, _, _, Err = Consultar(Forja, ["foo"])
    Falhas += [] if all(P in Err for P in ("simbolos", "escopo", "refs", "tipo", "assinatura", "campos")) else ["pergunta desconhecida nao lista as 6 perguntas"]
    Falhas += FalhasDeProjetoComErro(Forja)
    A, B = Consultar(Forja, ["simbolos", "main.fj"])[2], Consultar(Forja, ["simbolos", "main.fj"])[2]
    Falhas += [] if A and A == B else ["2 execucoes de simbolos nao deram bytes iguais"]
    Falhas += [] if Consultar(Forja, ["simbolos", "--json", "main.fj"])[2] == A else ["--json mudou a resposta"]
    return Falhas


def FalhasDeProjetoComErro(Forja):
    import tempfile
    with tempfile.TemporaryDirectory(prefix="forja_uso_") as Pasta:
        Arq = Path(Pasta) / "main.fj"
        Arq.write_text("proc main() {\n    let x: i32 = nao_existe\n}\n", encoding="utf-8")
        Cod, Doc, Bruto, _ = Consultar(Forja, ["simbolos", str(Arq)], Cwd=Pasta)
        return [] if Cod == 1 and b"E0201" in Bruto else [f"projeto com erro: exit {Cod}, esperado 1 com o diagnostico do check"]


def CasoUso(Forja):
    Falhas = FalhasDeUso(Forja)
    return Caso("uso", "ok" if not Falhas else f"{len(Falhas)} falhas", "ok", Falhas)


# ---------- orquestracao ----------

def RodarCasos(Forja, Quais, Jobs):
    Tabela = {"mi1": lambda: CasoMI1(Forja, Jobs), "mu4": lambda: CasoMU4(Forja, Jobs), "mi3": lambda: CasoMI3(Forja), "ma3x": lambda: CasoMA3X(Forja, Jobs), "uso": lambda: CasoUso(Forja)}
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

    Exigir("Confere subconjunto", Confere({"a": 1}, {"a": 1, "b": 2}) == [])
    Exigir("Confere detecta valor", len(Confere({"a": 1}, {"a": 2})) == 1)
    Exigir("Confere detecta chave", len(Confere({"z": 1}, {"a": 2})) == 1)
    Exigir("Confere lista exata", Confere([1, 2], [1]) != [] and Confere([1, 2], [1, 2]) == [])
    Exigir("Confere None", Confere({"a": None}, {"a": None}) == [] and Confere({"a": None}, {"a": 0}) != [])
    Exigir("Pos acha linha e coluna", Pos("main.fj", "import \"std:mem\"") == [2, 1])
    Exigir("Pos desloca", Pos("main.fj", "import \"std:mem\"", 1, 7) == [2, 8])
    Exigir("Texto monta arq:L:C", Texto("main.fj", "import \"std:mem\"") == "main.fj:2:1")
    Exigir("SimbolosDoFonte topo e foreign", SimbolosDoFonte("pub struct A {\n}\nforeign {\n    proc f(x: i32)\n}\nproc g() {\n}\n// proc h\n") == ["A", "f", "g"])
    Exigir("ArgDoAlvo global", ArgDoAlvo("m:f", None, {}) == ("m:f", None))
    Exigir("ArgDoAlvo param", ArgDoAlvo("m:f/params[0]", None, {"m:f/params[0]": ({"nome": "a"}, None)}) == ("m:f.a", None))
    Exigir("ArgDoAlvo local", ArgDoAlvo("m:f/corpo[0]", "x", {"m:f/corpo[0]": ({"pos": [3, 5]}, None)}) == ("m:f.x", [3, 5]))
    Exigir("ArgDoAlvo modulo com barra", ArgDoAlvo("jogo/m:f/corpo[1]/casos[0]", "k", {"jogo/m:f/corpo[1]/casos[0]": ({"pos": [4, 5]}, None)}) == ("jogo/m:f.k", [4, 5]))
    Exigir("Norm", Norm("C:\\A\\b.fj") == "c:/a/b.fj")
    Exigir("gabaritos grandes o bastante", len(GabaritoTipo()) + len(GabaritoAssinatura()) + len(GabaritoCampos()) + len(GabaritoRefs()) + 9 + 6 >= MINIMO_PONTOS_M_I1)
    Exigir("Caso vermelho", not Caso("x", "", "", ["f"])["verde"] and Caso("x", "", "", [])["verde"])
    for E in Erros:
        print(json.dumps({"selftest": E, "verde": False}))
    print(json.dumps({"selftest": "verificar_consultar", "verde": not Erros, "falhas": Erros}))
    return 0 if not Erros else 1


def Principal():
    Args = argparse.ArgumentParser(description="P-IA1-I2: forja consultar")
    Args.add_argument("--forja")
    Args.add_argument("--caso", default="mi1,mu4,mi3,ma3x,uso")
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
