#!/usr/bin/env python3
"""verificar_linhas.py - dump --debug-code linhas contra o fonte e prova cruzada de offsets (M6 P65b, D-M6-2/D-M6-13).

Versao: 0.1.0
Changelog: 0.1.0 - P65b: 6 fixtures de gdb\\*.fj (+ mods\\geo.fj), alvos windows e linux; .esperado em linhas\\<fixture>_<alvo>.esperado.
Uso: python verificar_linhas.py --forja <forja.exe> [--alvo windows|linux|ambos] [--fixture NOME] [--gravar]
  --gravar: regrava os .esperado (so se a conferencia contra o fonte passou). Sem ela, o dump normalizado tem de ser IGUAL ao .esperado.
Saida: 0 tudo PASSOU; 1 alguma conferencia FALHOU.
Conferencias por proc do usuario (oraculo = o fonte, nao o gerador):
  1. nomes e tipos: {parametros + locais} == o que o fonte declara (verificar_nomes.ORACULO); parametros = os do cabecalho; ids 0..n-1 sem buraco
     (nenhum id >= Decl.nlocais: um temporario do lowering teria id acima).
  2. linhas: todo comando do corpo (linha nao vazia e que nao e so "}") aparece no dump, e nenhuma linha do dump sai do proc; offsets dentro de [inicio, fim) e crescentes.
  3. prova cruzada: a PRIMEIRA instrucao da listagem (--debug-code asm) que cita [rbp+off] do local (off do dump) cai na linha da declaracao `let|var nome`
     (pela tabela de linhas do proprio dump); a de parametro cai no prologo (antes da 1a linha da tabela).
"""
import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from verificar_nomes import ORACULO

RAIZ = Path(__file__).resolve().parent
FIXTURES = RAIZ / "gdb"
ESPERADOS = RAIZ / "linhas"
REGEX_PROC = re.compile(r"^\s*(?:pub\s+)?proc\s+(\w+)\s*\(([^)]*)\)")
REGEX_REF_RBP = re.compile(r"\[rbp([+-]\d+)?\]")


def FontesDaFixture(Nome: str) -> list:
    return [FIXTURES / f"{Nome}.fj"] + sorted((FIXTURES / "mods").glob("*.fj"))


def AcharProcNoFonte(Nome: str, Sufixo: str):
    """(arquivo, linha_inicial, linha_final, linhas_do_arquivo, nomes_dos_parametros) do proc `Sufixo` (1-based); None se ausente."""
    for Arquivo in FontesDaFixture(Nome):
        Linhas = Arquivo.read_text(encoding="utf-8").splitlines()
        for Indice, Linha in enumerate(Linhas):
            Achou = REGEX_PROC.match(Linha)
            if Achou and Achou[1] == Sufixo:
                Parametros = [P.split(":")[0].strip() for P in Achou[2].split(",") if P.strip()]
                return Arquivo, Indice + 1, FimDoBloco(Linhas, Indice) + 1, Linhas, Parametros
    return None


def FimDoBloco(Linhas: list, Inicio: int) -> int:
    """Indice (0-based) da linha que fecha o bloco aberto na linha Inicio (contagem de chaves; as fixtures nao tem chave em texto)."""
    Fundo = 0
    for Indice in range(Inicio, len(Linhas)):
        Fundo += Linhas[Indice].count("{") - Linhas[Indice].count("}")
        if Fundo == 0:
            return Indice
    return len(Linhas) - 1


def RodarBuild(Forja: Path, Fonte: Path, Alvo: str, Pasta: Path) -> list:
    """Build com --debug-code asm,linhas,json; devolve os registros do jsonl."""
    Exe = Pasta / f"{Fonte.stem}_{Alvo}.exe"
    Comando = [Forja.as_posix(), "build"] + (["--target", "x86_64-linux"] if Alvo == "linux" else [])
    Comando += [Fonte.as_posix(), "-o", Exe.as_posix(), "--debug-code", "asm,linhas,json"]
    Saida = subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
    Jsonl = Path(str(Exe) + ".debug.jsonl")
    if not Jsonl.exists():
        raise RuntimeError(f"sem .debug.jsonl: {(Saida.stdout + Saida.stderr)[-300:]}")
    return [json.loads(L) for L in Jsonl.read_text(encoding="utf-8").splitlines() if L.strip()]


def ProcsDoDump(Registros: list) -> dict:
    return {R["simbolo"]: R for R in Registros if R["tipo"] == "linhas"}


