#!/usr/bin/env python3
"""medir_linux.py - medicoes do alvo x86_64-linux do M5 P30: build (01_ola e corpus_200000) e bench (x64 linux contra gcc-musl -O0 e -O2).

Objetivo: fechar as metas (3) e (4) do portao P30 com a regua de medir_nativo.py (01_ola: mediana de 15 builds com 2 aquecimentos; corpus_200000: build frio em
ABBA pareado de 15 pares, linux contra windows) e de medir_bench.py (cada bench: mediana de 5 execucoes com 1 aquecimento; razao x64/O0 e geomedia; meta <= 1,30).
Nao havia ferramenta de linux: medir_nativo.py e medir_bench.py so medem o Windows. O bench roda no WSL (distro alpine-build) num lote so: os ELF gcc-musl congelados (D0730)
e o python3 da distro cronometra (time.perf_counter em volta de cada execucao), sem o custo do wsl.exe entrar na medida.
Uso:   python -I medir_linux.py --build --forja <forja.exe absoluto> [--saida <arq.json>]
       python -I medir_linux.py --bench --forja <forja.exe absoluto> [--saida <arq.json>]
       python -I medir_linux.py --ir --forja <forja.exe absoluto> [--tempos-windows <otim_historico.jsonl>] [--saida <arq.json>]
       python -I medir_linux.py --por-funcao <bench> --forja <forja.exe absoluto> [--saida <arq.json>]
       python -I medir_linux.py --selftest
--ir (M7b F0a): Ir (perf stat -e instructions:u, mediana de 3 rodadas) do ELF x64 --release da Forja e do ELF gcc-musl -O2 congelado (D0730), nos 14 benches; razao Forja/gcc e geomedias total e codegen (sem formato_texto: runtime Ryu). --tempos-windows: compara a razao com a de tempo x64/O2 da rodada do medir_bench de MESMO sha e marca DIVERGE acima de 15%.
--por-funcao <bench>: valgrind --tool=cachegrind --cache-sim=no nos dois binarios; top 10 funcoes por Ir. O cg_annotate nao vem no pacote Alpine: o somatorio sai do formato cachegrind aqui mesmo.
Saida: uma linha por medicao e a linha PORTAO no fim (as metas sao lidas pelo main). Exit 1 so por falha de build, de programa ou de saida; 0 se a medicao rodou.
Escrita: %TEMP%\\forja_p30\\build, \\bench, \\ir e \\ir\\perf, \\funcao (binarios, C e saidas); %TEMP%\\medindo_compilador.lock durante a medicao (apagado na saida); no WSL, /tmp/forja_bench_p30, /tmp/forja_ir_p30 e /tmp/forja_funcao_p30 (apagados).
Depende de: perf, valgrind (apk, WSL alpine-build)
Maquina: exige a maquina quieta. Se medindo_compilador.lock ou testando_gpu.lock ja existir em %TEMP%, recusa (nao mede por cima de outro medidor).
Reversao: apagar este arquivo.
Verificacao: python -I medir_linux.py --selftest

Versao: 0.3.0
Changelog: 0.3.0 (18/11/2026) 0.21.0 F3 - --bench, --ir e --por-funcao deixam de gerar o C com forja --modo c (X0003 na 0.21.0) e usam o ELF gcc-musl O0/O2 congelado
           (oraculo_congelado_ref.json, BinarioDeReferencia; D0730); ausente ou sha trocado = erro, nunca recompila. O lado x64 e o --build nao mudam.
           0.2.0 (17/11/2026) M7b F0a - --ir (Ir por perf stat, mediana de 3; gcc-musl -O2; razao e geomedias total/codegen; correlacao com o tempo Windows) e
           --por-funcao (cachegrind, top 10). Selftest cobre o parse do perf e do cachegrind, o script do WSL e a correlacao. Build e bench inalterados.
           0.1.0 (14/11/2026) M5 P30 - primeira versao: --build (01_ola linux dev e release, corpus_200000 linux x windows em ABBA de 15 pares, corpus linux rodado
           uma vez no WSL contra o esperado) e --bench (8 benchs de tests\\nativo\\bench: x64 linux --release contra gcc-musl -O0 e -O2 do C, mediana de 5, geomedia).
"""
import argparse
import hashlib
import json
import math
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
sys.path.insert(0, str(AQUI.parent / "rapido"))  # binarios gcc congelados (D0730): oraculo_congelado_ref.json
import oraculo_congelado as Oraculo  # noqa: E402
import wsl_lote  # noqa: E402

LANGUAGE = AQUI.parents[2]
ARQUIVO_OLA = LANGUAGE.parent / "examples" / "01_ola.fj"
ARQUIVO_CORPUS = LANGUAGE / "benchmark" / "corpus" / "gerado" / "corpus_200000.fj"
PASTA_BENCH = AQUI.parent / "bench"
STUB_A52 = AQUI / "stubs_posix_a52.h"  # mesmo -include do oraculo_c.py: o runtime C tem as funcoes A52 do std:os
PASTA_TRABALHO = Path(tempfile.gettempdir()) / "forja_p30"
LOCKS = ("medindo_compilador.lock", "testando_gpu.lock")
AQUECIMENTOS_OLA = 2
REPETICOES_OLA = 15
PARES_CORPUS = 15
AQUECIMENTOS_BENCH = 1
REPETICOES_BENCH = 5
META_OLA_MS = 25
META_CORPUS_RAZAO = 1.05
META_BENCH_RAZAO = 1.30
TEMPO_BUILD = 300
TEMPO_PROGRAMA = 60
TEMPO_WSL = 900
W_BENCH = "/tmp/forja_bench_p30"
FLAGS_GCC = "-std=c11 -D_POSIX_C_SOURCE=200809L -static -Wno-type-limits"
REPETICOES_IR = 3
W_IR = "/tmp/forja_ir_p30"
W_FUNCAO = "/tmp/forja_funcao_p30"
PASTA_IR = "perf"  # subpasta de PASTA_TRABALHO/ir (builds em ir\)
PASTA_FUNCAO = "funcao"
BENCH_FORA_CODEGEN = "formato_texto"  # runtime Ryu: fica de fora da geomedia codegen
TOLERANCIA_CORRELACAO = 0.15
HISTORICO_BENCH = LANGUAGE / "benchmark" / "resultados" / "otim_historico.jsonl"
TOP_FUNCOES = 10


