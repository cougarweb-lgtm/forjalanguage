# -*- coding: utf-8 -*-
"""Verifica o runtime do perfil (rt_perfil.c, M8 P1): 10 casos de teste_rt_perfil.c, cada um um exe nativo (rt_win.o + rt_perfil.o) com tabela FPF1 sintetica.

Regras:
  - Compila com as flags do runtime (FlagsNativo, as mesmas do verificar_rt_nativo.py): rt_win.c (release), rt_curto.c, rt_thread_win.c e rt_perfil.c; liga com -lkernel32.
  - Cada exe roda com FORJA_PERFIL_SAIDA=<pasta>\\casoN: le <base>.perfil.json (json.load, nunca stdin) e <base>.perfil.txt e confere CONTAGENS e relacoes entre os numeros
    (proprio = total - filhos, soma dos proprios = duracao da raiz, aninhamento dos intervalos); nunca um tempo absoluto fixo.
  - Casos: 1 tres nomes (a x3, b x2, c x1) | 2 aninhamento a>b>c | 3 recursao de 5 niveis | 4 dois fios (a aberta no principal quando a thread liga o rt_multithread) |
    5 teto de 1.048.576 eventos estourado (truncado true, agregado intacto) | 6 saida no meio de a>b por forja_sair(7) | 7 forja_perfil_finalizar (caminho do os.exit) |
    8 nenhuma proc medida (nenhum arquivo) | 9 id fora da tabela e saida sem entrada | 10 recursao de 5000 niveis (a pilha cresce).
  - nm -u do rt_perfil.o: so os 5 imports de kernel32 que ja estao na tabela fixa do ligador, forja_perfil_tabela, forja_rt_ao_sair, rt_multithread e sys_*; objdump -p: so KERNEL32.dll.
  - --linux compila tambem a variante Linux (as flags do FLAGS_RTL) e confere o nm -u (sem __imp_, so sys_*, forja_*, rt_multithread).

Versao: 0.1.7
Changelog: 0.1.9 - corte 0.29.0: otherData.forja esperado passa a 0.29.0. 0.1.8 - corte 0.28.0: otherData.forja esperado passa a 0.28.0. 0.1.7 - corte 0.27.0: otherData.forja esperado passa a 0.27.0. 0.1.6 - corte 0.26.0: otherData.forja esperado passa a 0.26.0. 0.1.5 - corte 0.25.1: otherData.forja esperado passa a 0.25.1. 0.1.4 - corte 0.25.0: otherData.forja esperado passa a 0.25.0. 0.1.3 - PREPARO (0.24.1): otherData.forja esperado passa a 0.24.1. 0.1.2 - corte 0.24.0 do G3: otherData.forja esperado passa a 0.24.0. 0.1.1 - portao LG do G3: o campo otherData.forja esperado passa a 0.23.0. 0.1.0 - M8 P1: primeira versao.
Verificacao: python -I verificar_rt_perfil.py
"""

import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaLanguage = PastaAqui.parents[2]
PastaRuntime = PastaLanguage / "runtime" / "nativo"
PastaTerceiros = PastaLanguage / "third"
ArquivoTeste = PastaAqui / "teste_rt_perfil.c"
Casos = range(1, 11)
ExitEsperado = {6: 7}
TimeoutSegundos = 120
FlagsNativo = [
    "-std=c11", "-O2", "-ffreestanding", "-nostdlib", "-fno-builtin", "-fno-tree-loop-distribute-patterns",
    "-mno-stack-arg-probe", "-fno-stack-protector", "-fno-ident", "-ffunction-sections", "-fdata-sections", "-Wframe-larger-than=4000",
    "-Wall", "-Wextra", "-Werror",
]
ImportsPermitidos = {"__imp_CloseHandle", "__imp_CreateFileW", "__imp_GetEnvironmentVariableW", "__imp_GetModuleFileNameW", "__imp_WriteFile"}
SimbolosPermitidos = {"forja_perfil_tabela", "forja_rt_ao_sair", "rt_multithread"}
Nomes = {0: "t.a", 1: "t.b", 2: "t.c", 3: "t.r", 4: "t.x"}
ReEventoDecimal = re.compile(r'"ts":\d+\.\d{3},"dur":\d+\.\d{3},"pid":1,"tid":\d+\}')
ReLinhaResumo = re.compile(r"^\s*(\d+)\s+(\d+\.\d{3})\s+(\d+\.\d{3})\s+(\d+\.\d)\s+(\S+)$")


