#!/usr/bin/env python3
"""conferir_display.py - oraculo fixo do Display curto de real (P3 do marco Ryu).

Gera 1.000.000 de f64 e 1.000.000 de f32 pelo splitmix64 (semente 20261116, mesmo fluxo de gerar_display.fj:
um gerador so, f64 primeiro e f32 no passo seguinte; nao finitos pulados sem contar), calcula o texto esperado com
numpy.format_float_positional(x, unique=True, trim='-') e compara a saida do programa em tres modos:
dev x64 (forja run), release x64 (forja run --release) e ELF linux via WSL (forja build --target x86_64-linux + wsl_lote).

Uso:       python conferir_display.py --forja C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/compiler/forja.exe [--modos dev,release,elf] [--gravar-sha]
Selftest:  python conferir_display.py --selftest   (sem forja e sem WSL; exit 0 = verde)
Reversao:  apagar esta pasta tests\\nativo\\ratio_display.

Versao: 0.1.1
Changelog: 0.1.1 (18/11/2026) 0.21.0 F3: modo release passa a ser x64 (backend C aposentado); rotulo e docstring ajustados, oraculo numpy e sha256 iguais.
           0.1.0 (16/11/2026) P3 Ryu - primeira versao: splitmix64 escalar (referencia lenta) + numpy (vetorial), modos dev/release/elf,
           sha256 do esperado em display_1m.sha256 (gravado na 1a execucao; depois so confere; --gravar-sha regrava).
"""
import argparse
import hashlib
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np

PASTA = Path(__file__).resolve().parent
PASTA_LINUX = PASTA.parent / "linux"
FJ_GERADOR = PASTA / "gerar_display.fj"
ARQ_SHA = PASTA / "display_1m.sha256"
SEMENTE = 20261116
QUANTIDADE = 1000000
GAMA = 0x9E3779B97F4A7C15
MULT_1 = 0xBF58476D1CE4E5B9
MULT_2 = 0x94D049BB133111EB
MASCARA_64 = (1 << 64) - 1
LIMITE_SEGUNDOS = 900
MODOS_PADRAO = "dev,release,elf"

sys.path.insert(0, str(PASTA_LINUX))


def SplitmixEscalar(Passos):
    """Referencia lenta com inteiros Python mascarados a 64 bits; prova o vetorial no selftest."""
    Estado = SEMENTE
    Saida = []
    for _ in range(Passos):
        Estado = (Estado + GAMA) & MASCARA_64
        Z = ((Estado ^ (Estado >> 30)) * MULT_1) & MASCARA_64
        Z = ((Z ^ (Z >> 27)) * MULT_2) & MASCARA_64
        Saida.append(Z ^ (Z >> 31))
    return Saida


def SplitmixVetor(Passos):
    """Mesmo fluxo em uint64 do numpy (soma e multiplicacao com volta). O passo k (k=1..) tem estado SEMENTE + k*GAMA."""
    Indices = np.arange(1, Passos + 1, dtype=np.uint64)
    Z = np.uint64(SEMENTE) + Indices * np.uint64(GAMA)
    Z = Z ^ (Z >> np.uint64(30))
    Z = Z * np.uint64(MULT_1)
    Z = Z ^ (Z >> np.uint64(27))
    Z = Z * np.uint64(MULT_2)
    return Z ^ (Z >> np.uint64(31))


def MascaraF64(Z):
    """True onde o f64 e finito: expoente (bits 52..62) diferente de 0x7FF."""
    return ((Z >> np.uint64(52)) & np.uint64(0x7FF)) != np.uint64(0x7FF)


def MascaraF32(Bits32):
    """True onde o f32 e finito: expoente (bits 23..30) diferente de 0xFF."""
    return ((Bits32 >> np.uint32(23)) & np.uint32(0xFF)) != np.uint32(0xFF)


def SelecionarValores(Quantidade):
    """Devolve (bits64, bits32) com Quantidade finitos cada. O f32 comeca no passo seguinte ao ultimo f64 consumido."""
    Passos = 3 * Quantidade
    Z = SplitmixVetor(Passos)
    Posicoes64 = np.flatnonzero(MascaraF64(Z))
    if len(Posicoes64) < Quantidade:
        raise RuntimeError(f"fluxo curto para f64: {len(Posicoes64)} < {Quantidade}; aumente Passos")
    Ultimo = int(Posicoes64[Quantidade - 1])
    Bits64 = Z[Posicoes64[:Quantidade]]
    Restante = (Z[Ultimo + 1:] & np.uint64(0xFFFFFFFF)).astype(np.uint32)
    Posicoes32 = np.flatnonzero(MascaraF32(Restante))
    if len(Posicoes32) < Quantidade:
        raise RuntimeError(f"fluxo curto para f32: {len(Posicoes32)} < {Quantidade}; aumente Passos")
    return Bits64, Restante[Posicoes32[:Quantidade]]


