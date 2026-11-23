#!/usr/bin/env python3
"""verificar_callback_agregado.py - M5b P38c: callback SysV com agregado por valor (24 B MEMORY, 12/16/8 B em registradores) e retorno MEMORY/registrador,
com reentrancia (a Forja chama cba_chama24 de dentro de um callback chamado pelo C), no alpine-build.

Objetivo: callback_agregado.fj imprime callback_agregado.esperado com exit 0, em dev e release; o oraculo C (oraculo_agregado.c) tambem tem que imprimir
o mesmo arquivo (o esperado e conferido pelo C, nao so pela Forja). Cada fonte e construida 2 vezes por modo e os 2 ELFs tem de ser identicos.
Pre-requisito: construir_callback_agregado.sh (vendor/linux/libcb_agregado.a). Uso: python -I verificar_callback_agregado.py --forja <copia do forja.exe>
Verificacao: exit 0 = tudo verde (2 modos + oraculo).
Versao: 0.1.0
Changelog: 0.1.0 (15/11/2026) M5b P38c - primeira versao.
"""
import argparse
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI.parent))        # verificar_abi_sysv (Construir, MODOS)
sys.path.insert(0, str(AQUI.parent.parent))  # wsl_lote
import verificar_abi_sysv as Chamada  # noqa: E402
import wsl_lote  # noqa: E402

SUITE = "callback_agregado"
NOME = "callback_agregado"
TEMPO_GCC = 120


def ConstruirDuas(Forja: Path, Modo: str, Pasta: Path):
    """Constroi 2 vezes; devolve (elf, erro)."""
    Saidas = [Pasta / f"{NOME}_{Modo}_{N}.elf" for N in (1, 2)]
    for Saida in Saidas:
        Erro = Chamada.Construir(Forja, AQUI / f"{NOME}.fj", Modo, Saida)
        if Erro:
            return None, Erro
    if Saidas[0].read_bytes() != Saidas[1].read_bytes():
        return None, "2 builds do mesmo fonte deram ELFs diferentes"
    return Saidas[0], None


def CompilarOraculo(Pasta: Path) -> Path:
    """gcc do alpine-build sobre oraculo_agregado.c (que inclui cb_agregado.c): o C e a referencia."""
    Saida = Pasta / "oraculo_agregado.elf"
    Processo = wsl_lote.Wsl(["gcc", "-std=gnu11", "-O0", "-o", wsl_lote.CaminhoWsl(Saida), wsl_lote.CaminhoWsl(AQUI / "oraculo_agregado.c")], TEMPO_GCC)
    if Processo.returncode != 0 or not Saida.is_file():
        raise RuntimeError("oraculo nao compilou: " + wsl_lote.TextoDoWsl(Processo.stderr)[:400])
    return Saida


def Avaliar(Esperado: bytes, R) -> str | None:
    if R is None:
        return "sem resultado do lote"
    if R.Sinal is not None:
        return f"morreu por sinal {R.Sinal} (stderr: {R.Stderr[:200]!r})"
    if R.Exit != 0:
        return f"exit {R.Exit} (esperado 0): {R.Stderr[:200]!r}"
    if R.Stdout != Esperado:
        return f"stdout diverge: obtido {R.Stdout[:200]!r} esperado {Esperado[:200]!r}"
    return None


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Callback SysV com agregado e reentrancia (P38c).")
    Analisador.add_argument("--forja", required=True, help="copia do forja.exe")
    Forja = Path(Analisador.parse_args().forja).resolve()
    if not wsl_lote.DistroDisponivel():
        print("callback_agregado: PULADA (WSL/distro ausente)")
        return 0
    Esperado = (AQUI / f"{NOME}.esperado").read_bytes()
    Pasta = Path(tempfile.gettempdir()) / "forja_callback_agregado"
    Pasta.mkdir(parents=True, exist_ok=True)
    Falhas, Elfs = [], {}
    for Modo in Chamada.MODOS:
        Elf, Erro = ConstruirDuas(Forja, Modo, Pasta)
        if Erro:
            Falhas.append((Modo, Erro))
        else:
            Elfs[Modo] = Elf
    Programas = [wsl_lote.Programa(f"{NOME}/{Modo}", Elf) for Modo, Elf in Elfs.items()]
    Oraculo = CompilarOraculo(Pasta)
    Programas.append(wsl_lote.Programa("oraculo", Oraculo))
    Resultados = {R.Nome: R for R in wsl_lote.ExecutarLote(Programas, SUITE)}
    Motivo = Avaliar(Esperado, Resultados.get("oraculo"))
    if Motivo:
        Falhas.append(("oraculo", f"oraculo C diverge do esperado: {Motivo}"))
    for Modo in Elfs:
        Motivo = Avaliar(Esperado, Resultados.get(f"{NOME}/{Modo}"))
        if Motivo:
            Falhas.append((Modo, Motivo))
    for Modo, Motivo in Falhas:
        print(f"FALHOU {Modo}: {Motivo}")
    Total = len(Chamada.MODOS) + 1
    print(f"callback_agregado: {Total - len(Falhas)}/{Total}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Principal())