class ErroMedicao(RuntimeError):
    """Falha de build, de programa ou de saida; a mensagem diz o que falhou e onde."""


class Trava:
    """Lock de medicao (medindo_compilador.lock em %TEMP%): recusa medir se outro medidor ou a GPU estiver ativo; apaga o proprio na saida."""

    def __init__(self):
        self.Caminho = Path(tempfile.gettempdir()) / "medindo_compilador.lock"

    def __enter__(self):
        for Nome in LOCKS:
            if (Path(tempfile.gettempdir()) / Nome).exists():
                raise ErroMedicao(f"lock {Nome} existe em %TEMP%: a maquina nao esta quieta; nao medi")
        self.Caminho.write_text(f"medir_linux pid {os.getpid()}\n", encoding="utf-8")
        return self

    def __exit__(self, *_):
        self.Caminho.unlink(missing_ok=True)


def Normalizar(Bruto: bytes) -> bytes:
    """CRLF vira LF: o esperado e LF, e o Windows grava CRLF."""
    return Bruto.replace(b"\r\n", b"\n")


def Geomedia(Valores: list) -> float:
    return math.exp(sum(math.log(Valor) for Valor in Valores) / len(Valores))


def Forja(Exe: Path, Argumentos: list, Cwd: Path) -> None:
    Processo = subprocess.run([str(Exe)] + Argumentos, capture_output=True, cwd=str(Cwd), stdin=subprocess.DEVNULL, timeout=TEMPO_BUILD)
    if Processo.returncode != 0:
        Texto = (Processo.stdout + Processo.stderr).decode("utf-8", "replace")[:400]
        raise ErroMedicao(f"forja {' '.join(Argumentos[:2])} saiu {Processo.returncode}: {Texto}")


def Cronometrar(Acao) -> float:
    Inicio = time.perf_counter()
    Acao()
    return (time.perf_counter() - Inicio) * 1000.0


def MedianaMs(Acao, Aquecimentos: int, Repeticoes: int) -> float:
    for _ in range(Aquecimentos):
        Acao()
    return statistics.median([Cronometrar(Acao) for _ in range(Repeticoes)])


def ArgumentosBuild(Alvo: str, Fonte: Path, Saida: Path, Release: bool = False) -> list:
    """Alvo 'linux' = --target x86_64-linux (implica --backend x64); 'windows' = --backend x64, o mesmo x64 que medir_nativo.py mede."""
    Extra = ["--target", "x86_64-linux"] if Alvo == "linux" else ["--backend", "x64"]
    return ["build", "--modo", "exe"] + (["--release"] if Release else []) + [str(Fonte), "-o", str(Saida)] + Extra


def MedirOla(Exe: Path, Pasta: Path) -> dict:
    """01_ola: linux dev (meta), windows x64 (informado) e os bytes do ELF dev e release (item 5)."""
    Saida = {}
    for Alvo in ("linux", "windows"):
        Binario = Pasta / f"ola_{Alvo}.bin"
        Argumentos = ArgumentosBuild(Alvo, ARQUIVO_OLA, Binario)
        Forja(Exe, Argumentos, Pasta)
        Saida[Alvo] = MedianaMs(lambda: Forja(Exe, Argumentos, Pasta), AQUECIMENTOS_OLA, REPETICOES_OLA)
        Saida[f"bytes_{Alvo}_dev"] = Binario.stat().st_size
    Release = Pasta / "ola_linux_release.bin"
    Forja(Exe, ArgumentosBuild("linux", ARQUIVO_OLA, Release, True), Pasta)
    Saida["bytes_linux_release"] = Release.stat().st_size
    return Saida


def ParesCorpus(Exe: Path, Pasta: Path) -> list:
    """ABBA: o par i roda linux,windows (i par) ou windows,linux (i impar); build frio, sem aquecimento. O ultimo ELF linux fica para o WSL."""
    Pares = []
    for Indice in range(PARES_CORPUS):
        Tempos = {}
        for Alvo in (("linux", "windows") if Indice % 2 == 0 else ("windows", "linux")):
            Binario = Pasta / f"corpus_{Alvo}.bin"
            Binario.unlink(missing_ok=True)
            Argumentos = ArgumentosBuild(Alvo, ARQUIVO_CORPUS, Binario)
            Tempos[Alvo] = Cronometrar(lambda: Forja(Exe, Argumentos, Pasta))
        Pares.append(Tempos)
        print(f"(3) corpus par {Indice + 1}/{PARES_CORPUS}: linux {Tempos['linux']:.0f} ms, windows {Tempos['windows']:.0f} ms", flush=True)
    return Pares


def ResumoPares(Pares: list) -> dict:
    Linux = [Par["linux"] for Par in Pares]
    Windows = [Par["windows"] for Par in Pares]
    Razoes = [Par["linux"] / Par["windows"] for Par in Pares]
    Quartis = statistics.quantiles(Razoes, n=4)
    return {
        "mediana_linux_ms": statistics.median(Linux),
        "mediana_windows_ms": statistics.median(Windows),
        "razao_medianas": statistics.median(Linux) / statistics.median(Windows),
        "razao_pareada_mediana": statistics.median(Razoes),
        "razao_pareada_iqr_sobre_mediana": (Quartis[2] - Quartis[0]) / statistics.median(Razoes),
        "razao_pareada_min": min(Razoes),
        "razao_pareada_max": max(Razoes),
    }


def CorpusNoWsl(Binario: Path, Pasta: Path) -> dict:
    """Roda o ELF do corpus UMA vez no WSL (copia para /tmp, roda, apaga) e compara a saida com corpus_200000.esperado.txt.
    Via arquivo .sh e nao sh -c: no wsl.exe deste ambiente as variaveis do sh -c saem vazias (diagnosticado no P30)."""
    Destino = "/tmp/forja_p30_corpus"
    Saida = Pasta / "corpus_linux.out"
    Script = Pasta / "corpus_wsl.sh"
    Linhas = ["#!/bin/sh", f"cp {wsl_lote.Aspas(wsl_lote.CaminhoWsl(Binario))} {Destino} && chmod 755 {Destino} && "
              f"{Destino} > {wsl_lote.Aspas(wsl_lote.CaminhoWsl(Saida))}", "RC=$?", f"rm -f {Destino}", 'echo "$RC"']
    Script.write_bytes(("\n".join(Linhas) + "\n").encode("utf-8"))
    Processo = wsl_lote.Wsl(["sh", wsl_lote.CaminhoWsl(Script)], TEMPO_PROGRAMA * 3)
    Codigos = wsl_lote.TextoDoWsl(Processo.stdout).splitlines()
    if not Codigos:
        raise ErroMedicao(f"corpus linux no WSL: sem codigo de saida (wsl rc {Processo.returncode})")
    Rc = int(Codigos[-1])
    Esperado = ARQUIVO_CORPUS.with_suffix(".esperado.txt")
    Igual = Normalizar(Saida.read_bytes()) == Normalizar(Esperado.read_bytes())
    return {"rc": Rc, "igual": Igual, "bytes": Saida.stat().st_size}


