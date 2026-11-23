#!/usr/bin/env python3
"""oraculo_c.py - oraculo C-musl do alvo Linux (M5 P28): o C portavel da Forja compilado pelo gcc do alpine-build.

Objetivo: para os casos da lista oraculo_c.txt, o x64-linux precisa dar a MESMA saida do `forja build --modo c` compilado no musl (gcc da distro, so ferramenta de TESTE,
Q18). No Windows roda `forja build --modo c [--release] <fonte> -o <x.c>`; no alpine, UMA chamada do wsl.exe compila todos: gcc -std=c11 -O0 -static -lm (4 em paralelo).
API:  GerarC(Forja, Fonte, Modo, Saida) -> erro (str) ou None | CompilarLote(Itens) -> {nome: erro ou None}, Itens = [(nome, arquivo.c, arquivo_elf, dev)]
CONGELADO (P3, classe linux_c): ChaveDoCaso / BancoLinuxC / ReferenciaCongelada leem o JSON congelado (oraculo_congelado_g1_linux_c.json, selado, sem gcc);
GravarCongelado escreve o JSON uma vez, com {chave: (stdout, exit)} do C-musl que bateu com o x64. Gerar o C com FORJA_G1 (forja_g1.exe 0.20.4).
0.21.0 F3: o padrao do verificar_linux so le o JSON (ReferenciaCongelada); GerarC, CompilarLote e GravarCongelado ficam so para --congelar-c.
Verificacao: python -I oraculo_c.py --selftest   (exit 0 = verde ou PULADO sem a distro)
Reversao: apagar este arquivo.

Versao: 0.3.0
Changelog: 0.3.0 (18/11/2026) 0.21.0 F3: o verificar_linux le o C-musl so do JSON selado por padrao; nenhuma funcao removida (GerarC e CompilarLote seguem no --congelar-c).
           0.2.0 (18/11/2026) P3 - oraculo C congelado (linux_c): chave, leitura selada sem gcc e gravacao do JSON; ReferenciaCongelada recusa com OraculoCIndisponivel.
           0.1.0 (14/11/2026) M5 P28 - primeira versao.
"""
import argparse
import base64
import subprocess
import sys
import tempfile
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
sys.path.insert(0, str(AQUI.parent / "rapido"))  # oraculo_congelado.py: o banco e a chave do oraculo congelado (so importa, nao muda nada)
import oraculo_congelado  # noqa: E402
import wsl_lote  # noqa: E402

CAMINHO_CONGELADO = AQUI.parent / "rapido" / "oraculo_congelado_g1_linux_c.json"
FORJA_G1 = oraculo_congelado.FORJA_G1  # 0.20.4 arquivado em gaveta\g1: o forja que gera o C do congelamento
CLASSE = "linux_c"
CHECAGEM = "modo-c"  # a chave do build --modo c (sem --release nem --checagem)
ORIGEM_CONGELADO = "congelado-c"
TEMPO_BUILD_C = 120
PARALELO_GCC = 4
# -D_POSIX_C_SOURCE: com -std=c11 a musl esconde nanosleep/clock_gettime (forja_rt_proc.h, ramo POSIX); a sonda de pasta do forja_rt_arquivo.h segue C11 pura.
OPCOES_GCC = "-std=c11 -D_POSIX_C_SOURCE=200809L -O0 -static"
STUBS_A52 = Path(__file__).resolve().parent / "stubs_posix_a52.h"  # funcoes A52 do std:os sem ramo POSIX no forja_rt_proc.h (M5b)


def GerarC(Forja, Fonte: Path, Modo: str, Saida: Path) -> str | None:
    Comando = [str(Forja), "build", "--modo", "c", str(Fonte), "-o", str(Saida)]  # o C e o mesmo nos dois modos: dev = -DFORJA_DEV no gcc
    Processo = subprocess.run(Comando, capture_output=True, timeout=TEMPO_BUILD_C, stdin=subprocess.DEVNULL)
    if Processo.returncode != 0 or not Saida.is_file():
        return f"build --modo c exit {Processo.returncode}: " + (Processo.stdout + Processo.stderr).decode("utf-8", "replace")[:300]
    return None


def ScriptDeCompilacao(Itens: list) -> str:
    Linhas = ["#!/bin/sh"]
    for Posicao, (_Nome, ArquivoC, ArquivoElf, Dev) in enumerate(Itens):
        C, Elf = wsl_lote.Aspas(wsl_lote.CaminhoWsl(ArquivoC)), wsl_lote.Aspas(wsl_lote.CaminhoWsl(ArquivoElf))
        Erro = wsl_lote.Aspas(wsl_lote.CaminhoWsl(ArquivoElf) + ".err")
        Opcoes = OPCOES_GCC + f" -include {wsl_lote.Aspas(wsl_lote.CaminhoWsl(STUBS_A52))}" + (" -DFORJA_DEV" if Dev else "")
        Linhas.append(f"gcc {Opcoes} -o {Elf} {C} -lm > {Erro} 2>&1 || echo falhou > {Erro}.rc &")
        if (Posicao + 1) % PARALELO_GCC == 0:
            Linhas.append("wait")
    Linhas.append("wait")
    return "\n".join(Linhas) + "\n"


