# -*- coding: utf-8 -*-
"""Gera fx_basica.dll (gcc), fx_lixo.dll (nasm + gcc -shared), fx_basica.dll.a (gendef + dlltool), fx_alinha.dll (gcc), fx_suja.dll (nasm), fx_chamador.dll (gcc), fx_lixo_entrada.dll (nasm), fx_vtbl.dll (gcc),
fx_preserva.exe, fx_realoca.exe, fx_host.exe, libfx_host.dll.a e fx_tipos.dll nesta pasta e regrava artefatos.sha256.

Versao: 0.7.0
Changelog: 0.7.0 - M3 P23: fx_tipos.dll (gcc -O1: handle opaco, enum com valor, struct aninhada por valor); --so-tipos refaz so ela (as outras levam carimbo de hora).
           0.6.0 - M3 P22b: fx_preserva.exe (fx_preserva.c + fx_preserva_stub.asm), fx_realoca.exe, fx_host.exe e libfx_host.dll.a (gcc/nasm; sem carimbo estavel, entram no artefatos.sha256); --so-dll refaz so eles.
           0.5.0 - M3 P21v-b: fx_vtbl.dll (gcc -O0, objeto com tabela de funcoes no estilo COM); --so-vtbl refaz so ela.
           0.4.0 -M3 P21: fx_chamador.dll (gcc -O0, callbacks) e fx_lixo_entrada.dll (nasm, callback chamado com lixo nos bits altos); --so-callback refaz so as duas.
           0.3.0 - M3 P20: fx_alinha.dll (gcc, alinhamento da pilha) e fx_suja.dll (nasm, suja os registradores volateis); --so-abi refaz so as duas (as outras DLLs levam carimbo de hora: reconstrui-las muda o sha256 sem motivo).
           0.2.0 - M3 P19b: fx_basica.dll.a (import library GNU de fx_basica.dll: gendef gera o .def da DLL ja pronta, dlltool gera o .dll.a); --so-implib refaz so ela, sem tocar nas DLLs
                   (a DLL leva carimbo de hora: reconstrui-la muda o sha256 sem motivo).
           0.1.0 (sem mudanca de codigo no P19c: so reconstruiu a fx_basica.dll, que ganhou fx_escreve) - M3 P19: primeira versao (gcc e nasm do PATH).
Verificacao: python -I construir_libs.py && python -I ..\\..\\verificar_nativo.py --pasta m3/foreign
"""

import hashlib
import subprocess
import sys
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
Artefatos = ("fx_basica.dll", "fx_lixo.dll", "fx_basica.dll.a", "fx_alinha.dll", "fx_suja.dll", "fx_chamador.dll", "fx_lixo_entrada.dll", "fx_vtbl.dll",
             "fx_preserva.exe", "fx_realoca.exe", "fx_host.exe", "libfx_host.dll.a", "fx_tipos.dll")
ArquivoHashes = "artefatos.sha256"


def Rodar(Comando):
    Resultado = subprocess.run(Comando, cwd=PastaAqui)
    if Resultado.returncode != 0:
        raise SystemExit(f"falhou: {' '.join(Comando)}")


def ConstruirDlls():
    Rodar(["gcc", "-O1", "-shared", "-o", "fx_basica.dll", "fx_basica.c"])
    Rodar(["nasm", "-f", "win64", "-o", "fx_lixo.obj", "fx_lixo.asm"])
    Rodar(["gcc", "-shared", "-nostdlib", "-o", "fx_lixo.dll", "fx_lixo.obj", "-Wl,--entry=0"])
    (PastaAqui / "fx_lixo.obj").unlink()


def ConstruirAbi():
    # -O0: com otimizacao o gcc tira o local da pilha e o MOVAPS que a bateria quer sumiria.
    Rodar(["gcc", "-O0", "-shared", "-o", "fx_alinha.dll", "fx_alinha.c"])
    Rodar(["nasm", "-f", "win64", "-o", "fx_suja.obj", "fx_suja.asm"])
    Rodar(["gcc", "-shared", "-nostdlib", "-o", "fx_suja.dll", "fx_suja.obj", "-Wl,--entry=0"])
    (PastaAqui / "fx_suja.obj").unlink()


def ConstruirCallback():
    # -O0: fx_chama_pilha mantem o quadro na pilha (sem chamada em cauda) para a prova do desenrolar de pilha.
    Rodar(["gcc", "-O0", "-shared", "-o", "fx_chamador.dll", "fx_chamador.c"])
    Rodar(["nasm", "-f", "win64", "-o", "fx_lixo_entrada.obj", "fx_lixo_entrada.asm"])
    Rodar(["gcc", "-shared", "-nostdlib", "-o", "fx_lixo_entrada.dll", "fx_lixo_entrada.obj", "-Wl,--entry=0"])
    (PastaAqui / "fx_lixo_entrada.obj").unlink()


def ConstruirVtbl():
    Rodar(["gcc", "-O0", "-shared", "-o", "fx_vtbl.dll", "fx_vtbl.c"])


def ConstruirTipos():
    Rodar(["gcc", "-O1", "-shared", "-o", "fx_tipos.dll", "fx_tipos.c"])


def ConstruirDll():
    """Os programas de teste da DLL nativa (P22b): fx_preserva.exe (C + stub NASM), fx_realoca.exe e fx_host.exe com a import library libfx_host.dll.a (o nome do modulo e fx_host.exe)."""
    Rodar(["nasm", "-f", "win64", "-o", "fx_preserva_stub.obj", "fx_preserva_stub.asm"])
    Rodar(["gcc", "-O1", "-o", "fx_preserva.exe", "fx_preserva.c", "fx_preserva_stub.obj"])
    (PastaAqui / "fx_preserva_stub.obj").unlink()
    Rodar(["gcc", "-O1", "-o", "fx_realoca.exe", "fx_realoca.c"])
    Rodar(["gcc", "-O1", "-o", "fx_host.exe", "fx_host.c", "-Wl,--out-implib,libfx_host.dll.a"])


def ConstruirImplib():
    """fx_basica.dll.a a partir da DLL que ja esta aqui: o .def sai da tabela de exportacao dela (gendef) e o dlltool monta a import library."""
    Rodar(["gendef", "fx_basica.dll"])
    Rodar(["dlltool", "-d", "fx_basica.def", "-D", "fx_basica.dll", "-l", "fx_basica.dll.a"])
    (PastaAqui / "fx_basica.def").unlink()


def GravarHashes():
    Linhas = [f"{hashlib.sha256((PastaAqui / Nome).read_bytes()).hexdigest()}  {Nome}\n" for Nome in Artefatos]
    (PastaAqui / ArquivoHashes).write_text("".join(Linhas), encoding="utf-8", newline="\n")
    print("".join(Linhas), end="")


if __name__ == "__main__":
    if "--so-abi" in sys.argv[1:]:
        ConstruirAbi()
    elif "--so-callback" in sys.argv[1:]:
        ConstruirCallback()
    elif "--so-vtbl" in sys.argv[1:]:
        ConstruirVtbl()
    elif "--so-dll" in sys.argv[1:]:
        ConstruirDll()
    elif "--so-tipos" in sys.argv[1:]:
        ConstruirTipos()
    else:
        if "--so-implib" not in sys.argv[1:]:
            ConstruirDlls()
            ConstruirAbi()
            ConstruirCallback()
            ConstruirVtbl()
            ConstruirDll()
            ConstruirTipos()
        ConstruirImplib()
    GravarHashes()
    sys.exit(0)
