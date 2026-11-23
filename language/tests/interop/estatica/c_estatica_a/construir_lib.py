# -*- coding: utf-8 -*-
"""construir_lib.py - c_estatica_a (P19i-b): compila est_mat.c e est_texto.c, empacota libest.a (ar, deterministico) e regrava artefatos.sha256.

Versao: 0.1.1
Changelog: 0.1.1 - 0.21.0 F3: padrao x64 na docstring de verificacao (--backend c trocado por x64).
           0.1.0 -M3 P19i-b: primeira versao (gcc e ar do PATH; 'D' no ar zera data/uid para o sha256 ser estavel).
Verificacao: python -I construir_lib.py && python -I ..\\..\\verificar_interop.py --forja <copia> --backend x64 --pasta estatica --caso c_estatica_a
"""

import sys
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
sys.path.insert(0, str(PastaAqui.parent.parent))
from construir_comum import GravarHashes, Rodar  # noqa: E402

if __name__ == "__main__":
    for Fonte in ("est_mat", "est_texto"):
        Rodar(["gcc", "-O1", "-c", f"{Fonte}.c", "-o", f"{Fonte}.o"], PastaAqui)
    Rodar(["ar", "rcsD", "libest.a", "est_mat.o", "est_texto.o"], PastaAqui)
    for Objeto in ("est_mat.o", "est_texto.o"):
        (PastaAqui / Objeto).unlink()
    GravarHashes(PastaAqui, ["libest.a"])
