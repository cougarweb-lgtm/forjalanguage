"""forja: compilador Forja (prototipo Python) - `forja check` e `forja build --modo c`.

Versao: 1.0.1
Changelog: 1.0.1 - passada de desempenho (gc desligado, lexico por bytes, fast paths no parser/checker/emissor).
Verificacao: python C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/medir/rodar_suite.py --compilador "python C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/forja.py" --corpus C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/corpus/dourado --registro C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/_tentativas.jsonl
"""

import gc
import json
import os
import sys
import time
import traceback
from dataclasses import dataclass, field

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from forja_check import ChecarArquivo  # noqa: E402
from forja_emit import EmitirC  # noqa: E402
from forja_erro import EntradaSaidaErro, ForjaErro, PosicaoFixaErro, TIPOS_POR_CODIGO  # noqa: E402
from forja_erro import EXPLICACAO_POR_CODIGO, FORMA_CERTA_POR_CODIGO  # noqa: E402
from forja_lex import LexExato, LexRapido, PosicaoDoDeslocamento  # noqa: E402
from forja_parse import ParsearArquivo  # noqa: E402

LIMITE_RECURSAO = 20000
USO = (
    "uso: forja check <arq.fj> [--tempos <t.json>]\n"
    "     forja build --modo c <arq.fj> -o <saida.c> [--tempos <t.json>]"
)


class UsoErro(Exception):
    """Linha de comando invalida (exit 2)."""


@dataclass
class Pedido:
    Comando: str
    Arquivo: str
    Saida: str = ""
    Tempos: str = ""


@dataclass
class Medidas:
    Lex: int = 0
    Parse: int = 0
    Check: int = 0
    Emit: int = 0
    Total: int = 0
    Linhas: int = 0
    Deslocamentos: list = field(default_factory=list)


def LerPedido(Argumentos: list[str]) -> Pedido:
    if not Argumentos or Argumentos[0] not in ("check", "build"):
        raise UsoErro("comando deve ser 'check' ou 'build'")
    Resto = Argumentos[1:]
    Opcoes = {}
    Soltos = []
    Indice = 0
    while Indice < len(Resto):
        if Resto[Indice] in ("--modo", "-o", "--tempos"):
            if Indice + 1 >= len(Resto):
                raise UsoErro(f"falta o valor de {Resto[Indice]}")
            Opcoes[Resto[Indice]] = Resto[Indice + 1]
            Indice += 2
        else:
            Soltos.append(Resto[Indice])
            Indice += 1
    if len(Soltos) != 1:
        raise UsoErro(f"esperado 1 arquivo .fj, recebido {len(Soltos)}")
    Pedido_ = Pedido(Argumentos[0], Soltos[0], Opcoes.get("-o", ""), Opcoes.get("--tempos", ""))
    if Pedido_.Comando == "build" and (Opcoes.get("--modo") != "c" or not Pedido_.Saida):
        raise UsoErro("build exige --modo c e -o <saida.c>")
    return Pedido_


def LerFonte(Caminho: str) -> str:
    try:
        with open(Caminho, "rb") as Arquivo:
            return Arquivo.read().decode("latin-1")
    except OSError as Falha:
        raise EntradaSaidaErro(f"nao abriu {Caminho}: {Falha}") from Falha


def ContarLinhas(Texto: str) -> int:
    return Texto.count("\n") + (1 if Texto and not Texto.endswith("\n") else 0)


def GravarTexto(Caminho: str, Texto: str) -> None:
    try:
        with open(Caminho, "w", encoding="ascii", newline="\n") as Arquivo:
            Arquivo.write(Texto)
    except (OSError, UnicodeEncodeError) as Falha:
        raise EntradaSaidaErro(f"nao gravou {Caminho}: {Falha}") from Falha


