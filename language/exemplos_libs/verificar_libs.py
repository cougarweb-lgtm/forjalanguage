#!/usr/bin/env python3
# verificar_libs.py 0.11.0 - (0.11.0: compressao_fluxo na lista (API de fluxo do vendor:compression 0.3.0, PREPARO F5); 0.10.0: --backend padrao x64, porque o backend c da Forja virou X0003 (backend unico x64); 0.9.0: marco rede R8, imap_local e pop3_local contra servidor_mail.py --imap/--pop3 (plain + TLS implicito); o esquema da base sai de --smtp/--imap/--pop3 (+ s com --tls); 0.8.0: marco rede R7, smtp_local: servidor_mail.py (tests\rede) com 4 servidores; "{SAIDA}" num argumento do servidor vira um arquivo temporario do verificador, e os caminhos vao ao exemplo depois das bases; 0.7.0: marco rede R6, mail_mime (vendor:net/mail) entra na lista e ProvarMimePython roda o email da stdlib sobre os dourados em tests\rede\mime; 0.6.0: marco rede R5, https_local: um exemplo pode ter VARIOS servidores de mentira (ServidoresDeTeste = lista de argumentos por servidor), todas as bases vao como argumentos 1..N, depois ArgsDoExemplo (o ca.pem); 0.5.0: marco rede R4, http_local: o verificador sobe tests\rede\servidor_http.py (127.0.0.1, porta efemera), passa a base ao exemplo e derruba o servidor;0.4.0: IA1-L8, db_e_json e json_e_net (pares de vendor no mesmo programa); 0.3.0: M3b P23g, --backend c|x64; no x64 o exe importa a ucrtbase.dll e os 8 exemplos passam, sem pulo: printf, strcasecmp, hypotf, opendir etc. vem do rt_win_legado.o; 0.2.0: + fisica2d_basico, raylib_basico, gltf_basico, audio_basico; DllsExtras por exemplo) prova de TODOS os pacotes de lib padrao da Forja (language\vendor) pelos exemplos desta pasta.
# Uso: python language\exemplos_libs\verificar_libs.py [--forja <forja.exe>] [--so <nome_do_exemplo>] [--backend c|x64]
# Para cada exemplo: forja run contra <nome>.esperado; forja build --release e confere que o .exe nao importa DLL da lib
# (so KERNEL32 e api-ms-win-crt-*, que sao do Windows); para teste_json.fj roda tambem forja test.
# Saida 0 = tudo certo; 1 = alguma prova falhou. Depende de: objdump no PATH (MinGW).
import argparse
import contextlib
import os
import subprocess
import sys
import tempfile
import threading

PastaExemplos = os.path.dirname(os.path.abspath(__file__))
ForjaPadrao = os.path.normpath(os.path.join(PastaExemplos, "..", "compiler", "forja.exe"))
DllsPermitidas = ("kernel32.dll", "api-ms-win-crt-")
# Exemplos com saida fixa (.esperado) e a lib que eles provam.
Exemplos = ["json_basico", "json_manifesto", "banco_basico", "compressao_basico", "compressao_fluxo", "zip_basico", "win32_basico",
            "fisica2d_basico", "raylib_basico", "gltf_basico", "audio_basico",
            "unicode_basico", "linebreak_basico", "image_basico", "html_basico", "shaping_basico",
            "db_e_json", "json_e_net", "http_local", "https_local", "mail_mime", "smtp_local", "imap_local", "pop3_local"]  # imap_local e pop3_local: leitura de vendor:net/mail contra servidor_mail.py (R8); smtp_local: SMTP de vendor:net/mail contra servidor_mail.py (R7); db_e_json e json_e_net: pares de vendor no mesmo programa (IA1-L8); http_local/https_local: servidor de mentira local (R4/R5); mail_mime: MIME de vendor:net/mail (R6)
# DLLs de sistema a mais que um exemplo pode importar (raylib estatica traz GLFW, que usa a API de janela do Windows).
DllsLibcurl = ("advapi32.dll", "bcrypt.dll", "crypt32.dll", "iphlpapi.dll", "secur32.dll", "ws2_32.dll")  # libcurl estatico (Schannel)
DllsExtras = {"win32_basico": ("user32.dll",),  # vendor:win32 liga as DLLs do Windows (so as usadas: user32 neste exemplo)
              "raylib_basico": ("gdi32.dll", "shell32.dll", "user32.dll", "winmm.dll", "opengl32.dll"),
              "json_e_net": DllsLibcurl, "http_local": DllsLibcurl, "https_local": DllsLibcurl, "smtp_local": DllsLibcurl,
              "imap_local": DllsLibcurl, "pop3_local": DllsLibcurl}
