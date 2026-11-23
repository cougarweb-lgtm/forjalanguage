"""Casos de cli do forja ast 1.0 (IA1-I1): ids estruturais, fim, ref, --sem-pos, --no, projeto/pasta e estabilidade dos ids.

Importado pelo motor verificar_cli.py; as funcoes comuns vem de la. A prova ampla (todos os .fj ok_, examples e std; esquema; 40+ casos de id estavel; recorte fiel)
mora em tests\\ast\\verificar_ast.py; aqui ficam os casos curtos do motor.

Regras:
  - todo no (objeto com "id") leva "pos" e "fim"; o id comeca com o do pai (o do modulo no topo); uso de nome resolvido leva ref, ref_modulo e ref_tipo;
  - --sem-pos tira pos, fim, op_pos, pos_nome e o caminho do arquivo e deixa ids e ligacoes; --no <id> devolve so a subarvore; id inexistente, vazio ou --no fora do ast = exit 2;
  - --projeto (ou uma pasta com main.fj) imprime um item por modulo na ordem dos imports, a raiz por ultimo; as ligacoes apontam para ids que existem no documento;
  - editar outro ponto do arquivo (linha em branco, comentario, proc novo) nao muda id nem conteudo das declaracoes que ja existiam.

Versao: 1.1.0
Changelog: 1.1.0 - 0.25.1: CasoAstLoadAusente (forja ast de #load ausente/absoluto: AST sintatica sem ref*, exit 0; antes R0005 em ast_json_ref.fj:328). 1.0.0 - IA1-I1: primeira versao (5 casos; fixtures import_ast.fj, v17.fj, ast_v19.fj e a pasta ast_pasta).
Verificacao: python verificar_cli.py --forja <forja.exe>
"""

import json
import shutil
from pathlib import Path

from verificar_cli import ConferirExecucao, PastaAqui, Rodar

CHAVES_POSICAO = {"pos", "fim", "op_pos", "pos_nome"}
EDICOES = {"linha em branco": [""], "comentario": ["// linha inserida pelo teste"], "proc novo": ["proc zz_ia1_novo() {", "}"]}


def AstDe(Forja, Argumentos, Problemas):
    """O JSON de forja ast <Argumentos>, ou None (e um problema anotado)."""
    Rodada = Rodar(Forja, ["ast"] + Argumentos)
    Problemas.extend(ConferirExecucao(Rodada, 0))
    try:
        return json.loads(Rodada.stdout)
    except json.JSONDecodeError:
        Problemas.append(f"forja ast {' '.join(Argumentos)} nao devolveu JSON")
        return None


def NosDe(Valor, Pai=None):
    """(no, pai) de todo objeto com 'id', pai = o no com 'id' mais proximo acima."""
    if isinstance(Valor, list):
        for Item in Valor:
            yield from NosDe(Item, Pai)
    elif isinstance(Valor, dict):
        Atual = Valor if "id" in Valor else Pai
        if "id" in Valor:
            yield Valor, Pai
        for Filho in Valor.values():
            yield from NosDe(Filho, Atual)


def SemPosicoes(Valor):
    if isinstance(Valor, list):
        return [SemPosicoes(X) for X in Valor]
    if isinstance(Valor, dict):
        return {K: SemPosicoes(V) for K, V in Valor.items() if K not in CHAVES_POSICAO and K != "arquivo"}
    return Valor


def NoPorId(Doc, Id):
    for No, _ in NosDe(Doc.get("declaracoes", Doc.get("modulos", [])), None):
        if No["id"] == Id:
            return No
    return None


def ConferirIdsPosFim(Doc, Modulo):
    Problemas, Vistos = [], set()
    for No, Pai in NosDe(Doc["declaracoes"]):
        Prefixo = Pai["id"] + "/" if Pai else Modulo + ":"
        if No["id"] in Vistos or not No["id"].startswith(Prefixo):
            Problemas.append(f"id repetido ou fora do pai: {No['id']} (pai {Prefixo})")
        Vistos.add(No["id"])
        if "pos" not in No or "fim" not in No:
            Problemas.append(f"no sem pos/fim: {No['id']}")
    return Problemas


