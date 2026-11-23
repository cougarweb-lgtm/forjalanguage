# gerar_win32: gera vendor/win32/win32.fj (import "vendor:win32/win32") a partir de windows.h pelo `forja ligar`.
# Objetivo: um modulo Win32 curado (procs, structs, handles ?^T, constantes) sem declarar nada a mao em cada app.
# Versao 0.1.0. Changelog: 0.1.0 - lista por DLL em simbolos.txt, 1 bloco foreign por DLL, constantes por prefixo.
# Depende de: libclang (usada pelo forja_ligar.py) e do gcc (mingw) no PATH.
# Verificar: python gerar_win32.py --selftest   (gera em %TEMP% e confere a contagem de procs e a divisao por DLL)
# Uso: python gerar_win32.py [--forja <forja.exe>]   (grava win32.fj ao lado deste script; reversao = refazer com o simbolos.txt anterior)
import os
import re
import subprocess
import sys
import tempfile

PASTA = os.path.dirname(os.path.abspath(__file__))
LIGAR = os.path.join(PASTA, "..", "..", "tools", "forja_ligar", "forja_ligar.py")
HEADERS_EXTRAS = ["commctrl.h", "shellapi.h", "shlobj.h", "commdlg.h", "objbase.h", "wtsapi32.h", "tlhelp32.h"]
RE_PROC = re.compile(r"^    proc (\w+)\(")
# O ligar entrega ponteiro de funcao em campo de struct como rawptr (campo pode ser nulo em C; a Forja v1.13 nao tem ?proc).
# Estes campos o app SEMPRE preenche, entao viram proc(...) para receber o nome do proc direto. Linha exata -> linha nova.
AJUSTES_DE_CAMPO = [
    ("    lpfn_wnd_proc: rawptr", "    lpfn_wnd_proc: proc(h_wnd: ?^Hwnd, msg: u32, w_param: u64, l_param: i64) -> i64"),
]


def LerSimbolos(Caminho):
    """Devolve (procs por DLL, prefixos de constantes) de simbolos.txt."""
    Secao = None
    PorDll = {}
    Prefixos = []
    for Linha in open(Caminho, encoding="utf-8"):
        Texto = Linha.split("#", 1)[0].strip()
        if not Texto:
            continue
        if Texto.startswith("[") and Texto.endswith("]"):
            Secao = Texto[1:-1]
            continue
        if Secao is None:
            raise ValueError("simbolo antes de qualquer [secao]: " + Texto)
        if Secao == "constantes":
            Prefixos.append(Texto)
        else:
            PorDll.setdefault(Secao, []).append(Texto)
    return PorDll, Prefixos


def EscreverPedidos(PorDll, Prefixos, Destino):
    Nomes = [Nome for Lista in PorDll.values() for Nome in Lista]
    Nomes += [P if P.endswith("_") is False else P + "*" for P in Prefixos]
    with open(Destino, "w", encoding="utf-8", newline="\n") as Saida:
        Saida.write("\n".join(Nomes) + "\n")


def RodarLigar(Pedidos, Bruto):
    Comando = [sys.executable, LIGAR, "windows.h"]
    for Extra in HEADERS_EXTRAS:
        Comando += ["--incluir", Extra]
    Comando += ["-D_WIN32_WINNT=0x0A00", "-DWINVER=0x0A00"]  # Windows 10: sem isto o mingw esconde GetDpiForSystem/SetProcessDpiAwarenessContext
    Comando += ["--simbolos", Pedidos, "--link", "user32", "-o", Bruto]
    Resultado = subprocess.run(Comando, capture_output=True, text=True)
    if Resultado.returncode != 0:
        raise RuntimeError("forja ligar falhou: " + Resultado.stderr[-600:])


def DividirPorDll(Bruto, PorDll):
    """Troca o unico bloco `pub foreign { link ... }` por um bloco por DLL; devolve o texto final."""
    DllDoProc = {Nome: Dll for Dll, Lista in PorDll.items() for Nome in Lista}
    Linhas = open(Bruto, encoding="utf-8").read().split("\n")
    Antes, Opacos, Procs, Depois = [], [], {}, []
    Estado = 0
    for Linha in Linhas:
        if Estado == 0:
            if Linha.strip() == "pub foreign {":
                Estado = 1
            else:
                Antes.append(Linha)
        elif Estado == 1:
            if Linha == "}":
                Estado = 2
            elif Linha.strip().startswith("link "):
                continue
            elif Linha.strip().startswith("opaque "):
                Opacos.append(Linha)
            else:
                Achou = RE_PROC.match(Linha)
                if Achou is None:
                    raise ValueError("linha inesperada no bloco foreign: " + Linha)
                Dll = DllDoProc.get(Achou.group(1))
                if Dll is None:
                    continue  # proc que so entrou por casar um prefixo de [constantes] (ex.: HWND_UserSize): nao foi pedido
                Procs.setdefault(Dll, []).append(Linha)
        else:
            Depois.append(Linha)
    Blocos = []
    for Indice, Dll in enumerate(sorted(Procs)):
        Corpo = ["pub foreign {", '    link "%s"' % Dll]
        if Indice == 0:
            Corpo += Opacos
        Corpo += Procs[Dll] + ["}", ""]
        Blocos += Corpo
    return "\n".join(Antes + Blocos + Depois), sum(len(L) for L in Procs.values())


def Gerar(Destino):
    PorDll, Prefixos = LerSimbolos(os.path.join(PASTA, "simbolos.txt"))
    with tempfile.TemporaryDirectory() as Tmp:
        Pedidos = os.path.join(Tmp, "pedidos.txt")
        Bruto = os.path.join(Tmp, "bruto.fj")
        EscreverPedidos(PorDll, Prefixos, Pedidos)
        RodarLigar(Pedidos, Bruto)
        Texto, QtdProcs = DividirPorDll(Bruto, PorDll)
    for Antigo, Novo in AJUSTES_DE_CAMPO:
        if Texto.count(Antigo + "\n") != 1:
            raise ValueError("campo a ajustar nao encontrado (uma vez): " + Antigo.strip())
        Texto = Texto.replace(Antigo + "\n", Novo + "\n")
    with open(Destino, "w", encoding="utf-8", newline="\n") as Saida:
        Saida.write(Texto)
    return QtdProcs, len(PorDll)


def Selftest():
    Destino = os.path.join(tempfile.gettempdir(), "win32_selftest.fj")
    QtdProcs, QtdDlls = Gerar(Destino)
    Texto = open(Destino, encoding="utf-8").read()
    Blocos = Texto.count("pub foreign {")
    assert QtdProcs >= 200, QtdProcs
    assert Blocos >= 8, Blocos
    assert 'link "gdi32"' in Texto and 'link "kernel32"' in Texto
    print("selftest ok: %d procs em %d blocos (%d DLLs pedidas)" % (QtdProcs, Blocos, QtdDlls))


def Principal():
    if "--selftest" in sys.argv:
        Selftest()
        return
    QtdProcs, _ = Gerar(os.path.join(PASTA, "win32.fj"))
    print("win32.fj gerado: %d procs" % QtdProcs)


if __name__ == "__main__":
    Principal()
