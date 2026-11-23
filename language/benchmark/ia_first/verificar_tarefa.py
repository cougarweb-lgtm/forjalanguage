# -*- coding: utf-8 -*-
"""verificar_tarefa.py - verificador UNICO das tarefas E do benchmark IA FIRST (P-IA1-a05).

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao: programa (args/env/stdin/stdout/exit/arquivos), DLL por ctypes,
        limite de 10 s, saida cortada em 64 KiB, sem traceback do verificador, --referencias, --selftest.

Uso:
  python verificar_tarefa.py --tarefa E01 --lingua forja --fonte <celula>\\prog.fj [--forja <forja.exe>] [--json]
  python verificar_tarefa.py --referencias [--lingua forja,c,python] [--tarefa E01,E02] [--json]
  python verificar_tarefa.py --selftest

Saida: uma linha por verificacao (ou JSON com --json). Codigo de saida: 0 verde, 1 vermelho, 2 uso, 3 interno.
Reversao: o script so le tarefas\\ e referencias\\ e escreve em diretorios temporarios que ele mesmo apaga.
Depende de: Python 3.9+ (biblioteca padrao). Compilador da Forja por --forja ou IA_FIRST_FORJA.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent
MARCA_TRACEBACK = "Traceback (most recent call last)"
FORJA_PADRAO = Path(os.environ.get("TEMP", ".")) / "forja_ia1_base" / "forja.exe"
TIPOS_CTYPES = {"i32": "c_int32", "i64": "c_int64", "u32": "c_uint32", "f64": "c_double", "f32": "c_float"}
LINGUAS_PADRAO = ["forja", "c", "python"]

# Roda num processo a parte: uma DLL com defeito nao derruba o verificador.
HARNESS_DLL = (
    "import ctypes, json, sys\n"
    "d = json.loads(sys.argv[2])\n"
    "t = {'c_int32': ctypes.c_int32, 'c_int64': ctypes.c_int64, 'c_uint32': ctypes.c_uint32,\n"
    "     'c_double': ctypes.c_double, 'c_float': ctypes.c_float}\n"
    "f = ctypes.CDLL(sys.argv[1])[d['simbolo']]\n"
    "f.argtypes = [t[x] for x in d['tipos_ctypes']]\n"
    "f.restype = t[d['retorno_ctypes']]\n"
    "print(json.dumps([f(*c['args']) for c in d['casos']]))\n"
)


def LerJson(Caminho):
    with open(Caminho, encoding="utf-8") as Arq:
        return json.load(Arq)


def Substituir(Comando, Subs):
    """Troca os placeholders; um valor lista (ex.: {fontes}) vira varios argumentos."""
    Saida = []
    for Item in Comando:
        Chave = Item.strip("{}")
        if Item.startswith("{") and Chave in Subs:
            Valor = Subs[Chave]
            Saida.extend(Valor if isinstance(Valor, list) else [Valor])
        else:
            Saida.append(Item)
    return Saida


def MontarAmbiente(Extra):
    """Ambiente do processo: o atual sem as variaveis TAREFA_*, mais o 'env' do caso (null remove)."""
    Env = {K: V for K, V in os.environ.items() if not K.startswith("TAREFA_")}
    for Nome, Valor in (Extra or {}).items():
        if Valor is None:
            Env.pop(Nome, None)
        else:
            Env[Nome] = Valor
    return Env


def Executar(Comando, Cwd, Opcoes):
    """Roda um processo com tempo limite. Devolve rc, stdout, stderr (bytes ja cortados) e flags."""
    Vazio = {"rc": None, "stdout": b"", "stderr": b"", "estourou": False, "cortada": False}
    try:
        Proc = subprocess.run(Comando, cwd=str(Cwd), env=MontarAmbiente(Opcoes.get("env")),
                              input=Opcoes.get("stdin", b""), capture_output=True, timeout=Opcoes["limite"])
    except subprocess.TimeoutExpired:
        return dict(Vazio, estourou=True)
    except OSError as Erro:
        return dict(Vazio, stderr=str(Erro).encode("utf-8"), falha_exec=True)
    Maximo = Opcoes["maximo"]
    return dict(Vazio, rc=Proc.returncode, stdout=Proc.stdout[:Maximo], stderr=Proc.stderr[:Maximo],
                cortada=len(Proc.stdout) > Maximo or len(Proc.stderr) > Maximo)


def Texto(Dados):
    return Dados.decode("utf-8", errors="replace").replace("\r\n", "\n")


def PrepararSubs(Ctx, Fonte, Tmp):
    Pasta = Path(Fonte).parent
    Fontes = sorted(str(P) for P in Pasta.glob("*" + Ctx["cfg_lingua"]["extensao"]))
    return {"forja": str(Ctx["forja"]), "fonte": str(Fonte), "fontes": Fontes,
            "exe": str(Tmp / "prog.exe"), "dll": str(Tmp / "prog.dll")}


def Compilar(Ctx, Chave, Subs, Tmp):
    """Roda o comando de build (build ou build_dll). Devolve (ok, saida_resumida)."""
    Comando = Ctx["cfg_lingua"].get(Chave)
    if not Comando:
        return True, ""
    R = Executar(Substituir(Comando, Subs), Tmp, {"limite": Ctx["limite_build"], "maximo": Ctx["maximo"]})
    if R["rc"] != 0:
        return False, "build falhou (rc=%s): %s" % (R["rc"], Texto(R["stdout"] + R["stderr"])[:600])
    return True, ""


def CompararArquivos(Esperados, Work):
    Motivos = []
    for Nome, Conteudo in Esperados.items():
        Alvo = Path(Work) / Nome
        if not Alvo.is_file():
            Motivos.append("arquivo %s nao foi criado" % Nome)
        elif Texto(Alvo.read_bytes()) != Conteudo:
            Motivos.append("arquivo %s difere: obtido %r" % (Nome, Texto(Alvo.read_bytes())[:200]))
    return Motivos


def CompararCaso(Esp, R, Work):
    """Lista de motivos de reprovacao do caso (vazia = verde)."""
    if R["estourou"]:
        return ["tempo excedido"]
    if R.get("falha_exec"):
        return ["nao executou: " + Texto(R["stderr"])]
    Motivos = []
    if R["cortada"]:
        Motivos.append("saida maior que o limite (cortada)")
    if MARCA_TRACEBACK in Texto(R["stderr"]):
        Motivos.append("traceback no stderr")
    if R["rc"] != Esp.get("exit", 0):
        Motivos.append("exit %s, esperado %s" % (R["rc"], Esp.get("exit", 0)))
    if Texto(R["stdout"]) != Esp.get("stdout", ""):
        Motivos.append("stdout difere: obtido %r, esperado %r" % (Texto(R["stdout"])[:200], Esp.get("stdout", "")[:200]))
    return Motivos + CompararArquivos(Esp.get("arquivos", {}), Work)


def CasosDoEsperado(Esp):
    return Esp["casos"] if "casos" in Esp else [Esp]


def RodarCasos(Ctx, Esp, Subs, Tmp):
    Resultados = []
    Base = Substituir(Ctx["cfg_lingua"]["run"], Subs)
    for I, Caso in enumerate(CasosDoEsperado(Esp)):
        Work = Tmp / ("caso%d" % I)
        if Ctx["entrada"].is_dir():
            shutil.copytree(Ctx["entrada"], Work)
        else:
            Work.mkdir()
        Opc = {"env": Caso.get("env"), "stdin": Caso.get("stdin", "").encode("utf-8"),
               "limite": Ctx["limite"], "maximo": Ctx["maximo"]}
        R = Executar(Base + [str(A) for A in Caso.get("args", [])], Work, Opc)
        Resultados.append({"caso": I, "args": Caso.get("args", []), "motivos": CompararCaso(Caso, R, Work)})
    return Resultados


def VerificarPrograma(Ctx, Esp, Fonte, Tmp):
    Subs = PrepararSubs(Ctx, Fonte, Tmp)
    Ok, Saida = Compilar(Ctx, "build", Subs, Tmp)
    if not Ok:
        return {"verde": False, "motivo": Saida, "casos": []}
    Casos = RodarCasos(Ctx, Esp, Subs, Tmp)
    Falhas = [C for C in Casos if C["motivos"]]
    Motivo = "; ".join("caso %d %s: %s" % (C["caso"], C["args"], " | ".join(C["motivos"])) for C in Falhas)
    return {"verde": not Falhas, "motivo": Motivo, "casos": Casos}


def AvaliarDll(Esp, R):
    if R["estourou"] or R["rc"] != 0:
        return {"verde": False, "motivo": "carga/chamada da DLL falhou: " + Texto(R["stderr"])[-300:], "casos": []}
    Obtido = json.loads(Texto(R["stdout"]).strip().splitlines()[-1])
    Esperado = [C["retorno"] for C in Esp["casos"]]
    Motivo = "" if Obtido == Esperado else "retornos %s, esperado %s" % (Obtido, Esperado)
    return {"verde": Obtido == Esperado, "motivo": Motivo, "casos": []}


def VerificarDll(Ctx, Esp, Fonte, Tmp):
    Subs = PrepararSubs(Ctx, Fonte, Tmp)
    Ok, Saida = Compilar(Ctx, "build_dll", Subs, Tmp)
    if not Ok:
        return {"verde": False, "motivo": Saida, "casos": []}
    Dados = {"simbolo": Esp["simbolo"], "tipos_ctypes": [TIPOS_CTYPES[T] for T in Esp["args_tipos"]],
             "retorno_ctypes": TIPOS_CTYPES[Esp["retorno"]], "casos": Esp["casos"]}
    Comando = [sys.executable, "-I", "-c", HARNESS_DLL, Subs["dll"], json.dumps(Dados)]
    R = Executar(Comando, Tmp, {"limite": Ctx["limite"], "maximo": Ctx["maximo"]})
    return AvaliarDll(Esp, R)


def MontarCtx(Args, Cfg, Lingua, TarefaDir):
    Forja = Args.Forja or os.environ.get("IA_FIRST_FORJA") or FORJA_PADRAO
    return {"cfg_lingua": Cfg["linguagens"][Lingua], "forja": Path(Forja),
            "limite": Cfg["limite_segundos"], "limite_build": Cfg["limite_build_segundos"],
            "maximo": Cfg["saida_max_bytes"], "entrada": TarefaDir / "entrada"}


def Disponivel(Ctx):
    """True se o programa que compila/roda a linguagem existe na maquina."""
    Cfg = Ctx["cfg_lingua"]
    Ferramenta = (Cfg.get("build") or Cfg["run"])[0]
    if Ferramenta == "{forja}":
        return Ctx["forja"].is_file()
    return shutil.which(Ferramenta) is not None


def Verificar(Args, Cfg, Alvo):
    """Verificacao completa de (tarefa, lingua, fonte). Nunca levanta excecao."""
    Tarefa, Lingua, Fonte = Alvo
    Base = {"tarefa": Tarefa, "lingua": Lingua}
    try:
        TarefaDir = Path(Args.Tarefas) / Tarefa
        Esp = LerJson(TarefaDir / "esperado.json")
        Ctx = MontarCtx(Args, Cfg, Lingua, TarefaDir)
        if Lingua not in Esp.get("so_linguagens", [Lingua]):
            return dict(Base, verde=None, motivo="tarefa so de " + ",".join(Esp["so_linguagens"]))
        if not Disponivel(Ctx):
            return dict(Base, verde=None, motivo="linguagem indisponivel")
        Corpo = VerificarDll if Esp.get("tipo") == "dll" else VerificarPrograma
        with tempfile.TemporaryDirectory(prefix="ia_first_") as Tmp:
            return dict(Base, **Corpo(Ctx, Esp, Path(Fonte).resolve(), Path(Tmp)))
    except Exception as Erro:  # o verificador nunca vaza traceback
        return dict(Base, verde=False, motivo="erro interno do verificador: %s: %s" % (type(Erro).__name__, Erro))


def ModoReferencias(Args, Cfg):
    Linguas = Args.Lingua.split(",") if Args.Lingua else LINGUAS_PADRAO
    Tarefas = Args.Tarefa.split(",") if Args.Tarefa else sorted(P.name for P in Path(Args.Tarefas).iterdir() if P.is_dir())
    Resultados = []
    for Tarefa in Tarefas:
        for Lingua in Linguas:
            Fonte = Path(Args.ReferenciasDir) / Lingua / Tarefa / ("prog" + Cfg["linguagens"][Lingua]["extensao"])
            if Fonte.is_file():
                Resultados.append(Verificar(Args, Cfg, (Tarefa, Lingua, Fonte)))
            else:
                Resultados.append({"tarefa": Tarefa, "lingua": Lingua, "verde": None, "motivo": "sem referencia"})
    return Resultados


def Simbolo(R):
    return {True: "OK", False: "FALHA", None: "-"}[R["verde"]]


def ImprimirTabela(Resultados, Linguas):
    print("tarefa  " + "  ".join("%-7s" % L for L in Linguas))
    for T in sorted({R["tarefa"] for R in Resultados}):
        Linha = {R["lingua"]: Simbolo(R) for R in Resultados if R["tarefa"] == T}
        print("%-7s " % T + "  ".join("%-7s" % Linha.get(L, "-") for L in Linguas))
    for R in Resultados:
        if R["verde"] is False:
            print("  FALHA %s %s: %s" % (R["tarefa"], R["lingua"], R["motivo"][:400]))


def Argumentos():
    P = argparse.ArgumentParser(description="Verificador unico das tarefas E (IA FIRST)")
    P.add_argument("--tarefa", dest="Tarefa", help="id (E01) ou lista separada por virgula no modo --referencias")
    P.add_argument("--lingua", dest="Lingua", help="forja|c|python|js (lista no modo --referencias)")
    P.add_argument("--fonte", dest="Fonte", help="arquivo principal da solucao")
    P.add_argument("--forja", dest="Forja", help="exe do compilador (padrao: IA_FIRST_FORJA ou %%TEMP%%\\forja_ia1_base\\forja.exe)")
    P.add_argument("--tarefas", dest="Tarefas", default=str(RAIZ / "tarefas" / "E"))
    P.add_argument("--referencias-dir", dest="ReferenciasDir", default=str(RAIZ / "referencias"))
    P.add_argument("--referencias", dest="Referencias", action="store_true", help="roda todas as referencias")
    P.add_argument("--selftest", dest="Selftest", action="store_true")
    P.add_argument("--json", dest="Json", action="store_true")
    return P.parse_args()


def Principal():
    Args = Argumentos()
    if Args.Selftest:
        sys.path.insert(0, str(RAIZ))
        import selftest_verificador
        return selftest_verificador.Executar(sys.modules[__name__])
    Cfg = LerJson(RAIZ / "linguagens.json")
    if Args.Referencias:
        Res = ModoReferencias(Args, Cfg)
        if Args.Json:
            print(json.dumps(Res, ensure_ascii=False))
        else:
            ImprimirTabela(Res, Args.Lingua.split(",") if Args.Lingua else LINGUAS_PADRAO)
        return 0 if all(R["verde"] is not False for R in Res) else 1
    if not (Args.Tarefa and Args.Lingua and Args.Fonte):
        print("uso: --tarefa --lingua --fonte (ou --referencias / --selftest)", file=sys.stderr)
        return 2
    R = Verificar(Args, Cfg, (Args.Tarefa, Args.Lingua, Args.Fonte))
    print(json.dumps(R, ensure_ascii=False) if Args.Json else "%s %s %s %s" % (Simbolo(R), R["tarefa"], R["lingua"], R["motivo"]))
    return 0 if R["verde"] else 1


if __name__ == "__main__":
    try:
        sys.exit(Principal())
    except Exception as Erro:  # sem traceback: uma linha e codigo 3
        print("erro interno do verificador: %s: %s" % (type(Erro).__name__, Erro))
        sys.exit(3)