def CasoAstIdsFimRef(Forja, PastaSaidas):
    Problemas = []
    Doc = AstDe(Forja, ["import_ast.fj"], Problemas)
    if Doc is None:
        return Problemas
    Repetido = Rodar(Forja, ["ast", "import_ast.fj"]).stdout
    if json.dumps(Doc, sort_keys=False) != json.dumps(json.loads(Repetido), sort_keys=False):
        Problemas.append("duas execucoes deram AST diferente")
    if Doc.get("modulo") != "import_ast":
        Problemas.append(f"modulo {Doc.get('modulo')!r}, esperado 'import_ast'")
    Problemas += ConferirIdsPosFim(Doc, "import_ast")
    Esperados = {"import_ast:main/corpo[0]/valor": ("std:mem:arena_create", "std:mem", "proc", "import_ast:import:std:mem"),
                 "import_ast:main/corpo[1]/expr/args[0]/valor": ("import_ast:main/corpo[0]", "import_ast", "local", None),
                 "import_ast:main/corpo[2]/expr/args[0]/valor": ("mods/falha_cli:dividir", "mods/falha_cli", "proc", "import_ast:import:mods/falha_cli")}
    for Id, (Ref, Modulo, Tipo, Import) in Esperados.items():
        No = NoPorId(Doc, Id)
        Obtido = None if No is None else (No.get("ref"), No.get("ref_modulo"), No.get("ref_tipo"), No.get("ref_import"))
        if Obtido != (Ref, Modulo, Tipo, Import):
            Problemas.append(f"{Id}: ligacao {Obtido}, esperado {(Ref, Modulo, Tipo, Import)}")
    Print = NoPorId(Doc, "import_ast:main/corpo[2]/expr")
    if Print is None or Print.get("ref_tipo") != "embutido" or "ref" in Print:
        Problemas.append(f"print deveria levar so ref_tipo 'embutido': {Print!r}")
    return Problemas


def CasoAstSemPos(Forja, PastaSaidas):
    Problemas = []
    Cheia = AstDe(Forja, ["v17.fj"], Problemas)
    Sem = AstDe(Forja, ["--sem-pos", "v17.fj"], Problemas)
    if Cheia is None or Sem is None:
        return Problemas
    if Sem.get("arquivo") is not None:
        Problemas.append(f"--sem-pos deixou o arquivo: {Sem.get('arquivo')!r}")
    for No, _ in NosDe(Sem["declaracoes"]):
        if CHAVES_POSICAO & set(No):
            Problemas.append(f"--sem-pos deixou posicao em {No['id']}")
            break
    if SemPosicoes(Cheia) != SemPosicoes(Sem):
        Problemas.append("a AST sem posicoes difere da AST cheia sem posicoes (ids e ligacoes tem de ficar)")
    return Problemas


def CasoAstNo(Forja, PastaSaidas):
    Problemas = []
    Doc = AstDe(Forja, ["import_ast.fj"], Problemas)
    Projeto = AstDe(Forja, ["--projeto", "--no", "std:mem:arena_create", "import_ast.fj"], Problemas)
    if Doc is None or Projeto is None:
        return Problemas
    for Id in ("import_ast:main", "import_ast:main/corpo[2]", "import_ast:main/corpo[2]/expr/args[0]/valor/args[1]/valor"):
        Um = AstDe(Forja, ["--no", Id, "import_ast.fj"], Problemas)
        if Um is not None and (Um.get("no_id") != Id or Um.get("subarvore") != NoPorId(Doc, Id) or Um.get("ast_versao") != "1.0.0"):
            Problemas.append(f"--no {Id}: a subarvore difere do no do documento inteiro")
    if Projeto.get("subarvore", {}).get("id") != "std:mem:arena_create":
        Problemas.append("--projeto --no nao achou o no de outro modulo (std:mem:arena_create)")
    Problemas += ConferirExecucao(Rodar(Forja, ["ast", "--no", "nao:existe", "import_ast.fj"]), 2)
    Problemas += ConferirExecucao(Rodar(Forja, ["ast", "--no", "", "import_ast.fj"]), 2)
    Problemas += ConferirExecucao(Rodar(Forja, ["check", "--no", "import_ast:main", "import_ast.fj"]), 2)
    Problemas += ConferirExecucao(Rodar(Forja, ["check", "--sem-pos", "import_ast.fj"]), 2)
    return Problemas


