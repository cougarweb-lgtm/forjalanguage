# -*- coding: utf-8 -*-
"""Trava unica da bancada de medicao (arquivo em %TEMP% + lock do SO via LockFileEx).

Objetivo: so um cronometro por vez e nenhum teste pesado de correcao rodando junto. Dois modos:
  - EXCLUSIVO: para cronometrar; espera ate ter a trava (timeout) e nao divide com ninguem;
  - COMPARTILHADO: para testes de correcao/Ir; varios ao mesmo tempo, mas nunca junto de um EXCLUSIVO.
O lock e do SO (morre com o processo): processo morto nao deixa trava presa.

Uso:
    with TravaBancada(MODO_EXCLUSIVO, TimeoutS=600):
        ...cronometrar...
Verificacao: python -I -B trava_bancada.py --selftest
Versao: 1.0.0
Changelog: 1.0.0 - primeira versao (decisao D0696, medir_rapido).
"""

import ctypes
import msvcrt
import os
import sys
import tempfile
import time
from ctypes import wintypes
from pathlib import Path

VERSAO = "1.0.0"
MODO_EXCLUSIVO = "exclusivo"
MODO_COMPARTILHADO = "compartilhado"
TIMEOUT_PADRAO_S = 1800.0
PASSO_ESPERA_S = 0.25
ARQUIVO_TRAVA = Path(tempfile.gettempdir()) / "forja_bancada.lock"
LOCKFILE_FAIL_IMMEDIATELY = 0x1
LOCKFILE_EXCLUSIVE_LOCK = 0x2
ERRO_LOCK_VIOLATION = 33


class ErroTrava(Exception):
    """Nao foi possivel obter a trava da bancada dentro do prazo."""


class _Overlapped(ctypes.Structure):
    _fields_ = [("Internal", ctypes.c_void_p), ("InternalHigh", ctypes.c_void_p),
                ("Offset", wintypes.DWORD), ("OffsetHigh", wintypes.DWORD), ("hEvent", wintypes.HANDLE)]


def _Kernel32():
    K = ctypes.WinDLL("kernel32", use_last_error=True)
    K.LockFileEx.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD,
                             ctypes.POINTER(_Overlapped)]
    K.LockFileEx.restype = wintypes.BOOL
    K.UnlockFileEx.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD, wintypes.DWORD,
                               ctypes.POINTER(_Overlapped)]
    K.UnlockFileEx.restype = wintypes.BOOL
    return K


def _TentarLock(Kernel, Handle, Flags):
    Sobreposto = _Overlapped()
    if Kernel.LockFileEx(Handle, Flags | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, ctypes.byref(Sobreposto)):
        return True
    Erro = ctypes.get_last_error()
    if Erro != ERRO_LOCK_VIOLATION:
        raise OSError(Erro, f"LockFileEx falhou (erro {Erro}) em {ARQUIVO_TRAVA}")
    return False


class TravaBancada:
    """Contexto `with`: obtem a trava no modo pedido (espera ate TimeoutS) e a solta ao sair."""

    def __init__(self, Modo=MODO_EXCLUSIVO, *, TimeoutS=TIMEOUT_PADRAO_S, Caminho=None):
        if Modo not in (MODO_EXCLUSIVO, MODO_COMPARTILHADO):
            raise ValueError(f"modo de trava invalido: {Modo!r}")
        self.Modo = Modo
        self.TimeoutS = TimeoutS
        self.Caminho = Path(Caminho) if Caminho else ARQUIVO_TRAVA
        self.Arquivo = None
        self.Kernel = None

    def __enter__(self):
        self.Kernel = _Kernel32()
        self.Arquivo = open(self.Caminho, "ab")
        Handle = msvcrt.get_osfhandle(self.Arquivo.fileno())
        Flags = LOCKFILE_EXCLUSIVE_LOCK if self.Modo == MODO_EXCLUSIVO else 0
        Limite = time.monotonic() + self.TimeoutS
        while not _TentarLock(self.Kernel, Handle, Flags):
            if time.monotonic() >= Limite:
                self._Fechar()
                raise ErroTrava(f"trava da bancada ({self.Modo}) ocupada por outro medidor apos {self.TimeoutS:.0f}s: {self.Caminho}")
            time.sleep(PASSO_ESPERA_S)
        return self

    def _Fechar(self):
        if self.Arquivo is not None:
            self.Arquivo.close()
            self.Arquivo = None

    def __exit__(self, TipoErro, Erro, Pilha):
        if self.Arquivo is not None:
            Handle = msvcrt.get_osfhandle(self.Arquivo.fileno())
            self.Kernel.UnlockFileEx(Handle, 0, 1, 0, ctypes.byref(_Overlapped()))
        self._Fechar()
        return False


def Afirmar(Condicao, Mensagem):
    if not Condicao:
        raise AssertionError(Mensagem)


def _Ocupada(Modo, Caminho):
    try:
        with TravaBancada(Modo, TimeoutS=0.3, Caminho=Caminho):
            return False
    except ErroTrava:
        return True


def Selftest():
    Caminho = Path(tempfile.gettempdir()) / f"forja_bancada_selftest_{os.getpid()}.lock"
    try:
        with TravaBancada(MODO_EXCLUSIVO, Caminho=Caminho):
            Afirmar(_Ocupada(MODO_EXCLUSIVO, Caminho), "exclusivo deveria bloquear outro exclusivo")
            Afirmar(_Ocupada(MODO_COMPARTILHADO, Caminho), "exclusivo deveria bloquear compartilhado")
        with TravaBancada(MODO_COMPARTILHADO, Caminho=Caminho):
            Afirmar(not _Ocupada(MODO_COMPARTILHADO, Caminho), "dois compartilhados deveriam coexistir")
            Afirmar(_Ocupada(MODO_EXCLUSIVO, Caminho), "compartilhado deveria bloquear exclusivo")
        Afirmar(not _Ocupada(MODO_EXCLUSIVO, Caminho), "trava deveria ter sido solta ao sair do with")
        try:
            TravaBancada("xis")
        except ValueError:  # lint-ok: iaf-erro-engolido o ValueError e o resultado esperado do teste
            pass
        else:
            Afirmar(False, "modo invalido deveria falhar")
    finally:
        Caminho.unlink(missing_ok=True)
    print(f"trava_bancada {VERSAO}: selftest OK")
    return True


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        sys.exit(0 if Selftest() else 1)
    print(__doc__)
