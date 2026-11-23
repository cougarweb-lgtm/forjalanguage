#!/usr/bin/env python3
"""verificar_rede_linux.py 0.1.0 - roda os exemplos de rede no ELF x86_64-linux dentro do WSL alpine-build (marco rede R10b).

Objetivo: o que exemplos_libs/verificar_libs.py prova no Windows (ProvarRun + ProvarExe) para http_local, https_local, smtp_local,
imap_local e pop3_local, mas com o ELF: o servidor de mentira (tests/rede/servidor_http.py e servidor_mail.py) sobe DENTRO do WSL, em
127.0.0.1, porta efemera; o ELF recebe as mesmas bases e argumentos (lidos de verificar_libs.py: ServidoresDeTeste, ServidorPorExemplo,
ArgsDoExemplo, PrazoDoServidorSeg), e a saida e comparada byte a byte com o .esperado do Windows (CRLF normalizado).
Por que um runner proprio: wsl_lote roda um lote sem processo de fundo, e cada caso aqui precisa do servidor vivo enquanto o ELF roda.
Casos: rede/<nome>/dev (build sem --release, como o ProvarRun) e rede/<nome>/release (build --release, como o ProvarExe).
Roteiro sh gerado por caso: stderr (do ELF e dos servidores) vai para um arquivo do Windows; os servidores morrem no fim do roteiro
(trap) e tambem por --prazo (30 s). Sem internet: so 127.0.0.1.
Uso:   python -I tests/nativo/linux/verificar_rede_linux.py [--forja <forja.exe>] [--so <trecho do caso, ex. http_local/dev>]
Verificacao: python -I tests/nativo/linux/verificar_rede_linux.py --selftest   (offline: gera os 10 roteiros e confere; exit 0 = verde)
Dependencias: WSL com a distro alpine-build e python3 com ssl (nada instalado). Sem a distro: PULADA, exit 0 (como o verificar_linux).
Reversao: apagar este arquivo.

Versao: 0.1.0
Changelog: 0.1.0 (19/11/2026) marco rede R10b - primeira versao: 5 exemplos de rede x dev/release, servidores no WSL, saida comparada ao .esperado.
"""
import argparse
import importlib.util
import os
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path

AQUI = Path(__file__).resolve().parent
LANGUAGE = AQUI.parents[2]
sys.path.insert(0, str(AQUI))
import wsl_lote  # noqa: E402

Especificacao = importlib.util.spec_from_file_location("verificar_libs_rede", LANGUAGE / "exemplos_libs" / "verificar_libs.py")
Libs = importlib.util.module_from_spec(Especificacao)
Especificacao.loader.exec_module(Libs)

EXEMPLOS_DE_REDE = ("http_local", "https_local", "smtp_local", "imap_local", "pop3_local")
MODOS_DE_REDE = ("dev", "release")
TEMPO_BUILD = 120
TEMPO_CASO = 150  # servidor com prazo de 30 s, o ELF mais lento (5 MB, 100 pedidos) e folga
TENTATIVAS_DA_PORTA = 50  # 50 x 0,2 s = 10 s, igual ao EsperaDaPortaSeg do verificar_libs
TRECHO_DO_ERRO = 4000


def CaminhoWsl(Caminho) -> str:
    """Caminho do Windows (C:\\...) como o WSL o ve (/mnt/c/...); caminho ja Linux (/tmp/...) passa igual."""
    Texto = str(Caminho)
    if re.match(r"^[A-Za-z]:", Texto):
        Texto = os.path.normpath(Texto)
        Texto = f"/mnt/{Texto[0].lower()}" + Texto[2:]
    return Texto.replace("\\", "/")


def ServidoresDoExemplo(Nome: str, Pasta: str) -> list:
    """Um item por servidor: (argv do servidor, esquema da base, arquivo de saida no Linux ou None). Mesma sequencia de argumentos do verificar_libs.ComServidores."""
    Itens = []
    for Indice, Extras in enumerate(Libs.ServidoresDeTeste.get(Nome, []), start=1):
        Saida = None
        if "{SAIDA}" in Extras:
            Saida = f"{Pasta}/saida{Indice}.bin"
            Extras = [Saida if E == "{SAIDA}" else E for E in Extras]
        Protocolo = next((P for P in ("smtp", "imap", "pop3") if "--" + P in Extras), None)
        Esquema = (Protocolo or "http") + ("s" if "--tls" in Extras else "")
        Servidor = CaminhoWsl(Libs.ServidorPorExemplo.get(Nome, Libs.ServidorDeTeste))
        Argv = ["python3", "-I", Servidor, "--prazo", str(Libs.PrazoDoServidorSeg)] + [CaminhoWsl(E) for E in Extras]
        Itens.append((Argv, Esquema, Saida))
    return Itens


