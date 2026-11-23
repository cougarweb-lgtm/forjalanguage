#!/usr/bin/env python3
"""verificar_libc.py - M5b P39-B2: programas Forja com foreign no alvo linux ligados ao musl (crt1.o + libc.a da semente + objeto de entrada libc).

Objetivo: cada fixture <nome>.fj (nesta pasta) constroi em dev e release com --target x86_64-linux e roda no alpine-build; a saida tem de ser
<nome>.esperado byte a byte, e o codigo de saida <nome>.codigo (padrao 0). Exemplo: stdout_ordem_panico sai 101 com a saida em ordem.
Pre-requisito: language\\third\\musl\\x86_64 (crt1.o, libc.a; LEIAME) e o forja.exe ao lado de language\\compiler.
Uso: python -I verificar_libc.py --forja <copia do forja.exe>   (exit 0 = tudo verde; PULADA sem WSL)
Versao: 0.2.0
Changelog: 0.2.0 (15/11/2026) M5b P39-B3 - fixtures copia_memcpy (memcpy do musl + forja_memcpy), qsort_callback (comparador Forja), errno (__errno_location), ctor_init_array (lib ctor_libc.a com construtor).
Changelog: 0.1.0 (15/11/2026) M5b P39-B2 - primeira versao (strlen, stdout+panic, malloc/strtod/getenv).
"""
import argparse
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI.parent / "abi_sysv"))  # verificar_abi_sysv (Construir, MODOS)
sys.path.insert(0, str(AQUI.parent))               # wsl_lote
import verificar_abi_sysv as Chamada  # noqa: E402
import wsl_lote  # noqa: E402

SUITE = "libc"


def FixturesDaPasta() -> list:
    return sorted(AQUI.glob("*.fj"))


def ConstruirFixture(Forja: Path, Fonte: Path, Modo: str, Pasta: Path):
    """Constroi um fixture no modo; devolve (elf, erro)."""
    Saida = Pasta / f"{Fonte.stem}_{Modo}.elf"
    Erro = Chamada.Construir(Forja, Fonte, Modo, Saida)
    return (Saida, None) if not Erro else (None, Erro)


def Avaliar(Esperado: bytes, CodigoEsperado: int, R) -> str | None:
    if R is None:
        return "sem resultado do lote"
    if R.Sinal is not None:
        return f"morreu por sinal {R.Sinal} (stderr: {R.Stderr[:200]!r})"
    if R.Exit != CodigoEsperado:
        return f"exit {R.Exit} (esperado {CodigoEsperado}): {R.Stderr[:200]!r}"
    if R.Stdout != Esperado:
        return f"stdout diverge: obtido {R.Stdout[:200]!r} esperado {Esperado[:200]!r}"
    return None


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Programas Forja ligados ao musl (P39-B2).")
    Analisador.add_argument("--forja", required=True, help="copia do forja.exe")
    Forja = Path(Analisador.parse_args().forja).resolve()
    if not wsl_lote.DistroDisponivel():
        print("libc: PULADA (WSL/distro ausente)")
        return 0
    Pasta = Path(tempfile.gettempdir()) / "forja_libc"
    Pasta.mkdir(parents=True, exist_ok=True)
    Falhas, Programas, Esperados = [], [], {}
    for Fonte in FixturesDaPasta():
        Codigo = int((AQUI / f"{Fonte.stem}.codigo").read_text().strip()) if (AQUI / f"{Fonte.stem}.codigo").is_file() else 0
        Esperados[Fonte.stem] = ((AQUI / f"{Fonte.stem}.esperado").read_bytes(), Codigo)
        for Modo in Chamada.MODOS:
            Elf, Erro = ConstruirFixture(Forja, Fonte, Modo, Pasta)
            if Erro:
                Falhas.append((f"{Fonte.stem}/{Modo}", f"build: {Erro}"))
            else:
                Programas.append(wsl_lote.Programa(f"{Fonte.stem}/{Modo}", Elf))
    Resultados = {R.Nome: R for R in wsl_lote.ExecutarLote(Programas, SUITE)}
    for Programa in Programas:
        Base = Programa.Nome.split("/")[0]
        Esperado, Codigo = Esperados[Base]
        Motivo = Avaliar(Esperado, Codigo, Resultados.get(Programa.Nome))
        if Motivo:
            Falhas.append((Programa.Nome, Motivo))
    for Rotulo, Motivo in Falhas:
        print(f"FALHOU {Rotulo}: {Motivo}")
    Total = len(FixturesDaPasta()) * len(Chamada.MODOS)
    print(f"libc: {Total - len(Falhas)}/{Total}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Principal())
