# -*- coding: utf-8 -*-
"""construir_abi.py - gera as DLLs da bateria de ABI (abi_lib_<s>.dll) com o gcc e regrava artefatos.sha256.

So para quem REGENERA (a suite nunca chama o gcc): para cada semente, gera o abi_lib_<s>.c pelo gerar_abi.py, compila com 'gcc -O1 -shared' e grava duas linhas:
o sha256 da DLL (artefato versionado) e o sha256 do TEXTO do .c gerado (o verificar_abi regenera o .c e confere: prova que a DLL vem deste gerador).

Uso: python -I construir_abi.py [--sementes 1,2,3] [--quantidade 200]
Lado export (M3 P22b): tambem compila abi_exp_<s>.exe (o programa C que chama a DLL da Forja); --so-export mexe so nessas linhas e deixa as das DLLs intactas.

Versao: 0.3.0
Changelog: 0.3.0 - M3 P22b: ConstruirExport e --so-export (3 exe e 6 linhas novas no artefatos.sha256).
           0.2.0 - M3 P21v-c: tambem as DLLs da variante indireta (abi_ind_<s>.dll, structs sem array; 6 DLLs e 12 linhas no artefatos.sha256).
           0.1.0 - M3 P20: primeira versao.
Verificacao: python -I construir_abi.py && python -I verificar_abi.py --forja <copia> --sementes 1,2,3
"""

import argparse
import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
sys.path.insert(0, str(PastaAqui))
import gerar_abi  # noqa: E402  (o -I tira a pasta do script do sys.path)

ArquivoHashes = "artefatos.sha256"
SementesPadrao = "1,2,3"


def Sha256(Bytes):
    return hashlib.sha256(Bytes).hexdigest()


def ConstruirSemente(Semente, Quantidade, Indireto):
    """Compila a DLL da semente (abi_lib_<s> ou, na variante indireta, abi_ind_<s>) em PastaAqui; devolve as duas linhas de hash (DLL e texto do .c)."""
    Nome = gerar_abi.NomeDaLib(Semente, Indireto)
    TextoC, _ = gerar_abi.Gerar(Semente, Quantidade, Indireto)
    with tempfile.TemporaryDirectory(prefix="construir_abi_") as Temp:
        Fonte = Path(Temp) / f"{Nome}.c"
        Fonte.write_text(TextoC, encoding="utf-8", newline="\n")
        Comando = ["gcc", "-O1", "-shared", "-o", str(PastaAqui / f"{Nome}.dll"), str(Fonte)]
        if subprocess.run(Comando).returncode != 0:
            raise SystemExit(f"falhou: {' '.join(Comando)}")
    Dll = (PastaAqui / f"{Nome}.dll").read_bytes()
    return [f"{Sha256(Dll)}  {Nome}.dll", f"{Sha256(TextoC.encode('utf-8'))}  {Nome}.c({Quantidade})"]


def ConstruirExport(Semente, Quantidade):
    """Compila o programa C do lado export (abi_exp_<s>.exe, gcc -O1) em PastaAqui; devolve as duas linhas de hash (exe e texto do .c). O gcc nao e reprodutivel: o sha256 do exe
    e do artefato versionado, nao de uma regeneracao."""
    Nome = gerar_abi.NomeDoExport(Semente)
    TextoC, _ = gerar_abi.GerarExport(Semente, Quantidade)
    with tempfile.TemporaryDirectory(prefix="construir_abi_") as Temp:
        Fonte = Path(Temp) / f"{Nome}.c"
        Fonte.write_text(TextoC, encoding="utf-8", newline="\n")
        Comando = ["gcc", "-O1", "-o", str(PastaAqui / f"{Nome}.exe"), str(Fonte)]
        if subprocess.run(Comando).returncode != 0:
            raise SystemExit(f"falhou: {' '.join(Comando)}")
    Exe = (PastaAqui / f"{Nome}.exe").read_bytes()
    return [f"{Sha256(Exe)}  {Nome}.exe", f"{Sha256(TextoC.encode('utf-8'))}  {Nome}.c({Quantidade})"]


def Principal():
    Analise = argparse.ArgumentParser()
    Analise.add_argument("--sementes", default=SementesPadrao)
    Analise.add_argument("--quantidade", type=int, default=200)
    Analise.add_argument("--so-export", action="store_true", help="so os programas do lado export; as linhas das DLLs ficam como estao")
    Opcoes = Analise.parse_args()
    Sementes = [int(S) for S in Opcoes.sementes.split(",")]
    Linhas = []
    if Opcoes.so_export:
        Linhas = [L for L in (PastaAqui / ArquivoHashes).read_text(encoding="utf-8").splitlines() if "  abi_exp_" not in L]
    else:
        for Semente in Sementes:
            Linhas += ConstruirSemente(Semente, Opcoes.quantidade, False)
        for Semente in Sementes:
            Linhas += ConstruirSemente(Semente, Opcoes.quantidade, True)
    for Semente in Sementes:
        Linhas += ConstruirExport(Semente, Opcoes.quantidade)
    (PastaAqui / ArquivoHashes).write_text("\n".join(Linhas) + "\n", encoding="utf-8", newline="\n")
    print("\n".join(Linhas))
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