def RodarBuild(Exe: Path, Pasta: Path) -> dict:
    Ola = MedirOla(Exe, Pasta)
    print(f"(3) 01_ola linux dev: {Ola['linux']:.1f} ms (mediana de {REPETICOES_OLA}, {AQUECIMENTOS_OLA} aquecimentos); windows x64 (informado): {Ola['windows']:.1f} ms", flush=True)
    print(f"(5) ELF 01_ola: dev {Ola['bytes_linux_dev']} B, release {Ola['bytes_linux_release']} B", flush=True)
    Pares = ParesCorpus(Exe, Pasta)
    Resumo = ResumoPares(Pares)
    print(f"(3) corpus_200000: linux mediana {Resumo['mediana_linux_ms']:.0f} ms, windows mediana {Resumo['mediana_windows_ms']:.0f} ms, "
          f"razao das medianas {Resumo['razao_medianas']:.3f}, razao pareada mediana {Resumo['razao_pareada_mediana']:.3f}, IQR/mediana {Resumo['razao_pareada_iqr_sobre_mediana']:.3f}", flush=True)
    Corpus = CorpusNoWsl(Pasta / "corpus_linux.bin", Pasta)
    if Corpus["rc"] != 0 or not Corpus["igual"]:
        raise ErroMedicao(f"corpus linux no WSL: rc {Corpus['rc']}, saida igual ao esperado: {Corpus['igual']}")
    print(f"(3) corpus linux no WSL: rc 0, saida IGUAL ao corpus_200000.esperado.txt ({Corpus['bytes']} B)", flush=True)
    return {"ola": Ola, "corpus": Resumo, "pares": Pares, "corpus_wsl": Corpus}


def BinarioCongelado(Fonte: Path, Nivel: str) -> Path:
    """ELF gcc-musl O0 ou O2 congelado do bench (D0730). Chave ausente, arquivo ausente ou sha trocado vira ErroMedicao: nunca pula, nunca recompila."""
    try:
        return Oraculo.BinarioDeReferencia(Fonte, "linux", Nivel)
    except Oraculo.ErroOraculo as Erro:
        raise ErroMedicao(f"ELF gcc congelado (linux/{Nivel}) indisponivel: {Erro}") from Erro


def ConstruirBenchs(Exe: Path, Pasta: Path, Nomes: list | None = None) -> list:
    """Por bench: ELF x64 linux --release da Forja e os ELF gcc-musl O0/O2 congelados (copiados para a pasta do Windows como <nome>.o0 e <nome>.o2).
    Nomes filtra os benches (None = todos). O forja e o Windows: o ELF sai do forja.exe com --target x86_64-linux, como no --bench."""
    Itens = []
    for Fonte in sorted(PASTA_BENCH.glob("*.fj")):
        Nome = Fonte.stem
        if Nomes is not None and Nome not in Nomes:
            continue
        Arquivos = {"x64": Pasta / f"{Nome}.x64.elf", "o0": Pasta / f"{Nome}.o0", "o2": Pasta / f"{Nome}.o2"}
        Forja(Exe, ["build", "--target", "x86_64-linux", "--release", str(Fonte), "-o", str(Arquivos["x64"])], Pasta)
        shutil.copyfile(BinarioCongelado(Fonte, "O0"), Arquivos["o0"])
        shutil.copyfile(BinarioCongelado(Fonte, "O2"), Arquivos["o2"])
        Itens.append({"nome": Nome, "arquivos": Arquivos, "esperado": PASTA_BENCH / f"{Nome}.esperado.txt"})
    return Itens


def ScriptBench(Itens: list, Pasta: Path) -> tuple:
    """Gera o .sh do WSL (LF): copia os ELF x64 e os gcc-musl O0/O2 congelados para /tmp, tempo no python3 da distro, apaga /tmp. Devolve (texto, manifesto)."""
    Copias = []
    Manifesto = []
    Linhas = ["#!/bin/sh", "# gerado por medir_linux.py --bench (P30): ELF gcc-musl O0/O2 congelados (D0730); o tempo sai do python3 da distro",
              f"W={wsl_lote.Aspas(W_BENCH)}", 'rm -rf "$W" && mkdir -p "$W" || exit 1']
    for Item in Itens:
        Copias += [wsl_lote.Aspas(wsl_lote.CaminhoWsl(Arq)) for Arq in Item["arquivos"].values()]
    Linhas.append('cp ' + " ".join(Copias) + ' "$W"/ || exit 1')
    for Item in Itens:
        Nome = Item["nome"]
        Linhas.append(f'chmod 755 "$W/{Nome}.x64.elf" "$W/{Nome}.o0" "$W/{Nome}.o2"')
        for Rotulo, Sufixo in (("x64", f"{Nome}.x64.elf"), ("O0", f"{Nome}.o0"), ("O2", f"{Nome}.o2")):
            Manifesto.append({"nome": Nome, "rotulo": Rotulo, "binario": f"{W_BENCH}/{Sufixo}", "esperado": wsl_lote.CaminhoWsl(Item["esperado"])})
    Python = wsl_lote.CaminhoWsl(Path(__file__).resolve())
    Linhas.append(f'python3 -I {wsl_lote.Aspas(Python)} --tempo-wsl {wsl_lote.Aspas(wsl_lote.CaminhoWsl(Pasta / "manifesto.json"))} '
                  f'> {wsl_lote.Aspas(wsl_lote.CaminhoWsl(Pasta / "tempo.json"))} || exit 3')
    Linhas.append('rm -rf "$W"')
    return "\n".join(Linhas) + "\n", Manifesto