def Compilar(Pedido_: Pedido, Texto: str, Medidas_: Medidas) -> None:
    """Roda as etapas e anota os tempos; levanta ForjaErro no 1o diagnostico."""
    Relogio = time.perf_counter_ns
    Inicio = Relogio()
    Tokens = LexRapido(Texto)
    if Tokens is None:
        Tokens, Medidas_.Deslocamentos = LexExato(Texto)
    Lexado = Relogio()
    Medidas_.Lex = Lexado - Inicio
    Decls = ParsearArquivo(Tokens)
    Parseado = Relogio()
    Medidas_.Parse = Parseado - Lexado
    ChecarArquivo(Decls)
    Checado = Relogio()
    Medidas_.Check = Checado - Parseado
    if Pedido_.Comando == "build":
        GravarTexto(Pedido_.Saida, EmitirC(Decls))
        Medidas_.Emit = Relogio() - Checado


def LocalizarErro(Erro: ForjaErro, Texto: str, Medidas_: Medidas) -> tuple[int, int]:
    if isinstance(Erro, PosicaoFixaErro):
        return Erro.Linha, Erro.Coluna
    if Erro.Deslocamento >= 0:
        return PosicaoDoDeslocamento(Texto, Erro.Deslocamento)
    if not Medidas_.Deslocamentos:
        _, Medidas_.Deslocamentos = LexExato(Texto)
    return PosicaoDoDeslocamento(Texto, Medidas_.Deslocamentos[Erro.Tok])


def MontarDiagnostico(Erro: ForjaErro, Arquivo: str, Linha: int, Coluna: int) -> str:
    Registro = {
        "codigo": Erro.Codigo,
        "tipo": TIPOS_POR_CODIGO[Erro.Codigo],
        "arquivo": Arquivo.replace("\\", "/"),
        "linha": Linha,
        "coluna": Coluna,
        "causa": Erro.Causa,
        "explicacao": EXPLICACAO_POR_CODIGO[Erro.Codigo],
        "forma_certa": FORMA_CERTA_POR_CODIGO[Erro.Codigo],
    }
    return json.dumps(Registro, ensure_ascii=True) + "\n"


def GravarTempos(Caminho: str, Medidas_: Medidas) -> None:
    Dados = {
        "lex_ns": Medidas_.Lex, "parse_ns": Medidas_.Parse, "check_ns": Medidas_.Check,
        "emit_ns": Medidas_.Emit, "total_ns": Medidas_.Total, "linhas": Medidas_.Linhas,
    }
    GravarTexto(Caminho, json.dumps(Dados) + "\n")


def Executar(Argumentos: list[str]) -> int:
    Pedido_ = LerPedido(Argumentos)
    Relogio = time.perf_counter_ns
    Inicio = Relogio()
    Texto = LerFonte(Pedido_.Arquivo)
    Medidas_ = Medidas(Linhas=ContarLinhas(Texto))
    Codigo = 0
    try:
        Compilar(Pedido_, Texto, Medidas_)
    except ForjaErro as Erro:
        Linha, Coluna = LocalizarErro(Erro, Texto, Medidas_)
        sys.stdout.buffer.write(MontarDiagnostico(Erro, Pedido_.Arquivo, Linha, Coluna).encode("ascii"))
        Codigo = 1
    Medidas_.Total = Relogio() - Inicio
    if Pedido_.Tempos:
        GravarTempos(Pedido_.Tempos, Medidas_)
    return Codigo


def Principal() -> int:
    sys.setrecursionlimit(LIMITE_RECURSAO)
    gc.disable()  # a AST nao tem ciclos e o processo e curto: a coleta ciclica so gastaria tempo
    try:
        return Executar(sys.argv[1:])
    except (UsoErro, EntradaSaidaErro) as Falha:
        sys.stderr.write(f"forja: {Falha}\n{USO}\n")
        return 2
    except Exception:  # noqa: BLE001 - qualquer outra falha e erro interno do compilador (exit 3)
        traceback.print_exc()
        return 3


if __name__ == "__main__":
    sys.exit(Principal())
