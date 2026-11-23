# -*- coding: utf-8 -*-
"""Apoio dos construir_lib.py da bateria tests\\interop: roda o compilador de C/C++ e grava artefatos.sha256 do caso.

Por que .py e nao .bat: o Write nao grava CRLF e o hook de .bat exige CRLF (decisao do P19i-a; mesmo caminho de nativo\\m3\\libs\\construir_libs.py).
Formato do artefatos.sha256: "# comentario" ou "<sha256 hex>  <nome>"; <nome> e relativo a pasta do caso, ou "vendor:<arquivo>"
(arquivo de language\\vendor, conferido e copiado pela bateria, nunca gerado aqui).

Versao: 0.1.0
Changelog: 0.1.0 - M3 P19i-a: primeira versao (Rodar, GravarHashes).
Verificacao: python -I dinamica\\c_dll\\construir_lib.py   (gcc no PATH; so para quem REGENERA os artefatos)
"""

import hashlib
import subprocess
from pathlib import Path

ArquivoHashes = "artefatos.sha256"
CabecalhoHashes = "# artefatos binarios do caso (regerados por construir_lib.py); a bateria confere antes de usar e nunca chama gcc\n"


def Rodar(Comando, Pasta):
    """Roda Comando em Pasta; falha com SystemExit (mensagem com o comando) se o exit != 0."""
    Resultado = subprocess.run(Comando, cwd=Pasta)
    if Resultado.returncode != 0:
        raise SystemExit(f"falhou: {' '.join(str(P) for P in Comando)}")


def Sha256(Caminho):
    return hashlib.sha256(Path(Caminho).read_bytes()).hexdigest()


def GravarHashes(Pasta, Nomes, Extras=()):
    """Grava Pasta\\artefatos.sha256 com os Nomes (arquivos da pasta) e as Extras (linhas ja prontas, ex.: vendor:)."""
    Pasta = Path(Pasta)
    Linhas = [f"{Sha256(Pasta / Nome)}  {Nome}\n" for Nome in Nomes] + list(Extras)
    (Pasta / ArquivoHashes).write_text(CabecalhoHashes + "".join(Linhas), encoding="utf-8", newline="\n")
    print("".join(Linhas), end="")
