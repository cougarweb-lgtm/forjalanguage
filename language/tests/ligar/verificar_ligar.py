"""Verifica o 'forja ligar' (Forja v1.8b): header C -> modulo .fj, recusas G, erros de ferramenta e o catalogo de codigos.

Regras:
  - Casos 1-4 leem header pela libclang: sem clang.cindex saem PULADO (nunca passam em silencio).
  - 1 gerar forjateste.h = bytes de mods\\forjateste.esperado.fj; 2 recusas G0004 e G0008 (--estrito: exit 1);
    3 programa Forja usa o modulo gerado e a DLL de teste; 4 math.h real com --prefixo sqrt.
  - 5 sem libclang (G0102, nada gravado); 6 sem Python (G0101); 7 --codigos lista os 22 codigos G e todos tem 'explicar'.
  - 8 --cpp classe.hpp: classe.fj = mods\\classe.esperado.fj, g++ compila o wrapper, run = 15 e 12; 9 dividir(0) = R0010, exit 101.
  - 10 --dll forjateste.dll (13 exportacoes, .sig com G0025 na linha 7, run = 4 e true); 11 --listar do kernel32 tem GetTickCount64.
  - 12 lib110.h --wrapper-variadicas = mods\\lib110.esperado.fj + lib110_variadicas.esperado.c (stderr vazio); 13 gcc+ar e run
    (c_get/c_insert/c_has, consts dobradas, acessador l110_valor__campo_inteiro, l110_log_texto); 14 variadica sem const char * segue G0008.
  - 15 bitfield/union anonima/array flexivel (get/set x gemeo C); 16 cola C (mods\\inline_cola.h: static inline, global get/set, array global, macro
    com .sig; --compilar-cola; run = gemeo C no backend x64; erro do gcc = G0103 exit 2 sem .fj; sem --cola segue G0005).
  - Saida: 'N/M casos de ligar (P pulados)'; exit 0 sem FALHOU.

Versao: 1.8.1
Changelog: 1.8.1 - 0.21.0 F3: padrao x64, backend C aposentado (--backend so aceita x64).
           1.8.0 - M6d P92g (ligar 0.9.0): caso 20 (mods\\fim.h com --prefixo x_: --propor grava ends so em x_shutdown e x_global_cleanup = mods\\fim.esperado.fj; sem --propor sem ends); carimbos dos esperados 0.8.1 -> 0.9.0.
           1.7.0 - M6c P89 (ligar 0.8.0): caso 19 (mods\\propor.h: --propor = mods\\propor.esperado.fj, sem --propor sem clausula, forja check sem erro); carimbos dos esperados 0.7.1 -> 0.8.0.
           1.6.1 - M6c P84 (ligar 0.7.1): caso 18 confere que struct com c_ulong nao ganha static_assert; carimbos dos esperados 0.7.0 -> 0.7.1.
           1.6.0 - M6c P84 (ligar 0.7.0): caso 18 (mods\\clong.h: long/unsigned long saem c_long/c_ulong; ponte = mods\\clong.esperado.fj e forja check
           sem erro fora o E1005 do stub); carimbo da linha 1 dos esperados trocado de 0.6.2 para 0.7.0.
           1.5.0 - Interop F1f (ligar 0.6.1): caso 17 (mods\\array_param.h: 'const int v[4]' e 'int v[]' saem ?^i32; ponte = mods\\array_param.esperado.fj
           e forja check sem erro fora o E1005 do stub); esperados do 0.6.0 trocados para 0.6.1 (so o carimbo da linha 1).
           1.4.0 - Interop F1e (ligar 0.6.0): caso 16 (cola C: inline_cola.h + .sig, libinline_cola_cola.a compilada pelo ligar, programa Forja x gemeo C nos 2 backends,
           erro de gcc vira G0103); esperados na 0.6.0.
           1.3.0 - Interop F1c (ligar 0.5.0): caso 15 (mods\\bitfields.h: bitfield, union/struct anonima, array flexivel; get/set x gemeo C nos 2 backends); esperados na 0.5.0 (static_assert de layout).
           1.2.2 - Interop F1: testa o forja_ligar.py oficial por FORJA_LIGAR_SCRIPT (padrao, se nao definida), porque o forja.exe ainda embute o ligar 0.3.x ate o proximo construir.bat; esperados na 0.4.0.
           1.2.1 - M4 P25b: o 'forja run' leva sempre --backend c|x64 (o padrao do dev passou a ser x64; o padrao deste verificador segue c).
           1.2.0 - M3 P23: --backend c|x64 (padrao c): os 'forja run' dos casos 3, 4, 8, 9, 10 e 13 levam --backend x64; no caso 13 o wrapper de variadicas vira DLL
           (lib110_variadicas.dll, gcc -shared) em vez de .a, porque o .a estatico chega no M3b (P23d: X0103). 14/14 nos dois backends.
           1.1.0 - Linguagem v1.10 (ligar 0.3.0): casos 12-14 (alias c_, macros dobraveis, #define repetido, acessador de union
           que colide, --wrapper-variadicas); cabecalhos dos mods\\*.esperado trocados para 0.3.0.
           1.0.0 - Linguagem v1.8b P8a: casos 1-7 (C); P8b: casos 8-9 (C++: wrapper, classe.dll e R0010 exit 101; sem g++ saem
           PULADO); P8c: casos 10-11 (--dll: exportacoes, forjateste.sig, G0025, run; --listar do kernel32; sem pefile PULADO).
Verificacao: python verificar_ligar.py --forja <forja.exe> [--backend x64]
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
ForjaPadrao = PastaAqui.parent.parent / "compiler" / "forja.exe"
TimeoutSegundos = 300
LigarOficial = PastaAqui.parent.parent / "tools" / "forja_ligar" / "forja_ligar.py"
# O forja.exe embute o forja_ligar.py de quando foi construido; ate o proximo construir.bat o embutido pode ser mais velho que o arquivo oficial.
# Testar o arquivo oficial (o que o proximo .incbin embute) evita falso vermelho; FORJA_LIGAR_SCRIPT ja definido pelo chamador vence.
os.environ.setdefault("FORJA_LIGAR_SCRIPT", str(LigarOficial))
BackendEscolhido = "x64"  # so "x64" desde a 0.21.0 (backend C aposentado): preenchido por Principal (--backend); ComandoRun le
SaidaEsperadaCaso3 = "15\n4\n7\n7\n11\n9\n12\n42\n20\n10\n3\nforjateste\n"
SaidaEsperadaCaso13 = "12\n5\n1\n10\n20\n65280\n8\n-1\n1\n1005\n3\n"
CodigosG = ["G0001", "G0003", "G0004", "G0005", "G0006", "G0007", "G0008", "G0013", "G0014", "G0016", "G0017", "G0019",
            "G0021", "G0022", "G0023", "G0025", "G0026", "G0101", "G0102", "G0103", "G0104", "G0105"]


class Pulado(Exception):
    """Caso que nao pode rodar nesta maquina (falta libclang); conta como pulado, nunca como OK."""


def ComandoRun(Forja, Fonte):
    """forja run [--backend x64] <Fonte>: o backend vem de --backend (padrao x64)."""
    return [str(Forja), "run", "--backend", BackendEscolhido, str(Fonte)]


def Rodar(Comando, Ambiente=None, Pasta=None):
    return subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace",
                          timeout=TimeoutSegundos, env=Ambiente, cwd=Pasta)


def TemLibclang():
    try:
        return Rodar(["python", "-c", "import clang.cindex"]).returncode == 0
    except OSError:
        return False


def JsonsDoStderr(Texto):
    Dados = []
    for Linha in Texto.splitlines():
        if Linha.strip().startswith("{"):
            try:
                Dados.append(json.loads(Linha))
            except json.JSONDecodeError:
                continue  # linha que comeca com { mas nao e JSON do ligar: ignorada de proposito
    return Dados


def ExigirLibclang(Contexto):
    if not Contexto["TemLibclang"]:
        raise Pulado("libclang nao encontrada (python -c 'import clang.cindex' falhou)")


def GerarForjateste(Forja, Contexto, NomeSaida, Extras=()):
    Comando = [str(Forja), "ligar", str(PastaAqui / "lib" / "forjateste.h"), "--link", "forjateste",
               "-o", str(Contexto["Trabalho"] / NomeSaida)] + list(Extras)
    return Rodar(Comando)


def CasoGerarC(Forja, Contexto):
    ExigirLibclang(Contexto)
    Rodada = GerarForjateste(Forja, Contexto, "forjateste.fj")
    Contexto["Caso1"] = Rodada
    if Rodada.returncode != 0:
        return [f"exit {Rodada.returncode}: {Rodada.stderr.strip()[:300]!r}"]
    Gerado = (Contexto["Trabalho"] / "forjateste.fj").read_bytes()
    Esperado = (PastaAqui / "mods" / "forjateste.esperado.fj").read_bytes()
    return [] if Gerado == Esperado else ["forjateste.fj difere de mods\\forjateste.esperado.fj"]


def CasoRecusas(Forja, Contexto):
    ExigirLibclang(Contexto)
    Rodada = Contexto.get("Caso1") or GerarForjateste(Forja, Contexto, "forjateste.fj")
    Dados = JsonsDoStderr(Rodada.stderr)
    Problemas = []
    if [D.get("codigo") for D in Dados] != ["G0004", "G0008"] or [D.get("simbolo") for D in Dados] != ["FT_DOBRO", "ft_soma_tudo"]:
        Problemas.append(f"recusas inesperadas: {Rodada.stderr.strip()[:300]!r}")
    Estrito = GerarForjateste(Forja, Contexto, "forjateste_estrito.fj", ["--estrito"])
    if Estrito.returncode != 1 or not (Contexto["Trabalho"] / "forjateste_estrito.fj").exists():
        Problemas.append(f"--estrito: exit {Estrito.returncode} (esperado 1) ou arquivo ausente")
    return Problemas


def PrepararForjateste(Forja, Contexto):
    """Gera forjateste.fj e compila forjateste.dll em Trabalho; devolve a lista de problemas."""
    Trabalho = Contexto["Trabalho"]
    Rodada = GerarForjateste(Forja, Contexto, "forjateste.fj")
    if Rodada.returncode != 0:
        return [f"ligar: exit {Rodada.returncode}: {Rodada.stderr.strip()[:300]!r}"]
    if not (Trabalho / "forjateste.dll").exists():
        if not Contexto["DllOk"]:
            return ["forjateste.dll nao compilou"]
    return []


def CasoRodarC(Forja, Contexto):
    ExigirLibclang(Contexto)
    Problemas = PrepararForjateste(Forja, Contexto)
    if Problemas:
        return Problemas
    Trabalho = Contexto["Trabalho"]
    shutil.copy(PastaAqui / "usa_forjateste.fj", Trabalho / "usa_forjateste.fj")
    Rodada = Rodar(ComandoRun(Forja, Trabalho / "usa_forjateste.fj"), Pasta=Trabalho)
    if Rodada.returncode != 0 or Rodada.stdout != SaidaEsperadaCaso3:
        return [f"run: exit {Rodada.returncode}, stdout {Rodada.stdout!r}, stderr {Rodada.stderr.strip()[:300]!r}"]
    return []


def CasoMath(Forja, Contexto):
    ExigirLibclang(Contexto)
    Trabalho = Contexto["Trabalho"]
    Rodada = Rodar([str(Forja), "ligar", "math.h", "--link", "m", "--prefixo", "sqrt", "-o", str(Trabalho / "mat.fj")])
    if Rodada.returncode != 0:
        return [f"ligar math.h: exit {Rodada.returncode}: {Rodada.stderr.strip()[:300]!r}"]
    shutil.copy(PastaAqui / "usa_mat.fj", Trabalho / "usa_mat.fj")
    Execucao = Rodar(ComandoRun(Forja, Trabalho / "usa_mat.fj"), Pasta=Trabalho)
    if Execucao.returncode != 0 or Execucao.stdout != "9\n":
        return [f"run: exit {Execucao.returncode}, stdout {Execucao.stdout!r}, stderr {Execucao.stderr.strip()[:300]!r}"]
    return []


def CasoSemLibclang(Forja, Contexto):
    Ambiente = dict(os.environ, FORJA_LIGAR_SEM_LIBCLANG="1")
    Saida = Contexto["Trabalho"] / "nada.fj"
    Rodada = Rodar([str(Forja), "ligar", str(PastaAqui / "lib" / "forjateste.h"), "--link", "forjateste", "-o", str(Saida)], Ambiente)
    Dados = JsonsDoStderr(Rodada.stderr)
    Problemas = []
    if Rodada.returncode != 2:
        Problemas.append(f"exit {Rodada.returncode}, esperado 2")
    if not Dados or Dados[-1].get("codigo") != "G0102":
        Problemas.append(f"ultimo JSON sem G0102: {Rodada.stderr.strip()[:200]!r}")
    if Saida.exists():
        Problemas.append("nada.fj foi gravado")
    return Problemas


def CasoSemPython(Forja, Contexto):
    Ambiente = dict(os.environ, FORJA_PYTHON=r"C:\nao\existe\python.exe")
    Rodada = Rodar([str(Forja), "ligar", str(PastaAqui / "lib" / "forjateste.h"), "--link", "forjateste"], Ambiente)
    Dados = JsonsDoStderr(Rodada.stderr)
    Problemas = []
    if Rodada.returncode != 2:
        Problemas.append(f"exit {Rodada.returncode}, esperado 2")
    if not Dados or Dados[-1].get("codigo") != "G0101":
        Problemas.append(f"ultimo JSON sem G0101: {Rodada.stderr.strip()[:200]!r}")
    return Problemas


def CasoCodigosExplicados(Forja, Contexto):
    Rodada = Rodar([str(Forja), "ligar", "--codigos"])
    Problemas = []
    if Rodada.returncode != 0 or Rodada.stdout.split() != CodigosG:
        Problemas.append(f"--codigos: exit {Rodada.returncode}, saida {Rodada.stdout.split()}")
    for Codigo in CodigosG:
        Explicacao = Rodar([str(Forja), "explicar", Codigo])
        if Explicacao.returncode != 0:
            Problemas.append(f"explicar {Codigo}: exit {Explicacao.returncode}")
    return Problemas


def ExigirGpp(Contexto):
    if not Contexto["TemGpp"]:
        raise Pulado("g++ nao encontrado no PATH")


def CompilarClasse(Trabalho):
    """g++ do wrapper gerado + classe.cpp -> classe.dll em Trabalho; devolve a lista de problemas."""
    Lib = PastaAqui / "lib"
    Rodada = Rodar(["g++", "-std=c++17", "-O2", "-shared", "-static-libgcc", "-static-libstdc++", "-I", str(Lib), "-o",
                    str(Trabalho / "classe.dll"), str(Lib / "classe.cpp"), str(Trabalho / "classe_c.cpp")])
    return [] if Rodada.returncode == 0 else [f"g++: exit {Rodada.returncode}: {Rodada.stderr.strip()[:400]!r}"]


def CasoCpp(Forja, Contexto):
    ExigirLibclang(Contexto)
    ExigirGpp(Contexto)
    Trabalho = Contexto["Trabalho"]
    Rodada = Rodar([str(Forja), "ligar", "--cpp", str(PastaAqui / "lib" / "classe.hpp"), "--link", "classe",
                    "-o", str(Trabalho / "classe.fj")])
    if Rodada.returncode != 0:
        return [f"ligar --cpp: exit {Rodada.returncode}: {Rodada.stderr.strip()[:300]!r}"]
    Problemas = []
    if (Trabalho / "classe.fj").read_bytes() != (PastaAqui / "mods" / "classe.esperado.fj").read_bytes():
        Problemas.append("classe.fj difere de mods\\classe.esperado.fj")
    if not (Trabalho / "classe_c.cpp").exists():
        return Problemas + ["classe_c.cpp nao foi gravado"]
    Problemas += CompilarClasse(Trabalho)
    if Problemas:
        return Problemas
    shutil.copy(PastaAqui / "usa_classe.fj", Trabalho / "usa_classe.fj")
    Execucao = Rodar(ComandoRun(Forja, Trabalho / "usa_classe.fj"), Pasta=Trabalho)
    if Execucao.returncode != 0 or Execucao.stdout != "15\n12\n":
        return [f"run: exit {Execucao.returncode}, stdout {Execucao.stdout!r}, stderr {Execucao.stderr.strip()[:300]!r}"]
    return []


def CasoCppExcecao(Forja, Contexto):
    ExigirLibclang(Contexto)
    ExigirGpp(Contexto)
    Trabalho = Contexto["Trabalho"]
    if not (Trabalho / "classe.dll").exists():
        return ["classe.dll nao existe (depende do caso 8)"]
    shutil.copy(PastaAqui / "usa_classe_falha.fj", Trabalho / "usa_classe_falha.fj")
    Execucao = Rodar(ComandoRun(Forja, Trabalho / "usa_classe_falha.fj"), Pasta=Trabalho)
    Esperado = {"codigo": "R0010", "arquivo": "classe.hpp", "linha": 13,
                "causa": "excecao C++ em teste_cpp::Contador::dividir: divisao por zero"}
    Achou = any(all(D.get(Chave) == Valor for Chave, Valor in Esperado.items()) for D in JsonsDoStderr(Execucao.stderr))
    if Execucao.returncode != 101 or not Achou:
        return [f"run: exit {Execucao.returncode} (esperado 101), stderr {Execucao.stderr.strip()[:400]!r}"]
    return []


NomesFuncoesForjateste = sorted(["ft_contador_criar", "ft_contador_somar", "ft_contador_valor", "ft_contador_destruir", "ft_cor_valor",
                                 "ft_vec2_soma", "ft_ret_mover", "ft_ret_area", "ft_valor_inteiro", "ft_visitar", "ft_nome",
                                 "ft_contar_textos", "ft_soma_tudo"])


def TemPefile():
    try:
        return Rodar(["python", "-c", "import pefile"]).returncode == 0
    except OSError:
        return False


def CasoDll(Forja, Contexto):
    if not TemPefile():
        raise Pulado("pefile nao encontrado (python -c 'import pefile' falhou)")
    Trabalho = Contexto["Trabalho"]
    if not Contexto["DllOk"]:
        return ["forjateste.dll nao compilou"]
    Dll = str(Trabalho / "forjateste.dll")
    Listagem = Rodar([str(Forja), "ligar", "--dll", Dll, "--listar"])
    if Listagem.returncode != 0 or sorted(Listagem.stdout.split()) != NomesFuncoesForjateste:
        return [f"exportacoes inesperadas: exit {Listagem.returncode}, {sorted(Listagem.stdout.split())}"]
    Rodada = Rodar([str(Forja), "ligar", "--dll", Dll, "--assinaturas", str(PastaAqui / "lib" / "forjateste.sig"),
                    "-o", str(Trabalho / "forjateste_dll.fj")])
    Problemas = []
    Dados = JsonsDoStderr(Rodada.stderr)
    if Rodada.returncode != 0:
        return [f"ligar --dll: exit {Rodada.returncode}: {Rodada.stderr.strip()[:300]!r}"]
    if [D.get("codigo") for D in Dados] != ["G0025"] or Dados[0].get("linha") != 7:
        Problemas.append(f"recusas inesperadas: {Rodada.stderr.strip()[:300]!r}")
    if (Trabalho / "forjateste_dll.fj").read_bytes() != (PastaAqui / "mods" / "forjateste_dll.esperado.fj").read_bytes():
        Problemas.append("forjateste_dll.fj difere de mods\\forjateste_dll.esperado.fj")
    shutil.copy(PastaAqui / "usa_forjateste_dll.fj", Trabalho / "usa_forjateste_dll.fj")
    Execucao = Rodar(ComandoRun(Forja, Trabalho / "usa_forjateste_dll.fj"), Pasta=Trabalho)
    if Execucao.returncode != 0 or Execucao.stdout != "4\ntrue\n":
        Problemas.append(f"run: exit {Execucao.returncode}, stdout {Execucao.stdout!r}, stderr {Execucao.stderr.strip()[:300]!r}")
    return Problemas


def CasoDllListar(Forja, Contexto):
    if not TemPefile():
        raise Pulado("pefile nao encontrado (python -c 'import pefile' falhou)")
    Rodada = Rodar([str(Forja), "ligar", "--dll", r"C:\Windows\System32\kernel32.dll", "--listar"])
    if Rodada.returncode != 0 or "GetTickCount64" not in Rodada.stdout.splitlines():
        return [f"--listar kernel32: exit {Rodada.returncode}, {Rodada.stderr.strip()[:200]!r}"]
    return []


def CasoLib110Gerar(Forja, Contexto):
    """v1.10: alias c_<nome> (get/insert/has), macros dobraveis (A7), #define repetido (A8), acessador que colide (A9), wrapper (A11)."""
    ExigirLibclang(Contexto)
    Trabalho = Contexto["Trabalho"]
    Rodada = Rodar([str(Forja), "ligar", str(PastaAqui / "lib" / "lib110.h"), "--link", "lib110", "--wrapper-variadicas",
                    "-o", str(Trabalho / "lib110.fj")])
    if Rodada.returncode != 0 or Rodada.stderr.strip():
        return [f"ligar lib110.h: exit {Rodada.returncode}, stderr {Rodada.stderr.strip()[:300]!r}"]
    Problemas = []
    if (Trabalho / "lib110.fj").read_bytes() != (PastaAqui / "mods" / "lib110.esperado.fj").read_bytes():
        Problemas.append("lib110.fj difere de mods\\lib110.esperado.fj")
    if (Trabalho / "lib110_variadicas.c").read_bytes() != (PastaAqui / "mods" / "lib110_variadicas.esperado.c").read_bytes():
        Problemas.append("lib110_variadicas.c difere de mods\\lib110_variadicas.esperado.c")
    return Problemas


