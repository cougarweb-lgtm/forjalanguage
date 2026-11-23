# -*- coding: utf-8 -*-
"""Verifica o runtime nativo da Forja (P2a, P10a, P12b, P13a): 14 casos x 3 formas de stdout, oraculo x nativo.

Regras:
  - Cada caso e compilado duas vezes: com -DORACULO (forja_rt.h + CRT do gcc) e ligado ao rt_win.o
    do runtime nativo (gcc -nostdlib, entrada forja_entrada, -lkernel32). A unidade e o rt_win.c real
    (inclui rt_terceiros.c e rt_real.c), a mesma que o construir.bat compila em rt_win_dev.o e rt_win_rel.o.
  - Duas variantes do rt: release (padrao) e dev (-DFORJA_DEV), so para o caso 11 (R0002 de real).
    O oraculo do caso 11 tambem recebe -DFORJA_DEV.
  - Formas de stdout: pipe, arquivo e NUL. Em pipe e arquivo compara stdout, stderr e exit byte a byte.
    Em NUL o stdout vai para o dispositivo nulo e nao e capturavel: ali compara stderr e exit.
  - Exit esperado do oraculo: casos 1..3 -> 0; 4..6 -> 101; 7 -> 1; 8 -> 0; 9 (forja_panic) -> 101;
    10 (print_f32) -> 0; 11 (nan as i64 em dev) -> 101; 12 (saturacao em release) -> 0; 13 (salvar/saltar do rt_salto.c, 1e6 voltas) -> 0;
    14 (forja_str_igual e forja_str_de_cstr em release) -> 0;
    27 (executor de testes com FORJA_TESTE_FILTRO) -> 1; 28 (executor com FORJA_TESTE_JSON) -> 1;
    29 (vazamento por teste, dev: o teste que vaza falha com R0009) -> 1; 30 (queda por teste, dev: rawptr nulo vira R0011 e o teste seguinte passa) -> 1;
    15 (heap + R0008 de delete duplo) -> 101; 16 (R0009 de vazamento) -> 101; 17 (frame + lista do frame antigo, R0008) -> 101; 18 (lista) -> 0;
    19 (Arena) -> 0; 20 (Pool de outro dono, R0008) -> 101; 21 (R0007 de cap negativo) -> 101; 22 (release, sem R0009 e sem conferencia) -> 0.
    Os casos 15 a 22 (alocacao, teste_rt_aloc.c) rodam contra o rt_win.c normal (que inclui rt_rastreio.c em dev e rt_aloc.c); 15 a 21 em dev, 22 em release.
  - nm -u de cada rt_win.o: so os 17 __imp_ de kernel32 e forja_programa. objdump -p de cada exe nativo: so KERNEL32.dll.
  - Para na primeira divergencia e sai com 1.
  - --congelar (0.21.0 P3, contrato 0210): roda o diferencial inteiro sem parar na 1a divergencia; onde -DORACULO == rt_win.o (exit, stdout e stderr,
    por caso/forma) grava em rapido\\oraculo_congelado_g1_rt.json (classe "rt"); divergencia nao grava e sai com 1.
  - --contra-congelado (0.21.0 P3, contrato 0210, ordem do main): o lado ORACULO vem so do JSON congelado; o x64 (rt_win.o, nativo) e compilado
    e ligado com o gcc do WinLibs, como na verificacao normal. Nao roda -DORACULO, nao usa forja_g1 nem runtime\\forja_rt.h e nao regera; chave
    ausente = falha do item. Sem selo, o cabecalho nao e conferido (ExigirCabecalho nao e chamado).
  - --selftest: prova a comparacao com o congelado (igual, stdout, exit, stderr, NUL, chave ausente) sem gcc e sem o JSON.

Versao: 1.16.0
Changelog: 1.16.0 - 0.21.0 P3 (ordem do main): --contra-congelado (oraculo so do JSON, x64 com gcc do WinLibs para ligar) e --selftest. 1.15.0 - 0.21.0 P3 (contrato 0210) (contrato 0210): --congelar grava o -DORACULO por caso/forma no banco da classe rt (rapido\\oraculo_congelado_g1_rt.json). 1.14.3 - O0 P5: o oraculo em release leva -DFORJA_SEM_CHECAGEM (o padrao do release passou a --checagem nao; o forja_rt.h confere indice e nulo sem esse macro). 1.14.2 - M5 P44: 22 imports permitidos no rt_win.o (AcquireSRWLockExclusive e ReleaseSRWLockExclusive: a trava do estado compartilhado); mesmos casos. 1.14.1 - M5 P43: 20 imports permitidos no rt_win.o (TlsAlloc, TlsGetValue e TlsSetValue: o slot do RtFio); mesmos casos. 1.14.0 - Linguagem v1.11 L16b: casos 29 (vazamento por teste, R0009) e 30 (queda por teste, R0011) em teste_rt_nativo.c, em dev; 17 imports permitidos (AddVectoredExceptionHandler); 35 casos x 3 formas = 105.
  1.13.0 -Linguagem v1.11 L16a: casos 27 (FORJA_TESTE_FILTRO) e 28 (FORJA_TESTE_JSON) do executor de testes em teste_rt_nativo.c; o ambiente do caso (AmbienteDoCaso) vai ao oraculo e ao nativo;
  16 imports permitidos (GetEnvironmentVariableA); 33 casos x 3 formas = 99.
  1.12.0 - M2 P17c: FlagsNativo ganha -ffunction-sections e -fdata-sections (igual ao FLAGS_RT do construir.bat; o ligador descarta secao); 31 casos x 3 formas = 93 (igual).
  1.11.0 - M2 P17b: rt_win_mapa.c removido (o rt_win.c inclui rt_mapa.c e rt_soa.c); os casos 40 a 44 rodam contra o rt_win normal; 31 casos x 3 formas = 93 (igual).
  1.10.0 - M2 P16: caso 26 (forja_rt_conta_bits contra __builtin_popcountll) em teste_rt_nativo.c, em release; 31 casos x 3 formas = 93.
  1.9.0 - M2 P15b: casos 23 a 25 (forja_fmt_*: inteiros, reais, buffer de 10 KB) em teste_rt_formata.c contra o rt_win normal (que inclui rt_formata.c), todos em release; 30 casos x 3 formas = 90 (a numeracao pula 26..39).
  1.8.0 - M2 P14b: rt_win_aloc.c removido (o rt_win.c inclui rt_rastreio.c e rt_aloc.c); os casos 15 a 22 rodam contra o rt_win normal.
  1.7.0 - M2 P17a: casos 40 a 44 (map int -> int, map str -> struct, soa dinamica, R0008 de soa e de map do frame) em teste_rt_mapa.c contra rt_win_mapa.c (rt_win.c + rt_rastreio.c em dev + rt_aloc.c + rt_mapa.c + rt_soa.c); 41, 43 e 44 em dev; 27 casos x 3 formas = 81 (a numeracao pula 23..39: reservada aos passos P15b/P16 e seguintes).
  1.6.0 - M2 P14a: casos 15 a 22 (heap, vazamento R0009, frame, lista, Arena, Pool, R0007, release) em teste_rt_aloc.c contra rt_win_aloc.c; 22 casos x 3 formas = 66.
  1.5.0 - M2 P13a: caso 14 (forja_str_igual e forja_str_de_cstr contra o forja_rt.h); 14 casos x 3 formas = 42.
  1.4.0 - M2 P12b: caso 13 (forja_rt_salvar/forja_rt_saltar contra o setjmp/longjmp da CRT); 13 casos x 3 formas = 39.
  1.3.0 - P10b: usa o rt_win.c real (rt_win_m2.c removido); mesmos 12 casos x 3 formas.
  1.2.0 - P10a: casos 10 a 12 (print_f32, R0002 de real em dev, saturacao de real em release); unidade
  rt_win_m2.c com o formatador de real; variante dev do rt para o caso 11; -I third para o envelope do Ryu.
  1.1.1 - console UTF-8: 15 imports (GetConsoleMode, GetConsoleOutputCP, SetConsoleOutputCP). 1.1.0 - P7a do M1: caso 9 (forja_panic). 1.0.0 - P2a do M0: primeira versao (8 casos, 3 formas, nm e objdump).
Verificacao: python -I verificar_rt_nativo.py
Congelar: python -I verificar_rt_nativo.py --congelar
Contra: python -I verificar_rt_nativo.py --contra-congelado
Selftest: python -I verificar_rt_nativo.py --selftest
"""

