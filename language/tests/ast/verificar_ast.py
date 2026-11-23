# verificar_ast.py -- prova do P-IA1-I1: AST 1.0 do `forja ast` (ids estruturais, "fim", "ref"): M-A1..M-A4
"""Casos (cada um imprime uma linha JSON {id, valor, meta, verde, falhas}; exit 1 se algum nao e verde):
  ma1   M-A1 AST estavel: 100% dos .fj ok_ (semantica, sintaxe, dourado, regressao), examples e std validam em tests\\ast\\esquema_ast.json (validador proprio, sem lib),
        com ids unicos e estruturais (o id do filho comeca com o do pai), pos <= fim e filho dentro do pai; 2 execucoes = bytes iguais; o projeto inteiro (--projeto) tambem valida
  ma2   M-A2 ids estaveis: linha em branco, comentario e proc novo inseridos antes da 1a e da ultima declaracao (6 edicoes por arquivo, >= 40 casos): nenhuma declaracao
        existente muda de id nem de conteudo (sem posicoes), e a declaracao seguinte realmente andou
  ma3   M-A3 ligacao: 100% dos nos "nome" e "chamada" de arquivo que passa no check trazem "ref" (a declaracao existe no documento, no modulo dito e com o mesmo nome) ou
        ref_tipo "embutido"; `_ = x` (descarte) nao e uso. O confronto com `forja consultar refs` fica para o passo I2 (o comando ainda nao existe)
  ma4   M-A4 recorte fiel: o texto entre "pos" e "fim" de cada declaracao e de cada comando, reparseado, da a MESMA subarvore (sem posicoes, ids e ligacoes)
  ma4e  o mesmo para cada expressao (alem do que o M-A4 pede)
Uso:
    python verificar_ast.py --forja <forja.exe> [--caso ma1,ma3] [--jobs N]
    python verificar_ast.py --forja <forja_antigo.exe> --esperar-vermelho     (exit 0 so se algum caso fica vermelho: prova que a regua discrimina)
    python verificar_ast.py --selftest
Versao: 0.1.0
Changelog: 0.1.0 - P-IA1-I1: primeira versao.
Verificacao: python verificar_ast.py --selftest
"""

import argparse
import json
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

LANG = Path(__file__).resolve().parents[2]
ESQUEMA = Path(__file__).resolve().parent / "esquema_ast.json"
PASTAS_OK = [(LANG / "tests" / "semantica", "ok_*.fj"), (LANG / "tests" / "sintaxe", "ok_*.fj"), (LANG / "tests" / "dourado", "ok_*.fj"),
             (LANG / "tests" / "regressao", "ok_*.fj"), (LANG.parent / "examples", "*.fj"), (LANG / "std", "*.fj")]
VERSAO_AST = "1.0.0"
MINIMO_CASOS_M_A2 = 40
ARQUIVOS_M_A2 = 10
STMT_TIPOS = {"let", "var", "atribuicao", "if", "while", "for", "return", "chamada", "break", "continue", "defer", "switch", "expect", "let_multiplo"}
EXPR_TIPOS = {"int", "real", "bool", "texto", "nome", "campo", "chamada", "literal_struct", "unario", "binario", "conversao", "try", "or_else", "none", "indice",
              "fatia", "literal_array", "tupla", "ftexto"}
CHAVES_CORPO = ("corpo", "entao", "senao", "comando")  # "comando": o comando adiado do defer
CHAVES_POSICAO = {"pos", "fim", "pos_nome", "op_pos", "arquivo"}
CHAVES_LIGACAO = {"id", "ref", "ref_modulo", "ref_tipo", "ref_import"}
EDICOES = {"linha_em_branco": [b""], "comentario": [b"// linha inserida pelo verificar_ast"], "proc_novo": [b"proc zz_ia1_novo() {", b"}"]}
LIMITE_FALHAS = 8

sys.setrecursionlimit(20000)


def Rodar(Forja, Argumentos):
    return subprocess.run([str(Forja), *Argumentos], capture_output=True, timeout=180, stdin=subprocess.DEVNULL)