def CompilarLib110(Trabalho):
    """lib110.dll e liblib110_variadicas.a (gcc + ar) em Trabalho; devolve a lista de problemas."""
    Lib = PastaAqui / "lib"
    Passos = [["gcc", "-std=c11", "-O2", "-shared", "-o", str(Trabalho / "lib110.dll"), str(Lib / "lib110.c")],
              ["gcc", "-std=c11", "-O2", "-c", "-I", str(Lib), str(Trabalho / "lib110_variadicas.c"), "-o",
               str(Trabalho / "lib110_variadicas.o")]]
    if BackendEscolhido == "x64":
        # O x64 liga DLL, exe e import library; o .a estatico chega no M3b (P23d, X0103): o mesmo wrapper vira DLL.
        Passos.append(["gcc", "-shared", "-o", str(Trabalho / "lib110_variadicas.dll"),
                       str(Trabalho / "lib110_variadicas.o"), str(Trabalho / "lib110.dll")])
    else:
        Passos.append(["ar", "rcs", str(Trabalho / "liblib110_variadicas.a"), str(Trabalho / "lib110_variadicas.o")])
    for Passo in Passos:
        Rodada = Rodar(Passo)
        if Rodada.returncode != 0:
            return [f"{Passo[0]}: exit {Rodada.returncode}: {Rodada.stderr.strip()[:300]!r}"]
    return []