import argparse
import base64
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from collections import namedtuple
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "rapido"))  # oraculo_congelado (0.21.0 P3)
import oraculo_congelado as OraculoCongelado  # noqa: E402

PastaAqui = Path(__file__).resolve().parent
PastaTerceiros = PastaAqui.parents[2] / "third"
ArquivoTeste = PastaAqui / "teste_rt_nativo.c"
ArquivoRtWin = PastaAqui.parents[2] / "runtime" / "nativo" / "rt_win.c"
ArquivoTesteAloc = PastaAqui / "teste_rt_aloc.c"
ArquivoTesteMapa = PastaAqui / "teste_rt_mapa.c"
ArquivoTesteFormata = PastaAqui / "teste_rt_formata.c"
Casos = list(range(1, 10)) + list(range(11, 31)) + list(range(40, 45))  # P4 d2fixed: o caso 10 (forja_print_f32) saiu com a funcao
ArquivoRtCurto = PastaAqui.parents[2] / "runtime" / "nativo" / "rt_curto.c"
# P4 d2fixed: o rt_win.o chama o Display curto (rt_win_curto.o no forja.exe): aqui o rt_curto.c e compilado e ligado junto.
SimbolosDoCurto = {"forja_curto_texto_f64"}
CasosAloc = set(range(15, 23))  # fonte teste_rt_aloc.c; o rt e o rt_win.c normal
CasosFormata = set(range(23, 26))  # fonte teste_rt_formata.c; o rt e o rt_win.c normal
CasosMapa = set(range(40, 45))
Formas = ("pipe", "arquivo", "nul")
ExitEsperado = {1: 0, 2: 0, 3: 0, 4: 101, 5: 101, 6: 101, 7: 1, 8: 0, 9: 101, 11: 101, 12: 0, 13: 0, 14: 0,
                15: 101, 16: 101, 17: 101, 18: 0, 19: 0, 20: 101, 21: 101, 22: 0}