def ResumoBench(Tempos: list) -> dict:
    Falhas = [f"{T['nome']}/{T['rotulo']}" for T in Tempos if not T["igual"] or T["rc"] != 0]
    if Falhas:
        raise ErroMedicao(f"saida ou codigo de saida diferente do esperado em {Falhas}")
    Por = {}
    for T in Tempos:
        Por.setdefault(T["nome"], {})[T["rotulo"]] = T["ms"]
    Linhas = []
    for Nome in sorted(Por):
        X, O0, O2 = Por[Nome]["x64"], Por[Nome]["O0"], Por[Nome]["O2"]
        Linhas.append({"nome": Nome, "x64_ms": X, "O0_ms": O0, "O2_ms": O2, "razao_O0": X / O0, "razao_O2": X / O2})
    return {"benchs": Linhas,
            "geomedia_O0": Geomedia([L["razao_O0"] for L in Linhas]),
            "geomedia_O2": Geomedia([L["razao_O2"] for L in Linhas])}


def RodarBench(Exe: Path, Pasta: Path) -> dict:
    Itens = ConstruirBenchs(Exe, Pasta)
    Script, Manifesto = ScriptBench(Itens, Pasta)
    (Pasta / "manifesto.json").write_text(json.dumps(Manifesto, ensure_ascii=False, indent=1), encoding="utf-8")
    Arquivo = Pasta / "bench_wsl.sh"
    Arquivo.write_bytes(Script.encode("utf-8"))
    Processo = wsl_lote.Wsl(["sh", wsl_lote.CaminhoWsl(Arquivo)], TEMPO_WSL)
    if Processo.returncode != 0:
        Texto = wsl_lote.TextoDoWsl(Processo.stdout + Processo.stderr)[:400]
        raise ErroMedicao(f"bench no WSL saiu {Processo.returncode} (2 = gcc-musl, 3 = tempo): {Texto}")
    Tempos = json.loads((Pasta / "tempo.json").read_text(encoding="utf-8"))
    Resultado = ResumoBench(Tempos)
    Resultado["tempos"] = Tempos
    for Linha in Resultado["benchs"]:
        print(f"(4) {Linha['nome']:<16} x64 {Linha['x64_ms']:9.1f} ms | O0 {Linha['O0_ms']:9.1f} ms | O2 {Linha['O2_ms']:9.1f} ms | "
              f"x64/O0 {Linha['razao_O0']:.3f} | x64/O2 {Linha['razao_O2']:.3f}", flush=True)
    return Resultado


def TempoNoWsl(Manifesto: Path) -> list:
    """Roda DENTRO do WSL, com o python3 da distro: 1a execucao confere a saida e o codigo (aquecimento); depois REPETICOES_BENCH cronometradas, mediana em ms."""
    Itens = json.loads(Manifesto.read_text(encoding="utf-8"))
    Saida = []
    for Item in Itens:
        Esperado = Normalizar(Path(Item["esperado"]).read_bytes())
        Primeira = subprocess.run([Item["binario"]], capture_output=True, stdin=subprocess.DEVNULL, timeout=TEMPO_PROGRAMA)
        Tempos = []
        for _ in range(REPETICOES_BENCH):
            Tempos.append(Cronometrar(lambda: subprocess.run([Item["binario"]], stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, timeout=TEMPO_PROGRAMA)))
        Saida.append({"nome": Item["nome"], "rotulo": Item["rotulo"], "igual": Normalizar(Primeira.stdout) == Esperado,
                      "rc": Primeira.returncode, "ms": statistics.median(Tempos)})
    return Saida


def DividirValor(Dividendo: float, Divisor: float) -> float:
    # Divisor vem de medicao: zero vira 0.0 em vez de ZeroDivisionError (CROSS-R02).
    if Divisor == 0:
        return 0.0
    return Dividendo / Divisor


def ShaCurta(Exe: Path) -> str:
    # Mesmo formato do medir_bench (12 hex): o historico cruza as ferramentas pelo sha da forja.
    return hashlib.sha256(Path(Exe).read_bytes()).hexdigest()[:12]


def LerInstrucoesPerf(Texto: str) -> int:
    """Linha instructions:u do perf stat -x, (campos por virgula; 1o campo = contagem). '<not supported>' ou linha ausente = erro (PMU da VM)."""
    for Linha in Texto.splitlines():
        if "instructions:u" in Linha:
            Campo = Linha.split(",")[0].strip()
            if not Campo.isdigit():
                raise ErroMedicao(f"perf stat sem contagem de instrucoes: {Linha!r} (PMU indisponivel na VM?)")
            return int(Campo)
    raise ErroMedicao("perf stat sem a linha instructions:u")


def ResolverNomeFn(Resto: str, Nomes: dict) -> str:
    """'(3) nome' define o id 3; '(3)' sozinho reusa o nome (compressao de strings do cachegrind)."""
    Casa = re.match(r"\((\d+)\)\s*(.*)$", Resto)
    if Casa is None:
        return Resto
    Id, Nome = Casa.group(1), Casa.group(2)
    if Nome:
        Nomes[Id] = Nome
    return Nomes.get(Id, Id)


def SomarIrPorFuncao(Texto: str) -> dict:
    """Ir por funcao do arquivo do cachegrind (--cache-sim=no: um evento, Ir). 'fn=' abre a funcao; linhas 'numero contagem' somam nela; 'summary:' fica fora."""
    Nomes, Por, Atual = {}, {}, None
    for Linha in Texto.splitlines():
        if Linha.startswith("fn="):
            Atual = ResolverNomeFn(Linha[3:], Nomes)
            Por.setdefault(Atual, 0)
            continue
        Partes = Linha.split()
        if Atual is not None and len(Partes) == 2 and Linha[:1] in "0123456789+-*" and Partes[1].isdigit():
            Por[Atual] += int(Partes[1])
    return Por


def TopFuncoes(Por: dict, Quantas: int = TOP_FUNCOES) -> list:
    """As Quantas funcoes de maior Ir, com o percentual sobre o total."""
    Total = sum(Por.values())
    Ordem = sorted(Por.items(), key=lambda Item: (-Item[1], Item[0]))[:Quantas]
    return [{"funcao": Nome, "ir": Ir, "pct": 100.0 * DividirValor(Ir, Total)} for Nome, Ir in Ordem]


def RazoesCodegen(Benchs: list) -> list:
    """Razoes dos benches de codegen: sem formato_texto (runtime Ryu, nao e codigo gerado pela Forja)."""
    return [B["razao"] for B in Benchs if B["nome"] != BENCH_FORA_CODEGEN]