def CasoLib110Rodar(Forja, Contexto):
    ExigirLibclang(Contexto)
    Trabalho = Contexto["Trabalho"]
    if not (Trabalho / "lib110_variadicas.c").exists():
        return ["lib110_variadicas.c nao existe (depende do caso 12)"]
    Problemas = CompilarLib110(Trabalho)
    if Problemas:
        return Problemas
    shutil.copy(PastaAqui / "usa_lib110.fj", Trabalho / "usa_lib110.fj")
    Execucao = Rodar(ComandoRun(Forja, Trabalho / "usa_lib110.fj"), Pasta=Trabalho)
    if Execucao.returncode != 0 or Execucao.stdout != SaidaEsperadaCaso13:
        return [f"run: exit {Execucao.returncode}, stdout {Execucao.stdout!r}, stderr {Execucao.stderr.strip()[:300]!r}"]
    return []


def CasoVariadicaSemFormato(Forja, Contexto):
    """ft_soma_tudo(int32_t n, ...) nao tem const char *: segue G0008 mesmo com --wrapper-variadicas, sem gravar _variadicas.c."""
    ExigirLibclang(Contexto)
    Rodada = GerarForjateste(Forja, Contexto, "forjateste_wrap.fj", ["--wrapper-variadicas"])
    Codigos = [(D.get("codigo"), D.get("simbolo")) for D in JsonsDoStderr(Rodada.stderr)]
    Problemas = []
    if Rodada.returncode != 0 or ("G0008", "ft_soma_tudo") not in Codigos:
        Problemas.append(f"exit {Rodada.returncode}, recusas {Codigos}")
    if (Contexto["Trabalho"] / "forjateste_wrap_variadicas.c").exists():
        Problemas.append("forjateste_wrap_variadicas.c foi gravado sem variadica elegivel")
    return Problemas


