# -*- coding: utf-8 -*-
"""verificar_conjunto.py - prova por execucao dos conjuntos L, Cx, Dx, C_forja e D_forja (P-IA1-a06).

Versao 0.2.0
Changelog:
  0.2.0 (11/11/2026) esperado.json no formato "casos" (igual ao dos E); pasta temporaria vira <pasta> na saida (X0102
        trazia o caminho); checagens do catalogo em tabela (FalhaDeclarada); layout fechado com montagem\\montar_conjuntos.py.
  0.1.0 (11/11/2026) primeira versao: L (gabarito == execucao real, Forja == C), Cx/Dx (o bug falha como
        diagnostico_inicial.txt e como o catalogo declara; a referencia passa com o esperado.json),
        C_forja/D_forja (idem, so Forja), --selftest (gabarito errado e referencia quebrada sao acusados).

Uso:
  python -I verificar_conjunto.py --forja <forja.exe> [--conjunto L,Cx,Dx,C_forja,D_forja] [--tarefa L01,Cx03]
  python -I verificar_conjunto.py --forja <forja.exe> --selftest

Saida: uma linha JSON por tarefa+linguagem {conjunto,id,lingua,ok,motivos} e uma linha final {"resumo":...}.
Codigo de saida: 0 tudo verde, 1 algo vermelho, 2 uso, 3 interno.
Leitura: linguagens.json (SO comandos; nao e alterado). Escrita: apenas diretorios temporarios proprios.
Reversao: apagar este arquivo; nada mais depende dele.
Depende de: Python 3.9+ (biblioteca padrao), gcc no PATH, forja.exe (--forja ou IA_FIRST_FORJA).
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent
CONJUNTOS = ["L", "Cx", "Dx", "C_forja", "D_forja"]
CONJUNTOS_DUAS_LINGUAS = ["Cx", "Dx"]
COMANDO_TESTE_FORJA = ["{forja}", "test", "{fonte}"]  # linguagens.json nao tem "test"
LIMITE_SEGUNDOS = 10
LIMITE_BUILD = 60


def LerJson(Caminho):
    with open(Caminho, encoding="utf-8") as Arq:
        return json.load(Arq)


def Normalizar(Dados):
    return Dados.decode("utf-8", errors="replace").replace("\r\n", "\n")


def Executar(Comando, Cwd, Limite):
    """Roda um processo com tempo limite. Devolve rc, stdout, stderr (texto normalizado)."""
    try:
        P = subprocess.run(Comando, cwd=str(Cwd), capture_output=True, timeout=Limite)
    except subprocess.TimeoutExpired:
        return {"rc": None, "stdout": "", "stderr": "tempo excedido"}
    except OSError as Erro:
        return {"rc": None, "stdout": "", "stderr": "nao executou: %s" % Erro}
    return {"rc": P.returncode, "stdout": Normalizar(P.stdout), "stderr": Normalizar(P.stderr)}


def Substituir(Comando, Subs):
    Saida = []
    for Item in Comando:
        Chave = Item.strip("{}")
        if Item.startswith("{") and Chave in Subs:
            Saida.append(Subs[Chave])
        else:
            Saida.append(Item)
    return Saida


def Exibir(Comando, Tmp, Ctx):
    """Texto do comando sem caminhos da maquina (o diagnostico_inicial.txt tem de ser reproduzivel)."""
    Texto = " ".join(Comando).replace(str(Tmp) + os.sep, "").replace(str(Ctx["forja"]), "forja")
    return Texto.replace("\\", "/")


def FaseFalhou(Fase, R):
    if R["rc"] != 0:
        return True
    # check silencioso: qualquer texto (aviso do gcc, diagnostico da Forja) conta como falha
    return Fase == "check" and (R["stdout"].strip() != "" or R["stderr"].strip() != "")


def RodarFase(Ctx, Lingua, Fase, Tmp):
    Cfg = Ctx["cfg"][Lingua]
    Subs = {"forja": str(Ctx["forja"]), "fonte": "prog" + Cfg["extensao"], "fontes": "prog" + Cfg["extensao"],
            "exe": str(Tmp / "prog.exe")}
    Molde = COMANDO_TESTE_FORJA if Fase == "test" else Cfg[Fase]
    Comando = Substituir(Molde, Subs)
    R = Executar(Comando, Tmp, LIMITE_BUILD if Fase == "build" else LIMITE_SEGUNDOS)
    for Campo in ("stdout", "stderr"):
        R[Campo] = TirarPastaTemporaria(R[Campo], Tmp)
    R.update({"fase": Fase, "comando": Exibir(Comando, Tmp, Ctx)})
    return R


def TirarPastaTemporaria(Texto, Tmp):
    """A pasta temporaria muda a cada execucao (aparece em procurei_em do X0102): vira <pasta> para a saida ser deterministica."""
    for Forma in (Tmp.as_posix(), str(Tmp), str(Tmp).replace("\\", "\\\\")):
        Texto = Texto.replace(Forma, "<pasta>")
    return Texto


def FasesDoModo(Modo):
    return ["check", "test"] if Modo == "test" else ["check", "build", "run"]


def RodarFases(Ctx, Lingua, Pasta, Modo):
    """check -> (test | build -> run). Devolve o resultado da fase onde parou (a ultima, se nada falhou antes)."""
    Ext = Ctx["cfg"][Lingua]["extensao"]
    with tempfile.TemporaryDirectory(prefix="a06_") as Dir:
        Trabalho = Path(Dir)
        shutil.copyfile(Path(Pasta) / ("prog" + Ext), Trabalho / ("prog" + Ext))
        Fases = FasesDoModo(Modo)
        for Fase in Fases:
            Resposta = RodarFase(Ctx, Lingua, Fase, Trabalho)
            if Fase == Fases[-1] or FaseFalhou(Fase, Resposta):
                return Resposta


def TextoDiagnostico(R):
    return "$ %s\nexit: %s\n--- stdout ---\n%s--- stderr ---\n%s" % (
        R["comando"], R["rc"], ComNovaLinha(R["stdout"]), ComNovaLinha(R["stderr"]))


def ComNovaLinha(Texto):
    return Texto if Texto == "" or Texto.endswith("\n") else Texto + "\n"


def Resultado(Conj, Id, Lingua, Motivos):
    return {"conjunto": Conj, "id": Id, "lingua": Lingua, "ok": not Motivos, "motivos": Motivos}


def MotivosReferencia(R, Esp, Modo):
    """A referencia tem de passar limpa e bater com stdout/exit do esperado."""
    if R["fase"] == "run" or (Modo == "test" and R["fase"] == "test" and R["rc"] == 0):
        Motivos = []
        if R["rc"] != Esp.get("exit", 0):
            Motivos.append("referencia: exit %s, esperado %s" % (R["rc"], Esp.get("exit", 0)))
        if R["stdout"] != Esp.get("stdout", ""):
            Motivos.append("referencia: stdout %r, esperado %r" % (R["stdout"][:200], Esp.get("stdout", "")[:200]))
        return Motivos
    return ["referencia nao passou limpa na fase %s (rc=%s): %s" % (R["fase"], R["rc"], (R["stdout"] + R["stderr"])[:300])]


def MotivosL(Ctx, Id, Lingua):
    Ref = Ctx["raiz"] / "referencias" / Lingua / "L" / Id
    Prog = Ctx["raiz"] / "tarefas" / "L" / Id / Lingua
    Ext = Ctx["cfg"][Lingua]["extensao"]
    if (Ref / ("prog" + Ext)).read_bytes() != (Prog / ("prog" + Ext)).read_bytes():
        return ["o programa da tarefa difere da referencia"]
    Gab = LerJson(Ref / "gabarito.json")
    R = RodarFases(Ctx, Lingua, Ref, "run")
    Motivos = MotivosReferencia(R, Gab, "run")
    Ctx["gabaritos_L"].setdefault(Id, {})[Lingua] = (Gab.get("stdout"), Gab.get("exit"))
    return Motivos


def MotivosIgualdadeL(Ctx, Id):
    Par = Ctx["gabaritos_L"].get(Id, {})
    if len(Par) == 2 and Par["forja"] != Par["c"]:
        return ["gabarito Forja difere do gabarito C"]
    return []


def MotivoCodigo(R, Decl, Esp):
    if "codigo" in Decl and ('"codigo": "%s"' % Decl["codigo"]) not in R["stdout"] + R["stderr"]:
        return "codigo %s nao aparece na saida" % Decl["codigo"]
    return ""


def MotivoPadrao(R, Decl, Esp):
    if "padrao" in Decl and not re.search(Decl["padrao"], R["stdout"] + R["stderr"]):
        return "padrao %r nao aparece na saida" % Decl["padrao"]
    return ""


def MotivoExit(R, Decl, Esp):
    if "exit" in Decl and R["rc"] != Decl["exit"]:
        return "exit %s, catalogo diz %s" % (R["rc"], Decl["exit"])
    return "exit 0, catalogo diz falha" if Decl.get("exit_nao_zero") and R["rc"] == 0 else ""


def MotivoSaidaDifere(R, Decl, Esp):
    if Decl.get("saida_difere") and R["stdout"] == Esp.get("stdout", ""):
        return "stdout igual ao esperado, catalogo diz que difere"
    return ""


CHECAGENS_DA_DECLARACAO = [MotivoCodigo, MotivoPadrao, MotivoExit, MotivoSaidaDifere]


def FalhaDeclarada(R, Decl, Esp):
    """Confere que o programa com bug falha na fase e do jeito que o catalogo declara."""
    if R["fase"] != Decl["fase"]:
        return ["bug falhou na fase %s, catalogo diz %s" % (R["fase"], Decl["fase"])]
    Achados = [Checagem(R, Decl, Esp) for Checagem in CHECAGENS_DA_DECLARACAO]
    return [Motivo for Motivo in Achados if Motivo]


def MotivosBug(Ctx, Conj, Id, Lingua):
    Pasta = PastaTarefa(Ctx, Conj, Id, Lingua)
    Decl = Ctx["catalogos"][Conj][Id][Lingua]
    Esp = LerJson(Pasta / "esperado.json")["casos"][0]
    Modo = Decl.get("modo", "run")
    R = RodarFases(Ctx, Lingua, Pasta, Modo)
    Motivos = FalhaDeclarada(R, Decl, Esp)
    if (Pasta / "diagnostico_inicial.txt").read_text(encoding="utf-8") != TextoDiagnostico(R):
        Motivos.append("diagnostico_inicial.txt difere da saida real")
    Ref = Ctx["raiz"] / "referencias" / Lingua / Conj / Id
    Nome = "prog" + Ctx["cfg"][Lingua]["extensao"]
    if (Ref / Nome).read_bytes() == (Pasta / Nome).read_bytes():
        Motivos.append("referencia identica ao programa com bug")
    return Motivos + MotivosReferencia(RodarFases(Ctx, Lingua, Ref, Modo), Esp, Modo)


def PastaTarefa(Ctx, Conj, Id, Lingua):
    Base = Ctx["raiz"] / "tarefas" / Conj / Id
    return Base / Lingua if Conj in CONJUNTOS_DUAS_LINGUAS else Base


def LinguasDe(Conj):
    return ["forja", "c"] if Conj in ["L"] + CONJUNTOS_DUAS_LINGUAS else ["forja"]


def VerificarTarefa(Ctx, Conj, Id, Lingua):
    try:
        Motivos = MotivosL(Ctx, Id, Lingua) if Conj == "L" else MotivosBug(Ctx, Conj, Id, Lingua)
    except (OSError, KeyError, ValueError) as Erro:
        Motivos = ["erro ao verificar: %s %s" % (type(Erro).__name__, Erro)]
    return Resultado(Conj, Id, Lingua, Motivos)


def IdsDoConjunto(Ctx, Conj, Filtro):
    Cat = Ctx["catalogos"][Conj]
    return [Id for Id in Cat if not Filtro or Id in Filtro]


def CarregarCatalogos(Raiz, Conjuntos):
    Catalogos = {}
    for Conj in Conjuntos:
        Lista = LerJson(Raiz / "tarefas" / Conj / "catalogo.json")["tarefas"]
        Catalogos[Conj] = {Item["id"]: Item for Item in Lista}
    return Catalogos


def MontarCtx(Raiz, Forja, Conjuntos):
    Cfg = LerJson(RAIZ / "linguagens.json")["linguagens"]
    return {"raiz": Raiz, "forja": Forja, "cfg": Cfg, "gabaritos_L": {},
            "catalogos": CarregarCatalogos(Raiz, Conjuntos)}


def VerificarConjuntos(Raiz, Forja, Conjuntos, Filtro):
    Ctx = MontarCtx(Raiz, Forja, Conjuntos)
    Todos = []
    for Conj in Conjuntos:
        for Id in IdsDoConjunto(Ctx, Conj, Filtro):
            for Lingua in LinguasDe(Conj):
                Todos.append(VerificarTarefa(Ctx, Conj, Id, Lingua))
            if Conj == "L":
                Extra = MotivosIgualdadeL(Ctx, Id)
                Todos.append(Resultado(Conj, Id, "forja=c", Extra))
    return Todos


def EmitirResultados(Todos):
    for Item in Todos:
        print(json.dumps(Item, ensure_ascii=False))
    Falhas = [T for T in Todos if not T["ok"]]
    Resumo = {"resumo": {"verificacoes": len(Todos), "verdes": len(Todos) - len(Falhas), "vermelhas": len(Falhas)}}
    print(json.dumps(Resumo))
    return 0 if Todos and not Falhas else 1


def CopiarArvore(Raiz, Destino):
    for Pasta in ("tarefas", "referencias"):
        shutil.copytree(Raiz / Pasta, Destino / Pasta, ignore=shutil.ignore_patterns("E"))


def Substituir_Texto(Arq, De, Para):
    Texto = Arq.read_text(encoding="utf-8")
    if De not in Texto:
        raise ValueError("selftest: trecho %r nao existe em %s" % (De, Arq))
    Arq.write_text(Texto.replace(De, Para), encoding="utf-8")


def PlantarDefeitos(Dest):
    """Gabarito de L errado, referencia C quebrada, diagnostico adulterado, referencia de Cx quebrada."""
    Gab = Dest / "referencias" / "forja" / "L" / "L01" / "gabarito.json"
    Gab.write_text(json.dumps({"stdout": "999999\n", "exit": 0}), encoding="utf-8")
    Quebrada = Dest / "referencias" / "c" / "L" / "L02" / "prog.c"
    Quebrada.write_text(Quebrada.read_text(encoding="utf-8") + "\nint quebrado( {\n", encoding="utf-8")
    Diag = Dest / "tarefas" / "Cx" / "Cx01" / "forja" / "diagnostico_inicial.txt"
    Diag.write_text(Diag.read_text(encoding="utf-8").replace("E0", "E9", 1), encoding="utf-8")
    Ref = Dest / "referencias" / "forja" / "Cx" / "Cx02" / "prog.fj"
    Ref.write_text(Ref.read_text(encoding="utf-8") + "\nproc quebrado( {\n", encoding="utf-8")
    Cat = Dest / "tarefas" / "Dx" / "catalogo.json"
    Cat.write_text(Cat.read_text(encoding="utf-8").replace('"R0006"', '"R0099"', 1), encoding="utf-8")


def Selftest(Raiz, Forja):
    """Controle positivo (copia fiel passa) e controle negativo (defeitos plantados sao acusados)."""
    Alvos = {"L01", "L02", "Cx01", "Cx02", "Dx01"}
    with tempfile.TemporaryDirectory(prefix="a06_self_") as Dir:
        Dest = Path(Dir)
        CopiarArvore(Raiz, Dest)
        Limpo = VerificarConjuntos(Dest, Forja, ["L", "Cx", "Dx"], Alvos)
        PlantarDefeitos(Dest)
        Sujo = VerificarConjuntos(Dest, Forja, ["L", "Cx", "Dx"], Alvos)
    return ResultadoSelftest(Limpo, Sujo)


def ResultadoSelftest(Limpo, Sujo):
    VerdesLimpo = all(T["ok"] for T in Limpo) and len(Limpo) > 0
    Acusados = {(T["conjunto"], T["id"], T["lingua"]) for T in Sujo if not T["ok"]}
    Esperados = {("L", "L01", "forja"), ("L", "L01", "forja=c"), ("L", "L02", "c"),
                 ("Cx", "Cx01", "forja"), ("Cx", "Cx02", "forja"), ("Dx", "Dx01", "forja")}
    Faltou = sorted(Esperados - Acusados)
    print(json.dumps({"selftest": {"controle_positivo_verde": VerdesLimpo, "defeitos_acusados": sorted(map(list, Acusados)),
                                   "defeitos_nao_acusados": Faltou}}))
    return 0 if VerdesLimpo and not Faltou else 1


def AcharForja(Arg):
    Caminho = Arg or os.environ.get("IA_FIRST_FORJA")
    if not Caminho or not Path(Caminho).is_file():
        print("uso: informe --forja <forja.exe> ou IA_FIRST_FORJA", file=sys.stderr)
        sys.exit(2)
    return Path(Caminho).resolve()


def LerArgumentos():
    Parser = argparse.ArgumentParser(description="Prova por execucao dos conjuntos L, Cx, Dx, C_forja, D_forja")
    Parser.add_argument("--forja", help="forja.exe da copia congelada")
    Parser.add_argument("--conjunto", default=",".join(CONJUNTOS))
    Parser.add_argument("--tarefa", default="", help="ids separados por virgula (L01,Cx03,...)")
    Parser.add_argument("--raiz", default=str(RAIZ), help="pasta com tarefas\\ e referencias\\")
    Parser.add_argument("--selftest", action="store_true")
    return Parser.parse_args()


def Principal():
    Args = LerArgumentos()
    Forja = AcharForja(Args.forja)
    Raiz = Path(Args.raiz).resolve()
    if Args.selftest:
        return Selftest(Raiz, Forja)
    Filtro = {T for T in Args.tarefa.split(",") if T}
    return EmitirResultados(VerificarConjuntos(Raiz, Forja, Args.conjunto.split(","), Filtro))


if __name__ == "__main__":
    try:
        sys.exit(Principal())
    except Exception as Erro:  # sem traceback do verificador: erro interno vira exit 3
        print("erro interno: %s %s" % (type(Erro).__name__, Erro), file=sys.stderr)
        sys.exit(3)