def JsonDe(Bytes):
    try:
        return json.loads(Bytes.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return None


# ---------- validador de esquema (so as palavras-chave que esquema_ast.json usa) ----------

def TipoJsonDe(Dado):
    if Dado is None:
        return "null"
    if isinstance(Dado, bool):
        return "boolean"
    if isinstance(Dado, int):
        return "integer"
    if isinstance(Dado, float):
        return "number"
    return {str: "string", list: "array", dict: "object"}[type(Dado)]


def TipoConfere(Dado, Tipo):
    Tipos = Tipo if isinstance(Tipo, list) else [Tipo]
    Real = TipoJsonDe(Dado)
    return Real in Tipos or (Real == "integer" and "number" in Tipos)


def ValidarFolha(Dado, Esq, Caminho):
    """type, const, enum, pattern e minimum de um valor."""
    Erros = []
    if "type" in Esq and not TipoConfere(Dado, Esq["type"]):
        return [f"{Caminho}: esperava {Esq['type']}, veio {TipoJsonDe(Dado)}"]
    if "const" in Esq and (Dado != Esq["const"] or TipoJsonDe(Dado) != TipoJsonDe(Esq["const"])):
        Erros.append(f"{Caminho}: esperava {Esq['const']!r}, veio {Dado!r}")
    if "enum" in Esq and Dado not in Esq["enum"]:
        Erros.append(f"{Caminho}: {Dado!r} fora de {Esq['enum']}")
    if "pattern" in Esq and isinstance(Dado, str) and not re.search(Esq["pattern"], Dado):
        Erros.append(f"{Caminho}: {Dado!r} nao casa {Esq['pattern']}")
    if "minimum" in Esq and isinstance(Dado, (int, float)) and not isinstance(Dado, bool) and Dado < Esq["minimum"]:
        Erros.append(f"{Caminho}: {Dado} < {Esq['minimum']}")
    return Erros


def ValidarObjeto(Dado, Esq, Raiz, Caminho):
    Erros = [f"{Caminho}: falta a chave '{K}'" for K in Esq.get("required", []) if K not in Dado]
    Props = Esq.get("properties", {})
    Extra = Esq.get("additionalProperties", True)
    for Chave, Valor in Dado.items():
        if Chave in Props:
            Erros += Validar(Valor, Props[Chave], Raiz, f"{Caminho}.{Chave}")
        elif Extra is False:
            Erros.append(f"{Caminho}: chave inesperada '{Chave}'")
        elif isinstance(Extra, dict):
            Erros += Validar(Valor, Extra, Raiz, f"{Caminho}.{Chave}")
    return Erros


def ValidarLista(Dado, Esq, Raiz, Caminho):
    Erros = []
    if len(Dado) < Esq.get("minItems", 0):
        Erros.append(f"{Caminho}: tem {len(Dado)} itens, pede {Esq['minItems']}")
    if len(Dado) > Esq.get("maxItems", len(Dado)):
        Erros.append(f"{Caminho}: tem {len(Dado)} itens, aceita {Esq['maxItems']}")
    if "items" in Esq:
        for I, Item in enumerate(Dado):
            Erros += Validar(Item, Esq["items"], Raiz, f"{Caminho}[{I}]")
    return Erros


def ValidarComposicao(Dado, Esq, Raiz, Caminho):
    """allOf, oneOf e if/then/else."""
    Erros = []
    for Sub in Esq.get("allOf", []):
        Erros += Validar(Dado, Sub, Raiz, Caminho)
    if "oneOf" in Esq:
        Ramos = [Validar(Dado, Sub, Raiz, Caminho) for Sub in Esq["oneOf"]]
        if [R for R in Ramos if not R].__len__() != 1:
            Erros += min(Ramos, key=len) if all(Ramos) else [f"{Caminho}: mais de um ramo do oneOf casa"]
    if "if" in Esq:
        Ramo = "then" if not Validar(Dado, Esq["if"], Raiz, Caminho) else "else"
        if Ramo in Esq:
            Erros += Validar(Dado, Esq[Ramo], Raiz, Caminho)
    return Erros


def Validar(Dado, Esq, Raiz, Caminho="$"):
    """Lista de erros de Dado contra Esq (vazia = valido); Raiz resolve os $ref '#/$defs/x'."""
    if "$ref" in Esq:
        Alvo = Raiz
        for Parte in Esq["$ref"].lstrip("#/").split("/"):
            Alvo = Alvo[Parte]
        return Validar(Dado, Alvo, Raiz, Caminho)
    Erros = ValidarFolha(Dado, Esq, Caminho)
    if Erros and "type" in Esq and not TipoConfere(Dado, Esq["type"]):
        return Erros
    if isinstance(Dado, dict):
        Erros += ValidarObjeto(Dado, Esq, Raiz, Caminho)
    if isinstance(Dado, list):
        Erros += ValidarLista(Dado, Esq, Raiz, Caminho)
    return Erros + ValidarComposicao(Dado, Esq, Raiz, Caminho)


# ---------- percurso e julgamentos puros (o selftest planta falhas neles) ----------

def PercorrerNos(Raiz):
    """Gera (no, pai, chave) para todo objeto com 'id'; pai = o no com 'id' mais proximo acima (ou None); chave = a chave do objeto que o contem."""
    Pilha = [(Raiz, None, None)]
    while Pilha:
        Valor, Pai, Chave = Pilha.pop()
        if isinstance(Valor, list):
            Pilha.extend((Item, Pai, Chave) for Item in Valor)
        elif isinstance(Valor, dict):
            if "id" in Valor:
                yield Valor, Pai, Chave
                Pai = Valor
            Pilha.extend((Filho, Pai, K) for K, Filho in Valor.items())


def ModulosDoDocumento(Doc):
    if "modulos" in Doc:
        return [(M["modulo"], M["declaracoes"]) for M in Doc["modulos"]]
    return [(Doc["modulo"], Doc["declaracoes"])]


def ErrosDeEsquema(Doc, Esquema):
    return Validar(Doc, Esquema, Esquema)


def ErrosDeIds(Doc):
    Vistos, Erros = set(), []
    for Modulo, Decls in ModulosDoDocumento(Doc):
        for No, Pai, _ in PercorrerNos(Decls):
            Id = No["id"]
            if Id in Vistos:
                Erros.append(f"id repetido: {Id}")
            Vistos.add(Id)
            Prefixo = Pai["id"] + "/" if Pai else Modulo + ":"
            if not Id.startswith(Prefixo):
                Erros.append(f"id {Id} nao comeca com {Prefixo}")
    return Erros


def ErrosDePosicao(Doc):
    Erros = []
    for _, Decls in ModulosDoDocumento(Doc):
        for No, Pai, _ in PercorrerNos(Decls):
            Pos, Fim = tuple(No["pos"]), tuple(No["fim"])
            if Pos > Fim:
                Erros.append(f"{No['id']}: pos {Pos} depois de fim {Fim}")
            if Pai and (Pos < tuple(Pai["pos"]) or Fim > tuple(Pai["fim"])):
                Erros.append(f"{No['id']}: [{Pos}, {Fim}) fora do pai {Pai['id']} [{tuple(Pai['pos'])}, {tuple(Pai['fim'])})")
    return Erros


def JulgarDocumento(Doc, Esquema):
    """M-A1 numa AST: esquema primeiro; so se passa, a estrutura dos ids e as posicoes."""
    Erros = ErrosDeEsquema(Doc, Esquema)
    if Erros:
        return Erros
    return ErrosDeIds(Doc) + ErrosDePosicao(Doc)


def IndiceDeIds(Doc):
    return {No["id"]: (No, Pai) for _, Decls in ModulosDoDocumento(Doc) for No, Pai, _ in PercorrerNos(Decls)}


def NomesDoNo(No):
    Nomes = [No.get("nome"), No.get("membro"), No.get("tipo")]
    return {N.split(".")[-1] for N in Nomes if isinstance(N, str)}


def NomesDoAlvo(Alvo, Pai):
    """Nomes que a declaracao alvo introduz: nome, var/item/indice do for, nomes do let multiplo e a 'liga' do switch (o case que liga a declara via o pai)."""
    Nomes = {Alvo.get(K) for K in ("nome", "var", "item", "indice")}
    Nomes |= set(Alvo.get("nomes", []))
    if Pai is not None and "casos" in Pai:
        Nomes.add(Pai.get("liga"))
    return Nomes


PALAVRAS_SEM_REF = ("heap", "frame", "scope", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "int", "uint", "f32", "f64", "bool", "str", "cstr", "rawptr")


def ErrosDoRef(No, Indice, ObrigaRef):
    if No.get("ref_tipo") == "embutido":
        return [f"{No['id']}: embutido com ref" ] if "ref" in No else []
    if "ref" not in No:
        if No.get("nome") in PALAVRAS_SEM_REF:  # z5: heap/frame/scope (alloc = ...) e tipos primitivos (transmute(x, to = u64)) sao palavras da Forja, sem declaracao para ligar
            return []
        return [f"{No['id']}: uso de '{No.get('nome')}' sem ref"] if ObrigaRef else []
    Achado = Indice.get(No["ref"])
    if Achado is None:
        return [f"{No['id']}: ref {No['ref']} nao existe no documento"]
    if not No["ref"].startswith(No.get("ref_modulo", "\0") + ":"):
        return [f"{No['id']}: ref_modulo {No.get('ref_modulo')!r} nao e o modulo de {No['ref']}"]
    Alvo, Pai = Achado
    if not NomesDoAlvo(Alvo, Pai) & NomesDoNo(No):
        return [f"{No['id']}: ref {No['ref']} introduz {sorted(N for N in NomesDoAlvo(Alvo, Pai) if N)}, o uso e {sorted(NomesDoNo(No))}"]
    return []


def JulgarRefs(Doc):
    """M-A3: (erros, usos). Uso = no 'nome' ou 'chamada' com 'nome' (o descarte '_' nao conta). Todo 'ref' tem de apontar para uma declaracao do documento."""
    Indice, Erros, Usos = IndiceDeIds(Doc), [], 0
    for _, Decls in ModulosDoDocumento(Doc):
        for No, _, _ in PercorrerNos(Decls):
            Uso = No.get("no") in ("nome", "chamada") and "nome" in No and No["nome"] != "_"
            Usos += Uso
            Erros += ErrosDoRef(No, Indice, Uso)
    return Erros, Usos


def SemPosicoes(Valor):
    if isinstance(Valor, list):
        return [SemPosicoes(X) for X in Valor]
    if isinstance(Valor, dict):
        return {K: SemPosicoes(V) for K, V in Valor.items() if K not in CHAVES_POSICAO}
    return Valor


def Estrutura(Valor, Externo=False):
    """A subarvore sem posicoes, ids e ligacoes (o que o reparse do recorte tem de repetir). Externo: item de foreign (publico/exportado vem do bloco, nao do recorte)."""
    Tira = CHAVES_POSICAO | CHAVES_LIGACAO | ({"publico", "exportado"} if Externo else set())
    if isinstance(Valor, list):
        return [Estrutura(X) for X in Valor]
    if isinstance(Valor, dict):
        return {K: Estrutura(V) for K, V in Valor.items() if K not in Tira}
    return Valor


def JulgarIdsEstaveis(Antes, Depois, Novo=None):
    """M-A2: toda declaracao de Antes existe em Depois com o mesmo id e o mesmo conteudo (sem posicoes); Novo = id que a edicao criou (tem de existir 1 vez)."""
    Erros = []
    Mapa = {D.get("id"): D for D in Depois["declaracoes"]}
    if not Antes["declaracoes"]:
        return ["documento sem declaracoes"]
    for Decl in Antes["declaracoes"]:
        if "id" not in Decl:
            return ["a declaracao nao tem id"]
        Igual = Mapa.get(Decl["id"])
        if Igual is None:
            Erros.append(f"o id {Decl['id']} sumiu")
        elif SemPosicoes(Igual) != SemPosicoes(Decl):
            Erros.append(f"o conteudo de {Decl['id']} mudou")
    if Novo is not None and sum(1 for D in Depois["declaracoes"] if D.get("id") == Novo) != 1:
        Erros.append(f"a declaracao nova {Novo} nao saiu uma vez")
    return Erros


def Recortar(Linhas, Pos, Fim):
    """Os bytes de [Pos, Fim) numa fonte dividida em linhas (coluna 1-based em bytes, fim exclusivo)."""
    (L1, C1), (L2, C2) = Pos, Fim
    if L1 == L2:
        return Linhas[L1 - 1][C1 - 1:C2 - 1]
    return b"\n".join([Linhas[L1 - 1][C1 - 1:]] + Linhas[L1:L2 - 1] + [Linhas[L2 - 1][:C2 - 1]])


def ItensDeRecorte(Doc):
    """(tipo, no) de cada declaracao, comando e expressao do documento de um arquivo."""
    Itens = [("decl", D) for D in Doc["declaracoes"]]
    for No, Pai, Chave in PercorrerNos(Doc["declaracoes"]):
        if Pai is None:
            continue
        if No.get("no") in STMT_TIPOS and Chave in CHAVES_CORPO:
            Itens.append(("stmt", No))
        elif No.get("no") in EXPR_TIPOS:
            Itens.append(("expr", No))
    return Itens


def DeclEhExterna(No):
    return No.get("externo") is True or No.get("no") == "link"


def TrechoDoLote(Tipo, No, Texto, Indice):
    """(texto do trecho, 0 normal | 1 foreign | 2 proc) que reparsea o recorte no contexto em que ele vale."""
    if Tipo in ("decl", "ctx"):
        return (b"foreign {\n" + Texto + b"\n}", 1) if DeclEhExterna(No) else (Texto, 0)
    Cabeca = b"proc zz_r%d() {\n" % Indice
    if Tipo == "stmt":
        return Cabeca + Texto + b"\n}", 2
    Prefixo = b"return " if No["no"] == "tupla" else b"let zz_v = "  # a tupla sem parenteses so existe depois de return
    return Cabeca + Prefixo + Texto + b"\n}", 2


def MontarLote(Itens, Linhas):
    """(fonte do lote, itens na ordem em que saem do parser): declaracoes normais, foreign, depois um proc por comando/expressao."""
    Grupos = {0: [], 1: [], 2: []}
    for Indice, (Tipo, No) in enumerate(Itens):
        Texto = Recortar(Linhas, No["pos"], No["fim"])
        Trecho, Grupo = TrechoDoLote(Tipo, No, Texto, Indice)
        Grupos[Grupo].append((Trecho, Tipo, No))
    Ordem = Grupos[0] + Grupos[1] + Grupos[2]
    return b"\n".join(T for T, _, _ in Ordem) + b"\n", [(Tipo, No) for _, Tipo, No in Ordem]


def AlvoDoReparse(Tipo, Decl):
    """O no reparseado que vale como o original: a propria declaracao, ou o 1o comando / o valor do 1o let do proc embrulhado."""
    if Tipo == "decl":
        return Decl
    Corpo = Decl["corpo"][0]
    return Corpo if Tipo == "stmt" else Corpo["valor"]


def JulgarLote(Ordem, Doc):
    """(falhas, total): cada item da Ordem comparado com a declaracao de mesmo indice do Doc reparseado."""
    Decls = Doc["declaracoes"]
    if len(Decls) != len(Ordem):
        return [f"o lote deu {len(Decls)} declaracoes, esperava {len(Ordem)}"], len(Ordem)
    Falhas = []
    for (Tipo, No), Decl in zip(Ordem, Decls):
        if Tipo == "ctx":
            continue
        Novo = AlvoDoReparse(Tipo, Decl)
        if Estrutura(Novo, DeclEhExterna(No)) != Estrutura(No, DeclEhExterna(No)):
            Falhas.append(f"{No['id']}: o recorte reparseado da outra subarvore")
    return Falhas, sum(1 for Tipo, _ in Ordem if Tipo != "ctx")


# ---------- coleta (executa o forja) ----------

def ListarFixtures():
    return [A for Pasta, Padrao in PASTAS_OK for A in sorted(Pasta.glob(Padrao))]


def ColetarArquivo(Forja, Arq):
    Dados = Arq.read_bytes()
    Unico, Repetido = Rodar(Forja, ["ast", str(Arq)]), Rodar(Forja, ["ast", str(Arq)])
    Limpo = Rodar(Forja, ["check", str(Arq)]).returncode == 0
    Projeto = Rodar(Forja, ["ast", "--projeto", str(Arq)]) if Limpo else None
    return {"arq": Arq, "dados": Dados, "unico": Unico, "repetido": Repetido, "projeto": Projeto, "limpo": Limpo, "doc": JsonDe(Unico.stdout)}


def Coletar(Forja, Jobs):
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        return list(Pool.map(lambda A: ColetarArquivo(Forja, A), ListarFixtures()))


def LoteDeUmArquivo(Forja, Reg, Tipos):
    """Falhas e total do reparse dos itens dos Tipos de um arquivo, em 1 lote (se o lote nao parseia, item a item para achar o culpado)."""
    Todos = ItensDeRecorte(Reg["doc"])
    Itens = [I for I in Todos if I[0] in Tipos]
    if not Itens:
        return [], 0
    # os imports vao junto como contexto: sem eles 'mod.f()' reparsea como campo de uma variavel, nao como chamada do modulo
    Contexto = [] if "decl" in Tipos else [("ctx", D) for T, D in Todos if T == "decl" and D["no"] == "import"]
    Linhas = Reg["dados"].split(b"\n")
    with tempfile.TemporaryDirectory(prefix="forja_ast_") as Pasta:
        Fonte, Ordem = MontarLote(Contexto + Itens, Linhas)
        Doc = RodarFonte(Forja, Pasta, Fonte)
        if Doc is not None:
            return JulgarLote(Ordem, Doc)
        Falhas = []
        for Item in Itens:
            Um, UmOrdem = MontarLote(Contexto + [Item], Linhas)
            Doc = RodarFonte(Forja, Pasta, Um)
            Falhas += [f"{Item[1]['id']}: o recorte nao parseia"] if Doc is None else JulgarLote(UmOrdem, Doc)[0]
        return Falhas, len(Itens)


def RodarFonte(Forja, Pasta, Fonte):
    Caminho = Path(Pasta) / "lote.fj"
    Caminho.write_bytes(Fonte)
    Saida = Rodar(Forja, ["ast", str(Caminho)])
    return JsonDe(Saida.stdout) if Saida.returncode == 0 else None


# ---------- casos ----------

def Caso(Id, Valor, Meta, Falhas):
    return {"id": Id, "valor": Valor, "meta": Meta, "verde": not Falhas, "falhas": Falhas[:LIMITE_FALHAS]}


def FalhasDoRegistro(Reg, Esquema):
    Nome = Reg["arq"].name
    Falhas = []
    if Reg["unico"].returncode != 0 or Reg["doc"] is None:
        return [f"{Nome}: forja ast saiu {Reg['unico'].returncode} sem JSON"]
    if Reg["unico"].stdout != Reg["repetido"].stdout:
        Falhas.append(f"{Nome}: duas execucoes deram bytes diferentes")
    Falhas += [f"{Nome}: {E}" for E in JulgarDocumento(Reg["doc"], Esquema)[:3]]
    if Reg["projeto"] is not None:
        Doc = JsonDe(Reg["projeto"].stdout)
        Erros = [f"projeto saiu {Reg['projeto'].returncode}"] if Doc is None else JulgarDocumento(Doc, Esquema)[:3]
        Falhas += [f"{Nome}: {E}" for E in Erros]
    return Falhas


def CasoMA1(Registros, Esquema):
    Falhas = []
    Bons = 0
    for Reg in Registros:
        Novas = FalhasDoRegistro(Reg, Esquema)
        Bons += not Novas
        Falhas += Novas
    return Caso("ma1", f"{Bons}/{len(Registros)}", f"{len(Registros)}/{len(Registros)}", Falhas)


def CasoMA3(Registros):
    Falhas, Usos, Arquivos = [], 0, 0
    for Reg in Registros:
        if Reg["projeto"] is None or Reg["projeto"].returncode != 0:
            continue
        Doc = JsonDe(Reg["projeto"].stdout)
        if Doc is None:
            Falhas.append(f"{Reg['arq'].name}: --projeto sem JSON")
            continue
        Erros, N = JulgarRefs(Doc)
        Usos += N
        Arquivos += 1
        Falhas += [f"{Reg['arq'].name}: {E}" for E in Erros]
    Sem = len(Falhas)
    return Caso("ma3", f"{Usos - min(Sem, Usos)}/{Usos} usos em {Arquivos} arquivos", f"{Usos}/{Usos}", Falhas + ([] if Usos else ["nenhum uso de nome verificado"]))


def ProvarEdicao(Forja, Reg, Tipo, Onde):
    """Insere Tipo antes da primeira/ultima declaracao do arquivo e devolve as falhas do M-A2."""
    Doc = Reg["doc"]
    Alvo = Doc["declaracoes"][0] if Onde == "inicio" else Doc["declaracoes"][-1]
    Linha, Eol = Alvo["pos"][0], (b"\r" if b"\r\n" in Reg["dados"] else b"")
    Nova = [T + Eol for T in EDICOES[Tipo]]
    Linhas = Reg["dados"].split(b"\n")
    Linhas[Linha - 1:Linha - 1] = Nova
    with tempfile.TemporaryDirectory(prefix="forja_ast_") as Pasta:
        Caminho = Path(Pasta) / Reg["arq"].name
        Caminho.write_bytes(b"\n".join(Linhas))
        Depois = JsonDe(Rodar(Forja, ["ast", str(Caminho)]).stdout)
    Nome = f"{Reg['arq'].name}/{Tipo}@{Onde}"
    if Depois is None:
        return [f"{Nome}: a AST editada nao saiu"]
    Novo = f"{Doc['modulo']}:zz_ia1_novo" if Tipo == "proc_novo" else None
    Falhas = [f"{Nome}: {E}" for E in JulgarIdsEstaveis(Doc, Depois, Novo)]
    Moveu = next((D for D in Depois["declaracoes"] if D["id"] == Alvo["id"]), None)
    if Moveu is not None and Moveu["pos"][0] != Linha + len(Nova):
        Falhas.append(f"{Nome}: a declaracao {Alvo['id']} nao andou {len(Nova)} linha(s) ({Moveu['pos'][0]} em vez de {Linha + len(Nova)})")
    return Falhas


def ElegiveisM_A2(Registros):
    Bons = [R for R in Registros if R["doc"] is not None and R["unico"].returncode == 0 and not R["dados"].startswith(b"\xef\xbb\xbf")
            and len(R["doc"]["declaracoes"]) >= 3 and all(D["no"] != "import" for D in R["doc"]["declaracoes"])
            and R["arq"].parent.name in ("semantica", "sintaxe")]
    return Bons[:ARQUIVOS_M_A2]


def CasoMA2(Forja, Registros, Jobs):
    Pedidos = [(R, T, O) for R in ElegiveisM_A2(Registros) for T in EDICOES for O in ("inicio", "fim")]
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Resultados = list(Pool.map(lambda P: ProvarEdicao(Forja, *P), Pedidos))
    Falhas = [F for R in Resultados for F in R]
    Bons = sum(1 for R in Resultados if not R)
    if len(Pedidos) < MINIMO_CASOS_M_A2:
        Falhas.append(f"so {len(Pedidos)} casos, o contrato pede {MINIMO_CASOS_M_A2}")
    return Caso("ma2", f"{Bons}/{len(Pedidos)}", f"{MINIMO_CASOS_M_A2}/{MINIMO_CASOS_M_A2}+", Falhas)


def CasoRecorte(Forja, Registros, Jobs, Definicao):
    Id, Tipos = Definicao
    Uteis = [R for R in Registros if R["doc"] is not None and R["unico"].returncode == 0]
    with ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Resultados = list(Pool.map(lambda R: LoteDeUmArquivo(Forja, R, Tipos), Uteis))
    Total = sum(T for _, T in Resultados)
    Falhas = [f"{R['arq'].name}: {F}" for R, (Fs, _) in zip(Uteis, Resultados) for F in Fs]
    return Caso(Id, f"{Total - len(Falhas)}/{Total}", f"{Total}/{Total}", Falhas + ([] if Total else ["nenhum recorte verificado"]))


def RodarCasos(Forja, Quais, Jobs):
    Esquema = json.loads(ESQUEMA.read_text(encoding="utf-8"))
    Registros = Coletar(Forja, Jobs)
    Tabela = [("ma1", lambda: CasoMA1(Registros, Esquema)), ("ma2", lambda: CasoMA2(Forja, Registros, Jobs)), ("ma3", lambda: CasoMA3(Registros)),
              ("ma4", lambda: CasoRecorte(Forja, Registros, Jobs, ("ma4", {"decl", "stmt"}))),
              ("ma4e", lambda: CasoRecorte(Forja, Registros, Jobs, ("ma4e", {"expr"})))]
    return [CasoSeguro(Id, Rodar_) for Id, Rodar_ in Tabela if Id in Quais]


def CasoSeguro(Id, Rodar_):
    """Um binario que nao tem a AST 1.0 (sem ids, sem --sem-pos) quebra o julgamento: isso e um caso vermelho, nao um traceback."""
    try:
        return Rodar_()
    except (KeyError, IndexError, TypeError, AttributeError) as Erro:
        return Caso(Id, "erro", "-", [f"o documento nao tem a forma esperada: {type(Erro).__name__} {Erro}"])


# ---------- selftest: planta falha em cada julgamento e exige que ele a pegue ----------

DOC_MINIMO = {"arquivo": "a.fj", "ast_versao": VERSAO_AST, "modulo": "a", "declaracoes": [
    {"no": "proc", "id": "a:soma", "pos": [1, 1], "fim": [4, 2], "nome": "soma", "params": [
        {"id": "a:soma/params[0]", "pos": [1, 11], "fim": [1, 17], "nome": "x"}], "corpo": [
        {"no": "return", "id": "a:soma/corpo[0]", "pos": [2, 5], "fim": [2, 13], "valor": {
            "no": "nome", "id": "a:soma/corpo[0]/valor", "pos": [2, 12], "fim": [2, 13], "ref": "a:soma/params[0]", "ref_modulo": "a", "ref_tipo": "param", "nome": "x"}},
        {"no": "chamada", "id": "a:soma/corpo[1]", "pos": [3, 5], "fim": [3, 13], "expr": {
            "no": "chamada", "id": "a:soma/corpo[1]/expr", "pos": [3, 5], "fim": [3, 13], "ref_tipo": "embutido", "nome": "print", "args": []}}]},
    {"no": "proc", "id": "a:main", "pos": [6, 1], "fim": [8, 2], "nome": "main", "params": [], "corpo": []}]}


def Copia(Doc):
    return json.loads(json.dumps(Doc))


def Mutada(Mutador):
    Doc = Copia(DOC_MINIMO)
    Mutador(Doc)
    return Doc


def Plantas(Esquema):
    """(nome, deveria_falhar, resultado_vazio?) -- cada planta e um documento quebrado de um jeito."""
    Soma = lambda D: D["declaracoes"][0]
    Retorno = lambda D: Soma(D)["corpo"][0]
    Valor = lambda D: Retorno(D)["valor"]
    Editado = Copia(DOC_MINIMO)
    Editado["declaracoes"][1]["pos"], Editado["declaracoes"][1]["fim"] = [7, 1], [9, 2]
    Plantadas = [
        ("planta_sem_fim", lambda D: Soma(D).pop("fim")),
        ("planta_versao_antiga", lambda D: D.__setitem__("ast_versao", "0.11.0")),
        ("planta_id_repetido", lambda D: Soma(D)["corpo"][1].__setitem__("id", "a:soma/corpo[0]")),
        ("planta_id_fora_do_pai", lambda D: Retorno(D).__setitem__("id", "a:main/corpo[0]")),
        ("planta_pos_depois_do_fim", lambda D: Retorno(D).__setitem__("pos", [9, 9])),
        ("planta_filho_fora_do_pai", lambda D: Valor(D).__setitem__("fim", [9, 1])),
        ("planta_ref_inexistente", lambda D: Valor(D).__setitem__("ref", "a:soma/params[7]")),
        ("planta_uso_sem_ref", lambda D: [Valor(D).pop(K) for K in ("ref", "ref_modulo", "ref_tipo")]),
        ("planta_ref_de_outro_nome", lambda D: Valor(D).__setitem__("nome", "y")),
        ("planta_ref_modulo_errado", lambda D: Valor(D).__setitem__("ref_modulo", "b")),
    ]
    Docs = [(N, Mutada(M)) for N, M in Plantadas]
    return Docs + [("planta_editado", Editado)]


def Selftest():
    Esquema = json.loads(ESQUEMA.read_text(encoding="utf-8"))
    Falhas, Total = [], 0

    def Exigir(Nome, Condicao):
        nonlocal Total
        Total += 1
        if not Condicao:
            Falhas.append(Nome)

    Exigir("minimo_valido", JulgarDocumento(DOC_MINIMO, Esquema) == [] and JulgarRefs(DOC_MINIMO) == ([], 2))
    for Nome, Doc in Plantas(Esquema):
        Exigir(Nome, bool(JulgarDocumento(Doc, Esquema) or JulgarRefs(Doc)[0]) if Nome != "planta_editado" else JulgarIdsEstaveis(DOC_MINIMO, Doc) == [])
    Exigir("ids_sumiu", bool(JulgarIdsEstaveis(DOC_MINIMO, Mutada(lambda D: D["declaracoes"].pop()))))
    Exigir("ids_mudou_conteudo", bool(JulgarIdsEstaveis(DOC_MINIMO, Mutada(lambda D: D["declaracoes"][0].__setitem__("nome", "outro")))))
    Exigir("ids_sem_declaracao", bool(JulgarIdsEstaveis({"declaracoes": []}, DOC_MINIMO)))
    Exigir("ids_proc_novo_ausente", bool(JulgarIdsEstaveis(DOC_MINIMO, DOC_MINIMO, "a:zz_ia1_novo")))
    Exigir("recorte_bytes", Recortar([b"ab", b"cde", b"f"], [1, 2], [3, 1]) == b"b\ncde\n" and Recortar([b"abcd"], [1, 2], [1, 4]) == b"bc")
    Exigir("recorte_estrutura_igual", Estrutura(DOC_MINIMO["declaracoes"][0]) == Estrutura(Copia(DOC_MINIMO)["declaracoes"][0]))
    Exigir("recorte_estrutura_diferente", Estrutura(DOC_MINIMO["declaracoes"][0]) != Estrutura(DOC_MINIMO["declaracoes"][1]))
    Exigir("lote_tamanho", bool(JulgarLote([("decl", DOC_MINIMO["declaracoes"][0])], DOC_MINIMO)[0]))
    Exigir("lote_igual", JulgarLote([("decl", D) for D in DOC_MINIMO["declaracoes"]], Copia(DOC_MINIMO))[0] == [])
    Exigir("lote_diferente", bool(JulgarLote([("decl", DOC_MINIMO["declaracoes"][0]), ("decl", DOC_MINIMO["declaracoes"][0])], DOC_MINIMO)[0]))
    Exigir("itens_do_documento", sorted(T for T, _ in ItensDeRecorte(DOC_MINIMO)) == ["decl", "decl", "expr", "expr", "stmt", "stmt"])
    print(json.dumps(Caso("selftest", f"{Total - len(Falhas)}/{Total}", f"{Total}/{Total}", Falhas), ensure_ascii=False))
    return not Falhas


def Principal():
    Args = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    Args.add_argument("--forja")
    Args.add_argument("--caso", default="ma1,ma2,ma3,ma4,ma4e")
    Args.add_argument("--jobs", type=int, default=8)
    Args.add_argument("--esperar-vermelho", action="store_true")
    Args.add_argument("--selftest", action="store_true")
    Opcoes = Args.parse_args()
    if Opcoes.selftest:
        return 0 if Selftest() else 1
    if not Opcoes.forja:
        Args.error("--forja e obrigatorio")
    Casos = RodarCasos(Opcoes.forja, set(Opcoes.caso.split(",")), Opcoes.jobs)
    for C in Casos:
        print(json.dumps(C, ensure_ascii=False))
    Vermelho = any(not C["verde"] for C in Casos)
    return (0 if Vermelho else 1) if Opcoes.esperar_vermelho else (1 if Vermelho else 0)


if __name__ == "__main__":
    sys.exit(Principal())