def CasoBitfields(Forja, Contexto):
    """0.5.0 (F1c): bitfield 1/3/5/12 bits, union e struct anonimas e array flexivel: ponte = esperado, e a saida do programa Forja
    (get/set gerados) bate byte a byte com o gemeo em C puro (mods\\bitfields_prova.c)."""
    ExigirLibclang(Contexto)
    Trabalho = Contexto["Trabalho"]
    Mods = PastaAqui / "mods"
    Rodada = Rodar([str(Forja), "ligar", str(Mods / "bitfields.h"), "--link", "bitfields", "-o", str(Trabalho / "bitfields.fj")])
    if Rodada.returncode != 0 or Rodada.stderr.strip():
        return [f"ligar bitfields.h: exit {Rodada.returncode}, stderr {Rodada.stderr.strip()[:300]!r}"]
    Problemas = []
    if (Trabalho / "bitfields.fj").read_bytes() != (Mods / "bitfields.esperado.fj").read_bytes():
        Problemas.append("bitfields.fj difere de mods\\bitfields.esperado.fj")
    Passos = [["gcc", "-std=c11", "-O2", "-shared", "-I", str(Mods), "-o", str(Trabalho / "bitfields.dll"), str(Mods / "bitfields.c")],
              ["gcc", "-std=c11", "-O1", "-I", str(Mods), "-o", str(Trabalho / "bitfields_gemeo.exe"), str(Mods / "bitfields_prova.c"),
               str(Mods / "bitfields.c")]]
    for Passo in Passos:
        Compilacao = Rodar(Passo)
        if Compilacao.returncode != 0:
            return Problemas + [f"gcc: exit {Compilacao.returncode}: {Compilacao.stderr.strip()[:300]!r}"]
    Gemeo = Rodar([str(Trabalho / "bitfields_gemeo.exe")])
    shutil.copy(PastaAqui / "usa_bitfields.fj", Trabalho / "usa_bitfields.fj")
    Execucao = Rodar(ComandoRun(Forja, Trabalho / "usa_bitfields.fj"), Pasta=Trabalho)
    if Execucao.returncode != 0 or Gemeo.returncode != 0 or Execucao.stdout != Gemeo.stdout or len(Gemeo.stdout.split()) != 71:
        Problemas.append(f"run: exit {Execucao.returncode}, {len(Execucao.stdout.split())} linhas, difere do gemeo C "
                         f"(exit {Gemeo.returncode}); stderr {Execucao.stderr.strip()[:300]!r}")
    return Problemas


