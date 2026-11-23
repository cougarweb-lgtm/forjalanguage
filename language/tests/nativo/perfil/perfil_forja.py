# -*- coding: utf-8 -*-
"""Perfil do compilador Forja (P24b do M4). Nao toca o compiler\\ original.

Copia compiler\\ e pastas do construir.bat para %TEMP%\\forja_perfil\\; na COPIA (sem -pg) linka o amostrador.c
(amostrador de RIP por suspensao, PLANO B do P24b) e tira o -s. Grava resultados\\<data>.txt com (a) registro "tempos"
(oficial, mediana de 15 builds x64) de corpus_200000, corpus_20000 e 01_ola e (b) self time por funcao e por etapa
(>= 5000 amostras do corpus_200000, agregador agregar_amostras.py). Medir com a maquina quieta: %TEMP%\\medindo_compilador.lock.

Uso:
    python -I tests\\nativo\\perfil\\perfil_forja.py [--forja-fonte <pasta compiler>]
Verificacao: python -I tests\\nativo\\perfil\\perfil_forja.py --selftest
Versao: 0.2.0
Changelog: 0.2.0 - M4 P24b PLANO B: gprof sem dados na copia -pg; amostrador de RIP (amostrador.c) e agregador.
           0.1.0 - M4 P24b: primeira versao (copia -pg sem -s, etapas do registro tempos, gprof 3x).
"""

import argparse
import datetime
import hashlib
import json
import os
import shutil
import statistics
import subprocess
import sys
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
sys.path.insert(0, str(PastaAqui))
import agregar_amostras as Agregador  # noqa: E402

RaizLanguage = PastaAqui.parents[2]
PastaResultados = PastaAqui / "resultados"
PastaTrabalho = Path(os.environ["TEMP"]) / "forja_perfil"
ArquivoLock = Path(os.environ["TEMP"]) / "medindo_compilador.lock"
Gerado = RaizLanguage / "benchmark" / "corpus" / "gerado"
Corpus = {
    "corpus_200000": Gerado / "corpus_200000.fj",
    "corpus_20000": Gerado / "corpus_20000.fj",
    "01_ola": RaizLanguage.parent / "examples" / "01_ola.fj",
}
CAMPOS_TEMPOS = tuple(Etapa + "_ns" for Etapa in ("lex", "parse", "check", "ir", "bir", "isel", "regs", "codificar", "ligar", "escrever")) + ("outros_ns", "total_ns", "linhas")
PASTAS_COPIADAS = ("compiler", "runtime", "third", "std", "tools/forja_ligar", "tests/nativo", "tests/unidade")
# Trechos do construir.bat trocados SO na copia: FLAGS sem -pg (o amostrador nao precisa), link sem -s e com amostrador.c e -lwinmm.
PARES_PERFIL = (  # (trecho original, trecho da copia)
    (b"set FLAGS=-std=c11 -O2 ", b"set FLAGS=-std=c11 -O2 -g "),
    (b"gcc %FLAGS% -s -Wl,--gc-sections -o forja.exe driver\\main.c",
     b"gcc %FLAGS% -Wl,--gc-sections -o forja.exe ..\\tests\\nativo\\perfil\\amostrador.c driver\\main.c"),
    (b"driver\\processo_win.c %COMUM% fadec_envelope.o", b"driver\\processo_win.c %COMUM% fadec_envelope.o -lwinmm"),
)
REPETICOES, TOP_TABELA, RODADAS_MAXIMAS, MINIMO_AMOSTRAS, TIMEOUT = 15, 25, 40, 5000, 900


def Agora():
    return datetime.datetime.now().strftime("%d/%m/%Y %H:%M:%S")


def Executar(Comando, Cwd, Ambiente=None):
    Env = {**os.environ, **Ambiente} if Ambiente else None
    return subprocess.run([str(Parte) for Parte in Comando], cwd=Cwd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=TIMEOUT, env=Env)


def HashArvore(Pasta):
    """md5 de toda a arvore (caminho relativo + conteudo): prova que o compiler\\ original nao mudou."""
    Acumulado = hashlib.md5()
    for Arquivo in sorted(Path(Pasta).rglob("*")):
        if Arquivo.is_file():
            Acumulado.update(str(Arquivo.relative_to(Pasta)).encode("utf-8"))
            Acumulado.update(hashlib.md5(Arquivo.read_bytes()).digest())
    return Acumulado.hexdigest()


def CopiarCompilador(Raiz, Destino):
    """Copia as pastas do construir.bat para Destino com a mesma forma relativa (sem .o/.exe gerados)."""
    Ignorar = shutil.ignore_patterns("*.o", "*.exe", "__pycache__")
    for Relativa in PASTAS_COPIADAS:
        shutil.copytree(Raiz / Relativa, Destino / Relativa, ignore=Ignorar, dirs_exist_ok=True)