ServidorDeTeste = os.path.join(PastaExemplos, "..", "tests", "rede", "servidor_http.py")
ServidorDeEmail = os.path.join(PastaExemplos, "..", "tests", "rede", "servidor_mail.py")
ServidorPorExemplo = {"smtp_local": ServidorDeEmail, "imap_local": ServidorDeEmail, "pop3_local": ServidorDeEmail}  # o padrao e o servidor_http.py
PastaCertsDeTeste = os.path.join(PastaExemplos, "..", "tests", "rede", "certs")
# Exemplos com servidor de mentira: uma lista por servidor, com os argumentos extras dele. O verificador sobe todos, passa as bases
# ("http://127.0.0.1:<porta>" ou "https://...", na ordem da lista) como argumentos 1..N, depois ArgsDoExemplo, e derruba todos.
ServidoresDeTeste = {
    "http_local": [[]],
    "https_local": [["--tls", os.path.join(PastaCertsDeTeste, "servidor.pem"), "--chave", os.path.join(PastaCertsDeTeste, "servidor.key")],
                    ["--tls", os.path.join(PastaCertsDeTeste, "outro.pem"), "--chave", os.path.join(PastaCertsDeTeste, "servidor.key")]],
    "smtp_local": [["--smtp", "--rcpt-recusa", "recusado@exemplo.com", "--saida", "{SAIDA}"],
                   ["--smtp", "--starttls", "--auth", "PLAIN", "--cert", os.path.join(PastaCertsDeTeste, "servidor.pem"), "--chave", os.path.join(PastaCertsDeTeste, "servidor.key"), "--saida", "{SAIDA}"],
                   ["--smtp", "--tls", "--auth", "LOGIN", "--cert", os.path.join(PastaCertsDeTeste, "servidor.pem"), "--chave", os.path.join(PastaCertsDeTeste, "servidor.key"), "--saida", "{SAIDA}"],
                   ["--smtp", "--mudo"]],
    "imap_local": [["--imap"],
                   ["--imap", "--tls", "--cert", os.path.join(PastaCertsDeTeste, "servidor.pem"), "--chave", os.path.join(PastaCertsDeTeste, "servidor.key")]],
    "pop3_local": [["--pop3"],
                   ["--pop3", "--tls", "--cert", os.path.join(PastaCertsDeTeste, "servidor.pem"), "--chave", os.path.join(PastaCertsDeTeste, "servidor.key")]],
}
# Argumentos extras depois das bases (o https_local precisa do ca.pem de teste para confiar nos servidores).
ArgsDoExemplo = {"https_local": [os.path.join(PastaCertsDeTeste, "ca.pem")], "smtp_local": [os.path.join(PastaCertsDeTeste, "ca.pem")],
                "imap_local": [os.path.join(PastaCertsDeTeste, "ca.pem")], "pop3_local": [os.path.join(PastaCertsDeTeste, "ca.pem")],
                "shaping_basico": [os.path.join(PastaExemplos, "oraculos", "dados")]}  # shaping_basico: pasta com a fonte de teste (Amiri-Regular.ttf)
PrazoDoServidorSeg = 30
EsperaDaPortaSeg = 10
# O backend x64 liga a CRT pela ucrtbase.dll (Q-M3-7), nao pelas api-ms-win-crt-* do gcc.
DllsPermitidasX64 = ("kernel32.dll", "ucrtbase.dll")
# Exemplos que dependem de servidor ou rede nao entram no verificador (ver MEM-FORJA-GUIA-10).


def Executar(Comando: list) -> subprocess.CompletedProcess:
    return subprocess.run(Comando, cwd=PastaExemplos, capture_output=True, text=True, encoding="utf-8", errors="replace")


def LerEsperado(Nome: str) -> str:
    with open(os.path.join(PastaExemplos, Nome + ".esperado"), encoding="utf-8", errors="replace") as Arquivo:
        return Arquivo.read().replace("\r\n", "\n")


def DllsImportadas(Exe: str) -> list:
    Saida = subprocess.run(["objdump", "-p", Exe], capture_output=True, text=True, errors="replace")
    return sorted({L.split(":", 1)[1].strip().lower() for L in Saida.stdout.splitlines() if "DLL Name" in L})


@contextlib.contextmanager
def ComServidores(Nome: str):
    """Sobe os servidores de mentira do exemplo (se ele tiver) e entrega a lista de bases ("http://127.0.0.1:<porta>" ou "https://...");
    lista vazia sem servidor, e a base vira None se o servidor nao anunciou porta. A porta e a 1a linha do stdout de cada servidor.
    Todos morrem sozinhos no prazo e sao mortos aqui ao sair."""
    Processos = []
    with tempfile.TemporaryDirectory() as PastaSaidas:
        try:
            Bases = []
            Saidas = []
            for Extras in ServidoresDeTeste.get(Nome, []):
                if "{SAIDA}" in Extras:  # arquivo onde o servidor grava o que recebeu; o exemplo o recebe depois das bases
                    Saidas.append(os.path.join(PastaSaidas, f"saida{len(Saidas) + 1}.bin"))
                    Extras = [Saidas[-1] if E == "{SAIDA}" else E for E in Extras]
                Processo = subprocess.Popen([sys.executable, "-I", ServidorPorExemplo.get(Nome, ServidorDeTeste), "--prazo", str(PrazoDoServidorSeg)] + Extras,
                                            stdout=subprocess.PIPE, text=True)
                Processos.append(Processo)
                Guarda = threading.Timer(EsperaDaPortaSeg, Processo.kill)
                Guarda.start()
                Porta = Processo.stdout.readline().strip()
                Guarda.cancel()
                Protocolo = next((P for P in ("smtp", "imap", "pop3") if "--" + P in Extras), None)  # sem nenhum deles e o servidor_http.py
                Esquema = (Protocolo or "http") + ("s" if "--tls" in Extras else "")
                Bases.append(f"{Esquema}://127.0.0.1:{Porta}" if Porta.isdigit() else None)
            yield Bases + Saidas
        finally:
            for Processo in Processos:
                Processo.kill()
                Processo.wait()