def CompilarLibInlineCola(Trabalho, Mods):
    """libinline_cola.a (a lib de prova, com as globais e ic_incrementa) e o gemeo C puro; devolve a lista de problemas."""
    Passos = [["gcc", "-std=c11", "-O2", "-c", "-I", str(Mods), "-o", str(Trabalho / "inline_cola.o"), str(Mods / "inline_cola.c")],
              ["ar", "rcs", str(Trabalho / "libinline_cola.a"), str(Trabalho / "inline_cola.o")],
              ["gcc", "-std=c11", "-O1", "-I", str(Mods), "-o", str(Trabalho / "inline_cola_gemeo.exe"), str(Mods / "inline_cola_prova.c"),
               str(Mods / "inline_cola.c")]]
    for Passo in Passos:
        Rodada = Rodar(Passo)
        if Rodada.returncode != 0:
            return [f"{Passo[0]}: exit {Rodada.returncode}: {Rodada.stderr.strip()[:300]!r}"]
    return []


def LigarInlineCola(Forja, Trabalho, Mods, Sig, Saida):
    return Rodar([str(Forja), "ligar", str(Mods / "inline_cola.h"), "--link", "inline_cola", "-o", str(Trabalho / Saida),
                  "--macros", str(Sig), "--compilar-cola"])