ExitEsperado.update({23: 0, 24: 0, 25: 0, 26: 0, 40: 0, 41: 0, 42: 0, 43: 101, 44: 101})
ExitEsperado.update({27: 1, 28: 1, 29: 1, 30: 1})
# Variaveis de ambiente do caso (L16a: forja test --filtro/--json chega ao exe por elas); o oraculo e o nativo recebem as mesmas.
AmbienteDoCaso = {27: {"FORJA_TESTE_FILTRO": "soma"}, 28: {"FORJA_TESTE_JSON": "1"}, 29: {"FORJA_TESTE_JSON": "1"}, 30: {"FORJA_TESTE_JSON": "1"}}
CasosDev = {11} | set(range(15, 22)) | {29, 30, 41, 43, 44}
VariantesRt = ("release", "dev")
ImportsPermitidos = {
    "__imp_ExitProcess", "__imp_GetFileType", "__imp_GetProcessHeap", "__imp_GetStdHandle",
    "__imp_HeapAlloc", "__imp_HeapFree", "__imp_HeapReAlloc", "__imp_QueryPerformanceCounter",
    "__imp_QueryPerformanceFrequency", "__imp_VirtualAlloc", "__imp_VirtualFree", "__imp_WriteFile",
    "__imp_GetConsoleMode", "__imp_GetConsoleOutputCP", "__imp_SetConsoleOutputCP", "__imp_GetEnvironmentVariableA",
    "__imp_AddVectoredExceptionHandler", "__imp_TlsAlloc", "__imp_TlsGetValue", "__imp_TlsSetValue",
    "__imp_AcquireSRWLockExclusive", "__imp_ReleaseSRWLockExclusive",
}
SimboloPrograma = "forja_programa"
DllPermitida = "kernel32.dll"
TimeoutSegundos = 60
CaminhoJsonRt = PastaAqui.parent / "rapido" / "oraculo_congelado_g1_rt.json"  # banco da classe rt (0.21.0 P3)
Classe = "rt"

