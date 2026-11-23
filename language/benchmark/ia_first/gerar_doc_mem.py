# gerar_doc_mem.py -- gera os TEXTOS de MEM-LJ-SPEC, MEM-FORJA-GUIA-NUCLEO e da lista de codigos do MEM-FORJA-GUIA-05
"""Gera, a partir de language\\doc\\spec.txt, cartao.txt, mapa_spec.tsv e do `forja explicar`, os textos que o passo z1 grava no
claude_memory.db. NAO grava no banco de .mem nem em ~/.claude/rules: so escreve arquivos em %TEMP%\\ia1_doc_mem\\ (o main aplica no z1).
Uso:
    python -I gerar_doc_mem.py --forja <forja.exe> [--doc <pasta>] [--saida <pasta>]
    python -I gerar_doc_mem.py --selftest [--forja <forja.exe>]
Arquivos gerados (UTF-8, LF, sem BOM) e hashes.json (sha256 de cada um, ordem fixa):
    MEM-LJ-SPEC.txt              secao "## MEM-LJ-SPEC": spec.txt
    MEM-FORJA-GUIA-NUCLEO.txt    secao "## MEM-FORJA-GUIA-NUCLEO": cartao.txt (o livro de bolso)
    regra_nativa_forja.txt       a mesma regra com o cabecalho de regra nativa forja.md (paths **/*.fj)
    MEM-FORJA-GUIA-05.txt        secao "## MEM-FORJA-GUIA-05": uma linha por codigo do mapa_spec.tsv, texto de `forja explicar`
Deterministico: 2 execucoes com a mesma doc e o mesmo binario dao bytes iguais (ordem do mapa_spec.tsv, sem data nem caminho).
Versao: 0.1.0
Changelog: 0.1.0 - P-IA1-a04: primeira versao (spec, nucleo, regra nativa e lista de codigos; hashes.json; --selftest).
Verificacao: python -I gerar_doc_mem.py --selftest
"""

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

LANG = Path(r"C:\Users\Cougar\Documents\TEMPO\DESENVOLVIMENTO\FORJA\language")
DOC_PADRAO = LANG / "doc"
SAIDA_PADRAO = Path(tempfile.gettempdir()) / "ia1_doc_mem"
FORJA_PADRAO = Path(tempfile.gettempdir()) / "forja_b01_main" / "forja.exe"
TEMPO_FORJA = 60
WORKERS = 8
NOMES_SAIDA = ("MEM-LJ-SPEC.txt", "MEM-FORJA-GUIA-NUCLEO.txt", "regra_nativa_forja.txt", "MEM-FORJA-GUIA-05.txt")
TITULO_SPEC = "Forja v0: spec compacta (texto injetado; teto 8 KB)"
TITULO_NUCLEO = "Cartão curto: escrever Forja sem errar (carregado ao ler `.fj`)"
TITULO_CODIGOS = "Catálogo resumido de códigos (a fonte é `forja explicar` e `common/diag.c`)"
CABECALHO_REGRA = (
    '---\npaths:\n  - "**/*.fj"\n---\n'
    "<!-- GERADO por tools/exportar_regras_nativas.py - NAO EDITAR A MAO (edite a fonte no claude_memory.db e rode o exportador). "
    "Fontes: MEM-FORJA-GUIA-NUCLEO -->\n\n# Núcleo Forja: escrever .fj sem errar (CLAUDE.md §12)\n\n"
)


def LerTexto(Caminho):
    """Texto UTF-8 do arquivo com fim de linha LF; arquivo ausente encerra com mensagem."""
    if not Path(Caminho).is_file():
        raise SystemExit(f"arquivo nao existe: {Caminho}")
    return Path(Caminho).read_text(encoding="utf-8").replace("\r\n", "\n")


def ChamarForja(Forja, Argumentos):
    """Roda o forja sem teclado e com tempo limite; devolve o CompletedProcess (texto UTF-8)."""
    return subprocess.run([str(Forja), *Argumentos], capture_output=True, text=True, encoding="utf-8",
                          errors="replace", stdin=subprocess.DEVNULL, timeout=TEMPO_FORJA)


def VersaoDoForja(Forja):
    """Texto de `forja --versao` sem fim de linha (ex.: 'forja 0.19.0 (Linguagem v1.11)')."""
    Res = ChamarForja(Forja, ["--versao"])
    if Res.returncode != 0:
        raise SystemExit(f"forja --versao falhou (rc {Res.returncode}): {Res.stderr.strip()[:200]}")
    return Res.stdout.strip()


def ExplicarCodigo(Forja, Codigo):
    """Objeto JSON de `forja explicar CODIGO`, ou None quando o catalogo do binario nao conhece o codigo."""
    Res = ChamarForja(Forja, ["explicar", Codigo])
    if Res.returncode != 0 or not Res.stdout.strip():
        return None
    try:
        return json.loads(Res.stdout.strip().splitlines()[0])
    except json.JSONDecodeError:
        return None


def CodigosDoMapa(DocDir):
    """Codigos do mapa_spec.tsv na ordem do arquivo (cabecalho 'codigo<TAB>secao' pulado)."""
    Linhas = LerTexto(Path(DocDir) / "mapa_spec.tsv").splitlines()
    return [Linha.split("\t")[0] for Linha in Linhas[1:] if Linha.strip()]


def Secao(Id, Titulo, Corpo):
    """Formata uma secao de .mem: '## ID — titulo', corpo, separador '---'."""
    return f"## {Id} — {Titulo}\n\n{Corpo.rstrip()}\n\n---\n"