def CasoColaErroDoGcc(Forja, Contexto):
    """Macro com retorno que o gcc recusa (int onde se espera IcPar): G0103 com o texto do gcc, exit 2, stdout vazio, .fj nao gravado."""
    Trabalho, Mods = Contexto["Trabalho"], PastaAqui / "mods"
    Sig = Trabalho / "inline_cola_ruim.sig"
    Sig.write_text("IC_TRIPLO(int32_t x) -> IcPar\n", encoding="utf-8")
    Rodada = LigarInlineCola(Forja, Trabalho, Mods, Sig, "inline_cola_ruim.fj")
    Dados = JsonsDoStderr(Rodada.stderr)
    Problemas = []
    if Rodada.returncode != 2 or not Dados or Dados[-1].get("codigo") != "G0103" or "gcc" not in Dados[-1].get("causa", ""):
        Problemas.append(f"gcc ruim: exit {Rodada.returncode} (esperado 2), stderr {Rodada.stderr.strip()[:300]!r}")
    if Rodada.stdout.strip() or (Trabalho / "inline_cola_ruim.fj").exists():
        Problemas.append("a falha do gcc vazou no stdout ou o .fj foi gravado")
    return Problemas


def CasoColaGerar(Forja, Contexto):
    """Gera a ponte com --macros + --compilar-cola: ponte = esperado, .a saiu, so IC_SEM_ASSINATURA segue G0004; sem --cola, ic_soma segue G0005."""
    Trabalho, Mods = Contexto["Trabalho"], PastaAqui / "mods"
    Rodada = LigarInlineCola(Forja, Trabalho, Mods, Mods / "inline_cola.sig", "inline_cola.fj")
    Dados = [(D.get("codigo"), D.get("simbolo")) for D in JsonsDoStderr(Rodada.stderr)]
    Problemas = []
    if Rodada.returncode != 0 or Dados != [("G0004", "IC_SEM_ASSINATURA")]:
        Problemas.append(f"ligar --cola: exit {Rodada.returncode}, recusas {Dados}, stderr {Rodada.stderr.strip()[:200]!r}")
    if (Trabalho / "inline_cola.fj").read_bytes() != (Mods / "inline_cola.esperado.fj").read_bytes():
        Problemas.append("inline_cola.fj difere de mods\\inline_cola.esperado.fj")
    if not (Trabalho / "libinline_cola_cola.a").is_file() or "fjcola_ic_soma" not in (Trabalho / "inline_cola_cola.c").read_text(encoding="utf-8"):
        Problemas.append("libinline_cola_cola.a ou inline_cola_cola.c sem o wrapper fjcola_ic_soma")
    Sem = Rodar([str(Forja), "ligar", str(Mods / "inline_cola.h"), "--link", "inline_cola", "-o", str(Trabalho / "inline_cola_sem.fj")])
    if ("G0005", "ic_soma") not in [(D.get("codigo"), D.get("simbolo")) for D in JsonsDoStderr(Sem.stderr)]:
        Problemas.append(f"sem --cola ic_soma deveria seguir G0005: {Sem.stderr.strip()[:200]!r}")
    return Problemas


def CasoCola(Forja, Contexto):
    """0.6.0 (F1e): static inline, global get/set, array global e macro-funcao com .sig pela cola C gerada e compilada pelo ligar; a saida do
    programa Forja (ic_soma, ic_troca, globais, macros) bate com o gemeo em C puro (mods\\inline_cola_prova.c) nos 2 backends."""
    ExigirLibclang(Contexto)
    Trabalho, Mods = Contexto["Trabalho"], PastaAqui / "mods"
    Problemas = CasoColaGerar(Forja, Contexto)
    if Problemas:
        return Problemas
    Problemas = CompilarLibInlineCola(Trabalho, Mods)
    if Problemas:
        return Problemas
    Gemeo = Rodar([str(Trabalho / "inline_cola_gemeo.exe")])
    shutil.copy(PastaAqui / "usa_inline_cola.fj", Trabalho / "usa_inline_cola.fj")
    Execucao = Rodar(ComandoRun(Forja, Trabalho / "usa_inline_cola.fj"), Pasta=Trabalho)
    if Execucao.returncode != 0 or Gemeo.returncode != 0 or Execucao.stdout != Gemeo.stdout or len(Gemeo.stdout.split()) != 10:
        return [f"run: exit {Execucao.returncode}, {Execucao.stdout.split()}, gemeo (exit {Gemeo.returncode}) {Gemeo.stdout.split()}; "
                f"stderr {Execucao.stderr.strip()[:300]!r}"]
    return CasoColaErroDoGcc(Forja, Contexto)