FlagsNativo = [
    "-std=c11", "-O2", "-ffreestanding", "-nostdlib", "-fno-builtin", "-fno-tree-loop-distribute-patterns",
    "-mno-stack-arg-probe", "-fno-stack-protector", "-fno-ident", "-ffunction-sections", "-fdata-sections", "-Wframe-larger-than=4000",
    "-Wall", "-Wextra", "-Werror",
]

Saida = namedtuple("Saida", "stdout stderr codigo")


class ErroVerificacao(Exception):
    pass


def Executar(Argumentos, Descricao):
    Resultado = subprocess.run(Argumentos, capture_output=True, timeout=TimeoutSegundos)
    if Resultado.returncode != 0:
        Texto = Resultado.stderr.decode("utf-8", "replace")
        raise ErroVerificacao(f"{Descricao} falhou ({Resultado.returncode}):\n{Texto}")
    return Resultado


def VarianteDoCaso(Caso):
    return "dev" if Caso in CasosDev else "release"


def CompilarRtWin(Pasta, Variante, Fonte=ArquivoRtWin):
    Objeto = Pasta / f"{Fonte.stem}_{Variante}.o"
    Extra = ["-DFORJA_DEV"] if Variante == "dev" else []
    Argumentos = ["gcc"] + FlagsNativo + Extra + ["-DRT_ALVO_WIN", f"-I{PastaTerceiros}",
                                                   "-c", str(Fonte), "-o", str(Objeto)]
    Executar(Argumentos, f"compilar {Fonte.name} ({Variante})")
    return Objeto


def CompilarCurto(Pasta):
    Objeto = Pasta / "rt_curto.o"
    Argumentos = ["gcc"] + FlagsNativo + ["-DRT_ALVO_WIN", f"-I{PastaTerceiros}", "-c", str(ArquivoRtCurto), "-o", str(Objeto)]
    Executar(Argumentos, "compilar rt_curto.c")
    return Objeto


def FonteDoCaso(Caso):
    if Caso in CasosMapa:
        return ArquivoTesteMapa
    if Caso in CasosFormata:
        return ArquivoTesteFormata
    return ArquivoTesteAloc if Caso in CasosAloc else ArquivoTeste


def CompilarOraculo(Caso, Pasta):
    Exe = Pasta / f"oraculo_{Caso}.exe"
    Extra = ["-DFORJA_DEV"] if Caso in CasosDev else ["-DFORJA_SEM_CHECAGEM"]  # O0 P5; M7a F9a: o oraculo segue o runtime nativo de release (rt_win_rel.o, sem conferir; o --checagem sim do release x64 e feito pelo compilador, nao pelo rt)
    Argumentos = [
        "gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-D__USE_MINGW_ANSI_STDIO=1",
        "-DORACULO", f"-DCASO={Caso}", *Extra, str(FonteDoCaso(Caso)), "-o", str(Exe),
    ]
    Executar(Argumentos, f"compilar oraculo do caso {Caso}")
    return Exe


def LigarNativo(Caso, Pasta, RtWinO, CurtoO):
    Objeto = Pasta / f"teste_{Caso}.o"
    Exe = Pasta / f"nativo_{Caso}.exe"
    Extra = ["-DFORJA_DEV"] if Caso in CasosDev else []
    CompilarObjeto = ["gcc"] + FlagsNativo + Extra + [f"-DCASO={Caso}", "-c", str(FonteDoCaso(Caso)), "-o", str(Objeto)]
    Executar(CompilarObjeto, f"compilar teste nativo do caso {Caso}")
    Ligar = [
        "gcc", "-nostdlib", "-ffreestanding", "-mno-stack-arg-probe", "-Wl,-e,forja_entrada",
        str(Objeto), str(RtWinO), str(CurtoO), "-Wl,--gc-sections", "-lkernel32", "-o", str(Exe),
    ]
    Executar(Ligar, f"ligar nativo do caso {Caso}")
    return Exe


