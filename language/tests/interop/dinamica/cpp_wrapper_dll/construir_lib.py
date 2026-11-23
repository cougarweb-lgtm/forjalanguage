# -*- coding: utf-8 -*-
"""construir_lib.py - cpp_wrapper_dll (P19i-a): 'forja ligar --cpp' gera pilha.fj + pilha_c.cpp, g++ compila pilha.dll, regrava artefatos.sha256.

Precisa de libclang no Python do 'forja ligar' e de g++ no PATH; so para quem REGENERA (a bateria nunca roda isto).
Uso: python -I construir_lib.py [--forja <forja.exe>]  (padrao: %TEMP%\\forja_p19i\\forja.exe)

Versao: 0.1.1
Changelog: 0.1.1 - 0.21.0 F3: padrao x64 na docstring de verificacao (--backend c trocado por x64).
           0.1.0 -M3 P19i-a: primeira versao.
Verificacao: python -I construir_lib.py && python -I ..\\..\\verificar_interop.py --forja <copia> --backend x64 --pasta dinamica --caso cpp_wrapper_dll
"""

import argparse
import os
import sys
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
sys.path.insert(0, str(PastaAqui.parent.parent))
from construir_comum import GravarHashes, Rodar  # noqa: E402

ForjaPadrao = Path(os.environ.get("TEMP", ".")) / "forja_p19i" / "forja.exe"

if __name__ == "__main__":
    Analise = argparse.ArgumentParser()
    Analise.add_argument("--forja", default=str(ForjaPadrao))
    Forja = Analise.parse_args().forja
    Rodar([Forja, "ligar", "--cpp", "pilha.hpp", "--link", "pilha", "-o", "pilha.fj"], PastaAqui)
    Rodar(["g++", "-std=c++17", "-O2", "-shared", "-static-libgcc", "-static-libstdc++", "-I", ".", "-o", "pilha.dll",
           "pilha.cpp", "pilha_c.cpp"], PastaAqui)
    GravarHashes(PastaAqui, ["pilha.dll", "pilha.fj", "pilha_c.cpp"])