def CasoArrayParametro(Forja, Contexto):
    """0.6.1 (F1f): parametro 'T x[N]' e 'T x[]' decaem em ponteiro (?^i32); ponte = esperado e forja check sem erro (so o E1005 do stub)."""
    ExigirLibclang(Contexto)
    Trabalho, Mods = Contexto["Trabalho"], PastaAqui / "mods"
    Rodada = Rodar([str(Forja), "ligar", str(Mods / "array_param.h"), "--link", "array_param", "-o", str(Trabalho / "array_param.fj")])
    if Rodada.returncode != 0 or Rodada.stderr.strip():
        return [f"ligar array_param.h: exit {Rodada.returncode}, stderr {Rodada.stderr.strip()[:200]!r}"]
    Problemas = []
    if (Trabalho / "array_param.fj").read_bytes() != (Mods / "array_param.esperado.fj").read_bytes():
        Problemas.append("array_param.fj difere de mods\\array_param.esperado.fj")
    Stub = Trabalho / "stub_array_param.fj"
    Stub.write_text('import "array_param"\n\nproc main() {\n}\n', encoding="utf-8", newline="\n")
    Check = Rodar([str(Forja), "check", str(Stub)])
    Codigos = [D.get("codigo") for D in JsonsDoStderr(Check.stdout + "\n" + Check.stderr)]
    Erros = [C for C in Codigos if C and C.startswith("E") and C != "E1005"]
    if Erros:
        Problemas.append(f"forja check da ponte array_param deu {Erros}")
    return Problemas


def CasoClong(Forja, Contexto):
    """0.7.0 (M6c P84): long/unsigned long do C saem como c_long/c_ulong (mods\\clong.h); ponte = mods\\clong.esperado.fj e forja check sem erro (so o E1005 do stub)."""
    ExigirLibclang(Contexto)
    Trabalho, Mods = Contexto["Trabalho"], PastaAqui / "mods"
    Rodada = Rodar([str(Forja), "ligar", str(Mods / "clong.h"), "--link", "clong", "-o", str(Trabalho / "clong.fj")])
    if Rodada.returncode != 0 or Rodada.stderr.strip():
        return [f"ligar clong.h: exit {Rodada.returncode}, stderr {Rodada.stderr.strip()[:200]!r}"]
    Problemas = []
    if (Trabalho / "clong.fj").read_bytes() != (Mods / "clong.esperado.fj").read_bytes():
        Problemas.append("clong.fj difere de mods\\clong.esperado.fj")
    if "static_assert(size_of(ClongDado)" in (Trabalho / "clong.fj").read_text(encoding="utf-8"):
        Problemas.append("ClongDado (com c_ulong) ganhou static_assert: 0.7.1 proibe (o tamanho segue o alvo)")
    Stub = Trabalho / "stub_clong.fj"
    Stub.write_text('import "clong"\n\nproc main() {\n}\n', encoding="utf-8", newline="\n")
    Check = Rodar([str(Forja), "check", str(Stub)])
    Codigos = [D.get("codigo") for D in JsonsDoStderr(Check.stdout + "\n" + Check.stderr)]
    Erros = [C for C in Codigos if C and C.startswith("E") and C != "E1005"]
    if Erros:
        Problemas.append(f"forja check da ponte clong deu {Erros}")
    return Problemas


def CasoPropor(Forja, Contexto):
    """0.8.0 (M6c P89): --propor (mods\\propor.h) = mods\\propor.esperado.fj; sem --propor nao sai clausula nem comentario; forja check da ponte proposta sem erro (so o E1005 do stub)."""
    ExigirLibclang(Contexto)
    Trabalho, Mods = Contexto["Trabalho"], PastaAqui / "mods"
    Base = [str(Forja), "ligar", str(Mods / "propor.h"), "--link", "propor"]
    Rodada = Rodar(Base + ["--propor", "-o", str(Trabalho / "propor.fj")])
    if Rodada.returncode != 0 or Rodada.stderr.strip():
        return [f"ligar --propor: exit {Rodada.returncode}, stderr {Rodada.stderr.strip()[:200]!r}"]
    Problemas = []
    if (Trabalho / "propor.fj").read_bytes() != (Mods / "propor.esperado.fj").read_bytes():
        Problemas.append("propor.fj difere de mods\\propor.esperado.fj")
    Rodada = Rodar(Base + ["-o", str(Trabalho / "sem_propor.fj")])
    Texto = (Trabalho / "sem_propor.fj").read_text(encoding="utf-8")
    if Rodada.returncode != 0 or "proposto" in Texto or " creates" in Texto or " destroys" in Texto:
        Problemas.append("sem --propor a saida ganhou clausula ou comentario")
    Stub = Trabalho / "stub_propor.fj"
    Stub.write_text('import "propor"\n\nproc main() {\n}\n', encoding="utf-8", newline="\n")
    Check = Rodar([str(Forja), "check", str(Stub)])
    Codigos = [D.get("codigo") for D in JsonsDoStderr(Check.stdout + "\n" + Check.stderr)]
    Erros = [C for C in Codigos if C and C.startswith("E") and C != "E1005"]
    if Erros:
        Problemas.append(f"forja check da ponte propor deu {Erros}")
    return Problemas


