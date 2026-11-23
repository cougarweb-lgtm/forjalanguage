# auditar_bir_wasm.py - W0 do backend WebAssembly da Forja (gaveta/plano_backend_wasm.txt, itens 1 e 8).
# Mede a BIR real que o compilador grava com --debug-code bir,json:
#   (1) censo de ops e de classes de vreg, por modo (dev e release);
#   (2) CFG por funcao e redutibilidade (dominadores + DFS).
# Com --ponto-corte <passe> (so release) mede a secao "== passe <passe>" do dump
# --debug-code passes (<exe>.debug.txt) em vez da BIR final; ex.: layout = BIR antes de alocador/isel.
# Versao: 1.1.0
# Depende de: so a biblioteca padrao (Python 3.12+, por Path.is_junction).
# Changelog:
#   1.1.0 (2026-11-20): --ponto-corte <passe> mede o CFG e o censo na secao do passe do dump --debug-code passes; selftest com dump de 2 passes.
#   1.0.2 (2026-11-20): --grupo resolve o caminho para absoluto (o build roda com cwd na pasta temporaria).
#   1.0.1 (2026-11-20): caixa CamelCase em todos os nomes locais e parametros (PY-R01).
#   1.0.0 (2026-11-20): primeira versao; --selftest com CFGs sinteticos (redutivel, irredutivel, aresta de frio).
# Uso:
#   python auditar_bir_wasm.py --selftest
#   python auditar_bir_wasm.py --forja <forja.exe> --saida <relatorio.txt> [--jobs N] [--orcamento-seg S]
#       [--ponto-corte layout]
#       --grupo "<caminho>|modos=dev,release|prefixo=ok_|max=200" [--grupo ...]

import argparse
import collections
import concurrent.futures
import datetime
import json
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

VERSAO = "1.1.0"
MODOS_VALIDOS = ("dev", "release")
CHAVES_CFG = ("blocos", "alcancaveis", "arestas", "arestas_frio", "lacos", "cabecalhos", "retro_nao_dom")

# Ops de nativo/bir/bir_imprimir.fj (NOMES_OP) + end (BIR_END_SIMBOLO, nao deve aparecer em instrucao)
OPS_CONHECIDAS = frozenset((
    "const", "copia", "add", "sub", "mul", "and", "or", "xor", "neg", "cmp", "ext_s", "ext_z", "trunc",
    "add_ov", "sub_ov", "mul_ov", "neg_ov", "end", "chama", "salta", "desvia", "retorna", "armadilha",
    "div", "rem", "shl", "shr", "not", "i_para_f", "f_para_f", "raiz", "chama_ind", "seleciona", "splat",
    "end_slot", "carrega", "grava", "copia_mem", "zera_mem",
))
# Terminadores: eh_terminador em nativo/bir/bir_validar.fj:40
TERMINADORES = ("salta", "desvia", "retorna", "armadilha")
# Aresta para bloco frio = sufixo "; frio bN" de add_ov/sub_ov/mul_ov/neg_ov (sucessor em nativo/optimizer/inline.fj:105)
CLASSES_CONHECIDAS = frozenset(("-", "i8", "i16", "i32", "i64", "f32", "f64", "v128"))

RE_PARAM = re.compile(r"v(\d+):([^/,)\s]+)")
RE_BLOCO = re.compile(r"^b(\d+)( frio)?:$")
RE_DEF = re.compile(r"^  v(\d+):(\S+) = (.*)$")
RE_POS = re.compile(r" ; \d+:\d+$")
RE_FRIO = re.compile(r" ; frio b(\d+)")
RE_MNEMONICO = re.compile(r"[a-z][a-z_0-9]*")
RE_ALVO = re.compile(r"\bb(\d+)\b")
RE_SECAO_PASSE = re.compile(r"^== passe (\S+)$")