def AmbienteParaOCaso(Caso):
    """O ambiente do processo mais as variaveis do caso; sem elas, as FORJA_TESTE_* herdadas saem (o runner nao pode filtrar por acidente)."""
    Ambiente = {Nome: Valor for Nome, Valor in os.environ.items() if not Nome.startswith("FORJA_TESTE_")}
    Ambiente.update(AmbienteDoCaso.get(Caso, {}))
    return Ambiente


def RodarBinario(Exe, Forma, Pasta, Rotulo, Caso):
    Argumentos = [str(Exe)]
    Ambiente = AmbienteParaOCaso(Caso)
    if Forma == "pipe":
        Resultado = subprocess.run(Argumentos, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   stdin=subprocess.DEVNULL, timeout=TimeoutSegundos, env=Ambiente)
        return Saida(Resultado.stdout, Resultado.stderr, Resultado.returncode)
    if Forma == "arquivo":
        Caminho = Pasta / f"stdout_{Rotulo}.bin"
        with open(Caminho, "wb") as Destino:
            Resultado = subprocess.run(Argumentos, stdout=Destino, stderr=subprocess.PIPE,
                                       stdin=subprocess.DEVNULL, timeout=TimeoutSegundos, env=Ambiente)
        return Saida(Caminho.read_bytes(), Resultado.stderr, Resultado.returncode)
    # NUL: o stdout vai para o dispositivo nulo, os bytes nao existem para comparar.
    with open(os.devnull, "wb") as Destino:
        Resultado = subprocess.run(Argumentos, stdout=Destino, stderr=subprocess.PIPE,
                                   stdin=subprocess.DEVNULL, timeout=TimeoutSegundos, env=Ambiente)
    return Saida(None, Resultado.stderr, Resultado.returncode)


def PrimeiraDiferenca(Esquerda, Direita):
    if Esquerda == Direita:
        return None
    Menor = min(len(Esquerda), len(Direita))
    for Pos in range(Menor):
        if Esquerda[Pos] != Direita[Pos]:
            return Pos
    return Menor


def AchaDivergencia(Oraculo, Nativo, Forma):
    if Forma != "nul":
        Pos = PrimeiraDiferenca(Oraculo.stdout, Nativo.stdout)
        if Pos is not None:
            return ("stdout", Pos, Oraculo.stdout, Nativo.stdout)
    Pos = PrimeiraDiferenca(Oraculo.stderr, Nativo.stderr)
    if Pos is not None:
        return ("stderr", Pos, Oraculo.stderr, Nativo.stderr)
    if Oraculo.codigo != Nativo.codigo:
        return ("exit", 0, bytes([Oraculo.codigo & 0xFF]), bytes([Nativo.codigo & 0xFF]))
    return None


def RelatarDivergencia(Caso, Forma, Divergencia):
    Fluxo, Pos, Esq, Dir = Divergencia
    Janela = slice(max(0, Pos - 16), Pos + 16)
    print(f"DIVERGENCIA caso={Caso} forma={Forma} fluxo={Fluxo} byte={Pos}")
    print(f"  oraculo: {Esq[Janela]!r}")
    print(f"  nativo : {Dir[Janela]!r}")


def ConferirNm(RtWinO):
    Resultado = Executar(["nm", "-u", str(RtWinO)], "nm -u rt_win.o")
    Simbolos = set()
    for Linha in Resultado.stdout.decode("utf-8", "replace").splitlines():
        Partes = Linha.split()
        if len(Partes) >= 2 and Partes[-2] == "U":
            Simbolos.add(Partes[-1])
    Permitidos = ImportsPermitidos | {SimboloPrograma} | SimbolosDoCurto
    Fora = Simbolos - Permitidos
    if Fora:
        raise ErroVerificacao(f"nm -u rt_win.o tem simbolo fora dos 22 + forja_programa: {sorted(Fora)}")
    Presentes = len(Simbolos & ImportsPermitidos)
    print(f"nm -u rt_win.o: {len(Simbolos)} simbolos, todos nos 22 __imp_ + forja_programa "
          f"(__imp_ presentes: {Presentes}/22)")


