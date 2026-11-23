"""Verifica as receitas da Forja (a "colinha de bolso": tests\\receitas\\<tag>.fj + <tag>.esperado). Tudo roda em copia em %TEMP%: o repositorio so e lido.

Formato de cada receita (cabecalho de comentarios, depois o programa completo):
    // receita: <tag>          (igual ao nome do arquivo; minusculas, digitos e '_')
    // tags: <tag> <sinonimo> ...   (palavras que o agente pode procurar; a 1a e a receita)
    // titulo: <uma linha>
    // nota: <1 a 3 linhas, uma por '// nota:'>
    <programa .fj completo, com proc main, que imprime algo>
O <tag>.esperado e a saida padrao exata, escrita ANTES de rodar (nao copiada da execucao).

Provas por receita: (1) cabecalho completo e coerente; (2) 'forja check' exit 0; (3) 'forja fmt' em dry-run exit 0 (ja canonica);
(4) 'forja migrar --dry-run --json' com 0 trocas (nenhuma forma antiga); (5) 'forja run --backend x64' exit 0 com a saida igual ao .esperado.
(6) todo codigo de erro citado nas notas existe no catalogo (forja explicar --lista); se o codigo CERTO para aquela situacao e outro, so a leitura da nota pega.
Entre receitas: tag e sinonimos sem repeticao (um sinonimo so aponta para uma receita).

Uso: python tests\\receitas\\verificar_receitas.py --forja compiler\\forja.exe [--listar] [--minimo N] [--so a,b]
Saida: 'OK: N receitas, M tags' (exit 0) ou uma linha FALHOU por problema (exit 1).
Versao: 0.2.2
Changelog: 0.2.2 - G3 LD: receita de pacote (import "pasta") leva a pasta de apoio <tag>_mods junto na copia em %TEMP%.
           0.2.1 - 0.21.0 F3: padrao x64, backend C aposentado (prova (5) so com x64).
           0.2.0 - os codigos citados nas notas tem de existir no catalogo; filtro --so.
           0.1.0 - POS-Z (colinha de bolso, parecer do Opus 14/11/2026): primeira versao.
Verificacao: python tests\\receitas\\verificar_receitas.py --forja compiler\\forja.exe
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

PASTA = Path(__file__).resolve().parent
TEMPO = 120
PADRAO_TAG = re.compile(r"^[a-z][a-z0-9_]*$")
CAMPOS = ("receita", "tags", "titulo", "nota")


def LerCabecalho(Texto):
    """Devolve ({campo: [valores]}, indice da 1a linha depois do cabecalho)."""
    Campos = {}
    Linhas = Texto.split("\n")
    Fim = 0
    for Numero, Linha in enumerate(Linhas):
        Achou = re.match(r"^// (receita|tags|titulo|nota): ?(.*)$", Linha.rstrip("\r"))
        if not Achou:
            break
        Campos.setdefault(Achou.group(1), []).append(Achou.group(2).strip())
        Fim = Numero + 1
    return Campos, Fim


def Rodar(Forja, Argumentos, Pasta):
    Res = subprocess.run([str(Forja), *Argumentos], cwd=Pasta, capture_output=True, timeout=TEMPO)
    return Res.returncode, Res.stdout.decode("utf-8", "replace"), Res.stderr.decode("utf-8", "replace")


def ConferirCabecalho(Arquivo, Campos):
    Falhas = []
    Nome = Arquivo.stem
    for Campo in CAMPOS:
        if not Campos.get(Campo):
            Falhas.append(f"{Arquivo.name}: falta '// {Campo}:' no cabecalho")
    if Falhas:
        return Falhas
    if len(Campos["receita"]) != 1 or Campos["receita"][0] != Nome or not PADRAO_TAG.match(Nome):
        Falhas.append(f"{Arquivo.name}: '// receita:' tem de ser igual ao nome do arquivo ({Nome}) em minusculas/digitos/_")
    Etiquetas = Campos["tags"][0].split()
    if not Etiquetas or Etiquetas[0] != Nome or not all(PADRAO_TAG.match(E) for E in Etiquetas):
        Falhas.append(f"{Arquivo.name}: '// tags:' tem de comecar pela propria tag e ter so minusculas/digitos/_")
    if len(Campos["titulo"]) != 1 or not Campos["titulo"][0]:
        Falhas.append(f"{Arquivo.name}: 1 linha de '// titulo:'")
    if not 1 <= len(Campos["nota"]) <= 3:
        Falhas.append(f"{Arquivo.name}: '// nota:' tem de ter de 1 a 3 linhas")
    return Falhas


def CodigosDoCatalogo(Forja):
    """Codigos que o 'forja explicar --lista --json' conhece."""
    Codigo, Saida, _ = Rodar(Forja, ["explicar", "--lista", "--json"], PASTA)
    return {Item["codigo"] for Item in json.loads(Saida)} if Codigo == 0 else set()


def VerificarUma(Forja, Arquivo, Catalogo):
    Falhas = []
    Texto = Arquivo.read_text(encoding="utf-8")
    Campos, Fim = LerCabecalho(Texto)
    Falhas += ConferirCabecalho(Arquivo, Campos)
    for Codigo in sorted(set(re.findall(r"\b[ERBF]\d{4}\b", " ".join(Campos.get("nota", []))))):
        if Codigo not in Catalogo:
            Falhas.append(f"{Arquivo.name}: a nota cita {Codigo}, que o forja explicar --lista nao conhece")
    Esperado = Arquivo.with_suffix(".esperado")
    if not Esperado.is_file():
        return Falhas + [f"{Arquivo.name}: falta {Esperado.name}"], Campos
    with tempfile.TemporaryDirectory(prefix="receita_") as Temp:
        Copia = Path(Temp) / Arquivo.name
        shutil.copyfile(Arquivo, Copia)
        Apoio = Arquivo.with_name(Arquivo.stem + "_mods")  # receita de pacote: a pasta <tag>_mods com os .fj vai junto (o import e "<tag>_mods")
        if Apoio.is_dir():
            shutil.copytree(Apoio, Path(Temp) / Apoio.name)
        Etapas = [("check", ["check", Copia.name]), ("fmt", ["fmt", Copia.name]), ("migrar", ["migrar", Copia.name, "--dry-run", "--json"])]
        for Rotulo, Argumentos in Etapas:
            Codigo, Saida, Erro = Rodar(Forja, Argumentos, Temp)
            if Codigo != 0:
                Falhas.append(f"{Arquivo.name}: forja {Rotulo} exit {Codigo}: {(Saida + Erro).strip()[:200]!r}")
            elif Rotulo == "migrar" and json.loads(Saida).get("total_trocas") != 0:
                Falhas.append(f"{Arquivo.name}: forja migrar --dry-run achou forma antiga: {Saida.strip()[:200]!r}")
        for Backend in ("x64",):
            Codigo, Saida, Erro = Rodar(Forja, ["run", "--backend", Backend, Copia.name], Temp)
            Quer = Esperado.read_bytes().decode("utf-8").replace("\r\n", "\n")
            if Codigo != 0 or Saida.replace("\r\n", "\n") != Quer:  # print no Windows termina a linha com CRLF
                Falhas.append(f"{Arquivo.name}: run --backend {Backend} exit {Codigo}, saida {Saida[:120]!r}, esperado {Quer[:120]!r} {Erro.strip()[:120]!r}")
    return Falhas, Campos


def Main():
    Analisador = argparse.ArgumentParser()
    Analisador.add_argument("--forja", required=True)
    Analisador.add_argument("--listar", action="store_true")
    Analisador.add_argument("--minimo", type=int, default=1)
    Analisador.add_argument("--so", default="", help="receitas separadas por virgula (sem .fj); verifica so elas")
    Opcoes = Analisador.parse_args()
    Escolhidas = {Nome.strip() for Nome in Opcoes.so.split(",") if Nome.strip()}
    Arquivos = [A for A in sorted(PASTA.glob("*.fj")) if not Escolhidas or A.stem in Escolhidas]
    if Opcoes.listar:
        for Arquivo in Arquivos:
            print(Arquivo.name)
        return 0
    Catalogo = CodigosDoCatalogo(Opcoes.forja)
    with ThreadPoolExecutor(max_workers=8) as Pool:
        Resultados = list(Pool.map(lambda A: VerificarUma(Opcoes.forja, A, Catalogo), Arquivos))
    Falhas = [F for Lista, _ in Resultados for F in Lista]
    Dono = {}
    for Arquivo, (_, Campos) in zip(Arquivos, Resultados):
        for Etiqueta in (Campos.get("tags") or [""])[0].split():
            if Dono.setdefault(Etiqueta, Arquivo.stem) != Arquivo.stem:
                Falhas.append(f"{Arquivo.name}: tag '{Etiqueta}' ja e de {Dono[Etiqueta]}")
    if len(Arquivos) < Opcoes.minimo:
        Falhas.append(f"so {len(Arquivos)} receitas, minimo {Opcoes.minimo}")
    for Falha in Falhas:
        print("FALHOU " + Falha)
    if Falhas:
        return 1
    print(f"OK: {len(Arquivos)} receitas, {len(Dono)} tags")
    return 0


if __name__ == "__main__":
    sys.exit(Main())