def CasoAstProjeto(Forja, PastaSaidas):
    Problemas = []
    Doc = AstDe(Forja, ["--projeto", "import_ast.fj"], Problemas)
    Pasta = AstDe(Forja, ["ast_pasta"], Problemas)
    Arq = AstDe(Forja, ["--projeto", "ast_pasta/main.fj"], Problemas)
    if Doc is None or Pasta is None or Arq is None:
        return Problemas
    Ordem = [M["modulo"] for M in Doc.get("modulos", [])]
    if Doc.get("projeto") is not True or Ordem != ["std:mem", "mods/falha_cli", "import_ast"]:
        Problemas.append(f"modulos {Ordem}, esperado ['std:mem', 'mods/falha_cli', 'import_ast'] com projeto true")
    Ids = {No["id"] for No, _ in NosDe(Doc.get("modulos", []))}
    for No, _ in NosDe(Doc.get("modulos", [])):
        if "ref" in No and No["ref"] not in Ids:
            Problemas.append(f"{No['id']}: ref {No['ref']} nao existe no documento do projeto")
            break
    if [M["modulo"] for M in Pasta["modulos"]] != ["util", "main"]:
        Problemas.append(f"pasta: modulos {[M['modulo'] for M in Pasta['modulos']]}, esperado ['util', 'main']")
    if [SemPosicoes(M) for M in Pasta["modulos"]] != [SemPosicoes(M) for M in Arq["modulos"]]:
        Problemas.append("forja ast <pasta> difere de forja ast --projeto <pasta>/main.fj")
    Problemas += ConferirExecucao(Rodar(Forja, ["ast", "mods"]), 2)
    Problemas += ConferirExecucao(Rodar(Forja, ["ast", "--projeto"]), 2)
    return Problemas


def ProvarEdicao(Forja, PastaSaidas, Fixture, Nome, Linhas):
    """Insere Linhas antes da 1a declaracao de Fixture e confere que nenhuma declaracao que ja existia mudou de id ou de conteudo."""
    Problemas = []
    Antes = AstDe(Forja, [Fixture], Problemas)
    if Antes is None:
        return Problemas
    Primeira = Antes["declaracoes"][0]["pos"][0]
    Fonte = (PastaAqui / Fixture).read_text(encoding="utf-8").split("\n")
    Fonte[Primeira - 1:Primeira - 1] = Linhas
    Destino = PastaSaidas / Fixture
    Destino.write_text("\n".join(Fonte), encoding="utf-8", newline="")
    Depois = AstDe(Forja, [str(Destino)], Problemas)
    if Depois is None:
        return Problemas
    Mapa = {D.get("id"): D for D in Depois["declaracoes"]}
    for Decl in Antes["declaracoes"]:
        if Decl.get("id") is None or SemPosicoes(Mapa.get(Decl["id"])) != SemPosicoes(Decl):
            Problemas.append(f"{Fixture} + {Nome}: {Decl.get('id')} sumiu, mudou ou nao tem id")
    Moveu = Mapa.get(Antes["declaracoes"][0].get("id"))
    if Moveu is None or Moveu["pos"][0] != Primeira + len(Linhas):
        Problemas.append(f"{Fixture} + {Nome}: a primeira declaracao nao andou {len(Linhas)} linha(s)")
    return Problemas


def CasoAstIdsEstaveis(Forja, PastaSaidas):
    Problemas = []
    for Fixture in ("v17.fj", "ast_v19.fj", "array_ast.fj"):
        for Nome, Linhas in EDICOES.items():
            Problemas += ProvarEdicao(Forja, PastaSaidas, Fixture, Nome, Linhas)
    return Problemas


def ChavesRef(Valor):
    """Quantas chaves 'ref*' (ref, ref_modulo, ref_tipo, ref_import) o documento tem."""
    if isinstance(Valor, list):
        return sum(ChavesRef(X) for X in Valor)
    if isinstance(Valor, dict):
        return sum((1 if K.startswith("ref") else 0) + ChavesRef(V) for K, V in Valor.items())
    return 0