TEXTO_REDUTIVEL = "\n".join([
    "funcao sint_redutivel(v0:i64/8) -> vazio",
    "b0:",
    "  v1:i8 = cmp.eq v0, #0:i64 ; 1:1",
    "  desvia v1, b1, b2 ; 1:1",
    "b1:",
    "  salta b3 ; 2:1",
    "b2:",
    "  salta b3 ; 3:1",
    "b3:",
    "  v2:i8 = cmp.eq v0, #1:i64 ; 4:1",
    "  desvia v2, b4, b5 ; 4:1",
    "b4:",
    "  retorna ; 5:1",
    "b5:",
    "  salta b3 ; 6:1",
    "",
])
TEXTO_IRREDUTIVEL = "\n".join([
    "funcao sint_irredutivel(v0:i64) -> vazio",
    "b0:",
    "  v1:i8 = cmp.eq v0, #0:i64",
    "  desvia v1, b1, b2",
    "b1:",
    "  v2:i8 = cmp.eq v0, #1:i64",
    "  desvia v2, b2, b3",
    "b2:",
    "  v3:i8 = cmp.eq v0, #2:i64",
    "  desvia v3, b1, b3",
    "b3:",
    "  retorna",
    "",
])
TEXTO_FRIO = "\n".join([
    "funcao sint_frio(v0:i64) -> vazio",
    "b0:",
    "  v1:i64 = add_ov v0, #1:i64 ; frio b2 ; 7:1",
    "  retorna ; 8:1",
    "b2 frio:",
    "  armadilha ; 9:1",
    "",
])
# Dump de --debug-code passes: cabecalho, uma secao por passe; a secao 'layout' tem o irredutivel
TEXTO_PASSES = "\n".join([
    "# Forja Native 0.42.0 --debug-code",
    "## passes",
    "== passe checagem",
    TEXTO_REDUTIVEL,
    "== passe layout",
    TEXTO_IRREDUTIVEL,
    "",
])


def Argumentos(Argv):
    Parser = argparse.ArgumentParser(description="Censo de ops/classes e CFG irredutivel da BIR da Forja (W0 wasm).")
    Parser.add_argument("--selftest", action="store_true", help="so a prova de sensibilidade (sem compilar)")
    Parser.add_argument("--forja", help="caminho ABSOLUTO do forja.exe")
    Parser.add_argument("--saida", help="relatorio de saida (txt)")
    Parser.add_argument("--tmp", help="pasta temporaria (default: <pasta do relatorio>/_tmp_bir)")
    Parser.add_argument("--jobs", type=int, default=8)
    Parser.add_argument("--orcamento-seg", type=float, default=480.0)
    Parser.add_argument("--timeout-seg", type=int, default=60)
    Parser.add_argument("--ponto-corte", default="", help="nome do passe (ex.: layout); mede so release, na secao do dump passes")
    Parser.add_argument("--grupo", action="append", default=[], help="CAMINHO|modos=dev,release|prefixo=ok_|max=200")
    return Parser.parse_args(Argv)


def RegistrarVreg(Funcao, Vreg, Classe):
    Antes = Funcao["vregs"].get(Vreg)
    if Antes is None:
        Funcao["vregs"][Vreg] = Classe
    elif Antes != Classe:
        Funcao["anom"]["classe_conflitante"] += 1
    if Classe not in CLASSES_CONHECIDAS:
        Funcao["anom"]["classe_desconhecida"] += 1


def NovaFuncao(Linha):
    Nome, Resto = Linha[len("funcao "):].split("(", 1)
    Params = Resto.split(") -> ", 1)[0]
    Funcao = {"nome": Nome, "vregs": {}, "blocos": {}, "bloco_atual": None,
              "ops": collections.Counter(), "defs": collections.Counter(), "anom": collections.Counter()}
    for Vreg, Classe in RE_PARAM.findall(Params):
        RegistrarVreg(Funcao, int(Vreg), Classe)
    return Funcao


def AbrirBloco(Funcao, Rotulo, Frio):
    if Rotulo in Funcao["blocos"]:
        Funcao["anom"]["bloco_duplicado"] += 1
    Funcao["blocos"][Rotulo] = {"frio": Frio, "alvos": [], "frio_alvos": [], "termino": None, "apos": 0}
    Funcao["bloco_atual"] = Rotulo


def RegistrarTerminador(Funcao, Bloco, Op, Corpo):
    Bloco["termino"] = Op
    if Op in ("salta", "desvia"):
        Bloco["alvos"] = [int(x) for x in RE_ALVO.findall(Corpo)]
        Esperado = 1 if Op == "salta" else 2
        if len(Bloco["alvos"]) != Esperado:
            Funcao["anom"]["terminador_mal_formado"] += 1


def AdicionarInstr(Funcao, Linha):
    Bloco = Funcao["blocos"].get(Funcao["bloco_atual"])
    Casou = RE_DEF.match(Linha)
    if Casou:
        RegistrarVreg(Funcao, int(Casou.group(1)), Casou.group(2))
        Funcao["defs"][Casou.group(2)] += 1
        Corpo = Casou.group(3)
    else:
        Corpo = Linha[2:]
    Corpo = RE_POS.sub("", Corpo)
    Frio = RE_FRIO.search(Corpo)
    Corpo = RE_FRIO.sub("", Corpo)
    Mnem = RE_MNEMONICO.match(Corpo)
    Op = Mnem.group(0) if Mnem else "?"
    Funcao["ops"][Op] += 1
    if Op not in OPS_CONHECIDAS:
        Funcao["anom"]["op_desconhecido:" + Op] += 1
    if Bloco is None:
        Funcao["anom"]["instr_fora_de_bloco"] += 1
        return
    if Bloco["termino"] is not None:
        Bloco["apos"] += 1
    if Frio:
        Bloco["frio_alvos"].append(int(Frio.group(1)))
    if Op in TERMINADORES:
        RegistrarTerminador(Funcao, Bloco, Op, Corpo)


