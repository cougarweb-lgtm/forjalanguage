"""medir_custo_base.py - linha de base de custo do forja (M6e, P93a' ERRATA E5, D-M6e-10(3)).

Mede com a COPIA do forja em %TEMP% (nunca compiler\\forja.exe direto), sem outra trilha pesada rodando:
- custo_aloc (tests\\nativo\\m6e\\custo\\custo_aloc.fj): run dev e run release em pares ABBA (a ordem alterna a cada par),
  mediana de cada um e a razao run dev / run release. N e M sao lidos das constantes do proprio fixture.
- corpus_200000 (benchmark\\corpus\\gerado): build dev e forja check dev, mediana de cada um.
Com --gravar escreve tests\\nativo\\m6e\\CUSTO_BASE.txt (numero fixo, oraculo do D-M6e-10(3)). Gates dos passos C:
razao <= 1,05x a gravada; run release do custo_aloc <= 1,05x o gravado; forja check dev <= 1,05x o CHECK gravado.
Quantizacao do cronometro: ~16 ms (relatada, como no M6d); o custo_aloc roda ~1 s, o erro relativo fica perto de 1,6 %.

Versao: 0.2.0
Changelog:
  0.2.0 - M6e P93a' (ERRATA E5): gate de custo = custo_aloc (ABBA run dev/release, N e M do fixture, checksum);
          corpus_200000 so build dev e forja check dev (sem run); gates gravados no CUSTO_BASE.
  0.1.0 - M6e P93a: primeira versao (ABBA, mediana, build dev + run dev/release do corpus_200000, --gravar).
Uso: python tests\\nativo\\m6e\\medir_custo_base.py --forja <copia_forja.exe> [--pares 9] [--gravar]
Depende de: stdlib do Python; copia do forja em %TEMP% (nao rodar com outra trilha pesada).
"""

import argparse
import datetime
import hashlib
import re
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

PastaM6e = Path(__file__).resolve().parent
PastaForja = PastaM6e.parents[2]
Corpus = PastaForja / "benchmark" / "corpus" / "gerado" / "corpus_200000.fj"
Custo = PastaM6e / "custo" / "custo_aloc.fj"
ArquivoBase = PastaM6e / "CUSTO_BASE.txt"
TetoSegundos = 900
GateRazao = 1.05


def CronometrarComando(Comando, Cwd):
    """Roda o comando (falha = excecao) e devolve os segundos gastos."""
    Inicio = time.perf_counter()
    subprocess.run(Comando, check=True, capture_output=True, cwd=Cwd, timeout=TetoSegundos)
    return time.perf_counter() - Inicio


def ShaArquivo(Caminho):
    return hashlib.sha256(Path(Caminho).read_bytes()).hexdigest()


def LerConstante(Texto, Nome):
    """Valor de `const NOME: int = N` no fixture (N, M e itens por lista ficam la, nao aqui)."""
    Achado = re.search(r"const " + Nome + r": int = (\d+)", Texto)
    if Achado is None:
        raise ValueError("constante ausente em custo_aloc.fj: " + Nome)
    return int(Achado.group(1))


def ConstruirCustoAloc(Forja, Tmp):
    """Builds dev e release do custo_aloc (fora da cronometragem). Devolve (dev, release)."""
    Dev = Path(Tmp) / "custo_dev.exe"
    Release = Path(Tmp) / "custo_release.exe"
    subprocess.run([str(Forja), "build", str(Custo), "-o", str(Dev)], check=True, capture_output=True, cwd=Tmp, timeout=TetoSegundos)
    subprocess.run([str(Forja), "build", "--release", str(Custo), "-o", str(Release)], check=True, capture_output=True, cwd=Tmp, timeout=TetoSegundos)
    return Dev, Release


def ChecksumDe(Exe, Tmp):
    """Saida do exe release (checksum impresso); exit diferente de 0 vira excecao."""
    Saida = subprocess.run([str(Exe)], check=True, capture_output=True, text=True, cwd=Tmp, timeout=TetoSegundos)
    return Saida.stdout.replace("\r\n", "\n").strip()


def MedirRunsAbba(Dev, Release, Quantidade, Tmp):
    """Run dev e run release do custo_aloc em pares ABBA (a ordem alterna a cada par)."""
    Coleta = {"run_dev": [], "run_release": []}
    for Par in range(Quantidade):
        Ordem = ["run_dev", "run_release"] if Par % 2 == 0 else ["run_release", "run_dev"]
        for Rotulo in Ordem:
            Exe = Dev if Rotulo == "run_dev" else Release
            Coleta[Rotulo].append(CronometrarComando([str(Exe)], Tmp))
    return Coleta