class ErroVerificacao(Exception):
    pass


def Executar(Argumentos, Descricao):
    Resultado = subprocess.run(Argumentos, capture_output=True, timeout=TimeoutSegundos)
    if Resultado.returncode != 0:
        raise ErroVerificacao(f"{Descricao} falhou ({Resultado.returncode}):\n{Resultado.stderr.decode('utf-8', 'replace')}")
    return Resultado


def CompilarObjeto(Fonte, Objeto, Extra=()):
    Argumentos = ["gcc"] + FlagsNativo + list(Extra) + ["-DRT_ALVO_WIN", f"-I{PastaTerceiros}", "-c", str(Fonte), "-o", str(Objeto)]
    Executar(Argumentos, f"compilar {Fonte.name}")
    return Objeto


def ConferirNmPerfil(Objeto, Prefixo="__imp_"):
    Saida = Executar(["nm", "-u", str(Objeto)], "nm -u do rt_perfil.o").stdout.decode()
    Indefinidos = {Linha.split()[-1] for Linha in Saida.splitlines() if Linha.strip()}
    Ruins = {S for S in Indefinidos if S not in ImportsPermitidos and S not in SimbolosPermitidos and not S.startswith("sys_")}
    if Ruins:
        raise ErroVerificacao(f"rt_perfil.o: simbolos indefinidos nao permitidos: {sorted(Ruins)}")
    print(f"nm -u rt_perfil.o: {len(Indefinidos)} indefinidos, todos permitidos")


def ConferirDlls(Exe):
    Saida = Executar(["objdump", "-p", str(Exe)], "objdump -p").stdout.decode("utf-8", "replace")
    Dlls = {Linha.split(":", 1)[1].strip().lower() for Linha in Saida.splitlines() if Linha.strip().startswith("DLL Name:")}
    if Dlls != {"kernel32.dll"}:
        raise ErroVerificacao(f"{Exe.name}: DLLs inesperadas {sorted(Dlls)}")


def LigarCaso(Caso, Pasta, Comuns):
    Objeto = Pasta / f"teste_{Caso}.o"
    Exe = Pasta / f"perfil_{Caso}.exe"
    Executar(["gcc"] + FlagsNativo + [f"-DCASO={Caso}", "-c", str(ArquivoTeste), "-o", str(Objeto)], f"compilar o caso {Caso}")
    Ligar = ["gcc", "-nostdlib", "-ffreestanding", "-mno-stack-arg-probe", "-Wl,-e,forja_entrada", str(Objeto)] + [str(O) for O in Comuns]
    Ligar += ["-Wl,--gc-sections", "-lkernel32", "-o", str(Exe)]
    Executar(Ligar, f"ligar o caso {Caso}")
    ConferirDlls(Exe)
    return Exe


def RodarCaso(Caso, Exe, Pasta):
    Base = Pasta / f"saida{Caso}"
    Ambiente = dict(os.environ)
    Ambiente["FORJA_PERFIL_SAIDA"] = str(Base)
    Resultado = subprocess.run([str(Exe)], capture_output=True, stdin=subprocess.DEVNULL, timeout=TimeoutSegundos, env=Ambiente)
    Esperado = ExitEsperado.get(Caso, 0)
    if Resultado.returncode != Esperado:
        raise ErroVerificacao(f"caso {Caso}: exit {Resultado.returncode}, esperado {Esperado}; stderr: {Resultado.stderr.decode('utf-8', 'replace')}")
    if Resultado.stderr:
        raise ErroVerificacao(f"caso {Caso}: stderr nao vazio: {Resultado.stderr.decode('utf-8', 'replace')}")
    return Base