def ParsearTextoBir(Texto):
    """Devolve as funcoes de um texto de BIR (formato de imprimir_funcao; simbolos sao ignorados)."""
    Funcoes = []
    Funcao = None
    for Linha in Texto.split("\n"):
        if Linha.startswith("funcao "):
            Funcao = NovaFuncao(Linha)
            Funcoes.append(Funcao)
        elif Linha.startswith("simbolos:"):
            Funcao = None
        elif Funcao is None or Linha == "" or Linha.startswith("  slot "):
            continue
        elif Linha.startswith("  "):
            AdicionarInstr(Funcao, Linha)
        else:
            Casou = RE_BLOCO.match(Linha)
            if Casou:
                AbrirBloco(Funcao, int(Casou.group(1)), Casou.group(2) is not None)
            else:
                Funcao["anom"]["linha_nao_reconhecida"] += 1
    return Funcoes


def Dfs(Succ, Entrada):
    """DFS iterativa: pos-ordem, arestas de retorno (destino cinza) e cores."""
    Cor = [0] * len(Succ)
    Pilha = [(Entrada, 0)]
    Cor[Entrada] = 1
    PosOrdem = []
    Retro = []
    while Pilha:
        Atual, Prox = Pilha[-1]
        if Prox < len(Succ[Atual]):
            Pilha[-1] = (Atual, Prox + 1)
            Alvo = Succ[Atual][Prox]
            if Cor[Alvo] == 0:
                Cor[Alvo] = 1
                Pilha.append((Alvo, 0))
            elif Cor[Alvo] == 1:
                Retro.append((Atual, Alvo))
        else:
            Cor[Atual] = 2
            PosOrdem.append(Atual)
            Pilha.pop()
    return PosOrdem, Retro, Cor


def Dominadores(Succ, Entrada, PosOrdem):
    """Dominadores iterativos com mascara de bits (so blocos alcancaveis). dom[v] >> u & 1 = u domina v."""
    Tamanho = len(Succ)
    Pred = [[] for _ in range(Tamanho)]
    Todos = 0
    for Origem in PosOrdem:
        Todos |= 1 << Origem
        for Destino in Succ[Origem]:
            Pred[Destino].append(Origem)
    Dom = [Todos] * Tamanho
    Dom[Entrada] = 1 << Entrada
    Ordem = PosOrdem[::-1]
    Mudou = True
    while Mudou:
        Mudou = False
        for Vertice in Ordem:
            if Vertice == Entrada:
                continue
            Novo = Todos
            for Pai in Pred[Vertice]:
                Novo &= Dom[Pai]
            Novo |= 1 << Vertice
            if Novo != Dom[Vertice]:
                Dom[Vertice] = Novo
                Mudou = True
    return Dom


def AnalisarCfg(Funcao, ComFrio):
    """CFG da funcao. Reducivel <=> toda aresta de retorno tem destino que domina a origem."""
    Rotulos = sorted(Funcao["blocos"])
    Resumo = {"blocos": len(Rotulos), "alcancaveis": 0, "arestas": 0, "arestas_frio": 0, "lacos": 0,
              "cabecalhos": 0, "retro_nao_dom": 0, "ruins": [], "anom": collections.Counter()}
    if 0 not in Funcao["blocos"]:
        Resumo["anom"]["sem_bloco_b0"] += 1
        return Resumo
    Idx = {Rot: i for i, Rot in enumerate(Rotulos)}
    Succ = [[] for _ in Rotulos]
    for Rot in Rotulos:
        Bl = Funcao["blocos"][Rot]
        if Bl["termino"] is None:
            Resumo["anom"]["bloco_sem_terminador"] += 1
        if Bl["apos"]:
            Resumo["anom"]["instr_apos_terminador"] += Bl["apos"]
        Resumo["arestas"] += len(Bl["alvos"])
        Resumo["arestas_frio"] += len(Bl["frio_alvos"])
        for Alvo in Bl["alvos"] + (Bl["frio_alvos"] if ComFrio else []):
            if Alvo in Idx:
                Succ[Idx[Rot]].append(Idx[Alvo])
            else:
                Resumo["anom"]["alvo_inexistente"] += 1
    PosOrdem, Retro, _ = Dfs(Succ, Idx[0])
    Dom = Dominadores(Succ, Idx[0], PosOrdem)
    Resumo["alcancaveis"] = len(PosOrdem)
    Cabecalhos = set()
    for Origem, Destino in Retro:
        if (Dom[Origem] >> Destino) & 1:
            Resumo["lacos"] += 1
            Cabecalhos.add(Destino)
        else:
            Resumo["retro_nao_dom"] += 1
            Resumo["ruins"].append((Rotulos[Origem], Rotulos[Destino]))
    Resumo["cabecalhos"] = len(Cabecalhos)
    return Resumo