def ConferirTextoObjeto(RtWinO):
    Resultado = Executar(["objdump", "-h", str(RtWinO)], "objdump -h rt_win.o")
    Padrao = re.compile(r"^\s*\d+\s+\.text\s+([0-9a-fA-F]+)")
    for Linha in Resultado.stdout.decode("utf-8", "replace").splitlines():
        Achado = Padrao.match(Linha)
        if Achado:
            Tamanho = int(Achado.group(1), 16)
            print(f"rt_win.o: .text = {Tamanho} bytes (0x{Tamanho:x})")
            return
    raise ErroVerificacao("secao .text nao encontrada em rt_win.o")


def ConferirDlls(Exe):
    Resultado = Executar(["objdump", "-p", str(Exe)], f"objdump -p {Exe.name}")
    Dlls = set()
    for Linha in Resultado.stdout.decode("utf-8", "replace").splitlines():
        if "DLL Name:" in Linha:
            Dlls.add(Linha.split("DLL Name:", 1)[1].strip().lower())
    if Dlls != {DllPermitida}:
        raise ErroVerificacao(f"{Exe.name} importa {sorted(Dlls)}; esperado so {DllPermitida}")


def ExecutarCaso(Caso, OraculoExe, NativoExe, Pasta):
    Iguais = 0
    Partes = []
    for Forma in Formas:
        Oraculo = RodarBinario(OraculoExe, Forma, Pasta, f"or{Caso}_{Forma}", Caso)
        if Oraculo.codigo != ExitEsperado[Caso]:
            raise ErroVerificacao(f"oraculo do caso {Caso} saiu com {Oraculo.codigo}, "
                                  f"esperado {ExitEsperado[Caso]}")
        Nativo = RodarBinario(NativoExe, Forma, Pasta, f"na{Caso}_{Forma}", Caso)
        Divergencia = AchaDivergencia(Oraculo, Nativo, Forma)
        if Divergencia is not None:
            RelatarDivergencia(Caso, Forma, Divergencia)
            return None
        Iguais += 1
        Partes.append(f"{Forma} ok")
    print(f"caso {Caso}: {', '.join(Partes)} (exit {ExitEsperado[Caso]})")
    return Iguais


def Verificar(Pasta):
    RtObjetos = {Variante: CompilarRtWin(Pasta, Variante) for Variante in VariantesRt}
    CurtoO = CompilarCurto(Pasta)
    for RtWinO in RtObjetos.values():
        ConferirNm(RtWinO)
    ConferirTextoObjeto(RtObjetos["release"])
    Exes = {}
    for Caso in Casos:
        OraculoExe = CompilarOraculo(Caso, Pasta)
        NativoExe = LigarNativo(Caso, Pasta, RtObjetos[VarianteDoCaso(Caso)], CurtoO)
        ConferirDlls(NativoExe)
        Exes[Caso] = (OraculoExe, NativoExe)
    print("objdump -p: {0} exes nativos, so KERNEL32.dll".format(len(Exes)))
    Total = len(Casos) * len(Formas)
    Iguais = 0
    for Caso in Casos:
        Resultado = ExecutarCaso(Caso, Exes[Caso][0], Exes[Caso][1], Pasta)
        if Resultado is None:
            return 1
        Iguais += Resultado
    print(f"stdout comparado em {len(Formas) - 1} de {len(Formas)} formas (NUL nao captura bytes)")
    print(f"OK: {Iguais}/{Total} iguais")
    return 0


def ChaveDoCaso(Caso, Forma):
    """Chave do banco: fonte do caso + caso, forma e variante (o que muda o -DORACULO)."""
    Args = (f"caso={Caso}", f"forma={Forma}", f"variante={VarianteDoCaso(Caso)}")
    return OraculoCongelado.ChaveDe(FonteDoCaso(Caso), "padrao", Args, None, Classe)


def RegistroDoLado(Saida):
    """Exit, stdout (None na forma NUL, que nao captura bytes) e stderr do lado -DORACULO, em base64."""
    Stdout = None if Saida.stdout is None else base64.b64encode(Saida.stdout).decode("ascii")
    return {"invalido": False, "origem": "gcc -DORACULO", "exit": Saida.codigo, "stdout": Stdout,
            "stderr_b64": base64.b64encode(Saida.stderr).decode("ascii")}