def ComandoCopia(Itens: list) -> str:
    Copias = []
    for Item in Itens:
        Copias += [wsl_lote.Aspas(wsl_lote.CaminhoWsl(Arq)) for Arq in Item["arquivos"].values()]
    return "cp " + " ".join(Copias) + ' "$W"/ || exit 1'


def ScriptIr(Itens: list, Pasta: Path) -> str:
    """Gera o .sh do WSL (LF): copia ELF x64 e o gcc-musl -O2 congelado, e perf stat -e instructions:u REPETICOES_IR vezes por binario.
    perf (-o), stdout e rc vao para a pasta do Windows (/mnt/c): o Windows le os arquivos, nao o stdout do WSL."""
    Saida = wsl_lote.CaminhoWsl(Pasta / PASTA_IR)
    Linhas = ["#!/bin/sh", "# gerado por medir_linux.py --ir (M7b F0a)",
              f"W={wsl_lote.Aspas(W_IR)}", 'rm -rf "$W" && mkdir -p "$W" || exit 1',
              f"P={wsl_lote.Aspas(Saida)}", 'rm -f "$P"/*.perf "$P"/*.out "$P"/*.rc || exit 1', ComandoCopia(Itens)]
    for Item in Itens:
        Nome = Item["nome"]
        Linhas.append(f'chmod 755 "$W/{Nome}.x64.elf" "$W/{Nome}.o2"')
        for Rotulo, Binario in (("x64", f"{Nome}.x64.elf"), ("O2", f"{Nome}.o2")):
            for Rodada in range(1, REPETICOES_IR + 1):
                Base = f'"$P/{Nome}.{Rotulo}.{Rodada}"'
                Linhas.append(f'timeout {TEMPO_PROGRAMA} perf stat -x, -e instructions:u -o {Base}.perf -- "$W/{Binario}" '
                              f'> {Base}.out 2>/dev/null < /dev/null; echo $? > {Base}.rc')
    Linhas.append('rm -rf "$W"')
    return "\n".join(Linhas) + "\n"


def ScriptPorFuncao(Item: dict, Pasta: Path) -> str:
    """valgrind --tool=cachegrind --cache-sim=no nos dois binarios (ELF da Forja e gcc-musl -O2 congelado); cachegrind, stdout e rc vao para a pasta do Windows."""
    Nome = Item["nome"]
    Saida = wsl_lote.CaminhoWsl(Pasta / PASTA_FUNCAO)
    Linhas = ["#!/bin/sh", "# gerado por medir_linux.py --por-funcao (M7b F0a): valgrind cachegrind; saidas na pasta do Windows",
              f"W={wsl_lote.Aspas(W_FUNCAO)}", 'rm -rf "$W" && mkdir -p "$W" || exit 1',
              f"P={wsl_lote.Aspas(Saida)}", 'rm -f "$P"/* || exit 1', ComandoCopia([Item]),
              f'chmod 755 "$W/{Nome}.x64.elf" "$W/{Nome}.o2"']
    for Rotulo, Binario in (("x64", f"{Nome}.x64.elf"), ("O2", f"{Nome}.o2")):
        Base = f'"$P/{Nome}.{Rotulo}"'
        Linhas.append(f'valgrind --tool=cachegrind --cache-sim=no --cachegrind-out-file={Base}.cg "$W/{Binario}" '
                      f'> {Base}.out 2> {Base}.vg.log < /dev/null; echo $? > {Base}.rc')
    Linhas.append('rm -rf "$W"')
    return "\n".join(Linhas) + "\n"


def ExecutarScriptWsl(Pasta: Path, NomeArquivo: str, Texto: str, Descricao: str) -> None:
    """Grava o .sh (LF) na pasta do Windows e roda numa chamada do WSL; rc != 0 vira ErroMedicao (2 = gcc-musl)."""
    Arquivo = Pasta / NomeArquivo
    Arquivo.write_bytes(Texto.encode("utf-8"))
    Processo = wsl_lote.Wsl(["sh", wsl_lote.CaminhoWsl(Arquivo)], TEMPO_WSL)
    if Processo.returncode != 0:
        Causa = wsl_lote.TextoDoWsl(Processo.stdout + Processo.stderr)[:400]
        raise ErroMedicao(f"{Descricao} no WSL saiu {Processo.returncode} (2 = gcc-musl): {Causa}")


def ConferirRodada(Pasta: Path, Base: str, Nome: str, Rotulo: str, Esperado: Path) -> None:
    """Rodada do WSL: .rc e .out existem, rc 0 e saida (LF) igual ao <nome>.esperado.txt. Sem o arquivo, o WSL parou no meio."""
    for Sufixo in (".rc", ".out"):
        if not (Pasta / (Base + Sufixo)).is_file():
            raise ErroMedicao(f"{Nome}/{Rotulo}: sem o arquivo {Base}{Sufixo} (o WSL parou antes?)")
    Codigo = int((Pasta / (Base + ".rc")).read_text(encoding="utf-8").strip())
    if Codigo != 0:
        raise ErroMedicao(f"{Nome}/{Rotulo}: programa saiu {Codigo} ({Base})")
    Saida = Normalizar((Pasta / (Base + ".out")).read_bytes())
    if Saida != Normalizar(Esperado.read_bytes()):
        raise ErroMedicao(f"{Nome}/{Rotulo}: saida diferente do .esperado.txt ({Base})")


def LerMedidaIr(Pasta: Path, Item: dict, Rotulo: str) -> dict:
    """REPETICOES_IR rodadas do perf, cada uma conferida; devolve a mediana das instrucoes e a dispersao (max-min)/min."""
    Nome = Item["nome"]
    Irs = []
    for Rodada in range(1, REPETICOES_IR + 1):
        Base = f"{Nome}.{Rotulo}.{Rodada}"
        ConferirRodada(Pasta / PASTA_IR, Base, Nome, Rotulo, Item["esperado"])
        Perf = Pasta / PASTA_IR / (Base + ".perf")
        if not Perf.is_file():
            raise ErroMedicao(f"{Nome}/{Rotulo}: sem o arquivo do perf ({Base})")
        Irs.append(LerInstrucoesPerf(Perf.read_text(encoding="utf-8")))
    Menor = min(Irs)
    return {"ir": statistics.median(Irs), "dispersao": DividirValor(max(Irs) - Menor, Menor)}