def MontarCodigos(Forja, DocDir):
    """Corpo do GUIA-05: uma linha por codigo do mapa, com tipo, explicacao e forma certa do `forja explicar`."""
    Codigos = CodigosDoMapa(DocDir)
    with ThreadPoolExecutor(max_workers=WORKERS) as Pool:
        Itens = list(Pool.map(lambda Cod: ExplicarCodigo(Forja, Cod), Codigos))
    Ausentes = [Cod for Cod, Item in zip(Codigos, Itens) if Item is None]
    if Ausentes:
        raise SystemExit(f"codigo(s) sem `forja explicar`: {' '.join(Ausentes)}")
    Linhas = [f"Gerado de `forja explicar` ({VersaoDoForja(Forja)}); {len(Codigos)} codigos; codigo -> secao do doc: mapa_spec.tsv.\n"]
    for Cod, Item in zip(Codigos, Itens):
        Linhas.append(f"- **{Cod}** ({Item.get('tipo', '?')}) {Item.get('explicacao', '')} Forma certa: {Item.get('forma_certa', '')}")
    return "\n".join(Linhas)


def Gerar(Forja, DocDir):
    """Dicionario nome do arquivo -> texto, para os 4 arquivos de saida (so memoria; nada vai a disco)."""
    Spec = LerTexto(Path(DocDir) / "spec.txt")
    Cartao = LerTexto(Path(DocDir) / "cartao.txt")
    return {
        "MEM-LJ-SPEC.txt": Secao("MEM-LJ-SPEC", TITULO_SPEC, Spec),
        "MEM-FORJA-GUIA-NUCLEO.txt": Secao("MEM-FORJA-GUIA-NUCLEO", TITULO_NUCLEO, Cartao),
        "regra_nativa_forja.txt": CABECALHO_REGRA + Cartao.rstrip() + "\n",
        "MEM-FORJA-GUIA-05.txt": Secao("MEM-FORJA-GUIA-05", TITULO_CODIGOS, MontarCodigos(Forja, DocDir)),
    }


def Sha256(Texto):
    """sha256 hexadecimal do texto em UTF-8."""
    return hashlib.sha256(Texto.encode("utf-8")).hexdigest()


def Gravar(Textos, Saida):
    """Grava os textos (UTF-8, LF) e hashes.json em Saida; devolve o dicionario nome -> sha256."""
    Saida = Path(Saida)
    Saida.mkdir(parents=True, exist_ok=True)
    Hashes = {}
    for Nome in NOMES_SAIDA:
        (Saida / Nome).write_bytes(Textos[Nome].encode("utf-8"))
        Hashes[Nome] = Sha256(Textos[Nome])
    (Saida / "hashes.json").write_bytes((json.dumps(Hashes, indent=2, sort_keys=True) + "\n").encode("utf-8"))
    return Hashes


def LerHashesGravados(Saida):
    """Hashes dos arquivos que ESTAO em Saida (recalculados dos bytes), ou None se faltar algum."""
    Hashes = {}
    for Nome in NOMES_SAIDA:
        Arq = Path(Saida) / Nome
        if not Arq.is_file():
            return None
        Hashes[Nome] = hashlib.sha256(Arq.read_bytes()).hexdigest()
    return Hashes


def Selftest(Forja):
    """Duas geracoes iguais, mudanca na doc muda o hash, codigo inexistente no mapa e recusado."""
    with tempfile.TemporaryDirectory(prefix="ia1_gdm_") as Nome:
        Base = Path(Nome)
        H1 = Gravar(Gerar(Forja, DOC_PADRAO), Base / "a")
        H2 = Gravar(Gerar(Forja, DOC_PADRAO), Base / "b")
        assert H1 == H2, "2 geracoes divergem"
        shutil.copytree(DOC_PADRAO, Base / "doc")
        Cartao = Base / "doc" / "cartao.txt"
        Cartao.write_bytes(Cartao.read_bytes() + b"\nlinha plantada\n")
        assert Gravar(Gerar(Forja, Base / "doc"), Base / "c") != H1, "mudar a doc nao mudou o hash"
        (Base / "doc" / "mapa_spec.tsv").write_bytes(b"codigo\tsecao\nE9999\tformato\n")
        try:
            Gerar(Forja, Base / "doc")
        except SystemExit as Erro:
            assert "E9999" in str(Erro), "mensagem sem o codigo"
        else:
            raise AssertionError("codigo inexistente nao foi recusado")
    print("selftest gerar_doc_mem: ok")


def Principal():
    """Entrada: le os argumentos, gera, grava e imprime uma linha JSON com os hashes."""
    Args = argparse.ArgumentParser(description="Gera os textos de MEM-LJ-SPEC, MEM-FORJA-GUIA-NUCLEO e da lista de codigos do GUIA-05")
    Args.add_argument("--forja", type=Path, default=FORJA_PADRAO)
    Args.add_argument("--doc", type=Path, default=DOC_PADRAO)
    Args.add_argument("--saida", type=Path, default=SAIDA_PADRAO)
    Args.add_argument("--selftest", action="store_true")
    Opcoes = Args.parse_args()
    if Opcoes.selftest:
        return Selftest(Opcoes.forja)
    Hashes = Gravar(Gerar(Opcoes.forja, Opcoes.doc), Opcoes.saida)
    print(json.dumps({"saida": str(Opcoes.saida), "sha256": Hashes}, sort_keys=True))


if __name__ == "__main__":
    sys.exit(Principal())
