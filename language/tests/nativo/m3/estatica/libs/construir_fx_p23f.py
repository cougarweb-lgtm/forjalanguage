# -*- coding: utf-8 -*-
"""Gera libfx_ctor.a (construtores: ctor_a.c), libfx_cb.a (callback com quadros de C: cb_a.c), libfx_snp.a (vsnprintf externo: snp_a.c) e libfx_leg.a (camada fina do legado: leg_a.c) nesta pasta e grava artefatos_p23f.sha256: as estaticas de teste do P23f e P23g (M3b).

Versao: 0.3.0
Changelog: 0.3.0 - M3b P23g: +libfx_leg.a (fixture fx_leg: printf, fprintf, snprintf, strcasecmp, hypotf, sincosf, popcount, stat64i32 e opendir do rt_win_legado.o).
           0.2.0 - M3b P23f: +libfx_snp.a (fixture fx_snp: o vsnprintf do rt_win_legado.o).
           0.1.0 - M3b P23f: primeira versao.
Depende de: gcc e ar no PATH. O ar vai com -D (sem hora nem dono), entao o sha256 se repete entre execucoes.
Verificacao: python -I construir_fx_p23f.py && python -I ..\\..\\..\\verificar_nativo.py --pasta m3/estatica
"""

import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
ArquivoHashes = "artefatos_p23f.sha256"
FlagsC = ["-O0", "-ffunction-sections", "-fdata-sections", "-c"]
Bibliotecas = {"libfx_ctor.a": ["ctor_a.c"], "libfx_cb.a": ["cb_a.c"], "libfx_snp.a": ["snp_a.c"], "libfx_leg.a": ["leg_a.c"]}


def Rodar(Comando):
    Resultado = subprocess.run(Comando, cwd=PastaAqui, capture_output=True, text=True)
    if Resultado.returncode != 0:
        sys.exit(f"falhou: {' '.join(Comando)}\n{Resultado.stderr}")


def Construir(Biblioteca, Fontes):
    """Compila cada fonte num .o temporario e junta tudo na biblioteca; devolve o sha256 dela."""
    with tempfile.TemporaryDirectory() as Temp:
        Objetos = []
        for Fonte in Fontes:
            Objeto = str(Path(Temp) / (Path(Fonte).stem + ".o"))
            Rodar(["gcc", *FlagsC, Fonte, "-o", Objeto])
            Objetos.append(Objeto)
        Destino = PastaAqui / Biblioteca
        Destino.unlink(missing_ok=True)
        Rodar(["ar", "rcsD", str(Destino), *Objetos])
    return hashlib.sha256((PastaAqui / Biblioteca).read_bytes()).hexdigest()


def Principal():
    Linhas = []
    for Biblioteca, Fontes in Bibliotecas.items():
        Hash = Construir(Biblioteca, Fontes)
        Linhas.append(f"{Hash}  {Biblioteca}")
        print(f"{Biblioteca}: {(PastaAqui / Biblioteca).stat().st_size} bytes, sha256 {Hash}")
    (PastaAqui / ArquivoHashes).write_text("\n".join(Linhas) + "\n", encoding="utf-8")


if __name__ == "__main__":
    Principal()