def ResumoIr(Medidas: dict) -> dict:
    """Medidas[nome] = {'x64': {ir, dispersao}, 'O2': {...}} -> razao Forja/gcc por bench e geomedias (total e codegen)."""
    Benchs = []
    for Nome in sorted(Medidas):
        X64, O2 = Medidas[Nome]["x64"], Medidas[Nome]["O2"]
        Benchs.append({"nome": Nome, "ir_x64": X64["ir"], "ir_O2": O2["ir"], "razao": DividirValor(X64["ir"], O2["ir"]),
                       "dispersao_x64": X64["dispersao"], "dispersao_O2": O2["dispersao"]})
    return {"benchs": Benchs, "geomedia_total": Geomedia([B["razao"] for B in Benchs]),
            "geomedia_codegen": Geomedia(RazoesCodegen(Benchs))}


def RodarIr(Exe: Path, Pasta: Path) -> dict:
    """Ir do ELF x64 da Forja e do ELF gcc-musl -O2 congelado, nos 14 benches, numa chamada do WSL. Pasta = pasta de trabalho do Windows."""
    (Pasta / PASTA_IR).mkdir(parents=True, exist_ok=True)
    Itens = ConstruirBenchs(Exe, Pasta)
    ExecutarScriptWsl(Pasta, "ir_wsl.sh", ScriptIr(Itens, Pasta), "ir")
    Medidas = {Item["nome"]: {Rotulo: LerMedidaIr(Pasta, Item, Rotulo) for Rotulo in ("x64", "O2")} for Item in Itens}
    Resultado = ResumoIr(Medidas)
    Resultado["sha_forja"] = ShaCurta(Exe)
    return Resultado


def RodarIrComTrava(Exe: Path, Pasta: Path) -> dict:
    """Entrada do medir_bench --contra: sem a distro ou com lock de outro medidor levanta ErroMedicao (quem chama avisa e segue)."""
    if not wsl_lote.DistroDisponivel():
        raise ErroMedicao("sem a distro alpine-build (WSL): Ir nao medido")
    with Trava():
        return RodarIr(Exe, Pasta)


def RodarPorFuncao(Exe: Path, Pasta: Path, Nome: str) -> dict:
    """Top funcoes por Ir (cachegrind) dos dois binarios de um bench: Forja x64 e gcc-musl -O2."""
    if not (PASTA_BENCH / f"{Nome}.fj").is_file():
        raise ErroMedicao(f"bench inexistente em {PASTA_BENCH}: {Nome}")
    (Pasta / PASTA_FUNCAO).mkdir(parents=True, exist_ok=True)
    Itens = ConstruirBenchs(Exe, Pasta, [Nome])
    ExecutarScriptWsl(Pasta, f"funcao_{Nome}.sh", ScriptPorFuncao(Itens[0], Pasta), "por-funcao")
    Funcoes = {}
    for Rotulo in ("x64", "O2"):
        ConferirRodada(Pasta / PASTA_FUNCAO, f"{Nome}.{Rotulo}", Nome, Rotulo, Itens[0]["esperado"])
        Cachegrind = Pasta / PASTA_FUNCAO / f"{Nome}.{Rotulo}.cg"
        if not Cachegrind.is_file():
            raise ErroMedicao(f"{Nome}/{Rotulo}: sem o arquivo do cachegrind")
        Por = SomarIrPorFuncao(Cachegrind.read_text(encoding="utf-8"))
        Funcoes[Rotulo] = {"ir_total": sum(Por.values()), "top": TopFuncoes(Por)}
    return {"nome": Nome, "sha_forja": ShaCurta(Exe), "funcoes": Funcoes}


def AcumularTempoWindows(Linha: dict, Rodadas: dict) -> None:
    """Uma linha do historico do medir_bench entra no grupo da sua rodada; a linha 'contra' so conta a forja A (a sob teste)."""
    if Linha.get("tipo") == "contra":
        if Linha.get("rotulo") == "A":
            Grupo = Rodadas.setdefault(f"contra {Linha['sha_forja']}", {"sha": Linha["sha_forja"], "amostras": {}})
            for Nome, X64 in Linha["x64_ms"].items():
                if Nome in Linha.get("O2_ms", {}):
                    Grupo["amostras"].setdefault(Nome, []).append(DividirValor(X64, Linha["O2_ms"][Nome]))
        return
    if "bench" not in Linha or Linha["bench"] == "*RESUMO*" or not Linha.get("ok"):
        return
    Grupo = Rodadas.setdefault(Linha["rodada"], {"sha": Linha["sha_forja"], "amostras": {}})
    Grupo["amostras"].setdefault(Linha["bench"], []).append(Linha["razao_x64_o2"])


def RazoesTempoWindows(Caminho: Path, Sha: str) -> dict | None:
    """Razao de tempo x64/O2 por bench (mediana das amostras) de UMA rodada do historico: a mais recente com o sha da forja; sem ela, a mais recente, com aviso. None se nao ha."""
    if not Caminho.is_file():
        return None
    Rodadas = {}
    for Texto in Caminho.read_text(encoding="utf-8").splitlines():
        if Texto.strip():
            AcumularTempoWindows(json.loads(Texto), Rodadas)
    if not Rodadas:
        return None
    Iguais = [Chave for Chave, Valor in Rodadas.items() if Valor["sha"] == Sha]
    Chave = Iguais[-1] if Iguais else list(Rodadas)[-1]
    Grupo = Rodadas[Chave]
    Razoes = {Nome: statistics.median(Valores) for Nome, Valores in Grupo["amostras"].items()}
    return {"rodada": Chave, "sha": Grupo["sha"], "mesmo_sha": bool(Iguais), "razoes": Razoes}


def CorrelacaoIrTempo(Benchs: list, Tempos: dict) -> dict:
    """Por bench: razao de Ir (Forja/gcc) contra a razao de tempo x64/O2 do medir_bench; desvio = razao_Ir / razao_tempo - 1; DIVERGE acima de 15%."""
    Saida = {}
    for B in Benchs:
        Tempo = Tempos.get(B["nome"])
        if Tempo is None:
            Saida[B["nome"]] = None
            continue
        Desvio = DividirValor(B["razao"], Tempo) - 1.0
        Saida[B["nome"]] = {"razao_tempo": Tempo, "desvio": Desvio, "diverge": abs(Desvio) > TOLERANCIA_CORRELACAO}
    return Saida


