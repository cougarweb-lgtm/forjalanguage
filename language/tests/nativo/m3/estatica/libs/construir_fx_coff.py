# -*- coding: utf-8 -*-
"""Gera libfx_coff.a (gcc/g++ + ar) nesta pasta e regrava artefatos.sha256: a estatica de teste do P23e (M3b) com COMDAT em dois membros (C++ inline e selectany, com o .pdata/.xdata
associado), WEAK_EXTERNAL (fraco com e sem definicao forte em outro membro), simbolo COMUM (-fcommon) em dois membros e -ffunction-sections (secoes ".text$nome").

Versao: 0.1.0
Changelog: 0.1.0 - M3b P23e: primeira versao.
Depende de: gcc, g++ e ar no PATH. O ar vai com -D (sem hora nem dono), entao o sha256 se repete entre execucoes.
Verificacao: python -I construir_fx_coff.py && python -I ..\\..\\verificar_nativo.py --pasta m3/estatica
"""

import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
Biblioteca = "libfx_coff.a"
ArquivoHashes = "artefatos.sha256"
FlagsComuns = ["-O0", "-ffunction-sections", "-fdata-sections", "-fcommon", "-c"]
FlagsCpp = ["-O0", "-fno-exceptions", "-ffunction-sections", "-fdata-sections", "-c"]
Membros = ["coff_a.cpp", "coff_b.cpp", "coff_c.c", "coff_d.c", "coff_e.c", "coff_f.c"]


def Rodar(Comando):
    Resultado = subprocess.run(Comando, cwd=PastaAqui, capture_output=True, text=True)
    if Resultado.returncode != 0:
        sys.exit(f"falhou: {' '.join(Comando)}\n{Resultado.stderr}")


def Compilar(Fonte, Pasta):
    Objeto = Pasta / (Path(Fonte).stem + ".o")
    if Fonte.endswith(".cpp"):
        Rodar(["g++", *FlagsCpp, Fonte, "-o", str(Objeto)])
    else:
        Rodar(["gcc", *FlagsComuns, Fonte, "-o", str(Objeto)])
    return str(Objeto)


def Principal():
    with tempfile.TemporaryDirectory() as Temp:
        Objetos = [Compilar(Fonte, Path(Temp)) for Fonte in Membros]
        Destino = PastaAqui / Biblioteca
        Destino.unlink(missing_ok=True)
        Rodar(["ar", "rcsD", str(Destino), *Objetos])
    Hash = hashlib.sha256((PastaAqui / Biblioteca).read_bytes()).hexdigest()
    (PastaAqui / ArquivoHashes).write_text(f"{Hash}  {Biblioteca}\n", encoding="utf-8")
    print(f"{Biblioteca}: {(PastaAqui / Biblioteca).stat().st_size} bytes, sha256 {Hash}")


if __name__ == "__main__":
    Principal()
