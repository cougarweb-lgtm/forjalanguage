"""Prova de R2 do contrato A64: os bytes do x86-64 nao mudam.

Compila 01_ola + examples 02-04 + tests\\nativo\\m1 (todos os .fj da raiz) em
x86_64-windows e x86_64-linux, dev e release, e grava o sha256 de cada saida
(sem os bytes de versao do forja, via mascara_versao).

  --gravar            grava bytes_x64.json (linha de base)
  --conferir          recompila e compara com bytes_x64.json
  --forja <exe>       compilador usado (padrao language\\compiler\\forja.exe)

Saida de --conferir: `bytes_x64: N/N iguais` ; exit 0 so com 0 diferencas.
Versao 0.1.0 (22/11/2026) - A1.2 do contrato_a64.txt (secao 6.4).
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LANGUAGE = AQUI.parents[2]
FORJA_PADRAO = LANGUAGE / "compiler" / "forja.exe"
ARQUIVO_JSON = AQUI / "bytes_x64.json"
RAIZ_EXEMPLOS = LANGUAGE.parent / "examples"
EXEMPLOS = ["01_ola.fj", "02_resultado.fj", "03_unidades_e_faixa.fj", "04_defer_e_testes.fj"]
ALVOS_X64 = ["x86_64-windows", "x86_64-linux"]
MODOS = {"dev": [], "release": ["--release"]}

sys.path.insert(0, str(LANGUAGE / "tests" / "nativo" / "otim"))
import mascara_versao as Mascara  # noqa: E402


def ListarFontes() -> list:
    """(nome, caminho) dos fontes do conjunto: 01-04 de examples + todos os .fj da raiz de m1."""
    Fontes = [(Nome, RAIZ_EXEMPLOS / Nome) for Nome in EXEMPLOS]
    Fontes += [("m1/" + Caminho.name, Caminho) for Caminho in sorted((LANGUAGE / "tests" / "nativo" / "m1").glob("*.fj"))]
    return Fontes


def CompilarUm(Forja: Path, Fonte: Path, Alvo: str, Modo: str, Pasta: Path) -> str:
    """sha256 sem versao da saida, ou 'FALHA rc=N' se o build nao gerou arquivo."""
    Saida = Pasta / f"{Fonte.stem}_{Alvo}_{Modo}.bin"
    Comando = [str(Forja), "build", "--target", Alvo, *MODOS[Modo], str(Fonte), "-o", str(Saida)]
    Resultado = subprocess.run(Comando, capture_output=True, text=True)
    if Resultado.returncode != 0 or not Saida.is_file():
        return f"FALHA rc={Resultado.returncode}"
    return Mascara.HashSemVersao(Saida, Mascara.VersoesDoForja(Forja))


def ColetarHashes(Forja: Path) -> dict:
    """{chave: sha ou FALHA} de todo o conjunto x alvos x modos.

    Pasta FIXA: o PDB do PE dev grava o caminho da saida, e pasta com nome aleatorio daria
    divergencia falsa entre duas rodadas do mesmo compilador.
    """
    Pasta = Path(tempfile.gettempdir()) / "forja_a64" / "bytes_x64"
    shutil.rmtree(Pasta, ignore_errors=True)
    Pasta.mkdir(parents=True, exist_ok=True)
    try:
        Hashes = {}
        for Nome, Fonte in ListarFontes():
            for Alvo in ALVOS_X64:
                for Modo in MODOS:
                    Hashes[f"{Nome}|{Alvo}|{Modo}"] = CompilarUm(Forja, Fonte, Alvo, Modo, Pasta)
        return Hashes
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def Main() -> int:
    Parser = argparse.ArgumentParser(description="Prova de R2: bytes x64 identicos (A1.2).")
    Grupo = Parser.add_mutually_exclusive_group(required=True)
    Grupo.add_argument("--gravar", action="store_true")
    Grupo.add_argument("--conferir", action="store_true")
    Parser.add_argument("--forja", default=str(FORJA_PADRAO))
    Args = Parser.parse_args()
    Forja = Path(Args.forja).resolve()
    Atual = ColetarHashes(Forja)
    if Args.gravar:
        ARQUIVO_JSON.write_text(json.dumps(Atual, indent=1, ensure_ascii=False), encoding="utf-8")
        print(f"bytes_x64: gravados {len(Atual)} hashes em {ARQUIVO_JSON} (forja {Forja})")
        return 0
    Base = json.loads(ARQUIVO_JSON.read_text(encoding="utf-8"))
    Diferentes = [Chave for Chave in Base if Atual.get(Chave) != Base[Chave]]
    Iguais = len(Base) - len(Diferentes)
    for Chave in Diferentes:
        print(f"DIVERGE {Chave}: base={Base[Chave]} atual={Atual.get(Chave)}")
    print(f"bytes_x64: {Iguais}/{len(Base)} iguais")
    return 0 if not Diferentes and len(Base) == len(Atual) else 1


if __name__ == "__main__":
    sys.exit(Main())
