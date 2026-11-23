"""Verifica os exemplos de examples\\*.fj: cada um compila, roda (forja run) e imprime a saida esperada.

Regras:
  - 01..04: `forja run` sai com exit 0 e o stdout e exatamente o da tabela CASOS.
  - 04 tambem passa em `forja test` (ultima linha '2 testes, 0 falharam').
  - Todo examples\\NN_*.fj precisa estar na tabela (exemplo novo sem saida esperada falha).

  - modulos_l8 (IA1-L8): dois_modulos.fj (raiz + 2 modulos com os mesmos nomes de topo) imprime dois_modulos.esperado.txt em x64, dev e release;
    panic_modulo.fj com --rastreio sai com 101 e mostra o proc "close" (nome puro, sem a chave por modulo) no backend x64.

Versao: 1.1.1
Changelog: 1.1.1 - 0.21.0 F3: padrao x64, backend C aposentado (run, test e modulos_l8 sem --backend c).
           1.1.0 - IA1-L8: casos de modulos_l8 (nomes de topo iguais em modulos diferentes, rastro com o nome puro).
           1.0.1 - M4 P25b: run e test levam --backend c explicito (o oraculo e o C; o padrao do dev passou a ser x64).
           1.0.0 - Fase 5i (Entrega): primeira versao.
Verificacao: python verificar_exemplos.py --forja <forja.exe>
"""

import argparse
import subprocess
import sys
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
RaizEngine = PastaAqui.parent.parent.parent
PastaExemplos = RaizEngine / "examples"
ForjaPadrao = RaizEngine / "language" / "compiler" / "forja.exe"
TimeoutSegundos = 120

# arquivo -> stdout esperado do `forja run`
CASOS = {
    "01_ola.fj": "Olá, Forja!\n42\n21\n",
    "02_resultado.fj": "30\n-1\n80\nnao achou\n",
    "03_unidades_e_faixa.fj": "5\n55\n9\n100\n",
    "04_defer_e_testes.fj": "55\nquase fim\nfim\n",
    "05_vetor_c.fj": "5\n30\n",
}
# arquivo -> ultima linha esperada do `forja test`
CASOS_TESTE = {"04_defer_e_testes.fj": "2 testes, 0 falharam"}


def Rodar(Forja, Argumentos):
    return subprocess.run(
        [str(Forja)] + Argumentos, capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=TimeoutSegundos, cwd=RaizEngine,
    )


def ConferirRun(Forja, Nome, Esperado):
    Rodada = Rodar(Forja, ["run", f"examples/{Nome}"])
    Problemas = []
    if Rodada.returncode != 0:
        Problemas.append(f"run: exit {Rodada.returncode}; stdout={Rodada.stdout.strip()[:200]!r}")
    elif Rodada.stdout.replace("\r\n", "\n") != Esperado:
        Problemas.append(f"run: stdout {Rodada.stdout!r}, esperado {Esperado!r}")
    return Problemas


def ConferirTeste(Forja, Nome, UltimaEsperada):
    Rodada = Rodar(Forja, ["test", f"examples/{Nome}"])
    Linhas = [Linha for Linha in Rodada.stdout.splitlines() if Linha.strip()]
    Ultima = Linhas[-1] if Linhas else ""
    if Rodada.returncode != 0 or Ultima != UltimaEsperada:
        return [f"test: exit {Rodada.returncode}, ultima linha {Ultima!r}, esperado {UltimaEsperada!r}"]
    return []


def RodarEmModulosL8(Forja, Argumentos):
    return subprocess.run(
        [str(Forja), "run"] + Argumentos, capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=TimeoutSegundos, cwd=PastaAqui / "modulos_l8",
    )


def ConferirModulosL8(Forja):
    Esperado = (PastaAqui / "modulos_l8" / "dois_modulos.esperado.txt").read_text(encoding="utf-8").replace("\r\n", "\n")
    Problemas = []
    for Backend in ("x64",):
        for Release in ([], ["--release"]):
            Rodada = RodarEmModulosL8(Forja, ["--backend", Backend] + Release + ["dois_modulos.fj"])
            if Rodada.returncode != 0 or Rodada.stdout.replace("\r\n", "\n") != Esperado:
                Problemas.append(f"dois_modulos {Backend} {Release}: exit {Rodada.returncode}; stdout {Rodada.stdout[:120]!r}")
        Rastro = RodarEmModulosL8(Forja, ["--backend", Backend, "--rastreio", "panic_modulo.fj"])
        if Rastro.returncode != 101 or '"proc": "close"' not in Rastro.stderr + Rastro.stdout:
            Problemas.append(f"panic_modulo {Backend}: exit {Rastro.returncode} (esperado 101) ou rastro sem o proc close")
    return Problemas


def ExemplosSemEsperado():
    Existentes = {Arquivo.name for Arquivo in PastaExemplos.glob("[0-9][0-9]_*.fj")}
    return sorted(Existentes - set(CASOS))


def Verificar(Forja):
    Total = Passou = 0
    for Nome, Esperado in CASOS.items():
        Problemas = ConferirRun(Forja, Nome, Esperado)
        if Nome in CASOS_TESTE:
            Problemas += ConferirTeste(Forja, Nome, CASOS_TESTE[Nome])
        Total += 1
        if Problemas:
            print(f"FALHOU {Nome}")
            for Problema in Problemas:
                print(f"    {Problema}")
        else:
            Passou += 1
    Total += 1
    Problemas = ConferirModulosL8(Forja)
    if Problemas:
        print("FALHOU modulos_l8")
        for Problema in Problemas:
            print(f"    {Problema}")
    else:
        Passou += 1
    for Nome in ExemplosSemEsperado():
        Total += 1
        print(f"FALHOU {Nome}\n    exemplo sem saida esperada na tabela CASOS")
    print(f"{Passou}/{Total} exemplos")
    return Passou == Total


def Principal():
    Analisador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Analisador.add_argument("--forja", type=Path, default=ForjaPadrao, help="caminho do forja.exe")
    Forja = Analisador.parse_args().forja.resolve()
    if not Forja.exists():
        print(f"forja.exe nao encontrado em {Forja}; rode construir.bat", file=sys.stderr)
        return 2
    return 0 if Verificar(Forja) else 1


if __name__ == "__main__":
    sys.exit(Principal())