def LinhasDoServidor(Indice: int, Argv: list) -> list:
    """Sobe o servidor em segundo plano, espera a porta (teto de TENTATIVAS_DA_PORTA) e prepara a base; sem porta, o roteiro sai com 96."""
    return [
        f"{shlex.join(Argv)} > \"$D/porta{Indice}\" 2> \"$D/servidor{Indice}.log\" &",
        f"I=0; while [ \"$I\" -lt {TENTATIVAS_DA_PORTA} ] && [ ! -s \"$D/porta{Indice}\" ]; do sleep 0.2; I=$((I + 1)); done",
        f"P{Indice}=$(head -n 1 \"$D/porta{Indice}\")",
        f"case \"$P{Indice}\" in ''|*[!0-9]*) echo \"servidor {Indice} nao anunciou porta\" >&2; exit 96 ;; esac",
    ]


def MontarRoteiro(Nome: str, Modo: str, Elf: str, Token: str, ArquivoErro: str) -> str:
    """Roteiro sh do caso: sobe os servidores em $D, roda o ELF com as bases, depois as saidas e ArgsDoExemplo (ordem do verificar_libs) e mata tudo na saida."""
    Pasta = f"/tmp/forja_rede_{Token}"
    Linhas = ["#!/bin/sh", f"# roteiro do caso rede/{Nome}/{Modo} (gerado por verificar_rede_linux.py)",
              f"exec 2> {shlex.quote(ArquivoErro)}", f"D={Pasta}", "mkdir -p \"$D\" || exit 95",
              "trap 'kill $(jobs -p) 2>/dev/null; for F in \"$D\"/servidor*.log; do echo \"--- $F\" >&2; cat \"$F\" >&2; done; rm -rf \"$D\"' EXIT",
              f"cp {shlex.quote(Elf)} \"$D/exemplo\" && chmod +x \"$D/exemplo\" || exit 95"]
    Bases, Saidas = [], []
    for Indice, (Argv, Esquema, Saida) in enumerate(ServidoresDoExemplo(Nome, Pasta), start=1):
        Linhas += LinhasDoServidor(Indice, Argv)
        Linhas.append(f"B{Indice}=\"{Esquema}://127.0.0.1:$P{Indice}\"")
        Bases.append(f"\"$B{Indice}\"")
        if Saida:
            Saidas.append(shlex.quote(Saida))
    Extras = [shlex.quote(CaminhoWsl(A)) for A in Libs.ArgsDoExemplo.get(Nome, [])]
    Linhas += ["cd /tmp || exit 95", " ".join(["\"$D/exemplo\""] + Bases + Saidas + Extras), "E=$?", "exit $E"]
    return "\n".join(Linhas) + "\n"


def ConstruirElf(Forja: str, Nome: str, Modo: str, Saida: Path) -> str | None:
    Comando = [Forja, "build", "--target", "x86_64-linux"] + (["--release"] if Modo == "release" else [])
    Comando += [f"exemplos_libs/{Nome}.fj", "-o", str(Saida)]
    Processo = subprocess.run(Comando, capture_output=True, timeout=TEMPO_BUILD, stdin=subprocess.DEVNULL, cwd=LANGUAGE)
    if Processo.returncode != 0 or not Saida.is_file():
        return f"build x86_64-linux exit {Processo.returncode}: " + (Processo.stdout + Processo.stderr).decode("utf-8", "replace")[-TRECHO_DO_ERRO:]
    return None


def PrimeiraDiferenca(Esperado: bytes, Obtido: bytes) -> str:
    Limite = min(len(Esperado), len(Obtido))
    Indice = next((I for I in range(Limite) if Esperado[I] != Obtido[I]), Limite)
    return f"primeira diferenca no byte {Indice} (tamanhos esperado={len(Esperado)} obtido={len(Obtido)}); obtido: {Obtido[max(0, Indice - 80):Indice + 200]!r}"


