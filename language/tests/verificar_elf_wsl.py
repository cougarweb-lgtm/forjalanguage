#!/usr/bin/env python3
"""verificar_elf_wsl.py - prova enxuta do alvo Linux (ELF64) no WSL alpine-build.

Objetivo: o examples\\01_ola.fj gerado no Windows (--target x86_64-linux) roda no Linux.
Verificacao: readelf -h -l -S sem aviso; file = ELF 64-bit estatico; stdout igual ao do
Windows (sem CR) e exit 0 em dev e release; dois builds com bytes iguais.
Uso:    python tests\\verificar_elf_wsl.py --forja <forja.exe>
Saida:  exit 0 = tudo verde; 1 = alguma checagem falhou; 2 = WSL/distro ausente.
Depende de: WSL com a distro alpine-build (so leitura; nada instalado nela).
Escrita: %TEMP%\\forja_linux_elf (Windows) e /tmp/forja_elf_<pid> (Linux, apagado no fim).
Reversao: apagar este arquivo.

Changelog:
  0.1.0 (14/11/2026) primeira versao, Portao Z z0 (casos 01_ola dev/release; sem o caso do indice plantado).
"""
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

DISTRO = "alpine-build"
RAIZ = Path(__file__).resolve().parent.parent
EXEMPLO = RAIZ.parent / "examples" / "01_ola.fj"
LIMITE_SEGUNDOS = 120


def Wsl(Comando, Limite=30):
    """Roda um comando sh no WSL e devolve (rc, stdout, stderr), stdout em bytes."""
    Processo = subprocess.run(["wsl", "-d", DISTRO, "--", "sh", "-c", Comando],
                              capture_output=True, timeout=Limite, stdin=subprocess.DEVNULL)
    return Processo.returncode, Processo.stdout, Processo.stderr


def CaminhoWsl(Caminho):
    """C:\\a\\b -> /mnt/c/a/b (a distro monta os discos do Windows)."""
    Texto = str(Caminho).replace("\\", "/")
    return "/mnt/" + Texto[0].lower() + Texto[2:]


def Construir(Forja, Saida, Alvo, Release):
    Comando = [str(Forja), "build", "--target", Alvo] + (["--release"] if Release else []) + [str(EXEMPLO), "-o", str(Saida)]
    return subprocess.run(Comando, capture_output=True, text=True, timeout=LIMITE_SEGUNDOS, stdin=subprocess.DEVNULL)


def SaidaDoWindows(Forja, Pasta):
    """Stdout do 01_ola nativo do Windows, sem CR (referencia da comparacao)."""
    Exe = Pasta / "ola_win.exe"
    Construir(Forja, Exe, "x86_64-windows", False)
    Processo = subprocess.run([str(Exe)], capture_output=True, timeout=30, stdin=subprocess.DEVNULL)
    return Processo.stdout.replace(b"\r\n", b"\n")


def ConferirElf(Elf, Temp, Rotulo):
    """Lista de falhas (vazia = ok) das checagens estruturais e da execucao de um ELF."""
    Falhas = []
    Alvo = "%s/%s" % (Temp, Rotulo)
    Wsl("cp '%s' '%s' && chmod +x '%s'" % (CaminhoWsl(Elf), Alvo, Alvo))
    Rc, Saida, Erro = Wsl("readelf -h -l -S '%s' 2>&1 >/dev/null | head -5; readelf -h -l -S '%s' 2>&1 | grep -i -c 'warning'" % (Alvo, Alvo))
    if Saida.strip() not in (b"0", b""):
        Falhas.append("%s: readelf com aviso: %s" % (Rotulo, Saida.decode("utf-8", "replace").strip()))
    Rc, Saida, Erro = Wsl("file '%s'" % Alvo)
    Texto = Saida.decode("utf-8", "replace")
    if "ELF 64-bit LSB" not in Texto or "x86-64" not in Texto or "statically linked" not in Texto:
        Falhas.append("%s: file inesperado: %s" % (Rotulo, Texto.strip()))
    return Falhas


def ExecutarElf(Temp, Rotulo, Esperado):
    Rc, Saida, Erro = Wsl("'%s/%s'" % (Temp, Rotulo), 15)
    Falhas = []
    if Rc != 0:
        Falhas.append("%s: exit %d (esperado 0); stderr=%s" % (Rotulo, Rc, Erro.decode("utf-8", "replace")[:200]))
    if Saida != Esperado:
        Falhas.append("%s: stdout difere do Windows: %r != %r" % (Rotulo, Saida[:80], Esperado[:80]))
    return Falhas


def Principal():
    Analisador = argparse.ArgumentParser()
    Analisador.add_argument("--forja", required=True)
    Argumentos = Analisador.parse_args()
    Forja = Path(Argumentos.forja).resolve()
    Rc, Saida, Erro = Wsl("echo ok", 20)
    if Rc != 0 or b"ok" not in Saida:
        print("WSL/%s indisponivel: %s" % (DISTRO, Erro.decode("utf-8", "replace")[:200]))
        return 2
    Falhas = []
    Pasta = Path(tempfile.gettempdir()) / "forja_linux_elf"
    Pasta.mkdir(exist_ok=True)
    PastaLinux = "/tmp/forja_elf_%d" % os.getpid()
    Wsl("mkdir -p '%s'" % PastaLinux)
    try:
        Esperado = SaidaDoWindows(Forja, Pasta)
        for Rotulo, Release in (("dev", False), ("rel", True)):
            Elf = Pasta / ("ola_%s.elf" % Rotulo)
            Copia = Pasta / ("ola_%s_2.elf" % Rotulo)
            Primeiro = Construir(Forja, Elf, "x86_64-linux", Release)
            Construir(Forja, Copia, "x86_64-linux", Release)
            if Primeiro.returncode != 0 or not Elf.is_file():
                Falhas.append("%s: build linux falhou: %s" % (Rotulo, (Primeiro.stdout + Primeiro.stderr)[:200]))
                continue
            if Elf.read_bytes() != Copia.read_bytes():
                Falhas.append("%s: dois builds com bytes diferentes" % Rotulo)
            Falhas += ConferirElf(Elf, PastaLinux, Rotulo) + ExecutarElf(PastaLinux, Rotulo, Esperado)
            print("%s: %d bytes, %s" % (Rotulo, Elf.stat().st_size, "ok" if not Falhas else "FALHAS"))
    finally:
        Wsl("rm -rf '%s'" % PastaLinux)
    for Falha in Falhas:
        print("FALHA:", Falha)
    print("verificar_elf_wsl: %s" % ("VERDE" if not Falhas else "%d falha(s)" % len(Falhas)))
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Principal())