def CasoFim(Forja, Contexto):
    """0.9.0 (M6d P92g): --propor grava ends so em proc sem parametro cujo nome e shutdown/global_cleanup/library_end; sem --propor nao sai ends."""
    ExigirLibclang(Contexto)
    Trabalho, Mods = Contexto["Trabalho"], PastaAqui / "mods"
    Base = [str(Forja), "ligar", str(Mods / "fim.h"), "--link", "fim", "--prefixo", "x_"]
    Rodada = Rodar(Base + ["--propor", "-o", str(Trabalho / "fim.fj")])
    if Rodada.returncode != 0 or Rodada.stderr.strip():
        return [f"ligar --propor (fim): exit {Rodada.returncode}, stderr {Rodada.stderr.strip()[:200]!r}"]
    Problemas = []
    if (Trabalho / "fim.fj").read_bytes() != (Mods / "fim.esperado.fj").read_bytes():
        Problemas.append("fim.fj difere de mods\\fim.esperado.fj")
    Rodada = Rodar(Base + ["-o", str(Trabalho / "sem_fim.fj")])
    if Rodada.returncode != 0 or " ends" in (Trabalho / "sem_fim.fj").read_text(encoding="utf-8"):
        Problemas.append("sem --propor a saida ganhou ends")
    return Problemas


CASOS = [
    ("gerar forjateste.h (C)", CasoGerarC),
    ("recusas G0004 e G0008", CasoRecusas),
    ("rodar o modulo gerado", CasoRodarC),
    ("math.h real com --prefixo", CasoMath),
    ("sem libclang (G0102)", CasoSemLibclang),
    ("sem python (G0101)", CasoSemPython),
    ("codigos G explicados", CasoCodigosExplicados),
    ("C++: wrapper e classe", CasoCpp),
    ("C++: excecao vira R0010", CasoCppExcecao),
    ("DLL: assinaturas e run", CasoDll),
    ("DLL: --listar kernel32", CasoDllListar),
    ("v1.10: gerar lib110.h (alias, macros, acessador, wrapper)", CasoLib110Gerar),
    ("v1.10: rodar lib110 (alias c_, consts, variadica)", CasoLib110Rodar),
    ("v1.10: variadica sem formato segue G0008", CasoVariadicaSemFormato),
    ("0.5.0: bitfield, anonima e flexivel (get/set x gemeo C)", CasoBitfields),
    ("0.6.0: cola C (inline, global, macro com .sig) x gemeo C", CasoCola),
    ("0.6.1: array em parametro vira ?^ e forja check", CasoArrayParametro),
    ("0.7.0: long/unsigned long viram c_long/c_ulong e forja check", CasoClong),
    ("0.8.0: --propor grava creates/destroys e forja check", CasoPropor),
    ("0.9.0: --propor grava ends em shutdown/global_cleanup", CasoFim),
]


def CompilarDll(Trabalho):
    Rodada = Rodar(["gcc", "-std=c11", "-O2", "-shared", "-Wall", "-Wextra", "-Werror", "-o",
                    str(Trabalho / "forjateste.dll"), str(PastaAqui / "lib" / "forjateste.c")])
    if Rodada.returncode != 0:
        print(f"forjateste.dll nao compilou: {Rodada.stderr.strip()[:300]}")
    return Rodada.returncode == 0


def Verificar(Forja, Manter):
    Trabalho = Path(tempfile.gettempdir()) / f"forja_ligar_teste_{os.getpid()}"
    Trabalho.mkdir(parents=True, exist_ok=True)
    Contexto = {"Trabalho": Trabalho, "TemLibclang": TemLibclang(), "DllOk": CompilarDll(Trabalho),
                "TemGpp": shutil.which("g++") is not None}
    Passou = Pulados = 0
    try:
        for Nome, Caso in CASOS:
            try:
                Problemas = Caso(Forja, Contexto)
            except Pulado as Motivo:
                Pulados += 1
                print(f"PULADO {Nome}: {Motivo}")
                continue
            if Problemas:
                print(f"FALHOU {Nome}")
                for Problema in Problemas:
                    print(f"    {Problema}")
            else:
                Passou += 1
    finally:
        if not Manter:
            shutil.rmtree(Trabalho, ignore_errors=True)
    print(f"{Passou}/{len(CASOS)} casos de ligar ({Pulados} pulados)")
    return Passou + Pulados == len(CASOS)


def Principal():
    Analisador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Analisador.add_argument("--forja", type=Path, default=ForjaPadrao, help="caminho do forja.exe")
    Analisador.add_argument("--manter", action="store_true", help="nao apaga a pasta de trabalho no %TEMP%")
    Analisador.add_argument("--backend", default="x64", choices=("x64",), help="backend dos 'forja run' (padrao x64)")
    Argumentos = Analisador.parse_args()
    global BackendEscolhido
    BackendEscolhido = Argumentos.backend
    Forja = Argumentos.forja.resolve()
    if not Forja.exists():
        print(f"forja.exe nao encontrado em {Forja}; rode construir.bat", file=sys.stderr)
        return 2
    return 0 if Verificar(Forja, Argumentos.manter) else 1


if __name__ == "__main__":
    sys.exit(Principal())