def RodarCaso(Forja: str, Nome: str, Modo: str, Pasta: Path) -> tuple:
    """Build do ELF, roteiro no WSL e comparacao com o .esperado. Devolve (ok, relato para imprimir se falhou)."""
    Elf = Pasta / f"{Nome}_{Modo}.elf"
    Erro = ConstruirElf(Forja, Nome, Modo, Elf)
    if Erro:
        return False, Erro
    Arquivo_Erro = Pasta / f"{Nome}_{Modo}.stderr"
    Roteiro = Pasta / f"{Nome}_{Modo}.sh"
    with open(Roteiro, "w", encoding="utf-8", newline="\n") as Arq:
        Arq.write(MontarRoteiro(Nome, Modo, CaminhoWsl(Elf), uuid.uuid4().hex[:12], CaminhoWsl(Arquivo_Erro)))
    try:
        Processo = subprocess.run(["wsl", "-d", wsl_lote.DISTRO, "--", "sh", CaminhoWsl(Roteiro)], capture_output=True, timeout=TEMPO_CASO, stdin=subprocess.DEVNULL)
    except subprocess.TimeoutExpired:
        return False, f"timeout de {TEMPO_CASO} s no WSL"
    Esperado = (Path(Libs.PastaExemplos) / f"{Nome}.esperado").read_bytes().replace(b"\r\n", b"\n")
    Obtido = Processo.stdout.replace(b"\r\n", b"\n")
    Erro_Obtido = Arquivo_Erro.read_text(encoding="utf-8", errors="replace") if Arquivo_Erro.is_file() else "(sem stderr)"
    if Processo.returncode == 0 and Obtido == Esperado:
        return True, ""
    Relato = f"exit={Processo.returncode}; {PrimeiraDiferenca(Esperado, Obtido)}\nstderr (servidores e ELF):\n{Erro_Obtido[-TRECHO_DO_ERRO:]}"
    return False, Relato


def Selftest() -> int:
    """Offline: gera os 10 roteiros e confere que nao sobrou caminho do Windows, que o stderr vai para arquivo e que o trap mata os servidores."""
    Casos = [(N, M) for N in EXEMPLOS_DE_REDE for M in MODOS_DE_REDE]
    Problemas = []
    for Nome, Modo in Casos:
        Roteiro = MontarRoteiro(Nome, Modo, "/mnt/c/x/exemplo.elf", "teste0", "/mnt/c/x/erro.txt")
        if "\\" in Roteiro or "C:" in Roteiro:
            Problemas.append(f"{Nome}/{Modo}: caminho do Windows no roteiro")
        if "exec 2>" not in Roteiro or "trap " not in Roteiro or "kill $(jobs -p)" not in Roteiro:
            Problemas.append(f"{Nome}/{Modo}: falta stderr para arquivo ou trap que mata os servidores")
    print(f"verificar_rede_linux selftest: {len(Casos)} casos; problemas {Problemas}")
    return 1 if Problemas else 0


def Principal() -> int:
    Analisador = argparse.ArgumentParser(description="Roda os exemplos de rede no ELF x86_64-linux dentro do WSL alpine-build.")
    Analisador.add_argument("--forja", default=str(LANGUAGE / "compiler" / "forja.exe"))
    Analisador.add_argument("--so", default="", help="so os casos cujo nome contem este trecho (ex.: http_local/dev)")
    Analisador.add_argument("--selftest", action="store_true")
    Opcoes = Analisador.parse_args()
    if Opcoes.selftest:
        return Selftest()
    if not wsl_lote.DistroDisponivel():
        print("rede linux: PULADA (WSL/distro ausente)")
        return 0
    Casos = [(N, M) for N in EXEMPLOS_DE_REDE for M in MODOS_DE_REDE if not Opcoes.so or Opcoes.so in f"rede/{N}/{M}"]
    Pasta = Path(tempfile.mkdtemp(prefix="forja_rede_linux_"))
    try:
        Resultados = [(N, M, *RodarCaso(Opcoes.forja, N, M, Pasta)) for N, M in Casos]
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    for Nome, Modo, Ok, Relato in Resultados:
        print(f"[rede/{Nome}/{Modo}]", "OK" if Ok else "FALHOU")
        if not Ok:
            print(Relato)
    Boas = sum(1 for R in Resultados if R[2])
    print(f"RESULTADO rede linux: {Boas}/{len(Casos)} casos")
    return 0 if Boas == len(Casos) else 1


if __name__ == "__main__":
    sys.exit(Principal())
