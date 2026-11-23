# -*- coding: utf-8 -*-
"""Poda do inalcancavel (B07, M4 P24p): 7 fixtures x {dev, release} = 14 casos, x64 contra valor fixo (o backend C saiu na 0.21.0).

Cada caso prova duas coisas: (1) o x64 com poda produz a saida fixa (exit 0, stderr vazio e stdout byte a byte igual ao .esperado quando a fixture tem um, ou ao valor congelado no script);
(2) a poda aconteceu: o registro "poda" do .debug.jsonl do x64 traz o procs_podados esperado da fixture naquele modo. Uma fixture por raiz da D-M4-6:
  so_main            main como unica raiz; chamada direta alcanca, o resto sai
  cadeia_morta       proc usado so por outro proc podado tambem sai (cadeia e ciclo mortos)
  proc_valor_campo   endereco de proc guardado em campo e chamado de forma indireta mantem o proc vivo
  callback_foreign   proc passado a foreign (trampolim fjt_) fica vivo; o que ninguem passa sai
  modulo_grande      modulo importado com 50 procs e 1 usado: 49 saem
  teste_raiz         procs 'test' sao raizes em forja test (dev); no exe release eles nem entram
  export_dll         'pub export proc' sao raizes em --modo dll; a DLL x64 e carregada por ctypes e chamada

Uso: python -I verificar_poda.py [--forja <exe>]
Verificacao: a ultima linha e 'poda: iguais=14 diferentes=0' (exit 0).
Versao: 0.1.2
Changelog: 0.1.2 - 0.21.0 F3: o oraculo C saiu; cada caso roda so o x64 contra o valor fixo (exit 0, stderr vazio, stdout do .esperado ou valor congelado), capturado uma vez do C e igual.
           0.1.1 - M4 P25b: o lado C do forja test leva --backend c explicito (o padrao do dev passou a ser x64).
           0.1.0 - M4 P24p: primeira versao.
"""

import argparse
import ctypes
import json
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
ForjaPadrao = PastaAqui.parents[3] / "compiler" / "forja.exe"
TimeoutBuild = 120
TimeoutRun = 30
CasosEsperados = 14
Modos = ("dev", "release")
# fixture -> procs podados esperados por modo (None = o modo nao tem registro "poda": 'forja test' nao aceita --debug-code)
Fixtures = {
    "so_main": {"dev": 2, "release": 2},
    "cadeia_morta": {"dev": 5, "release": 5},
    "proc_valor_campo": {"dev": 1, "release": 1},
    "callback_foreign": {"dev": 2, "release": 2},
    "modulo_grande": {"dev": 49, "release": 49},
    "teste_raiz": {"dev": None, "release": 2},
    "export_dll": {"dev": 1, "release": 1},
}
# export_dll: (funcao, argumentos) -> resultado esperado
ChamadasDaDll = (("soma_dobro", (3, 4), 14), ("triplo", (5,), 15), ("soma_dobro", (-1, 1), 0))
# forja test (dev) de teste_raiz: saida do x64 congelada na 0.21.0 (capturada uma vez do backend C, igual); LF aqui, CRLF no Windows
TesteDevEsperado = b"ok   soma\n1 testes, 0 falharam\n"


def Rodar(Comando, Cwd=None):
    """(exit, stdout, stderr) de um processo; timeout vira exit -999."""
    try:
        Cp = subprocess.run(Comando, capture_output=True, stdin=subprocess.DEVNULL, timeout=TimeoutBuild, cwd=Cwd)
    except subprocess.TimeoutExpired:
        return -999, b"", b"timeout"
    return Cp.returncode, Cp.stdout, Cp.stderr


def Construir(Forja, Fonte, Saida, Modo, Dll=False):
    """forja build --backend x64; pede o .debug.jsonl (so o registro poda interessa). Devolve (exit, texto do erro)."""
    Comando = [Forja, "build", "--backend", "x64", "--modo", "dll" if Dll else "exe"]
    Comando += ["--release"] if Modo == "release" else []
    Comando += ["--debug-code", "json"]
    Comando += [str(Fonte), "-o", str(Saida)]
    Codigo, Out, Err = Rodar(Comando)
    return Codigo, (Out + Err).decode("utf-8", "replace")[:300]


def PodadosDoJsonl(Saida):
    """procs_podados do registro 'poda' do .debug.jsonl que acompanha Saida; None se nao ha registro."""
    Jsonl = Path(str(Saida) + ".debug.jsonl")
    if not Jsonl.is_file():
        return None
    for Linha in Jsonl.read_text(encoding="utf-8").splitlines():
        Registro = json.loads(Linha)
        if Registro["tipo"] == "poda":
            if Registro["procs_total"] - Registro["procs_gerados"] != Registro["procs_podados"]:
                return -1
            return Registro["procs_podados"]
    return None


def CompararSaidas(Rotulo, Saida, Esperado):
    """Problemas (lista de texto) da saida (exit, stdout, stderr) do x64 contra o valor fixo: exit 0, stderr vazio e stdout igual ao Esperado (bytes ou None)."""
    Problemas = []
    if Saida[0] != 0:
        Problemas.append(f"{Rotulo}: exit do x64 e {Saida[0]}, esperava 0")
    if Saida[2] != b"":
        Problemas.append(f"{Rotulo}: stderr do x64 nao esta vazio ({Saida[2]!r})")
    # o Esperado e escrito com LF; no Windows o x64 emite CRLF, entao a comparacao e em bytes crus contra a versao CRLF
    if Esperado is not None and Saida[1] != Esperado.replace(b"\n", b"\r\n"):
        Problemas.append(f"{Rotulo}: stdout do x64 difere do esperado ({Saida[1]!r} contra {Esperado!r})")
    return Problemas