def AcharSimbolo(Procs: dict, Sufixo: str):
    Achados = [S for S in Procs if S == "fj_" + Sufixo or S.endswith("_" + Sufixo)]
    return Procs[Achados[0]] if len(Achados) == 1 else None


def ConferirNomes(Sufixo: str, Dump: dict, Fonte: tuple, Esperado: dict) -> list:
    Erros = []
    Variaveis = Dump["parametros"] + Dump["locais"]
    Tipos = {V["nome"]: V["tipo"] for V in Variaveis}
    if Tipos != Esperado:
        Erros.append(f"{Sufixo}: {{nome: tipo}} do dump {Tipos} != fonte {Esperado}")
    if [P["nome"] for P in Dump["parametros"]] != Fonte[4]:
        Erros.append(f"{Sufixo}: parametros {[P['nome'] for P in Dump['parametros']]} != cabecalho {Fonte[4]}")
    if sorted(V["id"] for V in Variaveis) != list(range(len(Variaveis))):
        Erros.append(f"{Sufixo}: ids {sorted(V['id'] for V in Variaveis)} nao sao 0..{len(Variaveis) - 1}")
    return Erros


def LinhasDeComando(Fonte: tuple) -> set:
    """Linhas (1-based) do corpo do proc que sao comandos: nao vazias e nao so '}'."""
    _, Inicio, Fim, Linhas, _ = Fonte
    return {N for N in range(Inicio + 1, Fim) if Linhas[N - 1].strip() not in ("", "}")}


def ConferirLinhas(Sufixo: str, Dump: dict, Fonte: tuple) -> list:
    Erros = []
    Arquivo, Inicio, Fim = Fonte[0], Fonte[1], Fonte[2]
    Vistas = {L["linha"] for L in Dump["linhas"]}
    if Path(Dump["arquivo"]).name != Arquivo.name or any(Path(L["arquivo"]).name != Arquivo.name for L in Dump["linhas"]):
        Erros.append(f"{Sufixo}: arquivo {Dump['arquivo']} != {Arquivo.name}")
    if not LinhasDeComando(Fonte) <= Vistas:
        Erros.append(f"{Sufixo}: faltam linhas do fonte {sorted(LinhasDeComando(Fonte) - Vistas)}")
    if any(not Inicio <= N <= Fim for N in Vistas):
        Erros.append(f"{Sufixo}: linha do dump {sorted(Vistas)} fora de [{Inicio},{Fim}]")
    Offsets = [L["off"] for L in Dump["linhas"]]
    if Offsets != sorted(Offsets) or any(not Dump["inicio"] <= O < Dump["fim"] for O in Offsets):
        Erros.append(f"{Sufixo}: offsets fora de [inicio,fim) ou nao crescentes {Offsets}")
    return Erros


def InstrsDoProc(Registros: list, Dump: dict) -> list:
    """[(offset em .text do programa, asm)] das instrucoes do proc, na ordem."""
    Itens = [R for R in Registros if R["tipo"] == "instr" and R["funcao"] == Dump["simbolo"]]
    Base = Itens[0]["rva"]
    return [(Dump["inicio"] + R["rva"] - Base, R["asm"]) for R in Itens]


def LinhaDoOffset(Dump: dict, Off: int):
    """Linha do fonte da tabela do dump para a instrucao em Off (None = antes da 1a entrada, o prologo)."""
    Linha = None
    for Entrada in Dump["linhas"]:
        if Entrada["off"] <= Off:
            Linha = Entrada["linha"]
    return Linha


def PrimeiraReferencia(Instrs: list, Rbp: int):
    for Off, Asm in Instrs:
        for Achado in REGEX_REF_RBP.finditer(Asm):
            if int(Achado[1] or 0) == Rbp:
                return Off, Asm
    return None


def LinhaDeclaracao(Fonte: tuple, Nome: str):
    _, Inicio, Fim, Linhas, _ = Fonte
    Regex = re.compile(rf"^\s*(?:let|var)\s+{re.escape(Nome)}\b")
    for N in range(Inicio + 1, Fim):
        if Regex.match(Linhas[N - 1]):
            return N
    return None