def LerResumo(Base):
    """{nome: (chamadas, total_ms, proprio_ms, %proprio)} e o texto do cabecalho do .perfil.txt."""
    Texto = Path(f"{Base}.perfil.txt").read_text(encoding="utf-8")
    Linhas = {}
    Ordem = []
    for Linha in Texto.splitlines():
        Achou = ReLinhaResumo.match(Linha)
        if Achou:
            Linhas[Achou.group(5)] = (int(Achou.group(1)), float(Achou.group(2)), float(Achou.group(3)), float(Achou.group(4)))
            Ordem.append(Achou.group(5))
    return Linhas, Ordem, Texto


def LerTrace(Base):
    """O .perfil.json por json.load (arquivo) + conferencia textual do formato de cada evento (ts/dur com 3 casas, pid 1)."""
    Caminho = Path(f"{Base}.perfil.json")
    with open(Caminho, "r", encoding="utf-8") as Arquivo:
        Dados = json.load(Arquivo)
    Bruto = Caminho.read_text(encoding="utf-8")
    Outros = Dados["otherData"]
    if Dados["displayTimeUnit"] != "ns" or Outros["forja"] != "0.46.0":
        raise ErroVerificacao(f"{Caminho.name}: displayTimeUnit/forja inesperados: {Dados['displayTimeUnit']} {Outros['forja']}")
    if Outros["eventos"] != len(Dados["traceEvents"]):
        raise ErroVerificacao(f"{Caminho.name}: otherData.eventos {Outros['eventos']} != {len(Dados['traceEvents'])}")
    if len(ReEventoDecimal.findall(Bruto)) != len(Dados["traceEvents"]):
        raise ErroVerificacao(f"{Caminho.name}: algum evento sem ts/dur com 3 casas e pid 1")
    for Evento in Dados["traceEvents"]:
        if Evento["ph"] != "X" or Evento["pid"] != 1 or Evento["ts"] < 0 or Evento["dur"] < 0 or Evento["name"] not in Nomes.values():
            raise ErroVerificacao(f"{Caminho.name}: evento invalido {Evento}")
    return Dados


def Exigir(Condicao, Mensagem):
    if not Condicao:
        raise ErroVerificacao(Mensagem)


def PorNome(Dados, Nome):
    return [E for E in Dados["traceEvents"] if E["name"] == Nome]


def ConferirCaso1(Base):
    Resumo, _, _ = LerResumo(Base)
    Dados = LerTrace(Base)
    Exigir({N: V[0] for N, V in Resumo.items()} == {"t.a": 3, "t.b": 2, "t.c": 1}, f"caso 1: contagens do resumo {Resumo}")
    Exigir([len(PorNome(Dados, N)) for N in ("t.a", "t.b", "t.c")] == [3, 2, 1] and Dados["otherData"]["truncado"] is False, "caso 1: eventos do json")
    Exigir({E["tid"] for E in Dados["traceEvents"]} == {1}, "caso 1: um so fio (tid 1)")


def ConferirCaso2(Base):
    Resumo, _, _ = LerResumo(Base)
    Dados = LerTrace(Base)
    A, B, C = (PorNome(Dados, N)[0] for N in ("t.a", "t.b", "t.c"))
    Exigir(A["dur"] >= 5500 and B["dur"] >= 3500 and C["dur"] >= 1500, f"caso 2: duracoes {A['dur']} {B['dur']} {C['dur']} us (esperado >= 6000/4000/2000)")
    Exigir(A["ts"] <= B["ts"] <= C["ts"] and C["ts"] + C["dur"] <= B["ts"] + B["dur"] + 0.002 <= A["ts"] + A["dur"] + 0.004, "caso 2: intervalos nao aninhados")
    Exigir(abs(Resumo["t.a"][1] * 1000 - A["dur"]) < 1.5 and abs(Resumo["t.b"][1] * 1000 - B["dur"]) < 1.5, "caso 2: total_ms do resumo != dur do json")
    PropriosMs = [Resumo[N][2] for N in ("t.a", "t.b", "t.c")]
    Exigir(abs(Resumo["t.a"][2] * 1000 - (A["dur"] - B["dur"])) < 2.0 and abs(Resumo["t.b"][2] * 1000 - (B["dur"] - C["dur"])) < 2.0, "caso 2: proprio != total - filhos")
    Exigir(abs(sum(PropriosMs) * 1000 - A["dur"]) < 3.0, f"caso 2: soma dos proprios {sum(PropriosMs)} ms != dur de a {A['dur']} us")
    Exigir(all(V[2] >= 1.5 for V in Resumo.values()), f"caso 2: proprio de alguma proc < 1.5 ms: {Resumo}")


