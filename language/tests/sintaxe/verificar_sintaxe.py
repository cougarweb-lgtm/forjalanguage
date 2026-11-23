"""Verifica lexico + parser do compilador Forja contra o corpus dourado e as fixtures de sintaxe.

Regras (Fase 5b):
  - .fj com <nome>.esperado.jsonl de codigo E0001/E0101: 'forja check' sai com 1 e o JSON bate em
    codigo/arquivo(basename)/linha/coluna; 'forja ast' devolve o mesmo diagnostico.
  - todos os demais (ok_* e erros semanticos E02xx+): a SINTAXE esta certa, ou seja 'forja ast' (que nao roda a
    semantica) sai com 0 e imprime JSON valido. 'forja check' agora tambem roda a semantica: quem a confere e
    tests/semantica/verificar_semantica.py.

Versao: 1.3.0
Changelog: 1.3.0 - IA1-L1: E0110 (forma de outra linguagem) tambem e codigo de sintaxe: check sai com 1 e o ast devolve o mesmo diagnostico.
           1.2.0 - IA1-b15: o parser segue depois do 1o erro (varios por execucao): vale a 1a linha do check/ast; linhas seguintes sao aceitas.
           1.1.0 - Fase 5c: sintaxe OK = 'forja ast' (sem semantica); 'forja check' deixou de ser so sintaxe.
           1.0.0 - Fase 5b: primeira versao (dourado + fixtures desta pasta, em paralelo, --selftest).
Verificacao: python verificar_sintaxe.py --selftest   |   python verificar_sintaxe.py
"""

import argparse
import json
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
ForjaPadrao = PastaAqui.parent.parent / "compiler" / "forja.exe"
DouradoPadrao = PastaAqui.parent / "dourado"
CodigosDeSintaxe = ("E0001", "E0101", "E0110")
CamposComparados = ("codigo", "arquivo", "linha", "coluna")
TimeoutSegundos = 30


def LerEsperado(CaminhoFj):
    """Primeira linha de <nome>.esperado.jsonl como dict, ou None se o caso nao tem esperado."""
    Esperado = CaminhoFj.with_suffix(".esperado.jsonl")
    if not Esperado.exists():
        return None
    Linhas = [L for L in Esperado.read_text(encoding="utf-8").splitlines() if L.strip()]
    return json.loads(Linhas[0]) if Linhas else None


def CompararDiagnostico(Obtido, Esperado):
    """Lista de diferencas entre o diagnostico obtido e o esperado (vazia = bateu). Arquivo: so o nome-base."""
    Diferencas = []
    for Campo in CamposComparados:
        ValorObtido = Obtido.get(Campo)
        ValorEsperado = Esperado.get(Campo)
        if Campo == "arquivo":
            ValorObtido = Path(str(ValorObtido)).name
            ValorEsperado = Path(str(ValorEsperado)).name
        if ValorObtido != ValorEsperado:
            Diferencas.append(f"{Campo}: obtido {ValorObtido!r}, esperado {ValorEsperado!r}")
    return Diferencas


def RodarForja(Forja, Comando, CaminhoFj):
    Processo = subprocess.run([str(Forja), Comando, str(CaminhoFj)], capture_output=True, text=True,
                              encoding="utf-8", timeout=TimeoutSegundos)
    return Processo.returncode, Processo.stdout, Processo.stderr


def ConferirErroDeSintaxe(Forja, Comando, CaminhoFj, Esperado):
    Codigo, Saida, Erro = RodarForja(Forja, Comando, CaminhoFj)
    if Codigo != 1:
        return [f"forja {Comando}: exit {Codigo}, esperado 1 (stderr: {Erro.strip()[:120]})"]
    Linhas = [L for L in Saida.splitlines() if L.strip()]
    if len(Linhas) < 1:  # b15: o parser segue depois do 1o erro; o esperado e a 1a linha, as seguintes sao erros que antes ficavam ocultos
        return [f"forja {Comando}: {len(Linhas)} linhas no stdout, esperado 1 ou mais"]
    try:
        Obtido = json.loads(Linhas[0])
    except json.JSONDecodeError as Falha:
        return [f"forja {Comando}: stdout nao e JSON ({Falha})"]
    return [f"forja {Comando}: {D}" for D in CompararDiagnostico(Obtido, Esperado)]