def TextosEsperados(Bits64, Bits32):
    """Texto esperado por valor: posicional, mais curto que volta ao mesmo valor, '-0' para -0.0."""
    Valores64 = Bits64.view(np.float64)
    Valores32 = Bits32.view(np.float32)
    Textos = [np.format_float_positional(X, unique=True, trim='-') for X in Valores64]
    Textos += [np.format_float_positional(X, unique=True, trim='-') for X in Valores32]
    return Textos


def GerarOraculo():
    """Monta (textos, bits64, bits32) do oraculo; imprime o tempo da geracao."""
    Inicio = time.perf_counter()
    Bits64, Bits32 = SelecionarValores(QUANTIDADE)
    Textos = TextosEsperados(Bits64, Bits32)
    print(f"oraculo gerado em {time.perf_counter() - Inicio:.1f} s")
    return Textos, Bits64, Bits32


def ConferirSha(Blob, Gravar):
    """Grava o sha256 na 1a vez (ou com --gravar-sha); depois so confere. Devolve (digest, estado)."""
    Digest = hashlib.sha256(Blob).hexdigest()
    if Gravar or not ARQ_SHA.exists():
        ARQ_SHA.write_text(Digest + "\n", encoding="utf-8")
        return Digest, "gravado"
    Antigo = ARQ_SHA.read_text(encoding="utf-8").strip()
    if Antigo != Digest:
        return Digest, "DIVERGE: oraculo mudou, sha gravado=" + Antigo
    return Digest, "ok"


def ExecutarForja(Forja, Modo):
    """forja run (dev x64) ou forja run --release (x64 desde a 0.21.0). Devolve (saida, segundos, aviso)."""
    Argumentos = [str(Forja), "run"]
    if Modo == "release":
        Argumentos.append("--release")
    Argumentos.append(str(FJ_GERADOR))
    Inicio = time.perf_counter()
    Proc = subprocess.run(Argumentos, capture_output=True, timeout=LIMITE_SEGUNDOS, check=False)
    Segundos = time.perf_counter() - Inicio
    Aviso = "" if Proc.returncode == 0 else f"exit={Proc.returncode} stderr={Proc.stderr[:300]!r}"
    return Proc.stdout, Segundos, Aviso


def ExecutarElf(Forja, Pasta):
    """ELF linux: forja build --target x86_64-linux, depois execucao em lote no WSL. Saida None = o modo nao rodou."""
    import wsl_lote
    if not wsl_lote.DistroDisponivel():
        return None, 0.0, "PULADO (WSL/distro alpine-build ausente)"
    Elf = Pasta / "gerar_display_elf"
    Inicio = time.perf_counter()
    Build = subprocess.run([str(Forja), "build", "--target", "x86_64-linux", str(FJ_GERADOR), "-o", str(Elf)],
                           capture_output=True, timeout=LIMITE_SEGUNDOS, check=False)
    print(f"  elf build: {time.perf_counter() - Inicio:.1f} s (exit={Build.returncode})")
    if Build.returncode != 0:
        return None, time.perf_counter() - Inicio, f"build ELF exit={Build.returncode} stdout={Build.stdout[:300]!r}"
    Inicio = time.perf_counter()
    try:
        Resultado = wsl_lote.ExecutarLote([wsl_lote.Programa(Nome="display_1m", Elf=Elf, Limite=LIMITE_SEGUNDOS)], "ratio_display")[0]
    except RuntimeError as Erro:
        return None, time.perf_counter() - Inicio, str(Erro)
    Segundos = time.perf_counter() - Inicio
    Aviso = "" if Resultado.Exit == 0 else f"ELF exit={Resultado.Exit} estourou={Resultado.Estourou} stderr={Resultado.Stderr[:300]!r}"
    return Resultado.Stdout, Segundos, Aviso


def ExecutarModo(Forja, Modo, Pasta):
    """Despacha o modo. Devolve (saida ou None, segundos, aviso)."""
    if Modo == "elf":
        return ExecutarElf(Forja, Pasta)
    return ExecutarForja(Forja, Modo)


def SepararLinhas(Bruto):
    """Bytes da saida -> lista de linhas (sem o \\r do Windows e sem a linha vazia final)."""
    Texto = Bruto.decode("utf-8", "replace")
    Linhas = [Linha.rstrip("\r") for Linha in Texto.split("\n")]
    if Linhas and Linhas[-1] == "":
        Linhas.pop()
    return Linhas


def DescreverPosicao(Indice, Bits64, Bits32):
    """Indice global -> (tipo, indice dentro do tipo, bits, largura em hex)."""
    Quantidade = len(Bits64)
    if Indice < Quantidade:
        return "f64", Indice, int(Bits64[Indice]), 16
    return "f32", Indice - Quantidade, int(Bits32[Indice - Quantidade]), 8