def ImprimirIr(Resultado: dict, Correlacao: dict | None) -> None:
    print(f"(F0a) {'bench':<18} {'Ir Forja':>14} {'Ir gcc-O2':>14} {'razao':>7} {'tempo Win':>10} {'desvio':>9}")
    for B in Resultado["benchs"]:
        C = (Correlacao or {}).get(B["nome"])
        Tempo, Desvio = "n/d", "n/d"
        if C is not None:
            Tempo = f"{C['razao_tempo']:.3f}"
            Desvio = f"{C['desvio'] * 100:+.1f}%" + (" DIVERGE" if C["diverge"] else "")
        print(f"(F0a) {B['nome']:<18} {B['ir_x64']:>14.0f} {B['ir_O2']:>14.0f} {B['razao']:>7.3f} {Tempo:>10} {Desvio:>9}")


def ImprimirFuncoes(Resultado: dict) -> None:
    for Rotulo, Descricao in (("x64", "Forja x64"), ("O2", "gcc-musl -O2")):
        Bloco = Resultado["funcoes"][Rotulo]
        print(f"(F0a) top {TOP_FUNCOES} funcoes de {Resultado['nome']} ({Descricao}; Ir total {Bloco['ir_total']}):")
        for Linha in Bloco["top"]:
            print(f"(F0a)   {Linha['ir']:>14} {Linha['pct']:6.2f}%  {Linha['funcao']}")


def ModoIr(Args, Exe: Path) -> int:
    """--ir e/ou --por-funcao: contagem de instrucoes (nao depende da maquina quieta para a contagem, mas o Trava segue por politica)."""
    Pasta = PASTA_TRABALHO / "ir"
    Pasta.mkdir(parents=True, exist_ok=True)
    Saida = {}
    try:
        with Trava():
            if Args.ir:
                Saida["ir"] = RodarIr(Exe, Pasta)
            if Args.por_funcao:
                Saida["por_funcao"] = RodarPorFuncao(Exe, Pasta, Args.por_funcao)
    except (ErroMedicao, subprocess.TimeoutExpired) as Erro:
        print(f"medir_linux: {Erro}")
        return 1
    if Args.ir:
        Resultado = Saida["ir"]
        Tempos = RazoesTempoWindows(Path(Args.tempos_windows), Resultado["sha_forja"])
        Correlacao = CorrelacaoIrTempo(Resultado["benchs"], Tempos["razoes"]) if Tempos else None
        ImprimirIr(Resultado, Correlacao)
        Divergentes = sum(1 for C in (Correlacao or {}).values() if C and C["diverge"])
        print(f"(F0a) geomedia total = {Resultado['geomedia_total']:.3f} ({len(Resultado['benchs'])} benches) | "
              f"geomedia codegen = {Resultado['geomedia_codegen']:.3f} (sem {BENCH_FORA_CODEGEN}) | sha {Resultado['sha_forja']}")
        if Tempos is None:
            print(f"(F0a) correlacao: sem rodada do medir_bench em {Args.tempos_windows}; nao comparei com o tempo")
        else:
            Aviso = "" if Tempos["mesmo_sha"] else f" (AVISO: sha {Tempos['sha']} != {Resultado['sha_forja']}; so referencia)"
            print(f"(F0a) correlacao com a rodada {Tempos['rodada']}: {Divergentes}/{len(Resultado['benchs'])} benches divergem >"
                  f"{TOLERANCIA_CORRELACAO * 100:.0f}%{Aviso}")
            Resultado["correlacao"] = Correlacao
        print(f"PORTAO ir: geomedia_total={Resultado['geomedia_total']:.3f} geomedia_codegen={Resultado['geomedia_codegen']:.3f} sha={Resultado['sha_forja']}")
    if Args.por_funcao:
        ImprimirFuncoes(Saida["por_funcao"])
    if Args.saida:
        Path(Args.saida).write_text(json.dumps(Saida, ensure_ascii=False, indent=1), encoding="utf-8")
        print(f"resultado gravado em {Args.saida}")
    return 0


def Selftest() -> int:
    Falhas = []
    if abs(Geomedia([2.0, 8.0]) - 4.0) > 1e-9:
        Falhas.append("geomedia")
    if Normalizar(b"a\r\nb\r\n") != b"a\nb\n":
        Falhas.append("normalizar")
    if abs(ResumoPares([{"linux": 10.0, "windows": 10.0}] * 4)["razao_medianas"] - 1.0) > 1e-9:
        Falhas.append("razao das medianas")
    Linux = ArgumentosBuild("linux", Path("a.fj"), Path("b.bin"), True)
    if "--release" not in Linux or Linux[-2:] != ["--target", "x86_64-linux"]:
        Falhas.append("argumentos linux")
    Windows = ArgumentosBuild("windows", Path("a.fj"), Path("b.bin"))
    if "--backend" not in Windows or "x86_64-linux" in Windows:
        Falhas.append("argumentos windows")
    Falhas += SelftestIr()
    print("medir_linux selftest:", "VERDE" if not Falhas else f"FALHOU {Falhas}")
    return 0 if not Falhas else 1


