# -*- coding: utf-8 -*-
"""variante_seg.py - segmentos de fonte com origem e mapa de posicao (P-IA1-a08): base do preprocessador variante.py.

Versao 0.1.0
Changelog:
  0.1.0 (11/11/2026) primeira versao: Seg (Txt + linha/coluna de origem + se e copia fiel), Lexar (uma linha em segmentos;
        o fonte e lido em latin-1, entao 1 caractere = 1 byte e a coluna e a do compilador, que conta bytes), Juntar, MapaDaLinha
        e PosicaoOrigem (linha/coluna da saida -> linha/coluna do fonte do agente).

Uso interno: importado por variante.py e pelas regras variante_*.py.
Reversao: apagar este arquivo. Depende de: Python 3.9+ (biblioteca padrao). Verificacao: python variante.py --selftest
"""
import re

PADRAO = re.compile(
    r'(?P<esp>[ \t\r]+)|(?P<coment>//.*)|(?P<ftexto>f"(?:[^"\\]|\\.)*")|(?P<texto>"(?:[^"\\]|\\.)*")'
    r'|(?P<num>0[xb][0-9A-Za-z_]+|[0-9]+(?:\.[0-9]+)?)|(?P<nome>[A-Za-z_][A-Za-z0-9_]*)'
    r'|(?P<simb>\.\.<|\.\.=|\.\.|==|!=|<=|>=|&&|\|\||\+\+|--|\+=|-=|\*=|/=|->|<<|>>|.)')
INSIGNIFICANTES = ("esp", "coment")


class Seg:
    """Pedaco de linha. Ident=True: copia fiel do fonte (cada caractere mapeia 1 a 1); False: texto novo, mapeia para (Lin, Col)."""
    __slots__ = ("Tipo", "Txt", "Lin", "Col", "Ident")

    def __init__(self, Tipo, Txt, Lin, Col, Ident=True):
        self.Tipo, self.Txt, self.Lin, self.Col, self.Ident = Tipo, Txt, Lin, Col, Ident

    def __repr__(self):
        return "Seg(%s,%r)" % (self.Tipo, self.Txt)


def Lexar(Linha, NumLinha):
    """Divide uma linha (sem o \\n) em segmentos fieis; Col e 0-based."""
    Segs = []
    for Achado in PADRAO.finditer(Linha):
        Segs.append(Seg(Achado.lastgroup, Achado.group(), NumLinha, Achado.start()))
    return Segs


def Novo(Tipo, Txt, Ancora):
    """Segmento sintetico que aponta para a origem da Ancora."""
    return Seg(Tipo, Txt, Ancora.Lin, Ancora.Col, False)


def Espaco(Ancora, Txt=" "):
    return Novo("esp", Txt, Ancora)


def Juntar(Segs):
    return "".join(S.Txt for S in Segs)


def Sig(Segs):
    """Indices dos segmentos significativos (fora espaco e comentario)."""
    return [I for I, S in enumerate(Segs) if S.Tipo not in INSIGNIFICANTES]


def MapaDaLinha(Segs):
    """[(col_saida, tamanho, lin_origem, col_origem, ident)] de uma linha de saida."""
    Mapa, Pos = [], 0
    for S in Segs:
        Mapa.append((Pos, len(S.Txt), S.Lin, S.Col, S.Ident))
        Pos += len(S.Txt)
    return Mapa


def PosicaoOrigem(Mapa, Coluna):
    """Coluna 1-based da saida -> (linha, coluna 1-based) do fonte; Mapa e o MapaDaLinha da linha de saida."""
    Alvo = Coluna - 1
    Ultimo = None
    for Ini, Tam, Lin, Col, Ident in Mapa:
        Ultimo = (Ini, Tam, Lin, Col, Ident)
        if Ini <= Alvo < Ini + Tam:
            return Lin, Col + (Alvo - Ini if Ident else 0) + 1
    if Ultimo is None:
        return None
    Ini, Tam, Lin, Col, Ident = Ultimo
    return Lin, Col + (Tam if Ident else 0) + 1