def MedirCorpus(Forja, Tmp, Quantidade):
    """Build dev e forja check dev do corpus_200000, cada um Quantidade vezes."""
    Saida = Path(Tmp) / "corpus_dev.exe"
    Coleta = {"build_dev": [], "check_dev": []}
    for _ in range(Quantidade):
        Coleta["build_dev"].append(CronometrarComando([str(Forja), "build", str(Corpus), "-o", str(Saida)], Tmp))
    for _ in range(Quantidade):
        Coleta["check_dev"].append(CronometrarComando([str(Forja), "check", str(Corpus)], Tmp))
    return Coleta


def ResumirColeta(Coleta):
    """Mediana de cada metrica e a razao run dev / run release."""
    Resumo = {Chave: round(statistics.median(Valores), 3) for Chave, Valores in Coleta.items()}
    Resumo["razao"] = round(statistics.median(Coleta["run_dev"]) / statistics.median(Coleta["run_release"]), 4)
    return Resumo


def VersaoForja(Forja):
    Saida = subprocess.run([str(Forja), "--versao"], capture_output=True, text=True, timeout=60)
    return (Saida.stdout + Saida.stderr).strip()


def TextoBase(Forja, Resumo, Dados, Pares):
    """Monta o CUSTO_BASE.txt: numeros medidos, constantes do custo_aloc, checksum e os gates dos passos C."""
    Agora = datetime.datetime.now().strftime("%d/%m/%Y %H:%M")
    Linhas = [
        "CUSTO_BASE - M6e P93a' (ERRATA E5; D-M6e-10(3)): forja ANTES de qualquer passo C",
        f"forja medido: {VersaoForja(Forja)} | sha256 {ShaArquivo(Forja)}",
        f"custo_aloc: tests\\nativo\\m6e\\custo\\custo_aloc.fj | sha256 {ShaArquivo(Custo)}",
        f"  N = {Dados['N']} (new+delete) | M = {Dados['M']} (listas) | itens por lista = {Dados['ITENS']} | checksum = {Dados['CHECKSUM']}",
        f"corpus: benchmark\\corpus\\gerado\\corpus_200000.fj | sha256 {ShaArquivo(Corpus)}",
        f"ABBA {Pares} pares (custo_aloc) e {Pares} repeticoes (corpus) | mediana | quantizacao ~16 ms",
        f"medido em: {Agora} | script: tests\\nativo\\m6e\\medir_custo_base.py 0.2.0",
        f"run dev custo_aloc            = {Resumo['run_dev']} s (mediana)",
        f"run release custo_aloc        = {Resumo['run_release']} s (mediana)",
        f"razao run dev/release         = {Resumo['razao']}x (custo_aloc)",
        f"build dev corpus_200000       = {Resumo['build_dev']} s (mediana)",
        f"forja check dev corpus_200000 = {Resumo['check_dev']} s (mediana)",
        "GATES dos passos C (mesma maquina; mudou a maquina = regravar a base):",
        f"  razao dev/release <= {round(Resumo['razao'] * GateRazao, 4)}x (= 1,05 x {Resumo['razao']})",
        f"  run release custo_aloc <= {round(Resumo['run_release'] * GateRazao, 3)} s (= 1,05 x {Resumo['run_release']})",
        f"  forja check dev corpus_200000 <= {round(Resumo['check_dev'] * GateRazao, 3)} s (= 1,05 x CHECK gravado {Resumo['check_dev']})",
        "",
    ]
    return "\n".join(Linhas)


def Principal(Argumentos):
    Parser = argparse.ArgumentParser(description="linha de base de custo do forja (M6e P93a')")
    Parser.add_argument("--forja", required=True, help="copia do forja em %TEMP%")
    Parser.add_argument("--pares", type=int, default=9)
    Parser.add_argument("--gravar", action="store_true", help="grava tests\\nativo\\m6e\\CUSTO_BASE.txt")
    Opcoes = Parser.parse_args(Argumentos)
    Forja = Path(Opcoes.forja)
    if not Forja.is_file() or not Corpus.is_file() or not Custo.is_file():
        print("falta o forja, o corpus ou o custo_aloc: " + str(Forja) + " | " + str(Corpus) + " | " + str(Custo))
        return 2
    Texto = Custo.read_text(encoding="utf-8")
    Dados = {"N": LerConstante(Texto, "CUSTO_N"), "M": LerConstante(Texto, "CUSTO_M"), "ITENS": LerConstante(Texto, "CUSTO_ITENS")}
    with tempfile.TemporaryDirectory() as Tmp:
        Dev, Release = ConstruirCustoAloc(Forja, Tmp)
        Dados["CHECKSUM"] = ChecksumDe(Release, Tmp)
        Coleta = MedirRunsAbba(Dev, Release, Opcoes.pares, Tmp)
        Coleta.update(MedirCorpus(Forja, Tmp, Opcoes.pares))
    Resumo = ResumirColeta(Coleta)
    Saida = TextoBase(Forja, Resumo, Dados, Opcoes.pares)
    print(Saida)
    if Opcoes.gravar:
        ArquivoBase.write_text(Saida, encoding="utf-8")
        print("gravado: " + str(ArquivoBase))
    return 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