def ResumirFuncao(Funcao):
    CfgFrio = AnalisarCfg(Funcao, True)
    Anom = collections.Counter(Funcao["anom"])
    Anom.update(CfgFrio["anom"])
    return {"nome": Funcao["nome"], "blocos": len(Funcao["blocos"]),
            "frio_blocos": sum(1 for Bl in Funcao["blocos"].values() if Bl["frio"]),
            "ops": dict(Funcao["ops"]), "defs": dict(Funcao["defs"]),
            "vregs": dict(collections.Counter(Funcao["vregs"].values())),
            "anom": dict(Anom), "cfg_com_frio": CfgFrio, "cfg_sem_frio": AnalisarCfg(Funcao, False)}


def LerDump(Jsonl):
    Funcoes = []
    Registros = 0
    with open(Jsonl, encoding="utf-8", errors="replace") as Arq:
        for Linha in Arq:
            if not Linha.strip():
                continue
            Rec = json.loads(Linha)
            if Rec.get("tipo") != "bir":
                continue
            Registros += 1
            for Funcao in ParsearTextoBir(Rec.get("texto", "")):
                Funcoes.append(ResumirFuncao(Funcao))
    return Funcoes, Registros


def SecoesDePasse(Texto):
    """Divide um dump --debug-code passes em {nome do passe: texto}; a secao vai ate o proximo '== '."""
    Secoes = {}
    Atual = None
    for Linha in Texto.split("\n"):
        Casou = RE_SECAO_PASSE.match(Linha)
        if Casou:
            Atual = Casou.group(1)
            Secoes.setdefault(Atual, [])
        elif Linha.startswith("== "):
            Atual = None
        elif Atual is not None:
            Secoes[Atual].append(Linha)
    return {Nome: "\n".join(Linhas) for Nome, Linhas in Secoes.items()}


def LerPontoCorte(Dump, Passe):
    """Funcoes do passe pedido no dump <exe>.debug.txt. Registros = 1 se a secao existe, senao 0."""
    Secao = SecoesDePasse(Dump.read_text(encoding="utf-8", errors="replace")).get(Passe)
    if Secao is None:
        return [], 0
    return [ResumirFuncao(Funcao) for Funcao in ParsearTextoBir(Secao)], 1


def Resultado(Base, Status, Motivo, Seg, Funcoes=None):
    return {**Base, "status": Status, "motivo": Motivo, "segundos": round(Seg, 3), "funcoes": Funcoes or []}


def PrimeiraLinhaErro(Proc):
    Bruto = (Proc.stderr or b"") + (Proc.stdout or b"")
    for Linha in Bruto.decode("utf-8", "replace").splitlines():
        if Linha.strip():
            return f"rc={Proc.returncode}: " + Linha.strip()[:160]
    return f"rc={Proc.returncode} sem saida"


def Compilar(Base, Forja, Pasta, Timeout):
    Exe = Pasta / "prog.exe"
    Ponto = Base["ponto"]
    Cmd = [Forja, "build", "--backend", "x64", "--debug-code", "passes" if Ponto else "bir,json"]
    if Base["modo"] == "release":
        Cmd.append("--release")
    Cmd += [Base["arquivo"], "-o", str(Exe)]
    Inicio = time.time()
    try:
        Proc = subprocess.run(Cmd, capture_output=True, timeout=Timeout, cwd=str(Pasta), stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        return Resultado(Base, "timeout", f"mais de {Timeout} s", time.time() - Inicio)
    Seg = time.time() - Inicio
    if Proc.returncode != 0:
        return Resultado(Base, "falhou", PrimeiraLinhaErro(Proc), Seg)
    Dump = Path(str(Exe) + (".debug.txt" if Ponto else ".debug.jsonl"))
    if not Dump.is_file():
        return Resultado(Base, "sem_dump", f"build sem {Dump.name}", Seg)
    if Ponto:
        Funcoes, Registros = LerPontoCorte(Dump, Ponto)
        Motivo = f"dump passes sem secao '== passe {Ponto}'"
    else:
        Funcoes, Registros = LerDump(Dump)
        Motivo = "jsonl sem registro bir"
    if Registros == 0:
        return Resultado(Base, "sem_dump", Motivo, Seg)
    return Resultado(Base, "ok", "", Seg, Funcoes)


def ExecutarUma(Indice, Tarefa, Forja, Deadline, PastaTmp, Timeout):
    Base = {"id": Indice, **Tarefa}
    if time.time() > Deadline:
        return Resultado(Base, "orcamento", "orcamento de tempo esgotado antes de iniciar", 0.0)
    Pasta = PastaTmp / f"t{Indice:05d}_{Tarefa['modo']}"
    Pasta.mkdir(parents=True, exist_ok=True)
    try:
        return Compilar(Base, Forja, Pasta, Timeout)
    except (OSError, ValueError, KeyError) as Erro:
        return Resultado(Base, "erro_interno", str(Erro)[:200], 0.0)
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def ExecutarTodas(Tarefas, Forja, Jobs, Deadline, PastaTmp, Timeout):
    Resultados = [None] * len(Tarefas)
    Feitos = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=Jobs) as Pool:
        Futuros = {Pool.submit(ExecutarUma, i, t, Forja, Deadline, PastaTmp, Timeout): i
                   for i, t in enumerate(Tarefas)}
        for Futuro in concurrent.futures.as_completed(Futuros):
            Resultados[Futuros[Futuro]] = Futuro.result()
            Feitos += 1
            if Feitos % 100 == 0:
                print(f"progresso: {Feitos}/{len(Tarefas)}", flush=True)
    return Resultados