def ConferirAstValida(Forja, CaminhoFj):
    Codigo, Saida, Erro = RodarForja(Forja, "ast", CaminhoFj)
    if Codigo != 0:
        return [f"forja ast: exit {Codigo}, esperado 0 -> {Saida.strip()[:200]} {Erro.strip()[:120]}"]
    try:
        Arvore = json.loads(Saida)
    except json.JSONDecodeError as Falha:
        return [f"forja ast: JSON invalido ({Falha})"]
    if not isinstance(Arvore.get("declaracoes"), list):
        return ["forja ast: falta a lista 'declaracoes'"]
    return []


def VerificarCaso(Forja, CaminhoFj):
    """Devolve (nome, [falhas]) de um .fj."""
    Esperado = LerEsperado(CaminhoFj)
    EhErroDeSintaxe = Esperado is not None and Esperado.get("codigo") in CodigosDeSintaxe
    if EhErroDeSintaxe:
        Falhas = ConferirErroDeSintaxe(Forja, "check", CaminhoFj, Esperado)
        Falhas += ConferirErroDeSintaxe(Forja, "ast", CaminhoFj, Esperado)
        return CaminhoFj.name, Falhas
    return CaminhoFj.name, ConferirAstValida(Forja, CaminhoFj)


def ListarCasos(*Pastas):
    return sorted(F for Pasta in Pastas for F in Pasta.glob("*.fj"))


def Verificar(Forja, Pastas):
    Casos = ListarCasos(*Pastas)
    with ThreadPoolExecutor() as Pool:
        Resultados = list(Pool.map(lambda F: VerificarCaso(Forja, F), Casos))
    Falhou = [(N, Fs) for N, Fs in Resultados if Fs]
    for Nome, Falhas in Falhou:
        print(f"FALHOU {Nome}")
        for Falha in Falhas:
            print(f"    {Falha}")
    Passou = len(Resultados) - len(Falhou)
    print(f"{Passou}/{len(Resultados)} casos de sintaxe")
    return Passou == len(Resultados) and len(Resultados) > 0


def Autoteste():
    Bom = {"codigo": "E0101", "arquivo": "C:/x/a.fj", "linha": 2, "coluna": 5}
    Mesmo = {"codigo": "E0101", "arquivo": "a.fj", "linha": 2, "coluna": 5}
    assert CompararDiagnostico(Bom, Mesmo) == []
    assert len(CompararDiagnostico({**Bom, "coluna": 6}, Mesmo)) == 1
    assert len(CompararDiagnostico({**Bom, "codigo": "E0001", "linha": 9}, Mesmo)) == 2
    assert LerEsperado(PastaAqui / "nao_existe.fj") is None
    print("autoteste OK: 4 verificacoes")
    return True


def Principal():
    Analisador = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    Analisador.add_argument("--forja", type=Path, default=ForjaPadrao, help="caminho do forja.exe")
    Analisador.add_argument("--dourado", type=Path, default=DouradoPadrao, help="pasta do corpus dourado")
    Analisador.add_argument("--selftest", action="store_true", help="testa as funcoes de comparacao e sai")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        return 0 if Autoteste() else 1
    if not Argumentos.forja.exists():
        print(f"forja.exe nao encontrado em {Argumentos.forja}; rode construir.bat", file=sys.stderr)
        return 2
    return 0 if Verificar(Argumentos.forja, [Argumentos.dourado, PastaAqui]) else 1


if __name__ == "__main__":
    sys.exit(Principal())
