"""Portao de cobertura da doc (marco Ryu P2b, regra "DOC EM DIA" do MEM-FORJA-GUIA-00): o agente de IA so usa o que o cartao mostra.

Confere que o texto de `forja spec --cartao` cita (1) todo modulo da std (um `std:<nome>` para cada `std\\<nome>.fj`) e (2) todo embutido do compilador;
e que `forja spec` + o cartao citam (3) todo operador do lexer (tokens de operador de token.h; token novo sem lexema conhecido reprova) e (4) toda opcao `--xxx` da tabela OPCOES_CLI de driver\\opcoes.c.
A lista de embutidos NAO e mantida a mao: sai do enum EmbutidoId de compiler\\frontend\\ast.h (EMB_FOO -> `foo`, que e o nome no fonte), entao um embutido
novo sem doc reprova sozinho. A citacao conta como palavra inteira (`sort` nao casa com `sort_by`).

Uso: python -I tests\\doc\\verificar_cobertura_doc.py --forja C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/compiler/forja.exe
     python -I tests\\doc\\verificar_cobertura_doc.py --selftest
Saida: uma linha por falta ("FALTA modulo std:x" / "FALTA embutido x" / "FALTA operador x" / "FALTA opcao --x") e "verificar_cobertura_doc: N falha(s)"; exit 0 so com N = 0.
Versao: 1.1.0
Changelog: 1.1.0 - O0 P6: cobertura de operadores (token.h) e de opcoes (opcoes.c) na spec + cartao; o selftest ganha os casos.
           1.0.0 - P2b (Ryu, 16/11/2026): primeira versao.
Verificacao: python -I tests\\doc\\verificar_cobertura_doc.py --selftest
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

LINGUAGEM = Path(__file__).resolve().parent.parent.parent
AST_H = LINGUAGEM / "compiler" / "frontend" / "ast.h"
PASTA_STD = LINGUAGEM / "std"
TOKEN_H = LINGUAGEM / "compiler" / "frontend" / "token.h"
OPCOES_C = LINGUAGEM / "compiler" / "driver" / "opcoes.c"
MINIMO_OPCOES = 30
# lexema de cada token de operador; pontuacao simples (parenteses, virgula, dois-pontos, '=') fica em IGNORADOS
LEXEMAS = {"TK_PLUS": "+", "TK_MINUS": "-", "TK_STAR": "*", "TK_SLASH": "/", "TK_PERCENT": "%", "TK_EQ": "==", "TK_NE": "!=",
           "TK_LT": "<", "TK_LE": "<=", "TK_GT": ">", "TK_GE": ">=", "TK_PLUSEQ": "+=", "TK_MINUSEQ": "-=", "TK_ARROW": "->",
           "TK_DOTDOT": "..", "TK_DOTDOTLT": "..<", "TK_DOTDOTEQ": "..=", "TK_DOT": ".", "TK_CARET": "^", "TK_AMP": "&",
           "TK_BANG": "!", "TK_PIPE": "|", "TK_TIL": "~", "TK_SHL": "<<", "TK_SHR": ">>", "TK_INTERROGA": "?", "TK_HASH": "#"}
IGNORADOS = {"TK_EOF", "TK_NL", "TK_IDENT", "TK_INT", "TK_REAL", "TK_TEXTO", "TK_LPAREN", "TK_RPAREN", "TK_LBRACE", "TK_RBRACE",
             "TK_COMMA", "TK_COLON", "TK_ASSIGN", "TK_LCOLCHETE", "TK_RCOLCHETE", "TK_QTD"}
MINIMO_EMBUTIDOS = 80  # o enum tem mais de 100; abaixo disto o parser do ast.h quebrou


def EmbutidosDoCompilador(TextoAst):
    """Nomes de fonte dos embutidos: os EMB_* do enum EmbutidoId, em minuscula e sem o prefixo."""
    Sem = re.sub(r"/\*.*?\*/", "", TextoAst, flags=re.S)
    Achou = re.search(r"typedef\s+enum\s*\{\s*EMB_PRINT.*?\}\s*EmbutidoId", Sem, flags=re.S)
    if not Achou:
        raise ValueError("enum EmbutidoId nao encontrado em ast.h")
    Nomes = [Nome[4:].lower() for Nome in re.findall(r"\bEMB_[A-Z0-9_]+", Achou.group(0))]
    if len(Nomes) < MINIMO_EMBUTIDOS:
        raise ValueError(f"so {len(Nomes)} embutidos lidos de ast.h (esperado >= {MINIMO_EMBUTIDOS})")
    return sorted(set(Nomes))


def OperadoresDoLexer(TextoToken):
    """Lexemas dos tokens de operador do enum TokenTipo; token fora de LEXEMAS e IGNORADOS (e que nao seja KW_*/TK_FTEXTO_*) e erro: ninguem decidiu se e operador."""
    Sem = re.sub(r"/\*.*?\*/", "", TextoToken, flags=re.S)
    Achou = re.search(r"typedef\s+enum\s*\{.*?\}\s*TokenTipo", Sem, flags=re.S)
    if not Achou:
        raise ValueError("enum TokenTipo nao encontrado em token.h")
    Lexemas = []
    for Nome in re.findall(r"\bTK_[A-Z0-9_]+", Achou.group(0)):
        if Nome in LEXEMAS:
            Lexemas.append(LEXEMAS[Nome])
        elif Nome not in IGNORADOS and not Nome.startswith("TK_FTEXTO_"):
            raise ValueError(f"token {Nome} sem lexema conhecido: acrescente a LEXEMAS ou a IGNORADOS")
    return sorted(set(Lexemas))


def OpcoesDoDriver(TextoOpcoes):
    """Toda `--xxx` da tabela OPCOES_CLI de opcoes.c."""
    Opcoes = sorted(set(re.findall(r'\{"(--[\w-]+)", OPCAO_', TextoOpcoes)))
    if len(Opcoes) < MINIMO_OPCOES:
        raise ValueError(f"so {len(Opcoes)} opcoes lidas de opcoes.c (esperado >= {MINIMO_OPCOES})")
    return Opcoes


def FaltasOperadoresOpcoes(TextoDoc, Operadores, Opcoes):
    Saida = [f"FALTA operador {Op}" for Op in Operadores if Op not in TextoDoc]
    Saida += [f"FALTA opcao {Nome}" for Nome in Opcoes if not re.search(r"(?<![\w-])" + re.escape(Nome) + r"(?![\w-])", TextoDoc)]
    return Saida


def ModulosDaStd():
    """Um `std:<nome>` por arquivo std\\<nome>.fj."""
    return sorted(f"std:{Arquivo.stem}" for Arquivo in PASTA_STD.glob("*.fj"))


def Citado(Texto, Nome):
    return re.search(r"(?<![\w])" + re.escape(Nome) + r"(?![\w])", Texto) is not None


def Faltas(TextoCartao, Modulos, Embutidos):
    Saida = [f"FALTA modulo {Modulo}" for Modulo in Modulos if not Citado(TextoCartao, Modulo)]
    Saida += [f"FALTA embutido {Nome}" for Nome in Embutidos if not Citado(TextoCartao, Nome)]
    return Saida


def LerCartao(Forja):
    Rodada = subprocess.run([Forja, "spec", "--cartao"], capture_output=True, timeout=60)
    if Rodada.returncode != 0:
        raise RuntimeError(f"forja spec --cartao saiu {Rodada.returncode}")
    return Rodada.stdout.decode("utf-8")


def LerSpec(Forja):
    Rodada = subprocess.run([Forja, "spec"], capture_output=True, timeout=60)
    if Rodada.returncode != 0:
        raise RuntimeError(f"forja spec saiu {Rodada.returncode}")
    return Rodada.stdout.decode("utf-8")


def Selftest():
    """O detector pega uma falta plantada (modulo e embutido) e nao acusa um texto completo."""
    Modulos = ["std:aaa", "std:bbb"]
    Embutidos = ["sort", "sort_by"]
    Completo = "std:aaa std:bbb `sort` e sort_by"
    Problemas = []
    if Faltas(Completo, Modulos, Embutidos):
        Problemas.append("acusou falta num texto completo")
    if Faltas("std:aaa `sort_by`", Modulos, Embutidos) != ["FALTA modulo std:bbb", "FALTA embutido sort"]:
        Problemas.append("nao pegou modulo e embutido plantados (sort nao pode casar com sort_by)")
    if Faltas("std:aaa std:bbbb sort sort_by", Modulos, Embutidos) != ["FALTA modulo std:bbb"]:
        Problemas.append("std:bbbb nao pode valer por std:bbb")
    Lidos = EmbutidosDoCompilador(AST_H.read_text(encoding="utf-8"))
    for Esperado in ("print", "sort_by", "heap_pop", "wrapping_add", "set_diff", "to_str"):
        if Esperado not in Lidos:
            Problemas.append(f"ast.h nao rendeu o embutido {Esperado}")
    if FaltasOperadoresOpcoes("a ~ b `--aaa`", ["~", "<<"], ["--aaa", "--bbb"]) != ["FALTA operador <<", "FALTA opcao --bbb"]:
        Problemas.append("nao pegou operador e opcao plantados")
    if FaltasOperadoresOpcoes("--no-x", [], ["--no"]) != ["FALTA opcao --no"]:
        Problemas.append("--no-x nao pode valer por --no")
    Ops = OperadoresDoLexer(TOKEN_H.read_text(encoding="utf-8"))
    for Esperado in ("~", "<<", ">>", "|", "&", "..<", "+="):
        if Esperado not in Ops:
            Problemas.append(f"token.h nao rendeu o operador {Esperado}")
    try:
        OperadoresDoLexer("typedef enum { TK_EOF, TK_NOVO } TokenTipo;")
        Problemas.append("token novo sem lexema nao reprovou")
    except ValueError:  # lint-ok: iaf-erro-engolido o ValueError e o resultado esperado do teste
        pass
    if "--checagem" not in OpcoesDoDriver(OPCOES_C.read_text(encoding="utf-8")):
        Problemas.append("opcoes.c nao rendeu --checagem")
    if "std:sync" not in ModulosDaStd():
        Problemas.append("std\\sync.fj nao listado")
    for Linha in Problemas:
        print(Linha)
    print(f"verificar_cobertura_doc --selftest: {len(Problemas)} falha(s)")
    return 0 if not Problemas else 1


def Main():
    Analisador = argparse.ArgumentParser(description="Cobertura da doc: todo std:* e todo embutido no cartao")
    Analisador.add_argument("--forja", help="caminho absoluto do forja.exe")
    Analisador.add_argument("--selftest", action="store_true")
    Opcoes = Analisador.parse_args()
    if Opcoes.selftest:
        return Selftest()
    if not Opcoes.forja or not Path(Opcoes.forja).is_absolute():
        Analisador.error("--forja ABSOLUTO e obrigatorio (exceto com --selftest)")
    Embutidos = EmbutidosDoCompilador(AST_H.read_text(encoding="utf-8"))
    Cartao = LerCartao(Opcoes.forja)
    Linhas = Faltas(Cartao, ModulosDaStd(), Embutidos)
    Linhas += FaltasOperadoresOpcoes(LerSpec(Opcoes.forja) + "\n" + Cartao, OperadoresDoLexer(TOKEN_H.read_text(encoding="utf-8")),
                                     OpcoesDoDriver(OPCOES_C.read_text(encoding="utf-8")))
    for Linha in Linhas:
        print(Linha)
    print(f"verificar_cobertura_doc: {len(Linhas)} falha(s)")
    return 0 if not Linhas else 1


if __name__ == "__main__":
    sys.exit(Main())