def AplicarPerfil(PastaCompilador):
    """Na COPIA: -g no FLAGS (sem -pg), amostrador.c e -lwinmm no link do forja.exe, sem -s."""
    Arquivo = PastaCompilador / "construir.bat"
    Texto = Arquivo.read_bytes()
    for Antes, Depois in PARES_PERFIL:
        if Texto.count(Antes) != 1:
            raise SystemExit("X0001 construir.bat: trecho " + repr(Antes) + " aparece " + str(Texto.count(Antes)) + " vezes")
        Texto = Texto.replace(Antes, Depois)
    Arquivo.write_bytes(Texto)


def ConstruirCopia(PastaCompilador):
    """cmd /c construir.bat na copia; falha do gcc vira X0002 com o fim da saida."""
    Feito = Executar(["cmd", "/c", ".\\construir.bat"], PastaCompilador)
    if Feito.returncode != 0 or not (PastaCompilador / "forja.exe").is_file():
        raise SystemExit("X0002 construir.bat da copia saiu com " + str(Feito.returncode) + "\n" + (Feito.stdout + Feito.stderr)[-2000:])


def LerRegistroTempos(Jsonl):
    """Registro tipo "tempos" do .debug.jsonl (um JSON por linha; medir_nativo.py faz o mesmo)."""
    for Linha in Jsonl.read_text(encoding="utf-8").splitlines():
        Registro = json.loads(Linha)
        if Registro.get("tipo") == "tempos":
            return Registro
    raise SystemExit("X0005 sem registro tempos em " + str(Jsonl))


def BuildComRegistro(Forja, Fonte, Saida):
    """Um build x64 com --debug-code tempos,json; devolve o registro tempos do <saida>.debug.jsonl."""
    Jsonl = Path(str(Saida) + ".debug.jsonl")
    Saida.unlink(missing_ok=True)
    Jsonl.unlink(missing_ok=True)
    Argumentos = [Forja, "build", "--modo", "exe", Fonte, "-o", Saida, "--backend", "x64", "--debug-code", "tempos,json"]
    Feito = Executar(Argumentos, Saida.parent)
    if Feito.returncode != 0 or not Jsonl.is_file():
        raise SystemExit("X0007 build " + Fonte.name + " saiu com " + str(Feito.returncode) + " (sem registro tempos)")
    return LerRegistroTempos(Jsonl)


def MedirEtapas(Forja, Trabalho):
    """(a) mediana de REPETICOES builds x64 de cada corpus, campo a campo do registro tempos."""
    Pasta = Trabalho / "etapas"
    Pasta.mkdir(parents=True, exist_ok=True)
    Medianas = {}
    for Nome, Fonte in Corpus.items():
        Registros = [BuildComRegistro(Forja, Fonte, Pasta / ("etapas_" + Nome + ".exe")) for _ in range(REPETICOES)]
        Medianas[Nome] = {Campo: statistics.median(Registro[Campo] for Registro in Registros) for Campo in CAMPOS_TEMPOS}
    return Medianas


