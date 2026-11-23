"""verificar_es.py - prova do S3 P5a: forja_rt_es.h e forja_rt_stdin.h contra o disco e a entrada padrao, sem o compilador Forja.

Versao: 0.1.1
Changelog:
  0.1.1 - variaveis locais em CamelCase (PY-R01, lint pos-edicao); a logica nao muda.
  0.1.0 - S3 P5a: compila banco_es.c com gcc (-std=c11 -O2 -Wall -Wextra -Werror); roda o modo disco numa pasta nova do %TEMP% (apagada no fim);
          roda a entrada por cano (LF, CRLF, sem LF final, vazio, linha de 1 MiB, NUL, UTF-8, CR no limite da parte, bruto e prompt sem LF, que so
          recebe a entrada depois de aparecer na saida); cada header compila sozinho em C; prova_nativo.c compila sem libc e o nm -u so tem kernel32,
          memcpy e memset.
Uso: python tests\\nativo\\s3\\es\\verificar_es.py   -> imprime "OK n casos" ou a lista das falhas; retorna 1 se houver falha.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import threading

PASTA_AQUI = os.path.dirname(os.path.abspath(__file__))
PASTA_RUNTIME = os.path.abspath(os.path.join(PASTA_AQUI, "..", "..", "..", "..", "runtime"))
ARQUIVO_BANCO = os.path.join(PASTA_AQUI, "banco_es.c")
ARQUIVO_PROVA = os.path.join(PASTA_AQUI, "prova_nativo.c")
HEADERS = ["forja_rt_es.h", "forja_rt_stdin.h"]
FLAGS_C = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
FLAGS_NATIVO = ["-ffreestanding", "-fno-builtin", "-mno-stack-arg-probe", "-Wframe-larger-than=4000", "-Wall", "-Wextra", "-Werror"]
VARIANTES_NATIVAS = [("fs e stdin", []), ("so fs", ["-DPROVA_SEM_STDIN"]), ("so stdin", ["-DPROVA_SEM_FS"])]
SIMBOLOS_NATIVOS_LIVRES = {"memcpy", "memset"}
TEMPO_MAX = 300
UM_MIB = 1 << 20
SAIDA_PROMPT_ESPERADA = "Nome: LINHA 4 1 4a6f616f\nFIM\n"

CASOS_LINHAS = [
    ("LF", b"abc\ndef\n", [(b"abc", 1), (b"def", 1)]),
    ("CRLF", b"abc\r\ndef\r\n", [(b"abc", 1), (b"def", 1)]),
    ("sem LF final", b"abc\ndef", [(b"abc", 1), (b"def", 0)]),
    ("vazio", b"", []),
    ("so LF", b"\n", [(b"", 1)]),
    ("linhas vazias com CR", b"\n\r\n", [(b"", 1), (b"", 1)]),
    ("CR sem LF no fim", b"abc\r", [(b"abc\r", 0)]),
    ("linha de 1 MiB", b"a" * UM_MIB + b"\n", [(b"a" * UM_MIB, 1)]),
    ("linha de 1 MiB sem LF final", b"a" * UM_MIB, [(b"a" * UM_MIB, 0)]),
    ("CR no limite da parte de 64 KiB", b"a" * 65535 + b"\r\n", [(b"a" * 65535, 1)]),
    ("64 KiB exatos e CRLF", b"a" * 65536 + b"\r\n", [(b"a" * 65536, 1)]),
    ("byte NUL no meio", b"ab\x00cd\n", [(b"ab\x00cd", 1)]),
    ("UTF-8 multibyte", "João ção\r\n".encode("utf-8"), [("João ção".encode("utf-8"), 1)]),
    ("0x1A no inicio em cano nao e fim", b"\x1aabc\n", [(b"\x1aabc", 1)]),
]

CASOS_BRUTOS = [
    ("bruto com CR, NUL e 0xFF", b"a\r\n\x00\xff", b"a\r\n\x00\xff"),
    ("bruto vazio", b"", b""),
]


def Texto(Dados):
    return Dados.replace(b"\r\n", b"\n").decode("utf-8", "replace")


def RodarGcc(Args):
    Resultado = subprocess.run(["gcc", *Args], capture_output=True, timeout=TEMPO_MAX)
    return Resultado.returncode, Texto(Resultado.stderr)[:600]


def Rodar(Args, Entrada):
    return subprocess.run(Args, input=Entrada, capture_output=True, timeout=TEMPO_MAX)


def EsperadoLinhas(Dados):
    """Regra do contrato: LF fecha a linha e um CR logo antes dele sai; a ultima linha sem LF vale; nada depois do ultimo LF."""
    Partes = Dados.split(b"\n")
    Esperado = []
    for Parte in Partes[:-1]:
        Esperado.append((Parte[:-1] if Parte.endswith(b"\r") else Parte, 1))
    if Partes[-1]:
        Esperado.append((Partes[-1], 0))
    return Esperado


def InterpretarLinhas(Saida):
    """Le 'LINHA n achou hex' e 'FIM'. Devolve (lista de (bytes, achou), terminou)."""
    Linhas = []
    Terminou = False
    for Linha in Saida.split("\n"):
        Partes = Linha.split(" ")
        if "LINHA" in Partes:
            Pos = Partes.index("LINHA")
            Hexa = Partes[Pos + 3] if len(Partes) > Pos + 3 else ""
            Linhas.append((bytes.fromhex(Hexa), int(Partes[Pos + 2])))
        elif Linha.strip() == "FIM":
            Terminou = True
    return Linhas, Terminou


def InterpretarBruto(Saida):
    for Linha in Saida.split("\n"):
        if Linha.startswith("BRUTO "):
            Partes = Linha.split(" ")
            return bytes.fromhex(Partes[2]) if len(Partes) > 2 else b""
    return None


def CompilarBanco(Pasta, Falhas):
    Exe = os.path.join(Pasta, "banco_es.exe")
    Codigo, Erro = RodarGcc([*FLAGS_C, "-I", PASTA_RUNTIME, ARQUIVO_BANCO, "-o", Exe])
    if Codigo != 0:
        Falhas.append(f"banco_es.c nao compila: {Erro}")
        return None
    return Exe


def ConferirDisco(Exe, Falhas):
    """Roda o modo disco numa pasta nova do %TEMP%. Devolve quantos casos o banco reportou."""
    Base = tempfile.mkdtemp(prefix="forja_es_s3_")
    Casos = 0
    Resumo = None
    try:
        Resultado = Rodar([Exe, "disco", Base], b"")
        for Linha in Texto(Resultado.stdout).splitlines():
            if Linha.startswith("CASO "):
                Casos += 1
                if not Linha.endswith(" OK"):
                    Falhas.append(f"disco: {Linha}")
            elif Linha.startswith("RESUMO "):
                Resumo = Linha
    finally:
        shutil.rmtree(Base, ignore_errors=True)
    if Resumo is None:
        Falhas.append("disco: o banco nao chegou ao RESUMO")
    return Casos


def ConferirLinhas(Exe, Falhas):
    for Nome, Entrada, Esperado in CASOS_LINHAS:
        Resultado = Rodar([Exe, "linhas"], Entrada)
        Obtido, Terminou = InterpretarLinhas(Texto(Resultado.stdout))
        if Resultado.returncode != 0 or not Terminou or Obtido != Esperado:
            Falhas.append(f"linhas '{Nome}': esperado {len(Esperado)} linha(s), obtido {len(Obtido)}, rc {Resultado.returncode}")
    return len(CASOS_LINHAS)


def ConferirBrutos(Exe, Falhas):
    for Nome, Entrada, Esperado in CASOS_BRUTOS:
        Resultado = Rodar([Exe, "bruto"], Entrada)
        Obtido = InterpretarBruto(Texto(Resultado.stdout))
        if Resultado.returncode != 0 or Obtido != Esperado:
            Falhas.append(f"bruto '{Nome}': obtido {Obtido!r}, rc {Resultado.returncode}")
    return len(CASOS_BRUTOS)


def RodarPrompt(Exe):
    """Sobe o banco com cano. So manda a entrada depois que 'Nome: ' aparece na saida: se a saida nao foi descarregada antes de bloquear, nunca aparece."""
    Proc = subprocess.Popen([Exe, "prompt"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    Recebido = bytearray()
    Visto = threading.Event()

    def Ler():
        while True:
            Pedaco = os.read(Proc.stdout.fileno(), 4096)
            if not Pedaco:
                break
            Recebido.extend(Pedaco)
            if b"Nome: " in Recebido:
                Visto.set()

    Leitor = threading.Thread(target=Ler, daemon=True)
    Leitor.start()
    if not Visto.wait(TEMPO_MAX):
        Proc.kill()
        Leitor.join(TEMPO_MAX)
        return None
    Proc.stdin.write(b"Joao\r\n")
    Proc.stdin.close()
    Proc.wait(TEMPO_MAX)
    Leitor.join(TEMPO_MAX)
    return Texto(bytes(Recebido))


def ConferirPrompt(Exe, Falhas):
    Saida = RodarPrompt(Exe)
    if Saida is None:
        Falhas.append("prompt: 'Nome: ' nao saiu antes da leitura (stdout nao descarregado)")
    elif Saida != SAIDA_PROMPT_ESPERADA:
        Falhas.append(f"prompt: saida {Saida!r}")
    return 1


def ConferirHeadersC(Pasta, Falhas):
    for Cabecalho in HEADERS:
        Obj = os.path.join(Pasta, Cabecalho + ".o")
        Codigo, Erro = RodarGcc([*FLAGS_C, "-x", "c", "-c", os.path.join(PASTA_RUNTIME, Cabecalho), "-o", Obj])
        if Codigo != 0:
            Falhas.append(f"{Cabecalho} sozinho em C nao compila: {Erro}")
    return len(HEADERS)


def Simbolos(Obj):
    Resultado = subprocess.run(["nm", "-u", Obj], capture_output=True, timeout=TEMPO_MAX)
    Nomes = []
    for Linha in Texto(Resultado.stdout).splitlines():
        Partes = Linha.split()
        if len(Partes) == 2 and Partes[0] == "U":
            Nomes.append(Partes[1])
    return Resultado.returncode, Nomes


def ProvarNativo(Pasta, Falhas):
    """Compila prova_nativo.c sem libc nas 3 variantes e confere que o nm -u so tem __imp_ (kernel32) e memcpy/memset."""
    Vistos = set()
    for Indice, (Nome, Extra) in enumerate(VARIANTES_NATIVAS):
        Obj = os.path.join(Pasta, f"prova_nativo_{Indice}.o")
        Codigo, Erro = RodarGcc([*FLAGS_NATIVO, *Extra, "-I", PASTA_RUNTIME, "-c", ARQUIVO_PROVA, "-o", Obj])
        if Codigo != 0:
            Falhas.append(f"nativo ({Nome}) nao compila: {Erro}")
            continue
        CodigoNm, Nomes = Simbolos(Obj)
        if CodigoNm != 0:
            Falhas.append(f"nativo ({Nome}): nm -u falhou")
        for Simbolo in Nomes:
            if not Simbolo.startswith("__imp_") and Simbolo not in SIMBOLOS_NATIVOS_LIVRES:
                Falhas.append(f"nativo ({Nome}): simbolo fora do permitido: {Simbolo}")
        Vistos.update(Nomes)
    return sorted(Vistos)


def Main():
    Falhas = []
    Casos = 0
    Simbolos_ = []
    Pasta = tempfile.mkdtemp(prefix="forja_es_build_")
    try:
        Exe = CompilarBanco(Pasta, Falhas)
        if Exe is not None:
            Casos += ConferirDisco(Exe, Falhas)
            Casos += ConferirLinhas(Exe, Falhas)
            Casos += ConferirBrutos(Exe, Falhas)
            Casos += ConferirPrompt(Exe, Falhas)
        Casos += ConferirHeadersC(Pasta, Falhas)
        Simbolos_ = ProvarNativo(Pasta, Falhas)
        Casos += len(VARIANTES_NATIVAS)
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    for Falha in Falhas:
        print(f"FALHA {Falha}")
    if Falhas:
        print(f"{len(Falhas)} falha(s) em {Casos} casos")
        return 1
    print(f"OK {Casos} casos")
    print("nm -u nativo (uniao): " + " ".join(Simbolos_))
    return 0


if __name__ == "__main__":
    sys.exit(Main())