def ParsearGrupo(Spec):
    Partes = Spec.split("|")
    Caminho = Path(Partes[0]).resolve()  # absoluto: o build roda com cwd na pasta temporaria
    Grupo = {"nome": Caminho.name, "caminho": Caminho, "modos": ["dev"], "prefixo": "", "max": 0}
    for Parte in Partes[1:]:
        Chave, _, Valor = Parte.partition("=")
        if Chave == "modos":
            Grupo["modos"] = Valor.split(",")
        elif Chave == "prefixo":
            Grupo["prefixo"] = Valor
        elif Chave == "max":
            Grupo["max"] = int(Valor)
        else:
            raise ValueError(f"opcao de grupo desconhecida: {Chave}")
    for Modo in Grupo["modos"]:
        if Modo not in MODOS_VALIDOS:
            raise ValueError(f"modo desconhecido: {Modo}")
    return Grupo


def VarrerFj(Pasta):
    for Item in sorted(Pasta.iterdir()):
        if Item.is_dir():
            if Item.name == "vendor" or Item.is_junction() or Item.is_symlink():
                continue
            yield from VarrerFj(Item)
        elif Item.suffix == ".fj" and Item.is_file():
            yield Item


def ListarFontes(Grupo):
    Raiz = Grupo["caminho"]
    Achados = [Raiz] if Raiz.is_file() else sorted(VarrerFj(Raiz))
    if Grupo["prefixo"]:
        Achados = [A for A in Achados if A.name.startswith(Grupo["prefixo"])]
    Maximo = Grupo["max"]
    if Maximo and len(Achados) > Maximo:
        Total = len(Achados)
        Achados = [Achados[i * Total // Maximo] for i in range(Maximo)]
    return Achados


def MontarTarefas(Grupos, Ponto):
    """Uma tarefa por fonte e modo; com ponto de corte so vai release (o dump passes so existe em release)."""
    Tarefas = []
    for Grupo in Grupos:
        for Arquivo in ListarFontes(Grupo):
            for Modo in Grupo["modos"]:
                if Ponto and Modo != "release":
                    continue
                Tarefas.append({"grupo": Grupo["nome"], "arquivo": str(Arquivo), "modo": Modo, "ponto": Ponto})
    return Tarefas


def VersaoForja(Forja):
    try:
        Proc = subprocess.run([Forja, "--versao"], capture_output=True, timeout=30, stdin=subprocess.DEVNULL)
    except (OSError, subprocess.TimeoutExpired) as Erro:
        return f"(falhou: {Erro})"
    Linhas = (Proc.stdout or b"").decode("utf-8", "replace").strip().splitlines()
    return Linhas[0] if Linhas else "(sem saida)"


def NovoAgregado(Executado):
    return {"executado": Executado, "funcoes": 0, "blocos": 0, "frio_blocos": 0, "instr": 0,
            "ops": collections.Counter(), "defs": collections.Counter(), "vregs": collections.Counter(),
            "anom": collections.Counter(),
            "cfg": {"com_frio": collections.Counter(), "sem_frio": collections.Counter()},
            "irred": {"com_frio": [], "sem_frio": []}}


def SomarCfg(Ag, Variante, Arquivo, Nome, Cfg):
    Contagem = Ag["cfg"][Variante]
    for Chave in CHAVES_CFG:
        Contagem[Chave] += Cfg[Chave]
    if Cfg["retro_nao_dom"]:
        Contagem["funcoes_irred"] += 1
        Ag["irred"][Variante].append((Arquivo, Nome, Cfg["retro_nao_dom"], Cfg["ruins"][:4]))


def SomarFuncao(Ag, Arquivo, Funcao):
    Ag["funcoes"] += 1
    Ag["blocos"] += Funcao["blocos"]
    Ag["frio_blocos"] += Funcao["frio_blocos"]
    Ag["instr"] += sum(Funcao["ops"].values())
    Ag["ops"].update(Funcao["ops"])
    Ag["defs"].update(Funcao["defs"])
    Ag["vregs"].update(Funcao["vregs"])
    Ag["anom"].update(Funcao["anom"])
    SomarCfg(Ag, "com_frio", Arquivo, Funcao["nome"], Funcao["cfg_com_frio"])
    SomarCfg(Ag, "sem_frio", Arquivo, Funcao["nome"], Funcao["cfg_sem_frio"])


def Agregar(Resultados, ModosRodados):
    Agregados = {Modo: NovoAgregado(Modo in ModosRodados) for Modo in MODOS_VALIDOS}
    for Res in Resultados:
        if Res["status"] != "ok":
            continue
        for Funcao in Res["funcoes"]:
            SomarFuncao(Agregados[Res["modo"]], Res["arquivo"], Funcao)
    return Agregados


def SelftestPasse():
    """Caso do dump passes: so a secao 'layout' entra no CFG, e o irredutivel plantado nela tem que ser achado."""
    Secoes = SecoesDePasse(TEXTO_PASSES)
    Funcoes = [ResumirFuncao(Funcao) for Funcao in ParsearTextoBir(Secoes.get("layout", ""))]
    Ok = sorted(Secoes) == ["checagem", "layout"] and len(Funcoes) == 1 and not Funcoes[0]["anom"]
    Ok = Ok and Funcoes[0]["nome"] == "sint_irredutivel" and Funcoes[0]["cfg_com_frio"]["retro_nao_dom"] == 1
    Estado = "OK" if Ok else "FALHOU"
    return Ok, f"SELFTEST passes_layout: {Estado} (secoes={sorted(Secoes)} funcoes_no_layout={len(Funcoes)})"


def Selftest(Imprimir=True):
    Casos = [
        ("redutivel_com_laco", TEXTO_REDUTIVEL,
         {"irred": 0, "lacos": 1, "cmp": 2, "salta": 3}),
        ("irredutivel_classico", TEXTO_IRREDUTIVEL,
         {"irred": 1, "lacos": 0, "cmp": 3, "salta": 0}),
        ("aresta_de_frio_ov", TEXTO_FRIO,
         {"irred": 0, "alc_com_frio": 2, "alc_sem_frio": 1, "add_ov": 1}),
    ]
    Linhas = []
    OkGeral = True
    for Nome, Texto, Esperado in Casos:
        Funcoes = ParsearTextoBir(Texto)
        if len(Funcoes) != 1:
            OkGeral = False
            Linhas.append(f"SELFTEST {Nome}: FALHOU (funcoes lidas = {len(Funcoes)})")
            continue
        Res = ResumirFuncao(Funcoes[0])
        Obtido = {
            "irred": 1 if Res["cfg_com_frio"]["retro_nao_dom"] > 0 else 0,
            "lacos": Res["cfg_com_frio"]["lacos"],
            "cmp": Res["ops"].get("cmp", 0),
            "salta": Res["ops"].get("salta", 0),
            "alc_com_frio": Res["cfg_com_frio"]["alcancaveis"],
            "alc_sem_frio": Res["cfg_sem_frio"]["alcancaveis"],
            "add_ov": Res["ops"].get("add_ov", 0),
        }
        Diferencas = [f"{k}: esperado {v}, obtido {Obtido.get(k)}" for k, v in Esperado.items() if Obtido.get(k) != v]
        if Res["anom"]:
            Diferencas.append(f"anomalias={Res['anom']}")
        if Diferencas:
            OkGeral = False
            Linhas.append(f"SELFTEST {Nome}: FALHOU " + "; ".join(Diferencas))
        else:
            Linhas.append(f"SELFTEST {Nome}: OK (irredutivel={Obtido['irred']}, lacos={Obtido['lacos']})")
    OkPasse, LinhaPasse = SelftestPasse()
    OkGeral = OkGeral and OkPasse
    Linhas.append(LinhaPasse)
    Linhas.insert(0, "SELFTEST " + ("OK" if OkGeral else "FALHOU") + " (detector: redutivel=0 / irredutivel=1)")
    if Imprimir:
        for Linha in Linhas:
            print(Linha, flush=True)
    return OkGeral, Linhas


def LinhaOps(Agregados, Op):
    Valores = [str(Agregados[M]["ops"][Op]) if Agregados[M]["executado"] else "-" for M in MODOS_VALIDOS]
    Total = sum(Agregados[M]["ops"][Op] for M in MODOS_VALIDOS)
    return f"{Op} | {Valores[0]} | {Valores[1]} | {Total}"


def SecaoOps(Agregados):
    Todos = set()
    for Ag in Agregados.values():
        Todos.update(Ag["ops"])
    Ordem = sorted(Todos, key=lambda o: (-sum(Agregados[M]["ops"][o] for M in MODOS_VALIDOS), o))
    Linhas = ["", "== CENSO DE OPS (instrucoes; mnemonico sem sufixo) ==", "op | dev | release | total"]
    Linhas += [LinhaOps(Agregados, Op) for Op in Ordem]
    for Modo in MODOS_VALIDOS:
        Ag = Agregados[Modo]
        Linhas.append(f"modo {Modo}: executado={'sim' if Ag['executado'] else 'nao'} funcoes={Ag['funcoes']} "
                      f"blocos={Ag['blocos']} frio_blocos={Ag['frio_blocos']} instrucoes={Ag['instr']}")
    return Linhas


def SecaoClasses(Agregados):
    Classes = set()
    for Ag in Agregados.values():
        Classes.update(Ag["vregs"])
        Classes.update(Ag["defs"])
    Linhas = ["", "== CENSO DE CLASSES DE VREG ==",
              "classe | vregs distintos dev | definicoes dev | vregs distintos release | definicoes release"]
    for Classe in sorted(Classes):
        Celulas = {}
        for Chave in ("vregs", "defs"):
            for Modo in MODOS_VALIDOS:
                Ag = Agregados[Modo]
                Celulas[(Chave, Modo)] = str(Ag[Chave][Classe]) if Ag["executado"] else "-"
        Linhas.append(f"{Classe} | {Celulas[('vregs', 'dev')]} | {Celulas[('defs', 'dev')]} | "
                      f"{Celulas[('vregs', 'release')]} | {Celulas[('defs', 'release')]}")
    return Linhas


def SecaoCfg(Agregados):
    Linhas = ["", "== CFG POR FUNCAO (dominadores + DFS; redutivel <=> toda aresta de retorno dominada) =="]
    Nomes = {"com_frio": "arestas explicitas + arestas de overflow para bloco frio (PRIMARIA)",
             "sem_frio": "so arestas explicitas (SECUNDARIA)"}
    for Variante in ("com_frio", "sem_frio"):
        Linhas.append(f"-- {Nomes[Variante]}")
        for Modo in MODOS_VALIDOS:
            Ag = Agregados[Modo]
            if not Ag["executado"]:
                Linhas.append(f"modo {Modo}: nao executado")
                continue
            Contagem = Ag["cfg"][Variante]
            Linhas.append(
                f"modo {Modo}: funcoes={Ag['funcoes']} blocos={Contagem['blocos']} alcancaveis={Contagem['alcancaveis']} "
                f"arestas={Contagem['arestas']} arestas_frio={Contagem['arestas_frio']} "
                f"lacos(arestas de retorno dominadas)={Contagem['lacos']} cabecalhos={Contagem['cabecalhos']} "
                f"arestas_retorno_nao_dominadas={Contagem['retro_nao_dom']} "
                f"funcoes_irredutiveis={Contagem['funcoes_irred']}")
    return Linhas


def SecaoIrredutiveis(Agregados):
    Linhas = ["", "== FUNCOES IRREDUTIVEIS =="]
    for Variante in ("com_frio", "sem_frio"):
        Linhas.append(f"-- variante {Variante}")
        Achados = []
        for Modo in MODOS_VALIDOS:
            for Arquivo, Nome, Qtd, Ruins in Agregados[Modo]["irred"][Variante]:
                Achados.append(f"modo={Modo} | {Arquivo} | funcao {Nome} | arestas_ruins={Qtd} | "
                               f"exemplos(origem->destino)={Ruins}")
        Linhas += Achados if Achados else ["(nenhuma)"]
    return Linhas


def SecaoAnomalias(Agregados):
    Linhas = ["", "== ANOMALIAS DO PARSER / CFG (contagem por modo) =="]
    Chaves = set()
    for Ag in Agregados.values():
        Chaves.update(Ag["anom"])
    if not Chaves:
        Linhas.append("(nenhuma)")
    for Chave in sorted(Chaves):
        Linhas.append(f"{Chave}: " + " ".join(f"{M}={Agregados[M]['anom'][Chave]}" for M in MODOS_VALIDOS))
    return Linhas


def SecaoCobertura(Resultados):
    Linhas = ["", "== COBERTURA (grupo x modo) ==",
              "grupo | modo | total | ok | falhou | sem_dump | timeout | orcamento | erro_interno | segundos de build"]
    Grupos = {}
    for Res in Resultados:
        Cont = Grupos.setdefault((Res["grupo"], Res["modo"]), collections.Counter())
        Cont["total"] += 1
        Cont[Res["status"]] += 1
        Cont["seg"] += Res["segundos"]
    for (Grupo, Modo), Cont in sorted(Grupos.items()):
        Linhas.append(f"{Grupo} | {Modo} | {Cont['total']} | {Cont['ok']} | {Cont['falhou']} | {Cont['sem_dump']} | "
                      f"{Cont['timeout']} | {Cont['orcamento']} | {Cont['erro_interno']} | {Cont['seg']:.1f}")
    return Linhas


def SecaoArquivos(Resultados):
    Linhas = ["", "== ARQUIVOS (um por linha: status | modo | segundos | arquivo | motivo) =="]
    for Res in sorted(Resultados, key=lambda x: (x["modo"], x["arquivo"])):
        Linhas.append(f"{Res['status']} | {Res['modo']} | {Res['segundos']} s | {Res['arquivo']} | {Res['motivo']}")
    return Linhas


def EscreverRelatorio(Caminho, Cabecalho, Resultados, Agregados):
    Linhas = list(Cabecalho)
    Linhas += SecaoCobertura(Resultados)
    Linhas += SecaoOps(Agregados)
    Linhas += SecaoClasses(Agregados)
    Linhas += SecaoCfg(Agregados)
    Linhas += SecaoIrredutiveis(Agregados)
    Linhas += SecaoAnomalias(Agregados)
    Linhas += SecaoArquivos(Resultados)
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    Caminho.write_text("\n".join(Linhas) + "\n", encoding="utf-8")


def LinhasPonto(Ponto):
    """Cabecalho do relatorio: qual ponto da pipeline foi medido e o comando usado."""
    if Ponto:
        return [f"Ponto de corte: passe '{Ponto}' (secao '== passe {Ponto}' do dump; so release)",
                "Comando por fonte: forja build --backend x64 --release --debug-code passes <fonte> -o <exe>; dump = <exe>.debug.txt"]
    return ["Ponto de corte: BIR do --debug-code bir,json (release = pos-todos-os-passes; dev = pre-otimizacao)",
            "Comando por fonte: forja build --backend x64 --debug-code bir,json [--release] <fonte> -o <exe>; dump = <exe>.debug.jsonl"]


def Principal(Argv=None):
    Args = Argumentos(Argv)
    if Args.selftest:
        Ok, _ = Selftest()
        return 0 if Ok else 1
    if not Args.forja or not Args.saida or not Args.grupo:
        print("ERRO: --forja, --saida e pelo menos um --grupo sao obrigatorios", file=sys.stderr)
        return 2
    SelftestOk, LinhasSelftest = Selftest()
    if not SelftestOk:
        print("SELFTEST FALHOU: a execucao em massa nao roda", file=sys.stderr)
        return 3
    Inicio = time.time()
    Grupos = [ParsearGrupo(G) for G in Args.grupo]
    Tarefas = MontarTarefas(Grupos, Args.ponto_corte)
    print(f"tarefas: {len(Tarefas)}", flush=True)
    Saida = Path(Args.saida)
    PastaTmp = Path(Args.tmp) if Args.tmp else Saida.parent / "_tmp_bir"
    PastaTmp.mkdir(parents=True, exist_ok=True)
    Versao = VersaoForja(Args.forja)
    Resultados = ExecutarTodas(Tarefas, Args.forja, Args.jobs, Inicio + Args.orcamento_seg, PastaTmp, Args.timeout_seg)
    shutil.rmtree(PastaTmp, ignore_errors=True)
    TempoTotal = time.time() - Inicio
    ModosRodados = {T["modo"] for T in Tarefas}
    Agregados = Agregar(Resultados, ModosRodados)
    Agora = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    Cabecalho = [
        "RELATORIO W0 - censo de ops, classes e CFG da BIR (auditar_bir_wasm.py v" + VERSAO + ")",
        f"Gerado em: {Agora}",
        f"Compilador: {Versao} (caminho: {Args.forja})",
        *LinhasPonto(Args.ponto_corte),
        f"Jobs: {Args.jobs} | timeout por build: {Args.timeout_seg} s | orcamento: {Args.orcamento_seg} s | tempo de parede: {TempoTotal:.1f} s",
        f"Tarefas: {len(Tarefas)} (fontes x modos)",
        *[f"Grupo {G['nome']}: caminho={G['caminho']} modos={','.join(G['modos'])} prefixo='{G['prefixo']}' max={G['max']}" for G in Grupos],
        *LinhasSelftest,
    ]
    EscreverRelatorio(Saida, Cabecalho, Resultados, Agregados)
    Contagem = collections.Counter(R["status"] for R in Resultados)
    print(f"status: {dict(Contagem)}", flush=True)
    print(f"tempo de parede: {TempoTotal:.1f} s", flush=True)
    print(f"relatorio: {Saida}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