def CongelarForma(Banco, Caso, Forma, Exes, Pasta):
    """Roda o oraculo e o rt_win.o nesta forma: igual grava e devolve None; diferente nao grava e devolve a divergencia."""
    OraculoExe, NativoExe = Exes
    Ref = RodarBinario(OraculoExe, Forma, Pasta, f"or{Caso}_{Forma}", Caso)
    if Ref.codigo != ExitEsperado[Caso]:
        raise ErroVerificacao(f"oraculo do caso {Caso} saiu com {Ref.codigo}, esperado {ExitEsperado[Caso]}")
    Nat = RodarBinario(NativoExe, Forma, Pasta, f"na{Caso}_{Forma}", Caso)
    Divergencia = AchaDivergencia(Ref, Nat, Forma)
    if Divergencia is None:
        Banco.Registrar(ChaveDoCaso(Caso, Forma), RegistroDoLado(Ref))
    return Divergencia


def CongelarCaso(Banco, Caso, Exes, Pasta):
    """Congela as 3 formas de um caso; devolve (gravados, divergencias)."""
    Gravados = 0
    Divergencias = []
    for Forma in Formas:
        Divergencia = CongelarForma(Banco, Caso, Forma, Exes, Pasta)
        if Divergencia is None:
            Gravados += 1
            continue
        RelatarDivergencia(Caso, Forma, Divergencia)
        Divergencias.append((Caso, Forma, Divergencia[0]))
    return Gravados, Divergencias


def Congelar(Pasta):
    Banco = OraculoCongelado.BancoOraculo(CaminhoJsonRt, OraculoCongelado.FORJA_G1)
    if Banco.Selado:
        raise ErroVerificacao(f"banco rt selado ({CaminhoJsonRt.name}): nao se recongela")
    RtObjetos = {Variante: CompilarRtWin(Pasta, Variante) for Variante in VariantesRt}
    CurtoO = CompilarCurto(Pasta)
    for RtWinO in RtObjetos.values():
        ConferirNm(RtWinO)
    ConferirTextoObjeto(RtObjetos["release"])
    Gravados = 0
    Divergencias = []
    for Caso in Casos:
        Exes = (CompilarOraculo(Caso, Pasta), LigarNativo(Caso, Pasta, RtObjetos[VarianteDoCaso(Caso)], CurtoO))
        ConferirDlls(Exes[1])
        Grav, Divs = CongelarCaso(Banco, Caso, Exes, Pasta)
        print(f"caso {Caso}: {Grav}/{len(Formas)} gravados")
        Gravados += Grav
        Divergencias += Divs
    print(f"congelado: {Gravados} itens gravados em {CaminhoJsonRt.name}; {len(Divergencias)} divergencia(s) nao gravada(s)")
    return 1 if Divergencias else 0


def CompararComCongelado(Registro, Nativo, Forma):
    """Compara o x64 com o registro congelado pelo mesmo criterio do AchaDivergencia; Registro None = chave ausente (falha)."""
    if Registro is None:
        return ("congelado-ausente", 0, b"", b"")
    Esperado = Saida(None if Registro["stdout"] is None else base64.b64decode(Registro["stdout"]),
                     base64.b64decode(Registro["stderr_b64"]), Registro["exit"])
    return AchaDivergencia(Esperado, Nativo, Forma)


def ContraCaso(Banco, Caso, NativoExe, Pasta):
    """Roda o x64 nas 3 formas e compara com o JSON. So le o Banco: sem -DORACULO, sem forja_g1, sem forja_rt.h."""
    Iguais = 0
    Diferentes = []
    for Forma in Formas:
        Registro = Banco.Itens.get(ChaveDoCaso(Caso, Forma))
        Nativo = RodarBinario(NativoExe, Forma, Pasta, f"na{Caso}_{Forma}", Caso)
        Divergencia = CompararComCongelado(Registro, Nativo, Forma)
        if Divergencia is None:
            Iguais += 1
            continue
        RelatarDivergencia(Caso, Forma, Divergencia)
        Diferentes.append((Caso, Forma, Divergencia[0]))
    return Iguais, Diferentes


