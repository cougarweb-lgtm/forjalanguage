# -*- coding: utf-8 -*-
"""construir_lib.py - c_callback (P19i-a): gera cback.dll (gcc do PATH) e regrava artefatos.sha256.

Versao: 0.1.1
Changelog: 0.1.1 - 0.21.0 F3: padrao x64 na docstring de verificacao (--backend c trocado por x64).
           0.1.0 -M3 P19i-a: primeira versao.
Verificacao: python -I construir_lib.py && python -I ..\\..\\verificar_interop.py --forja <copia> --backend x64 --pasta dinamica --caso c_callback
"""

import sys
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
sys.path.insert(0, str(PastaAqui.parent.parent))
from construir_comum import GravarHashes, Rodar  # noqa: E402

if __name__ == "__main__":
    Rodar(["gcc", "-O1", "-shared", "-o", "cback.dll", "cback.c"], PastaAqui)
    GravarHashes(PastaAqui, ["cback.dll"])
