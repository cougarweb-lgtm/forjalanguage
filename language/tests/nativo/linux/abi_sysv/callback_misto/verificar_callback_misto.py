#!/usr/bin/env python3
"""verificar_callback_misto.py - M5b P38b: valor de proc no alvo linux e panic dentro de callback SysV (dev e release, no alpine-build).

Objetivo: misto.fj (a Forja chama o stub fjt_ por CHAMA_IND SysV e o C chama o mesmo ponteiro; 8 argumentos com 2 na pilha; ponteiro de funcao C
chamado pela Forja) imprime misto.esperado com exit 0; panico_cb.fj (assert falso dentro do callback chamado pelo C) imprime panico_cb.esperado, sai
com 101 e traz R0003 no stderr, sem SIGSEGV. Cada fonte e construida 2 vezes por modo e os 2 ELFs tem de ser identicos (determinismo).
Pre-requisito: construir_abi_sysv_cb.sh (callback_misto/vendor/linux/libcb_misto.a). Uso: python -I verificar_callback_misto.py --forja <copia do forja.exe>
Verificacao: exit 0 = tudo verde (4 casos: 2 fontes x dev/release).
Versao: 0.1.0
Changelog: 0.1.0 (15/11/2026) M5b P38b - primeira versao.
"""
import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI.parent))        # verificar_abi_sysv
sys.path.insert(0, str(AQUI.parent.parent))  # wsl_lote
import verificar_abi_sysv as Chamada  # noqa: E402
import wsl_lote  # noqa: E402

SUITE = "callback_misto"
CASOS = (("misto", 0), ("panico_cb", 101))  # (nome, exit esperado)
TEXTO_PANICO = b"R0003"


def ConstruirDuas(Forja: Path, Nome: str, Modo: str, Pasta: Path):
    """Constroi 2 vezes; devolve (elf, erro)."""
    Saidas = [Pasta / f"{Nome}_{Modo}_{N}.elf" for N in (1, 2)]
    for Saida in Saidas:
        Erro = Chamada.Construir(Forja, AQUI / f"{Nome}.fj", Modo, Saida)
        if Erro:
            return None, Erro
    if Saidas[0].read_bytes() != Saidas[1].read_bytes():
        return None, "2 builds do mesmo fonte deram ELFs diferentes"
    return Saidas[0], None


def Avaliar(Nome: str, Esperado: int, R) -> str | None:
    if R is None:
        return "sem resultado do lote"
    if R.Sinal is not None:
        return f"morreu por sinal {R.Sinal} (stderr: {R.Stderr[:200]!r})"
    if R.Exit != Esperado:
        return f"exit {R.Exit} (esperado {Esperado}): {R.Stderr[:200]!r}"
    Texto = (AQUI / f"{Nome}.esperado").read_bytes()
    if R.Stdout != Texto:
        return f"stdout diverge: obtido {R.Stdout[:160]!r} esperado {Texto[:160]!r}"
    if Nome == "panico_cb" and TEXTO_PANICO not in R.Stderr:
        return f"stderr sem {TEXTO_PANICO!r}: {R.Stderr[:200]!r}"
    return None


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Valor de proc e panic em callback SysV (P38b).")
    Analisador.add_argument("--forja", required=True, help="copia do forja.exe")
    Forja = Path(Analisador.parse_args().forja).resolve()
    if not wsl_lote.DistroDisponivel():
        print("callback_misto: PULADA (WSL/distro ausente)")
        return 0
    Pasta = Path(tempfile.gettempdir()) / "forja_callback_misto"
    Pasta.mkdir(parents=True, exist_ok=True)
    Falhas, Elfs = [], {}
    for Nome, _Exit in CASOS:
        for Modo in Chamada.MODOS:
            Elf, Erro = ConstruirDuas(Forja, Nome, Modo, Pasta)
            if Erro:
                Falhas.append((Nome, Modo, Erro))
            else:
                Elfs[(Nome, Modo)] = Elf
    Programas = [wsl_lote.Programa(f"{N}/{M}", Elf) for (N, M), Elf in Elfs.items()]
    Resultados = {R.Nome: R for R in wsl_lote.ExecutarLote(Programas, SUITE)} if Programas else {}
    for Nome, Esperado in CASOS:
        for Modo in Chamada.MODOS:
            if (Nome, Modo) in Elfs:
                Motivo = Avaliar(Nome, Esperado, Resultados.get(f"{Nome}/{Modo}"))
                if Motivo:
                    Falhas.append((Nome, Modo, Motivo))
    for Nome, Modo, Motivo in Falhas:
        print(f"FALHOU {Nome} {Modo}: {Motivo}")
    print(f"callback_misto: {len(CASOS) * len(Chamada.MODOS) - len(Falhas)}/{len(CASOS) * len(Chamada.MODOS)}")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Principal())