def ConferirPoda(Rotulo, Saida, Esperado):
    """Problemas se o registro poda nao traz o procs_podados esperado."""
    Achado = PodadosDoJsonl(Saida)
    if Esperado is None or Achado == Esperado:
        return []
    return [f"{Rotulo}: procs_podados={Achado}, esperava {Esperado}"]


def LerEsperado(Fonte):
    Arquivo = Fonte.with_suffix(".esperado")
    return Arquivo.read_bytes() if Arquivo.is_file() else None


def CasoExe(Forja, Nome, Modo, Pasta):
    """exe: o x64 com poda roda e bate com o valor fixo (exit 0, stderr vazio, .esperado); a poda do x64 confere com a tabela."""
    Fonte = PastaAqui / f"{Nome}.fj"
    Rotulo = f"{Nome} {Modo}"
    Exe = Pasta / f"{Nome}_{Modo}_x64.exe"
    Codigo, Erro = Construir(Forja, Fonte, Exe, Modo)
    if Codigo != 0:
        return [f"{Rotulo}: build x64 falhou ({Codigo}): {Erro}"]
    Problemas = CompararSaidas(Rotulo, Rodar([str(Exe)], Cwd=Pasta), LerEsperado(Fonte))
    return Problemas + ConferirPoda(Rotulo, Exe, Fixtures[Nome][Modo])


def CasoTesteDev(Forja, Nome):
    """forja test (dev): os 'test' sao raizes; o x64 imprime o valor fixo (TesteDevEsperado) e a ajuda do teste sobrevive."""
    Fonte = PastaAqui / f"{Nome}.fj"
    Saida = Rodar([Forja, "test", "--backend", "x64", str(Fonte)])
    return CompararSaidas(f"{Nome} dev (forja test)", Saida, TesteDevEsperado)


def ChamarDll(Caminho):
    """Modo --chamar: carrega a DLL e imprime um resultado por chamada de ChamadasDaDll (em processo a parte: DllMain do runtime)."""
    Dll = ctypes.CDLL(str(Caminho))
    for Funcao, Args, _ in ChamadasDaDll:
        Alvo = getattr(Dll, Funcao)  # lint-ok: iaf-simbolo-dinamico os nomes vem da tabela literal ChamadasDaDll (funcoes exportadas de ctypes so se acham por nome)
        Alvo.restype = ctypes.c_int64
        Alvo.argtypes = [ctypes.c_int64] * len(Args)
        print(f"{Funcao}{Args}={Alvo(*Args)}")


def CasoDll(Forja, Modo, Pasta):
    """--modo dll: a DLL do x64 (com poda) chamada por ctypes da os resultados de ChamadasDaDll; so os exports e seus auxiliares entram no x64."""
    Fonte = PastaAqui / "export_dll.fj"
    Rotulo = f"export_dll {Modo}"
    Dll = Pasta / f"export_dll_{Modo}_x64.dll"
    Codigo, Erro = Construir(Forja, Fonte, Dll, Modo, Dll=True)
    if Codigo != 0:
        return [f"{Rotulo}: build dll x64 falhou ({Codigo}): {Erro}"]
    Esperado = "".join(f"{F}{A}={R}\n" for F, A, R in ChamadasDaDll).encode()
    Saida = Rodar([sys.executable, "-I", str(Path(__file__).resolve()), "--chamar", str(Dll)], Cwd=Pasta)
    return CompararSaidas(Rotulo, Saida, Esperado) + ConferirPoda(Rotulo, Dll, Fixtures["export_dll"][Modo])


def CasoDoModo(Forja, Nome, Modo, Pasta):
    if Nome == "export_dll":
        return CasoDll(Forja, Modo, Pasta)
    if Nome == "teste_raiz" and Modo == "dev":
        return CasoTesteDev(Forja, Nome)
    return CasoExe(Forja, Nome, Modo, Pasta)


def Principal():
    Argumentos = argparse.ArgumentParser(description="Poda do inalcancavel: C contra x64 em 7 fixtures x 2 modos.")
    Argumentos.add_argument("--forja", default=str(ForjaPadrao))
    Argumentos.add_argument("--chamar", help="(interno) carrega a DLL e chama os exports")
    Dados = Argumentos.parse_args()
    if Dados.chamar:
        ChamarDll(Dados.chamar)
        return 0
    Iguais = Diferentes = 0
    with tempfile.TemporaryDirectory(prefix="poda_") as Temp:
        for Nome in Fixtures:
            for Modo in Modos:
                Problemas = CasoDoModo(str(Dados.forja), Nome, Modo, Path(Temp))
                Iguais, Diferentes = Iguais + (not Problemas), Diferentes + bool(Problemas)
                print(("OK    " if not Problemas else "FALHA ") + f"{Nome} {Modo}")
                for Problema in Problemas:
                    print("      " + Problema)
    print(f"poda: iguais={Iguais} diferentes={Diferentes}")
    return 0 if Diferentes == 0 and Iguais == CasosEsperados else 1


if __name__ == "__main__":
    sys.exit(Principal())