def ArgumentosDoExemplo(Nome: str, Bases: list) -> list:
    """Bases dos servidores (na ordem) mais os argumentos extras do exemplo; lista vazia se algum servidor nao subiu (o exemplo entao falha na prova)."""
    if not all(Bases):
        return []
    return Bases + ArgsDoExemplo.get(Nome, [])


def ProvarRun(Forja: str, Nome: str, Backend: str) -> bool:
    with ComServidores(Nome) as Bases:
        Args = ArgumentosDoExemplo(Nome, Bases)
        Saida = Executar([Forja, "run", "--backend", Backend, Nome + ".fj"] + (["--"] + Args if Args else []))
    Ok = Saida.returncode == 0 and Saida.stdout.replace("\r\n", "\n") == LerEsperado(Nome)
    print(f"[{Nome}] run", "OK" if Ok else f"FALHOU exit={Saida.returncode}")
    return Ok


def ProvarExe(Forja: str, Nome: str, Backend: str) -> bool:
    with tempfile.TemporaryDirectory() as Temp:
        Exe = os.path.join(Temp, Nome + ".exe")
        Saida = Executar([Forja, "build", "--release", "--backend", Backend, Nome + ".fj", "-o", Exe])
        if Saida.returncode != 0:
            print(f"[{Nome}] build FALHOU:", Saida.stderr[-300:])
            return False
        Permitidas = (DllsPermitidasX64 if Backend == "x64" else DllsPermitidas) + DllsExtras.get(Nome, ())
        Estranhas = [D for D in DllsImportadas(Exe) if not D.startswith(Permitidas)]
        with ComServidores(Nome) as Bases:
            Roda = subprocess.run([Exe] + ArgumentosDoExemplo(Nome, Bases), cwd=PastaExemplos, capture_output=True, text=True, encoding="utf-8", errors="replace")
        Ok = not Estranhas and Roda.returncode == 0 and Roda.stdout.replace("\r\n", "\n") == LerEsperado(Nome)
        print(f"[{Nome}] exe sem DLL de lib", "OK" if Ok else f"FALHOU dlls_estranhas={Estranhas} exit={Roda.returncode}")
        return Ok


def ProvarMimePython() -> bool:
    """Prova do MIME de vendor:net/mail pelo email da stdlib (tests\\rede\\mime\\verificar_mime.py) sobre os dourados que o mail_mime ja conferiu."""
    Script = os.path.normpath(os.path.join(PastaExemplos, "..", "tests", "rede", "mime", "verificar_mime.py"))
    Saida = Executar([sys.executable, "-I", Script])
    Ok = Saida.returncode == 0 and "mime python OK" in Saida.stdout
    print("[mail_mime] email da stdlib", "OK" if Ok else f"FALHOU exit={Saida.returncode} {Saida.stdout.strip()[-200:]}")
    return Ok


def ProvarTeste(Forja: str, Backend: str) -> bool:
    Saida = Executar([Forja, "test", "--backend", Backend, "teste_json.fj"])
    Ok = Saida.returncode == 0 and "0 falharam" in Saida.stdout
    print("[teste_json] forja test", "OK" if Ok else "FALHOU", Saida.stdout.strip().splitlines()[-1:])
    return Ok


def Principal() -> int:
    Analise = argparse.ArgumentParser()
    Analise.add_argument("--forja", default=ForjaPadrao)
    Analise.add_argument("--so", default="")
    Analise.add_argument("--backend", default="x64", choices=("c", "x64"))
    Opcoes = Analise.parse_args()
    Alvos = [E for E in Exemplos if not Opcoes.so or E == Opcoes.so]
    Provas = [ProvarTeste(Opcoes.forja, Opcoes.backend)] if not Opcoes.so else []
    for Nome in Alvos:
        Provas.append(ProvarRun(Opcoes.forja, Nome, Opcoes.backend))
        Provas.append(ProvarExe(Opcoes.forja, Nome, Opcoes.backend))
    if not Opcoes.so or Opcoes.so == "mail_mime":
        Provas.append(ProvarMimePython())
    print("RESULTADO:", "tudo certo" if all(Provas) else "FALHOU", f"({sum(Provas)}/{len(Provas)} provas)")
    return 0 if all(Provas) else 1


if __name__ == "__main__":
    sys.exit(Principal())