def ConferirCaso3(Base):
    Resumo, _, _ = LerResumo(Base)
    Dados = LerTrace(Base)
    Eventos = PorNome(Dados, "t.r")
    Maior = max(E["dur"] for E in Eventos)
    Exigir(len(Eventos) == 5 and Resumo["t.r"][0] == 5, f"caso 3: chamadas {len(Eventos)} / {Resumo['t.r'][0]}")
    Exigir(Maior >= 4500, f"caso 3: raiz com {Maior} us (esperado >= 5000)")
    Exigir(abs(Resumo["t.r"][1] * 1000 - Maior) < 1.5, f"caso 3: total_ms {Resumo['t.r'][1]} deveria ser so a chamada mais externa ({Maior} us), sem somar as aninhadas")
    Exigir(abs(Resumo["t.r"][2] * 1000 - Maior) < 3.0, "caso 3: soma dos proprios != raiz")


def ConferirCaso4(Base):
    Resumo, _, _ = LerResumo(Base)
    Dados = LerTrace(Base)
    Tid = {N: {E["tid"] for E in PorNome(Dados, N)} for N in ("t.a", "t.b", "t.c")}
    Exigir({N: V[0] for N, V in Resumo.items()} == {"t.a": 1, "t.b": 1, "t.c": 1}, f"caso 4: contagens {Resumo}")
    Exigir(Tid["t.a"] == Tid["t.c"] and len(Tid["t.a"]) == 1 and Tid["t.b"].isdisjoint(Tid["t.a"]), f"caso 4: tids {Tid} (a e c no mesmo fio, b em outro)")
    Exigir(len({E["tid"] for E in Dados["traceEvents"]}) == 2, "caso 4: deveria haver 2 fios")


def ConferirCaso5(Base):
    Resumo, _, Texto = LerResumo(Base)
    Dados = LerTrace(Base)
    Exigir(Dados["otherData"]["truncado"] is True and Dados["otherData"]["eventos"] == 1048576, f"caso 5: otherData {Dados['otherData']}")
    Exigir(Resumo["t.a"][0] == 1100000, f"caso 5: agregado {Resumo['t.a']} (esperado 1100000 chamadas)")
    Exigir("truncado" in "\n".join(Texto.splitlines()[:8]), "caso 5: o cabecalho do resumo deveria avisar o truncamento")


def ConferirCaso6(Base):
    Resumo, _, _ = LerResumo(Base)
    Dados = LerTrace(Base)
    A, B = PorNome(Dados, "t.a")[0], PorNome(Dados, "t.b")[0]
    Exigir(Resumo["t.a"][0] == 1 and Resumo["t.b"][0] == 1, f"caso 6: contagens {Resumo}")
    Exigir(A["dur"] >= 1500 and B["dur"] >= 500 and B["ts"] + B["dur"] <= A["ts"] + A["dur"] + 0.002, f"caso 6: quadros abertos mal fechados {A} {B}")


def ConferirCaso7(Base):
    Resumo, _, _ = LerResumo(Base)
    Dados = LerTrace(Base)
    Exigir({N: V[0] for N, V in Resumo.items()} == {"t.x": 1, "t.a": 1} and len(Dados["traceEvents"]) == 2, f"caso 7: {Resumo}")


def ConferirCaso8(Base):
    Exigir(not Path(f"{Base}.perfil.json").exists() and not Path(f"{Base}.perfil.txt").exists(), "caso 8: sem proc medida nao deveria haver arquivo")