def CompilarLote(Itens: list) -> dict:
    """Compila todos num so wsl.exe. Devolve {nome: None (ok) ou texto do erro do gcc}."""
    if not Itens:
        return {}
    Pasta = Path(tempfile.mkdtemp(prefix="forja_oraculo_c_"))
    Script = Pasta / "compilar.sh"
    Script.write_bytes(ScriptDeCompilacao(Itens).encode("utf-8"))
    Limite = wsl_lote.LIMITE_WSL_SEMDISTRO + 30 * len(Itens)
    try:
        wsl_lote.Wsl(["sh", wsl_lote.CaminhoWsl(Script)], Limite)
    except subprocess.TimeoutExpired as Erro:
        raise RuntimeError(f"oraculo C: gcc do alpine passou de {Limite} s") from Erro
    Resultado = {}
    for Nome, _ArquivoC, ArquivoElf, _Dev in Itens:
        Falhou = Path(str(ArquivoElf) + ".err.rc").exists() or not ArquivoElf.is_file()
        ArquivoErro = Path(str(ArquivoElf) + ".err")
        Texto = ArquivoErro.read_text(encoding="utf-8", errors="replace")[:400] if Falhou and ArquivoErro.is_file() else None
        Resultado[Nome] = (Texto or "gcc sem saida") if Falhou else None
        for Sufixo in (".err", ".err.rc"):
            Path(str(ArquivoElf) + Sufixo).unlink(missing_ok=True)
    for Resto in Pasta.glob("*"):
        Resto.unlink()
    Pasta.rmdir()
    return Resultado


class OraculoCIndisponivel(RuntimeError):
    """Sem o C congelado para o caso (chave ausente, banco selado adulterado): FALHA do caso; nunca gera o C de novo."""


def ChaveDoCaso(Fonte: Path, Modo: str, Identidade: list) -> str:
    """Chave do JSON congelado: fonte + modulos + checagem + identidade do caso (args, env, arquivos, stdin) + modo + classe."""
    return oraculo_congelado.ChaveDe(Fonte, CHECAGEM, Identidade, Modo, CLASSE)


def BancoLinuxC():
    return oraculo_congelado.BancoOraculo(CAMINHO_CONGELADO, FORJA_G1)


def ReferenciaCongelada(Banco, Fonte: Path, Modo: str, Identidade: list) -> tuple:
    """(stdout, exit) do C-musl congelado para o caso. Gerar=False: nunca roda gcc nem forja_g1 (o banco selado nao olha o gcc)."""
    try:
        Registro = Banco.Obter(Fonte, CHECAGEM, Identidade, Gerar=False, Modo=Modo, Classe=CLASSE)
    except oraculo_congelado.ErroOraculo as Erro:
        raise OraculoCIndisponivel(str(Erro)) from Erro
    if Registro is None:
        raise OraculoCIndisponivel(f"chave ausente no oraculo C congelado ({Fonte.name} {Modo})")
    return base64.b64decode(Registro["stdout"]), Registro["exit"]


def GravarCongelado(Itens: dict) -> int:
    """Grava o JSON novo com {chave: (stdout, exit)} do C-musl e sela. Recusa arquivo existente (para recongelar, apagar antes). Devolve a quantidade gravada."""
    if CAMINHO_CONGELADO.exists():
        raise oraculo_congelado.ErroOraculo(f"{CAMINHO_CONGELADO.name} ja existe: apague-o antes de recongelar")
    Banco = BancoLinuxC()
    Banco.Cabecalho = {"formato": oraculo_congelado.FORMATO, "classe": CLASSE, "forja": FORJA_G1.name,
                       "forja_referencia_sha256": oraculo_congelado.ShaBytes(FORJA_G1.read_bytes()),
                       "gcc": "gcc do alpine-build via WSL (musl, -std=c11 -O0 -static)", "flags": "build --modo c"}
    Banco.Itens = {Chave: {"invalido": False, "origem": FORJA_G1.name, "exit": Saida, "stdout": base64.b64encode(Stdout).decode("ascii")}
                   for Chave, (Stdout, Saida) in Itens.items()}
    Banco.Selar(f"{FORJA_G1.name} 0.20.4 + gcc musl (WSL alpine)")
    return len(Banco.Itens)


def Selftest() -> int:
    if not wsl_lote.DistroDisponivel():
        print("oraculo_c selftest: PULADO (WSL/distro ausente)")
        return 0
    Pasta = Path(tempfile.mkdtemp(prefix="forja_selftest_c_"))
    try:
        (Pasta / "ok.c").write_text('#include <stdio.h>\nint main(void){puts("c-ok");return 3;}\n')
        (Pasta / "ruim.c").write_text("int main(void){ return x; }\n")
        Itens = [("ok", Pasta / "ok.c", Pasta / "ok.elf", False), ("ruim", Pasta / "ruim.c", Pasta / "ruim.elf", True)]
        Saida = CompilarLote(Itens)
        Bom = Saida.get("ok") is None and (Pasta / "ok.elf").is_file() and Saida.get("ruim") is not None
        print("oraculo_c selftest:", "VERDE" if Bom else f"FALHOU {Saida}")
        return 0 if Bom else 1
    finally:
        for Resto in Pasta.glob("*"):
            Resto.unlink()
        Pasta.rmdir()


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Oraculo C-musl: forja build --modo c + gcc do alpine.")
    Analisador.add_argument("--selftest", action="store_true")
    if Analisador.parse_args().selftest:
        return Selftest()
    Analisador.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(Principal())