def Contra(Pasta):
    """--contra-congelado: o oraculo vem so do JSON; o gcc do WinLibs serve apenas para montar o lado x64."""
    Inicio = time.perf_counter()
    Banco = OraculoCongelado.BancoOraculo(CaminhoJsonRt, OraculoCongelado.FORJA_G1)
    if not Banco.Itens:
        raise ErroVerificacao(f"banco rt ausente ou vazio: {CaminhoJsonRt.name}")
    RtObjetos = {Variante: CompilarRtWin(Pasta, Variante) for Variante in VariantesRt}
    CurtoO = CompilarCurto(Pasta)
    for RtWinO in RtObjetos.values():
        ConferirNm(RtWinO)
    ConferirTextoObjeto(RtObjetos["release"])
    Iguais = 0
    Diferentes = []
    for Caso in Casos:
        NativoExe = LigarNativo(Caso, Pasta, RtObjetos[VarianteDoCaso(Caso)], CurtoO)
        ConferirDlls(NativoExe)
        Iu, Difs = ContraCaso(Banco, Caso, NativoExe, Pasta)
        Iguais += Iu
        Diferentes += Difs
    Segundos = time.perf_counter() - Inicio
    print(f"contra-congelado: {Iguais} iguais, {len(Diferentes)} diferentes (de {len(Casos) * len(Formas)}); {Segundos:.1f} s (inclui o build do x64)")
    return 1 if Diferentes else 0


def Selftest():
    """Sem gcc e sem o JSON: prova a comparacao com o congelado (igual, stdout, exit, stderr, NUL, chave ausente)."""
    Dados = b"ola\n"
    Reg = {"exit": 0, "stdout": base64.b64encode(Dados).decode("ascii"), "stderr_b64": ""}
    RegNul = {"exit": 0, "stdout": None, "stderr_b64": ""}
    Cenarios = (
        ("igual", Reg, Saida(Dados, b"", 0), "pipe", None),
        ("stdout", Reg, Saida(b"olb\n", b"", 0), "pipe", "stdout"),
        ("exit", Reg, Saida(Dados, b"", 1), "pipe", "exit"),
        ("stderr", Reg, Saida(Dados, b"x", 0), "pipe", "stderr"),
        ("NUL ignora stdout", RegNul, Saida(None, b"", 0), "nul", None),
        ("chave ausente", None, Saida(Dados, b"", 0), "pipe", "congelado-ausente"),
    )
    for Nome, Registro, Nativo, Forma, Esperado in Cenarios:
        Achado = CompararComCongelado(Registro, Nativo, Forma)
        Obtido = None if Achado is None else Achado[0]
        if Obtido != Esperado:
            raise ErroVerificacao(f"selftest {Nome}: esperado {Esperado}, obtido {Obtido}")
    print(f"selftest: {len(Cenarios)} cenarios OK (sem gcc, sem JSON)")
    return 0


def Main():
    Parser = argparse.ArgumentParser(description="Verifica o runtime nativo da Forja: -DORACULO x rt_win.o.")
    Modo = Parser.add_mutually_exclusive_group()
    Modo.add_argument("--congelar", action="store_true", help="grava o -DORACULO onde e igual ao rt_win.o (rapido\\oraculo_congelado_g1_rt.json)")
    Modo.add_argument("--contra-congelado", action="store_true", help="compara o rt_win.o com o JSON congelado; o -DORACULO nao roda")
    Modo.add_argument("--selftest", action="store_true", help="prova a comparacao com o congelado, sem gcc e sem o JSON")
    Args = Parser.parse_args()
    Pasta = Path(tempfile.mkdtemp(prefix="rt_nativo_"))
    try:
        if Args.selftest:
            return Selftest()
        if Args.congelar:
            return Congelar(Pasta)
        if Args.contra_congelado:
            return Contra(Pasta)
        return Verificar(Pasta)
    except ErroVerificacao as Erro:
        print(f"FALHA: {Erro}")
        return 1
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(Main())