def ConferirCaso9(Base):
    Resumo, _, _ = LerResumo(Base)
    Dados = LerTrace(Base)
    Exigir({N: V[0] for N, V in Resumo.items()} == {"t.a": 1} and len(Dados["traceEvents"]) == 1, f"caso 9: {Resumo}")


def ConferirCaso10(Base):
    Resumo, _, _ = LerResumo(Base)
    Dados = LerTrace(Base)
    Maior = max(E["dur"] for E in PorNome(Dados, "t.r"))
    Exigir(Resumo["t.r"][0] == 5000 and len(PorNome(Dados, "t.r")) == 5000, f"caso 10: chamadas {Resumo['t.r']}")
    Exigir(abs(Resumo["t.r"][1] * 1000 - Maior) < 1.5, "caso 10: total_ms != raiz")


Conferidores = {1: ConferirCaso1, 2: ConferirCaso2, 3: ConferirCaso3, 4: ConferirCaso4, 5: ConferirCaso5, 6: ConferirCaso6, 7: ConferirCaso7,
                8: ConferirCaso8, 9: ConferirCaso9, 10: ConferirCaso10}


def ConferirLinux(Pasta):
    """Compila rt_perfil.c com as flags do FLAGS_RTL (alvo linux) e confere o nm -u: sem __imp_, so sys_*, forja_* e rt_multithread."""
    Objeto = Pasta / "rt_linux_perfil.o"
    Extra = ["-fno-asynchronous-unwind-tables", "-fno-unwind-tables"]
    Argumentos = ["gcc"] + FlagsNativo + Extra + ["-DRT_ALVO_LINUX", f"-I{PastaTerceiros}", "-c", str(PastaRuntime / "rt_perfil.c"), "-o", str(Objeto)]
    Executar(Argumentos, "compilar rt_perfil.c (linux)")
    Saida = Executar(["nm", "-u", str(Objeto)], "nm -u do rt_linux_perfil.o").stdout.decode()
    Indefinidos = {Linha.split()[-1] for Linha in Saida.splitlines() if Linha.strip()}
    Ruins = {S for S in Indefinidos if S.startswith("__imp_") or not (S.startswith(("sys_", "forja_")) or S == "rt_multithread")}
    NomesDeMemoria = ("memcpy", "memset", "memmove", "memcmp", "strlen")
    Ruins |= {S for S in Indefinidos if S in NomesDeMemoria}
    if Ruins:
        raise ErroVerificacao(f"rt_linux_perfil.o: indefinidos nao permitidos: {sorted(Ruins)}")
    print(f"rt_linux_perfil.o: {len(Indefinidos)} indefinidos, todos permitidos")


def Verificar(Pasta, Linux):
    Comuns = [
        CompilarObjeto(PastaRuntime / "rt_win.c", Pasta / "rt_win.o"),
        CompilarObjeto(PastaRuntime / "rt_curto.c", Pasta / "rt_curto.o"),
        CompilarObjeto(PastaRuntime / "rt_thread_win.c", Pasta / "rt_thread_win.o"),
        CompilarObjeto(PastaRuntime / "rt_perfil.c", Pasta / "rt_perfil.o"),
    ]
    ConferirNmPerfil(Comuns[3])
    if Linux:
        ConferirLinux(Pasta)
    for Caso in Casos:
        Exe = LigarCaso(Caso, Pasta, Comuns)
        Base = RodarCaso(Caso, Exe, Pasta)
        Conferidores[Caso](Base)
        print(f"caso {Caso}: ok")
    print(f"OK: {len(Casos)}/{len(Casos)} casos")
    return 0


def Main():
    Linux = "--linux" in sys.argv[1:]
    with tempfile.TemporaryDirectory(prefix="perfil_rt_") as Temp:
        try:
            return Verificar(Path(Temp), Linux)
        except ErroVerificacao as Erro:
            print(f"FALHA: {Erro}")
            return 1


if __name__ == "__main__":
    sys.exit(Main())