def ContarRips(Arquivo):
    """Amostras do arquivo do amostrador 0.2.0: cabecalho de 7 tokens e 2 por amostra (RIP e chamador)."""
    return max((len(Arquivo.read_text(encoding="utf-8").split()) - 7) // 2, 0) if Arquivo.is_file() else 0


def RodarAmostragem(Forja, Trabalho):
    """Builds do corpus_200000 x64 no forja com o amostrador (FORJA_AMOSTRAS) ate MINIMO_AMOSTRAS RIPs; cada rodada, um arquivo."""
    Arquivos = []
    while sum(ContarRips(Arquivo) for Arquivo in Arquivos) < MINIMO_AMOSTRAS:
        if len(Arquivos) >= RODADAS_MAXIMAS:
            raise SystemExit("X0003 amostras insuficientes: " + str(sum(ContarRips(A) for A in Arquivos)) + " em " + str(RODADAS_MAXIMAS) + " rodadas")
        Pasta = Trabalho / ("amostra" + str(len(Arquivos) + 1))
        Pasta.mkdir(parents=True, exist_ok=True)
        Arquivo = Pasta / "rips.txt"
        Argumentos = [Forja, "build", "--modo", "exe", Corpus["corpus_200000"], "-o", Pasta / "corpus.exe", "--backend", "x64"]
        Feito = Executar(Argumentos, Pasta, {"FORJA_AMOSTRAS": str(Arquivo)})
        if Feito.returncode != 0 or not Arquivo.is_file():
            raise SystemExit("X0003 amostragem sem arquivo na rodada " + str(len(Arquivos) + 1) + " (rc " + str(Feito.returncode) + ")")
        Arquivos.append(Arquivo)
        if len(Arquivos) == 1 and ContarRips(Arquivo) == 0:
            raise SystemExit("X0003 amostrador sem RIP na 1a rodada (" + str(Arquivo) + ")")
    return Arquivos


def CabecalhoRelatorio(HashAntes, HashDepois, Rodadas):
    Situacao = "IGUAL" if HashAntes == HashDepois else "MUDOU!"
    return (f"PERFIL FORJA P24b (PLANO B amostrador) - perfil_forja.py 0.2.0 - {Agora()}\n"
            f"compiler original md5 antes {HashAntes} depois {HashDepois} ({Situacao})\n"
            f"rodadas do corpus_200000 com amostrador: {Rodadas} (~1 ms por RIP; so vira alvo o que a (a) confirma)")


def TabelaEtapas(Medianas):
    """(a) mediana por campo do registro tempos: ms nos campos _ns; linhas e contagem."""
    Linhas = ["(a) registro tempos, mediana de " + str(REPETICOES) + " builds x64 (ms)", "campo | " + " | ".join(Corpus)]
    for Campo in CAMPOS_TEMPOS:
        Fator = 1e-6 if Campo.endswith("_ns") else 1.0
        Linhas.append(f"{Campo:<11} | " + " | ".join(f"{Medianas[Nome][Campo] * Fator:>10.2f}" for Nome in Corpus))
    return "\n".join(Linhas)


def Medir(Fonte):
    """md5 do original -> copia com amostrador construida -> (a) etapas oficiais -> (b) amostras -> relatorio -> md5 de novo."""
    HashAntes = HashArvore(Fonte)
    shutil.rmtree(PastaTrabalho, ignore_errors=True)
    PastaTrabalho.mkdir(parents=True, exist_ok=True)
    Copia = PastaTrabalho / "copia"
    CopiarCompilador(Fonte.parent, Copia)
    AplicarPerfil(Copia / "compiler")
    ConstruirCopia(Copia / "compiler")
    Oficial = PastaTrabalho / "oficial" / "forja.exe"
    Oficial.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(Fonte / "forja.exe", Oficial)
    Medianas = MedirEtapas(Oficial, PastaTrabalho)
    Forja = Copia / "compiler" / "forja.exe"
    Arquivos = RodarAmostragem(Forja, PastaTrabalho)
    Resultado = Agregador.AgregarAmostras(Arquivos, Forja, Agregador.MapearFuncoes(Fonte))
    MsCorpus = Medianas["corpus_200000"]["total_ns"] * 1e-6
    Referencia = {Campo[:-3]: Valor * 1e-6 for Campo, Valor in Medianas["corpus_200000"].items() if Campo.endswith("_ns") and Campo != "total_ns"}
    HashDepois = HashArvore(Fonte)
    Texto = "\n\n".join([CabecalhoRelatorio(HashAntes, HashDepois, len(Arquivos)), TabelaEtapas(Medianas),
                         Agregador.TabelaDonos(Resultado, MsCorpus, TOP_TABELA),
                         Agregador.TabelaEtapasDonos(Resultado, MsCorpus, Referencia),
                         Agregador.TabelaSistema(Resultado)])
    PastaResultados.mkdir(exist_ok=True)
    Arquivo = PastaResultados / (datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S") + ".txt")
    Arquivo.write_text(Texto, encoding="utf-8")
    print(Texto + "\nRESULTADO: " + str(Arquivo))
    if HashAntes != HashDepois:
        raise SystemExit("X0006 o compiler original mudou durante a medida")
    return 0


def Principal():
    Parser = argparse.ArgumentParser(description="Perfil do compilador Forja (P24b do M4, plano B amostrador).")
    Parser.add_argument("--forja-fonte", help="pasta compiler a copiar (padrao: language\\compiler)")
    Parser.add_argument("--selftest", action="store_true", help="prova o agregador e a tabela com texto sintetico")
    Args = Parser.parse_args()
    if Args.selftest:
        Agregador.AutoTeste()
        return 0
    Fonte = Path(Args.forja_fonte).resolve() if Args.forja_fonte else RaizLanguage / "compiler"
    if ArquivoLock.exists():
        raise SystemExit("X0004 outro agente esta medindo: " + str(ArquivoLock))
    ArquivoLock.write_text(Agora(), encoding="utf-8")
    try:
        return Medir(Fonte)
    finally:
        ArquivoLock.unlink(missing_ok=True)


if __name__ == "__main__":
    sys.exit(Principal())