def SemRef(Valor):
    if isinstance(Valor, list):
        return [SemRef(X) for X in Valor]
    if isinstance(Valor, dict):
        return {K: SemRef(V) for K, V in Valor.items() if not K.startswith("ref")}
    return Valor


def ConferirAstDoLoadAusente(Forja, Fixture, Problemas):
    """forja ast sem --tipos de um #load que falha: AST sintatica sem ligacoes, exit 0, stderr vazio; check e ast --tipos seguem E1007 3:17."""
    Rodada = Rodar(Forja, ["ast", str(Fixture)])
    Problemas.extend(ConferirExecucao(Rodada, 0))
    Linhas = [L for L in Rodada.stdout.splitlines() if L.strip()]
    if Rodada.stderr.strip() or len(Linhas) != 1:
        Problemas.append(f"{Fixture.name}: ast deveria ter stderr vazio e 1 linha JSON (stderr {Rodada.stderr[:80]!r}, {len(Linhas)} linhas)")
        return None
    Doc = json.loads(Linhas[0])
    Problemas += ConferirIdsPosFim(Doc, Fixture.stem)
    Cargas = [N for N, _ in NosDe(Doc["declaracoes"]) if N.get("load") is True]
    if len(Cargas) != 1 or "texto" not in Cargas[0]:
        Problemas.append(f"{Fixture.name}: esperava 1 no texto com load true, achei {len(Cargas)}")
    if ChavesRef(Doc):
        Problemas.append(f"{Fixture.name}: ast sem --tipos de #load ausente nao deveria ter chave ref* ({ChavesRef(Doc)})")
    for Argumentos in (["ast", "--tipos"], ["check"]):
        Falha = Rodar(Forja, Argumentos + [str(Fixture)])
        Problemas.extend(ConferirExecucao(Falha, 1))
        try:
            Erro = json.loads(Falha.stdout.splitlines()[0])
        except (IndexError, json.JSONDecodeError):
            Problemas.append(f"{Fixture.name}: forja {' '.join(Argumentos)} nao devolveu o E1007 em JSON")
            continue
        Obtido = (Erro.get("codigo"), Erro.get("linha"), Erro.get("coluna"))
        if Obtido != ("E1007", 3, 17):
            Problemas.append(f"{Fixture.name}: forja {' '.join(Argumentos)} deu {Obtido}, esperado ('E1007', 3, 17)")
    return Doc


def CasoAstLoadAusente(Forja, PastaSaidas):
    """0.25.1: #load de arquivo ausente (relativo) ou de caminho absoluto inexistente nao derruba o forja ast (antes R0005; no C, segfault)."""
    Problemas = []
    Semantica = PastaAqui.parent / "semantica"
    Doc = ConferirAstDoLoadAusente(Forja, Semantica / "err_E1007_load_inexistente.fj", Problemas)
    ConferirAstDoLoadAusente(Forja, Semantica / "err_E1007_load_absoluto.fj", Problemas)
    Oraculo = Path(Forja).with_name("forja_c.exe")
    if Doc is None or not Oraculo.exists():
        return Problemas
    # Metamorfico: o C com o .bin PRESENTE imprime a mesma AST; tirando as ligacoes (ref*) tem de ser igual a deste forja com o arquivo ausente.
    for Nome in ("ausente", "presente"):
        Pasta = PastaSaidas / f"load_{Nome}"
        shutil.rmtree(Pasta, ignore_errors=True)
        Pasta.mkdir(parents=True)
        shutil.copy(Semantica / "err_E1007_load_inexistente.fj", Pasta / "load.fj")
    (PastaSaidas / "load_presente" / "nao_existe_b12.bin").write_bytes(b"abc")
    Novo = Rodar(Forja, ["ast", "--sem-pos", str(PastaSaidas / "load_ausente" / "load.fj")])
    Antigo = Rodar(str(Oraculo), ["ast", "--sem-pos", str(PastaSaidas / "load_presente" / "load.fj")])
    try:
        if SemRef(json.loads(Novo.stdout)) != SemRef(json.loads(Antigo.stdout)):
            Problemas.append("a AST do #load ausente difere (sem ref*) da AST do C com o .bin presente")
    except json.JSONDecodeError:
        Problemas.append("metamorfico do #load: saida sem JSON (forja novo ou forja_c.exe)")
    return Problemas
