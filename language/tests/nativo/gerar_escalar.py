# -*- coding: utf-8 -*-
"""Gera um .fj de ~20 mil linhas so com o escopo do M1 e confere que compila com o backend x64.

O arquivo vai para %TEMP%\\gerar_escalar_<pid>\\escalar_20k.fj (usado pelo medir_nativo como
corpus de BIR% e vazao). Blocos: proc com int (for, if), i32 (while), uint/bool (comparacoes)
e conversao checada (as). Nada de divisao, resto, bits, str, struct ou array.

Uso:
    python -I gerar_escalar.py [--linhas N] [--forja <exe>] [--so-gerar]

Versao: 1.0.2
Changelog: 1.0.2 - 0.21.0 F3: build sem --backend c (padrao x64; backend C aposentado).
           1.0.1 - M4 P25b: o build leva --backend c explicito (o padrao do dev passou a ser x64).
           1.0.0 - primeira versao (P8-T do M1).
Verificacao: python -I gerar_escalar.py
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaLanguage = PastaAqui.parents[1]
ArquivoForjaOrigem = PastaLanguage / "compiler" / "forja.exe"
LinhasPadrao = 20000
TentativasCopia = 5
EsperaCopiaSegundos = 30
TiposDeBloco = 4


def GerarBloco(Indice):
    Tipo = Indice % TiposDeBloco
    if Tipo == 0:
        return (
            f"proc soma_int_{Indice}(a: int, b: int) -> int {{\n"
            "    var acc: int = a\n"
            "    for i in 0..<4 {\n"
            "        if acc > b {\n"
            "            acc = acc - i\n"
            "        } else {\n"
            "            acc = acc + i * 2\n"
            "        }\n"
            "    }\n"
            "    return acc + b\n"
            "}\n"
            "\n"
        )
    if Tipo == 1:
        return (
            f"proc soma_i32_{Indice}(a: i32, b: i32) -> i32 {{\n"
            "    var n: i32 = 0\n"
            "    var acc: i32 = a\n"
            "    while n < 5 {\n"
            "        n += 1\n"
            "        acc = acc + b * 2 - n\n"
            "    }\n"
            "    return acc\n"
            "}\n"
            "\n"
        )
    if Tipo == 2:
        return (
            f"proc compara_uint_{Indice}(a: uint, b: uint) -> bool {{\n"
            "    let t: uint = a + b\n"
            "    return t >= b and not (a == b)\n"
            "}\n"
            "\n"
        )
    return (
        f"proc conv_i32_{Indice}(v: int) -> i32 {{\n"
        "    let w: i32 = (v as i32) + 1\n"
        "    return w\n"
        "}\n"
        "\n"
    )


def GerarMain():
    Linhas = [
        "proc main() {",
        "    print(soma_int_0(1, b = 2))",
        "    print(soma_i32_1(3, b = 4))",
        "    print(compara_uint_2(5, b = 6))",
        "    print(conv_i32_3(7))",
        "}",
    ]
    return "\n".join(Linhas) + "\n"


def GerarArquivo(Destino, LinhasAlvo):
    Partes = ["// escalar_20k.fj - gerado por gerar_escalar.py; so escopo do M1.\n\n"]
    Linhas = Partes[0].count("\n")
    Indice = 0
    while Linhas < LinhasAlvo:
        Bloco = GerarBloco(Indice)
        Partes.append(Bloco)
        Linhas += Bloco.count("\n")
        Indice += 1
    Partes.append(GerarMain())
    Destino.write_text("".join(Partes), encoding="utf-8", newline="\n")
    return Indice


def CopiarForja(PastaTrabalho):
    Destino = Path(PastaTrabalho) / "forja.exe"
    for Tentativa in range(1, TentativasCopia + 1):
        try:
            shutil.copy2(ArquivoForjaOrigem, Destino)
            return str(Destino)
        except OSError as Erro:
            if Tentativa == TentativasCopia:
                raise SystemExit(f"X0001 nao copiei o forja.exe apos {TentativasCopia} tentativas: {Erro}")
            time.sleep(EsperaCopiaSegundos)
    return str(Destino)


def Principal():
    Parser = argparse.ArgumentParser(description="Gera .fj de escopo M1 com ~20 mil linhas.")
    Parser.add_argument("--linhas", type=int, default=LinhasPadrao)
    Parser.add_argument("--forja", default=None, help="exe da Forja (padrao: copia em TEMP)")
    Parser.add_argument("--so-gerar", action="store_true", help="so gera o arquivo, sem compilar")
    Args = Parser.parse_args()

    PastaTrabalho = Path(tempfile.mkdtemp(prefix="gerar_escalar_"))
    Arquivo = PastaTrabalho / "escalar_20k.fj"
    try:
        Blocos = GerarArquivo(Arquivo, Args.linhas)
        Linhas = Arquivo.read_bytes().count(b"\n")
        print(f"gerado: {Arquivo} ({Linhas} linhas, {Blocos} blocos)")
        if Args.so_gerar:
            return 0
        Forja = Args.forja or CopiarForja(PastaTrabalho)
        Inicio = time.perf_counter()
        Resultado = subprocess.run(
            [Forja, "build", "--modo", "exe", str(Arquivo), "-o", str(PastaTrabalho / "escalar_20k.exe")],
            capture_output=True, cwd=str(PastaTrabalho),
        )
        Tempo = (time.perf_counter() - Inicio) * 1000.0
        if Resultado.returncode != 0:
            print(f"build x64 recusou o arquivo (exit {Resultado.returncode}):")
            print(Resultado.stderr.decode("utf-8", "replace")[:800])
            return 1
        print(f"build --modo exe com backend x64: OK em {Tempo:.1f} ms")
        return 0
    finally:
        if not Args.so_gerar:
            shutil.rmtree(PastaTrabalho, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(Principal())
