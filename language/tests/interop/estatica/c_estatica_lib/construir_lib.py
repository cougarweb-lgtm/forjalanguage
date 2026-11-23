# -*- coding: utf-8 -*-
"""construir_lib.py - c_estatica_lib (P19i-b): compila el.c e empacota o archive GNU direto com o nome estlib.lib (nao e COFF de MSVC).

Versao: 0.1.1
Changelog: 0.1.1 - 0.21.0 F3: padrao x64 na docstring de verificacao (--backend c trocado por x64).
           0.1.0 -M3 P19i-b: primeira versao (ar rcsD deterministico).
Verificacao: python -I construir_lib.py && python -I ..\\..\\verificar_interop.py --forja <copia> --backend x64 --pasta estatica --caso c_estatica_lib
"""

import sys
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
sys.path.insert(0, str(PastaAqui.parent.parent))
from construir_comum import GravarHashes, Rodar  # noqa: E402

if __name__ == "__main__":
    Rodar(["gcc", "-O1", "-c", "el.c", "-o", "el.o"], PastaAqui)
    (PastaAqui / "estlib.lib").unlink(missing_ok=True)
    Rodar(["ar", "rcsD", "estlib.lib", "el.o"], PastaAqui)
    (PastaAqui / "el.o").unlink()
    GravarHashes(PastaAqui, ["estlib.lib"])