def ConferirCruzada(Sufixo: str, Dump: dict, Fonte: tuple, Instrs: list) -> list:
    Erros = []
    for V, EhParam in [(V, True) for V in Dump["parametros"]] + [(V, False) for V in Dump["locais"]]:
        Primeira = PrimeiraReferencia(Instrs, V["rbp"])
        if Primeira is None:
            Erros.append(f"{Sufixo}.{V['nome']}: nenhuma instrucao cita [rbp{V['rbp']:+d}]")
            continue
        Obtida = LinhaDoOffset(Dump, Primeira[0])
        Esperada = None if EhParam else LinhaDeclaracao(Fonte, V["nome"])
        if Obtida != Esperada:
            Erros.append(f"{Sufixo}.{V['nome']}: 1a referencia a [rbp{V['rbp']:+d}] ('{Primeira[1]}') na linha {Obtida}, esperado {Esperada}")
    return Erros


def Normalizar(Procs: list) -> str:
    """Texto do .esperado: por proc, as linhas distintas do fonte, parametros e locais com tipo e rbp (sem offsets de codigo)."""
    Saida = []
    for Dump in Procs:
        Saida.append(f"proc {Dump['funcao']} {Path(Dump['arquivo']).name}:{Dump['linha']}")
        Saida.append("  linhas " + " ".join(str(N) for N in sorted({L["linha"] for L in Dump["linhas"]})))
        for Tipo, Chave in (("param", "parametros"), ("local", "locais")):
            for V in Dump[Chave]:
                Saida.append(f"  {Tipo} {V['nome']} {V['tipo']} rbp{V['rbp']:+d}{' indireto' if V['indireto'] else ''}")
    return "\n".join(Saida) + "\n"


def VerificarFixture(Forja: Path, Nome: str, Alvo: str, Pasta: Path) -> tuple:
    """(erros, texto normalizado)."""
    Registros = RodarBuild(Forja, FIXTURES / f"{Nome}.fj", Alvo, Pasta)
    Procs = ProcsDoDump(Registros)
    Erros, Usados = [], []
    for Sufixo, Esperado in ORACULO[Nome].items():
        Dump = AcharSimbolo(Procs, Sufixo)
        Fonte = AcharProcNoFonte(Nome, Sufixo)
        if Dump is None or Fonte is None:
            Erros.append(f"proc '{Sufixo}' ausente (dump={Dump is not None}, fonte={Fonte is not None})")
            continue
        Usados.append(Dump)
        Erros += ConferirNomes(Sufixo, Dump, Fonte, Esperado) + ConferirLinhas(Sufixo, Dump, Fonte)
        Erros += ConferirCruzada(Sufixo, Dump, Fonte, InstrsDoProc(Registros, Dump))
    if len(Procs) != len(ORACULO[Nome]):
        Erros.append(f"o dump tem {len(Procs)} procs {sorted(Procs)}; o oraculo {len(ORACULO[Nome])}")
    return Erros, Normalizar(Usados)


def ConferirEsperado(Caminho: Path, Texto: str, Gravar: bool) -> list:
    if Gravar:
        Caminho.parent.mkdir(exist_ok=True)
        Caminho.write_text(Texto, encoding="utf-8", newline="\n")
        return []
    if not Caminho.exists():
        return [f"falta {Caminho.name} (rode com --gravar)"]
    Gravado = Caminho.read_text(encoding="utf-8")
    return [] if Gravado == Texto else [f"{Caminho.name} difere do dump:\n--- esperado\n{Gravado}--- obtido\n{Texto}"]


def Principal() -> int:
    Pa = argparse.ArgumentParser()
    Pa.add_argument("--forja", required=True)
    Pa.add_argument("--alvo", default="ambos", choices=["windows", "linux", "ambos"])
    Pa.add_argument("--fixture")
    Pa.add_argument("--gravar", action="store_true")
    Args = Pa.parse_args()
    Forja = Path(Args.forja).resolve()
    Alvos = ["windows", "linux"] if Args.alvo == "ambos" else [Args.alvo]
    Nomes = [Args.fixture] if Args.fixture else list(ORACULO)
    Falhou = False
    with tempfile.TemporaryDirectory() as Temporaria:
        Pasta = Path(Temporaria)  # lint-ok: iaf-nome-generico mesmo nome do verificar_gdb.py (pasta de saida do teste)
        for Alvo in Alvos:
            for Nome in Nomes:
                Erros, Texto = VerificarFixture(Forja, Nome, Alvo, Pasta)
                if not Erros:
                    Erros = ConferirEsperado(ESPERADOS / f"{Nome}_{Alvo}.esperado", Texto, Args.gravar)
                print(f"{'FALHOU' if Erros else 'PASSOU'} {Alvo} {Nome} {'; '.join(Erros)}")
                Falhou = Falhou or bool(Erros)
    return 1 if Falhou else 0


if __name__ == "__main__":
    sys.exit(Principal())