def SelftestIr() -> list:
    """Testes sem WSL: parse do perf e do cachegrind, script, razoes, geomedias e correlacao. Devolve a lista de falhas."""
    Falhas = []
    if DividirValor(5.0, 0) != 0.0 or abs(DividirValor(6.0, 3) - 2.0) > 1e-9:
        Falhas.append("DividirValor")
    if LerInstrucoesPerf("6032883,,instructions:u,487379,100.00,,\n") != 6032883:
        Falhas.append("perf: contagem")
    Recusou = False
    try:
        LerInstrucoesPerf("<not supported>,,instructions:u,0,100.00,,\n")
    except ErroMedicao:
        Recusou = True  # o teste espera a recusa do perf sem contagem
    if not Recusou:
        Falhas.append("perf: <not supported> deveria falhar")
    Cachegrind = "desc: x\nevents: Ir\n\nfl=(1) /a.c\nfn=(1) main\n1 10\n2 5\nfn=(2) foo\n+1 7\nfn=(1)\n* 3\nfi=(2) b.h\n5 100\nsummary: 125\n"
    if SomarIrPorFuncao(Cachegrind) != {"main": 118, "foo": 7}:
        Falhas.append(f"cachegrind: {SomarIrPorFuncao(Cachegrind)}")
    if [T["funcao"] for T in TopFuncoes({"a": 5, "b": 10, "c": 1}, 2)] != ["b", "a"]:
        Falhas.append("top funcoes")
    Item = {"nome": "fib_rec", "arquivos": {"x64": Path("C:/a/fib_rec.x64.elf"), "o0": Path("C:/a/fib_rec.o0"), "o2": Path("C:/a/fib_rec.o2")}, "esperado": Path("C:/a/fib_rec.esperado.txt")}
    Script = ScriptIr([Item], Path("C:/a/ir"))
    if "perf stat -x, -e instructions:u" not in Script or "/mnt/c/a/fib_rec.o2" not in Script or "gcc " in Script or "/mnt/c/a/ir/perf" not in Script:
        Falhas.append("script ir")
    Funcao = ScriptPorFuncao(Item, Path("C:/a/ir"))
    if "valgrind --tool=cachegrind --cache-sim=no" not in Funcao or "fib_rec.x64.elf" not in Funcao:
        Falhas.append("script por-funcao")
    Medidas = {"fib_rec": {"x64": {"ir": 200.0, "dispersao": 0.0}, "O2": {"ir": 100.0, "dispersao": 0.0}},
               "formato_texto": {"x64": {"ir": 300.0, "dispersao": 0.0}, "O2": {"ir": 100.0, "dispersao": 0.0}}}
    Resumo = ResumoIr(Medidas)
    if abs(Resumo["geomedia_total"] - math.sqrt(6.0)) > 1e-9 or abs(Resumo["geomedia_codegen"] - 2.0) > 1e-9:
        Falhas.append(f"geomedias: {Resumo}")
    Correlacao = CorrelacaoIrTempo([{"nome": "a", "razao": 1.2}, {"nome": "b", "razao": 1.2}], {"a": 1.0, "b": 1.1})
    if not Correlacao["a"]["diverge"] or Correlacao["b"]["diverge"]:
        Falhas.append(f"correlacao: {Correlacao}")
    Historico = Path(tempfile.mkdtemp(prefix="selftest_tempo_")) / "historico.jsonl"
    Linhas = [{"tipo": "contra", "rotulo": "A", "sha_forja": "s1", "x64_ms": {"a": 20.0}, "O2_ms": {"a": 10.0}},
              {"tipo": "contra", "rotulo": "B", "sha_forja": "s1", "x64_ms": {"a": 5.0}, "O2_ms": {"a": 10.0}}]
    Historico.write_text("".join(json.dumps(L) + "\n" for L in Linhas), encoding="utf-8")
    Tempos = RazoesTempoWindows(Historico, "s1")
    if Tempos is None or Tempos["razoes"] != {"a": 2.0} or not Tempos["mesmo_sha"]:
        Falhas.append(f"razao de tempo (contra A): {Tempos}")
    return Falhas


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Medicoes do alvo x86_64-linux (M5 P30): build e bench; Ir e funcoes (M7b F0a).")
    Analisador.add_argument("--build", action="store_true", help="01_ola linux e corpus_200000 linux x windows (ABBA)")
    Analisador.add_argument("--bench", action="store_true", help="8 benchs: x64 linux contra gcc-musl -O0 e -O2")
    Analisador.add_argument("--ir", action="store_true", help="Ir (perf stat instructions:u) do ELF x64 da Forja e do gcc-musl -O2, 14 benches")
    Analisador.add_argument("--por-funcao", metavar="BENCH", help="top 10 funcoes por Ir (cachegrind) dos dois binarios do bench")
    Analisador.add_argument("--tempos-windows", default=str(HISTORICO_BENCH), metavar="JSONL", help="historico do medir_bench para a correlacao com o tempo (--ir)")
    Analisador.add_argument("--forja", help="forja.exe ABSOLUTO (use a copia fiel em %%TEMP%%)")
    Analisador.add_argument("--saida", help="grava o resultado em JSON")
    Analisador.add_argument("--selftest", action="store_true")
    Analisador.add_argument("--tempo-wsl", help=argparse.SUPPRESS)  # modo interno: roda dentro do WSL com o python3 da distro
    Args = Analisador.parse_args()
    if Args.selftest:
        return Selftest()
    if Args.tempo_wsl:
        print(json.dumps(TempoNoWsl(Path(Args.tempo_wsl)), ensure_ascii=False))
        return 0
    if not (Args.build or Args.bench or Args.ir or Args.por_funcao) or not Args.forja:
        Analisador.print_help()
        return 2
    Exe = Path(Args.forja)
    if not Exe.is_absolute() or not Exe.is_file():
        print(f"X0001 --forja precisa ser o caminho ABSOLUTO de um arquivo existente: {Args.forja}")
        return 2
    if not wsl_lote.DistroDisponivel():
        print("medir_linux: sem a distro alpine-build (WSL): nao mede")
        return 1
    if Args.ir or Args.por_funcao:
        return ModoIr(Args, Exe)
    Pasta = PASTA_TRABALHO / ("build" if Args.build else "bench")
    Pasta.mkdir(parents=True, exist_ok=True)
    try:
        with Trava():
            Resultado = RodarBuild(Exe, Pasta) if Args.build else RodarBench(Exe, Pasta)
    except (ErroMedicao, subprocess.TimeoutExpired) as Erro:
        print(f"medir_linux: {Erro}")
        return 1
    if Args.build:
        R = Resultado["corpus"]
        print(f"PORTAO build: 01_ola_linux={Resultado['ola']['linux']:.1f} ms (meta <={META_OLA_MS}) "
              f"corpus_razao_medianas={R['razao_medianas']:.3f} (meta <={META_CORPUS_RAZAO}; pareada mediana={R['razao_pareada_mediana']:.3f}, "
              f"IQR/med={R['razao_pareada_iqr_sobre_mediana']:.3f}) corpus_linux_mediana={R['mediana_linux_ms']:.0f} ms corpus_windows_mediana={R['mediana_windows_ms']:.0f} ms")
    else:
        print(f"PORTAO bench: geomedia_x64/O0={Resultado['geomedia_O0']:.3f} (meta <={META_BENCH_RAZAO}) geomedia_x64/O2={Resultado['geomedia_O2']:.3f} (informado)")
    if Args.saida:
        Path(Args.saida).write_text(json.dumps(Resultado, ensure_ascii=False, indent=1), encoding="utf-8")
        print(f"resultado gravado em {Args.saida}")
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
