"""Lexico da Forja: texto (latin-1, 1 char = 1 byte) para lista plana de tokens.

Versao: 1.0.0
Changelog: 1.0.0 - caminho rapido (findall) e caminho exato com deslocamentos (finditer).
Verificacao: python C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/medir/rodar_suite.py --compilador "python C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/forja.py" --corpus C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/corpus/dourado --registro C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/_tentativas.jsonl
"""

import re

from forja_erro import ForjaErro

TOKEN_NL = "\n"
TOKEN_EOF = "\x00EOF"

PADRAO_TOKEN = (
    r"[A-Za-z_][A-Za-z0-9_]*|[0-9]+\.[0-9]+|[0-9]+"
    r"|\.\.<|\.\.=|\.\.|==|!=|<=|>=|\+=|-=|->"
    r"|[-+*/%<>=(){},:.^&]|\n"
)
RE_RAPIDO = re.compile(PADRAO_TOKEN)
RE_COMENTARIO = re.compile(r"//[^\n]*")
# Qualquer coisa que o caminho rapido nao saiba tratar: manda o texto para o caminho exato.
RE_SUSPEITO = re.compile(r"[^ \t\r\nA-Za-z0-9_(){},:=+\-*/%<>.^&!]|!(?!=)")
# Bytes aceitos sem exame; '!' fica de fora porque so vale em '!=' (quem decide e a RE_SUSPEITO).
BYTES_VALIDOS = b" \t\r\nABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_(){},:=+-*/%<>.^&"
RE_EXATO = re.compile(
    r"(?P<esp>[ \t\r]+|//[^\n]*)"
    r'|(?P<str>"[^"\n]*")'
    r"|(?P<tok>" + PADRAO_TOKEN + r")"
    r'|(?P<aberta>")'
    r"|(?P<ruim>.)"
)


def LexExato(Texto: str) -> tuple[list[str], list[int]]:
    """Tokeniza com deslocamento de cada token; levanta E0001 no primeiro erro lexico."""
    Tokens: list[str] = []
    Deslocamentos: list[int] = []
    for Casado in RE_EXATO.finditer(Texto):
        Grupo = Casado.lastgroup
        if Grupo == "esp":
            continue
        if Grupo == "aberta":
            raise ForjaErro("E0001", "string sem fechar na linha", Deslocamento=Casado.start())
        if Grupo == "ruim":
            Simbolo = Casado.group()
            raise ForjaErro("E0001", f"caractere invalido {Simbolo!r}", Deslocamento=Casado.start())
        Tokens.append(Casado.group())
        Deslocamentos.append(Casado.start())
    Tokens.append(TOKEN_EOF)
    Deslocamentos.append(len(Texto))
    return Tokens, Deslocamentos


def LexRapido(Texto: str) -> list[str] | None:
    """Caminho rapido; devolve None quando o texto exige o caminho exato (aspas ou caractere estranho)."""
    if '"' in Texto:
        return None
    Limpo = RE_COMENTARIO.sub("", Texto) if "//" in Texto else Texto
    Restante = Limpo.encode("latin-1").translate(None, BYTES_VALIDOS)
    if Restante and RE_SUSPEITO.search(Limpo):
        return None
    Tokens = RE_RAPIDO.findall(Limpo)
    Tokens.append(TOKEN_EOF)
    return Tokens


def PosicaoDoDeslocamento(Texto: str, Deslocamento: int) -> tuple[int, int]:
    """Linha e coluna (1-based, colunas em bytes) de um deslocamento do texto."""
    Linha = Texto.count("\n", 0, Deslocamento) + 1
    Coluna = Deslocamento - (Texto.rfind("\n", 0, Deslocamento) + 1) + 1
    return Linha, Coluna
