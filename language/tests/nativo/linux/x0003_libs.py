#!/usr/bin/env python3
"""x0003_libs.py - lista os casos do conjunto Linux que o alvo x86_64-linux recusa com X0003 (foreign com link/dll) e a lib de cada um.

Objetivo: P32(e) do contrato_m5_resto - entrada do P40 (libs que o alpine-build precisa). Usa o MESMO conjunto do
verificar_linux (conjunto_linux.TodoOConjunto) e so recompila as fontes que citam `foreign`, porque X0003 so nasce ali.
Uso: python -I x0003_libs.py <forja_copia.exe> <saida.json>
     python -I x0003_libs.py --selftest
Versao: 0.1.0
Changelog:
  0.1.0 (14/11/2026) P32e: recompila cada fonte do conjunto com `forja build --target x86_64-linux` e colhe X0003 + link "lib".
Depende de: conjunto_linux.py (tests\\nativo\\linux) e um forja.exe (use copia fiel, nunca o de compiler\\ direto).
"""
import concurrent.futures
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

PADRAO_LINK = re.compile(r'\blink\s+"([^"]+)"')
PADRAO_X0003 = re.compile(r"\bX0003\b")
TRABALHADORES = 2


def LibsDoFonte(Texto: str) -> list:
    """Nomes de lib de cada `link "..."` do fonte, sem repeticao e na ordem em que aparecem."""
    Vistos = []
    for Nome in PADRAO_LINK.findall(Texto):
        if Nome not in Vistos:
            Vistos.append(Nome)
    return Vistos


def ConstruirLinux(Forja: Path, Fonte: Path, Saida: Path) -> tuple:
    """(codigo X0003 presente, trecho da mensagem) do build linux de uma fonte."""
    Proc = subprocess.run([str(Forja), "build", "--target", "x86_64-linux", str(Fonte), "-o", str(Saida)],
                          capture_output=True, text=True, timeout=300)
    SaidaTexto = Proc.stdout + Proc.stderr
    Achou = PADRAO_X0003.search(SaidaTexto)
    Trecho = SaidaTexto.strip().splitlines()[0][:200] if SaidaTexto.strip() else ""
    return bool(Achou), Trecho


def Candidatos() -> list:
    """Fontes unicas do conjunto que citam foreign (X0003 so nasce de foreign)."""
    import conjunto_linux
    Vistas = {}
    for Caso in conjunto_linux.TodoOConjunto():
        Vistas.setdefault(str(Caso.Fonte), Caso.Fonte)
    return [F for F in Vistas.values() if "foreign" in F.read_text(encoding="utf-8", errors="replace")]


def Varrer(Forja: Path) -> dict:
    """Recompila os candidatos e devolve o registro dos que recebem X0003 e suas libs."""
    Pasta = Path(tempfile.mkdtemp(prefix="x0003_"))
    Fontes = Candidatos()

    def Um(Par: tuple) -> dict:
        Indice, Fonte = Par
        Recusou, Trecho = ConstruirLinux(Forja, Fonte, Pasta / f"{Indice}.elf")
        Texto = Fonte.read_text(encoding="utf-8", errors="replace")
        return {"fonte": str(Fonte), "recusado": Recusou, "mensagem": Trecho, "libs": LibsDoFonte(Texto)}

    with concurrent.futures.ThreadPoolExecutor(max_workers=TRABALHADORES) as Piscina:
        Registros = list(Piscina.map(Um, enumerate(Fontes)))
    Recusados = [R for R in Registros if R["recusado"]]
    return {"candidatos": len(Fontes), "x0003": len(Recusados), "casos": Recusados}


def SelfTest() -> int:
    Exemplo = 'foreign {\n  link "kernel32"\n  link "c"\n  link "kernel32"\n}\n'
    if LibsDoFonte(Exemplo) != ["kernel32", "c"]:
        print("FALHOU selftest: extracao de link")
        return 1
    print("PASSOU selftest x0003_libs")
    return 0


def Principal(Argumentos: list) -> int:
    if Argumentos == ["--selftest"]:
        return SelfTest()
    if len(Argumentos) != 2:
        print("uso: python -I x0003_libs.py <forja_copia.exe> <saida.json>")
        return 2
    Relatorio = Varrer(Path(Argumentos[0]).resolve())
    Path(Argumentos[1]).write_text(json.dumps(Relatorio, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"candidatos={Relatorio['candidatos']} x0003={Relatorio['x0003']} saida={Argumentos[1]}")
    return 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