def CompararModo(Modo, Oraculo, Saida):
    """Compara linha a linha com o esperado; imprime a 1a divergencia e devolve o numero de falhas."""
    Textos, Bits64, Bits32 = Oraculo
    Linhas = SepararLinhas(Saida)
    Falhas = 0
    Primeira = None
    for Indice in range(max(len(Textos), len(Linhas))):
        Esperado = Textos[Indice] if Indice < len(Textos) else "<ausente>"
        ObtidoLinha = Linhas[Indice] if Indice < len(Linhas) else "<ausente>"
        if Esperado == ObtidoLinha:
            continue
        Falhas += 1
        if Primeira is None:
            Primeira = Indice
            Tipo, Relativo, Bits, Largura = DescreverPosicao(Indice, Bits64, Bits32)
            print(f"  DIVERGENCIA modo={Modo} tipo={Tipo} indice={Relativo} bits=0x{Bits:0{Largura}x} "
                  f"esperado={Esperado!r} obtido={ObtidoLinha!r}")
    print(f"modo {Modo}: {len(Linhas)} linhas recebidas, {len(Textos)} esperadas, {Falhas} falha(s)")
    return Falhas


def Selftest():
    """Prova o oraculo sem forja nem WSL: escalar x vetorial, casos de borda e a selecao sem nao finitos."""
    Falhas = 0
    Escalar = SplitmixEscalar(1000)
    if Escalar != SplitmixVetor(1000).tolist():
        Falhas += 1
        print("selftest: splitmix escalar difere do vetorial")
    Bits64, Bits32 = SelecionarValores(1000)
    Fluxo = SplitmixEscalar(3000)
    Finitos64 = [B for B in Fluxo if ((B >> 52) & 0x7FF) != 0x7FF]
    if [int(B) for B in Bits64] != Finitos64[:1000]:
        Falhas += 1
        print("selftest: selecao f64 difere do filtro escalar")
    Casos = [(np.float64(1e15), "1000000000000000"), (np.float64(0.5), "0.5"), (np.float64(3.0), "3"),
             (np.float64(-0.0), "-0"), (np.float32(0.1), "0.1"), (np.float64(0.1 + 0.2), "0.30000000000000004")]
    for Valor, Esperado in Casos:
        Obtido = np.format_float_positional(Valor, unique=True, trim='-')
        if Obtido != Esperado:
            Falhas += 1
            print(f"selftest: {Valor!r} -> {Obtido!r}, esperado {Esperado!r}")
    print(f"conferir_display selftest: {'ok' if Falhas == 0 else 'FALHOU'} ({Falhas} falha(s))")
    return 0 if Falhas == 0 else 1


def Principal():
    Analisador = argparse.ArgumentParser(description="Oraculo fixo do Display curto de real: 1M f64 + 1M f32 contra numpy.")
    Analisador.add_argument("--forja", help="caminho ABSOLUTO do forja.exe (obrigatorio fora do --selftest)")
    Analisador.add_argument("--modos", default=MODOS_PADRAO, help="dev, release e/ou elf, separados por virgula")
    Analisador.add_argument("--gravar-sha", action="store_true", help="regrava display_1m.sha256 com o esperado atual")
    Analisador.add_argument("--selftest", action="store_true", help="prova o oraculo sem forja nem WSL")
    Args = Analisador.parse_args()
    if Args.selftest:
        return Selftest()
    if not Args.forja or not Path(Args.forja).is_absolute() or not Path(Args.forja).is_file():
        print("conferir_display: --forja ABSOLUTO e obrigatorio (ex.: C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/compiler/forja.exe)")
        return 2
    Forja = Path(Args.forja)
    Modos = [M.strip() for M in Args.modos.split(",") if M.strip()]
    if any(M not in MODOS_PADRAO.split(",") for M in Modos):
        print(f"conferir_display: modo desconhecido em --modos ({Args.modos}); use dev, release e/ou elf")
        return 2
    Oraculo = GerarOraculo()
    Textos, Bits64, Bits32 = Oraculo
    Blob = ("\n".join(Textos) + "\n").encode("utf-8")
    Digest, Estado = ConferirSha(Blob, Args.gravar_sha)
    print(f"oraculo: {len(Bits64)} f64 + {len(Bits32)} f32, {len(Blob)} bytes, sha256={Digest} ({Estado})")
    Falhas = 1 if Estado.startswith("DIVERGE") else 0
    Pasta = Path(tempfile.mkdtemp(prefix="conferir_display_"))
    for Modo in Modos:
        Saida, Segundos, Aviso = ExecutarModo(Forja, Modo, Pasta)
        if Saida is None:
            print(f"modo {Modo}: {Aviso}" if Aviso.startswith("PULADO") else f"modo {Modo}: FALHA {Aviso}")
            Falhas += 0 if Aviso.startswith("PULADO") else 1
            continue
        if Aviso:
            Falhas += 1
            print(f"modo {Modo}: FALHA de execucao {Aviso}")
        Falhas += CompararModo(Modo, Oraculo, Saida)
        print(f"modo {Modo}: {Segundos:.1f} s")
    print(f"conferir_display: {Falhas} falha(s)")
    return 0 if Falhas == 0 else 1


if __name__ == "__main__":
    sys.exit(Principal())
