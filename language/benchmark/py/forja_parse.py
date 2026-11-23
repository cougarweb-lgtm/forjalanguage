"""Parser da Forja: lista de tokens para AST. Para no 1o E0101.

Versao: 1.0.0
Changelog: 1.0.0 - descida recursiva com escalada de precedencia para os operadores binarios.
Verificacao: python C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/medir/rodar_suite.py --compilador "python C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/forja.py" --corpus C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/corpus/dourado --registro C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/_tentativas.jsonl
"""

from forja_ast import (
    Assign, Binary, BoolLit, Call, CallStmt, Cast, ConstDecl, Field, FieldDecl, For, If, IntLit,
    LetVar, Name, ParamDecl, ProcDecl, RealLit, Return, StructDecl, StructLit, Unary, UnitDecl, While,
)
from forja_erro import ForjaErro
from forja_lex import TOKEN_EOF, TOKEN_NL

PALAVRAS_CHAVE = frozenset(
    "const let var proc struct unit if else for in while return as and or not true false".split()
)
PRECEDENCIA = {
    "or": 1, "and": 2,
    "==": 3, "!=": 3, "<": 3, "<=": 3, ">": 3, ">=": 3,
    "+": 4, "-": 4, "*": 5, "/": 5, "%": 5,
}
PREC_COMPARACAO = 3
OPERADORES_ATRIBUICAO = ("=", "+=", "-=")
INICIOS_ATRIBUICAO = (".", "=", "+=", "-=")
CLASSE_NOME = 1
CLASSE_INTEIRO = 2
CLASSE_OUTRO = 3
NOME_ESPECIAL = ("(", "{", ".")
# Texto do token -> classe; os nomes repetem muito, entao a classificacao e memorizada.
CLASSES: dict[str, int] = {}


def ClassificarToken(Texto: str) -> int:
    if Texto.isidentifier() and Texto not in PALAVRAS_CHAVE:
        Classe = CLASSE_NOME
    elif Texto.isdigit():
        Classe = CLASSE_INTEIRO
    else:
        Classe = CLASSE_OUTRO
    CLASSES[Texto] = Classe
    return Classe


class Parser:
    """Estado: T (tokens), I (indice atual), SemStruct (cabecalho de if/while/for: sem literal de struct)."""

    def __init__(self, Tokens: list[str]) -> None:
        self.T = Tokens
        self.I = 0
        self.SemStruct = False

    # ------------------------------------------------------------ utilitarios
    def Falha(self, Causa: str, Tok: int = -1) -> None:
        Onde = self.I if Tok < 0 else Tok
        Visto = self.T[Onde]
        Visto = "fim de arquivo" if Visto == TOKEN_EOF else ("fim de linha" if Visto == TOKEN_NL else repr(Visto))
        raise ForjaErro("E0101", f"{Causa}; encontrado {Visto}", Tok=Onde)

    def Esperar(self, Texto: str) -> int:
        Indice = self.I
        if self.T[Indice] != Texto:
            self.Falha(f"esperado {Texto!r}")
        self.I = Indice + 1
        return Indice

    def EsperarNome(self) -> tuple[int, str]:
        Indice = self.I
        Texto = self.T[Indice]
        if (CLASSES.get(Texto) or ClassificarToken(Texto)) != CLASSE_NOME:
            self.Falha("esperado um nome")
        self.I = Indice + 1
        return Indice, Texto

    def FimDeLinha(self) -> None:
        Texto = self.T[self.I]
        if Texto == TOKEN_NL:
            self.I += 1
        elif Texto != TOKEN_EOF:
            self.Falha("esperado fim de linha")

    def EhNome(self, Indice: int) -> bool:
        Texto = self.T[Indice]
        return (CLASSES.get(Texto) or ClassificarToken(Texto)) == CLASSE_NOME

    # ------------------------------------------------------------ declaracoes
    def ParseArquivo(self) -> list:
        Decls = []
        Tabela = {
            "unit": self.ParseUnit, "const": self.ParseConst,
            "struct": self.ParseStruct, "proc": self.ParseProc,
        }
        T = self.T
        while True:
            Texto = T[self.I]
            if Texto == TOKEN_NL:
                self.I += 1
            elif Texto == TOKEN_EOF:
                return Decls
            elif Texto in Tabela:
                self.I += 1
                Decls.append(Tabela[Texto]())
            else:
                self.Falha("esperada declaracao (unit, const, struct ou proc)")

    def ParseUnit(self) -> UnitDecl:
        _, Nome = self.EsperarNome()
        if self.T[self.I] == ":":
            self.I += 1
            self.Esperar("f32")
            self.FimDeLinha()
            return UnitDecl(Nome, "", "", -1, "", -1)
        self.Esperar("=")
        ATok, A = self.EsperarNome()
        Op = self.T[self.I]
        if Op not in ("*", "/"):
            self.Falha("esperado '*' ou '/' entre as unidades")
        self.I += 1
        BTok, B = self.EsperarNome()
        self.FimDeLinha()
        return UnitDecl(Nome, Op, A, ATok, B, BTok)

    def ParseConst(self) -> ConstDecl:
        _, Nome = self.EsperarNome()
        self.Esperar(":")
        TipoTok, TipoNome = self.EsperarNome()
        self.Esperar("=")
        Expr = self.ParseExpr()
        self.FimDeLinha()
        return ConstDecl(Nome, TipoTok, TipoNome, Expr)

    def ParseStruct(self) -> StructDecl:
        _, Nome = self.EsperarNome()
        self.Esperar("{")
        self.Esperar(TOKEN_NL)
        Campos = []
        while True:
            Texto = self.T[self.I]
            if Texto == TOKEN_NL:
                self.I += 1
            elif Texto == "}":
                self.I += 1
                break
            else:
                Campos.append(self.ParseCampo())
        self.FimDeLinha()
        return StructDecl(Nome, Campos)

    def ParseCampo(self) -> FieldDecl:
        _, Nome = self.EsperarNome()
        self.Esperar(":")
        TipoTok, TipoNome = self.EsperarNome()
        Faixa = None
        if self.T[self.I] == "in":
            self.I += 1
            Faixa = self.ParseFaixa()
        Padrao = None
        if self.T[self.I] == "=":
            self.I += 1
            Padrao = self.ParseExpr()
        self.FimDeLinha()
        return FieldDecl(Nome, TipoTok, TipoNome, Faixa, Padrao)

    def ParseFaixa(self) -> tuple[int, int, bool]:
        Minimo = self.ParseLimite()
        Operador = self.T[self.I]
        if Operador not in ("..=", "..<"):
            self.Falha("esperado '..=' ou '..<'")
        self.I += 1
        Maximo = self.ParseLimite()
        return Minimo, Maximo, Operador == "..="

    def ParseLimite(self) -> int:
        Sinal = 1
        if self.T[self.I] == "-":
            Sinal = -1
            self.I += 1
        Texto = self.T[self.I]
        if not Texto.isdigit():
            self.Falha("esperado limite inteiro")
        self.I += 1
        return Sinal * int(Texto)

    def ParseProc(self) -> ProcDecl:
        _, Nome = self.EsperarNome()
        self.Esperar("(")
        Params = self.ParseParams()
        RetTok, RetNome = -1, ""
        if self.T[self.I] == "->":
            self.I += 1
            RetTok, RetNome = self.EsperarNome()
        self.Esperar("{")
        Corpo, FimTok = self.ParseCorpo()
        self.FimDeLinha()
        return ProcDecl(Nome, Params, RetTok, RetNome, Corpo, FimTok)

    def ParseParams(self) -> list:
        Params = []
        if self.T[self.I] != ")":
            while True:
                Params.append(self.ParseParam())
                if self.T[self.I] != ",":
                    break
                self.I += 1
        self.Esperar(")")
        return Params

    def ParseParam(self) -> ParamDecl:
        _, Nome = self.EsperarNome()
        self.Esperar(":")
        Ponteiro = self.T[self.I] == "^"
        if Ponteiro:
            self.I += 1
        TipoTok, TipoNome = self.EsperarNome()
        Padrao = None
        if self.T[self.I] == "=":
            self.I += 1
            Padrao = self.ParseExpr()
        return ParamDecl(Nome, Ponteiro, TipoTok, TipoNome, Padrao)

    # ---------------------------------------------------------------- comandos
    def ParseCorpo(self) -> tuple[list, int]:
        """Apos '{': exige fim de linha, le comandos ate '}' e devolve (comandos, indice do '}')."""
        self.Esperar(TOKEN_NL)
        Comandos = []
        T = self.T
        while True:
            Texto = T[self.I]
            if Texto == TOKEN_NL:
                self.I += 1
            elif Texto == "}":
                self.I += 1
                return Comandos, self.I - 1
            else:
                Comandos.append(self.ParseComando(Texto))

    def ParseComando(self, Texto: str):
        if Texto == "let" or Texto == "var":
            return self.ParseLet()
        if Texto == "if":
            self.I += 1
            Comando = self.ParseIf()
        elif Texto == "while":
            self.I += 1
            Comando = self.ParseWhile()
        elif Texto == "for":
            self.I += 1
            Comando = self.ParseFor()
        elif Texto == "return":
            Comando = self.ParseReturn()
        elif self.EhNome(self.I):
            return self.ParseComandoNome()
        else:
            self.Falha("esperado um comando")
        self.FimDeLinha()
        return Comando

    def ParseLet(self) -> LetVar:
        self.I += 1
        NomeTok, Nome = self.EsperarNome()
        TipoTok, TipoNome = -1, ""
        if self.T[self.I] == ":":
            self.I += 1
            TipoTok, TipoNome = self.EsperarNome()
        self.Esperar("=")
        Expr = self.ParseExpr()
        self.FimDeLinha()
        return LetVar(NomeTok, Nome, TipoTok, TipoNome, Expr)

    def ParseComandoNome(self):
        Inicio = self.I
        Seguinte = self.T[Inicio + 1]
        if Seguinte == "(":
            Comando = CallStmt(self.ParseChamada())
        elif Seguinte in INICIOS_ATRIBUICAO:
            Comando = self.ParseAtribuicao(Inicio)
        else:
            self.Falha("esperado '(', '.' ou operador de atribuicao", Inicio + 1)
        self.FimDeLinha()
        return Comando

    def ParseAtribuicao(self, Inicio: int) -> Assign:
        Alvo = Name(Inicio, self.T[Inicio])
        self.I = Inicio + 1
        if self.T[self.I] == ".":
            self.I += 1
            CampoTok, Campo = self.EsperarNome()
            Alvo = Field(Inicio, Alvo, CampoTok, Campo)
        Operador = self.T[self.I]
        if Operador not in OPERADORES_ATRIBUICAO:
            self.Falha("esperado '=', '+=' ou '-='")
        self.I += 1
        return Assign(Alvo, Operador, self.ParseExpr())

    def ParseCabecalho(self):
        self.SemStruct = True
        Expr = self.ParseExpr()
        self.SemStruct = False
        return Expr

    def ParseIf(self) -> If:
        Cond = self.ParseCabecalho()
        self.Esperar("{")
        Entao, _ = self.ParseCorpo()
        Senao = None
        if self.T[self.I] == "else":
            self.I += 1
            if self.T[self.I] == "if":
                self.I += 1
                Senao = self.ParseIf()
            else:
                self.Esperar("{")
                Senao, _ = self.ParseCorpo()
        return If(Cond, Entao, Senao)

    def ParseWhile(self) -> While:
        Cond = self.ParseCabecalho()
        self.Esperar("{")
        Corpo, _ = self.ParseCorpo()
        return While(Cond, Corpo)

    def ParseFor(self) -> For:
        _, Nome = self.EsperarNome()
        self.Esperar("in")
        self.SemStruct = True
        Inicio = self.ParseExpr()
        self.Esperar("..<")
        Fim = self.ParseExpr()
        self.SemStruct = False
        self.Esperar("{")
        Corpo, _ = self.ParseCorpo()
        return For(Nome, Inicio, Fim, Corpo)

    def ParseReturn(self) -> Return:
        Tok = self.I
        self.I += 1
        if self.T[self.I] in (TOKEN_NL, TOKEN_EOF):
            return Return(Tok, None)
        return Return(Tok, self.ParseExpr())

    # -------------------------------------------------------------- expressoes
    def ParseExpr(self):
        Esq = self.ParseCast()
        if self.T[self.I] in PRECEDENCIA:
            return self.ParseBinario(1, Esq)
        return Esq

    def ParseBinario(self, Minimo: int, Esq=None):
        """Escalada de precedencia; Esq e o operando esquerdo ja lido (quando o chamador adiantou)."""
        T = self.T
        if Esq is None:
            Esq = self.ParseCast()
        UltimaComparacao = False
        while True:
            Op = T[self.I]
            Prec = PRECEDENCIA.get(Op)
            if Prec is None or Prec < Minimo:
                return Esq
            if Prec == PREC_COMPARACAO and UltimaComparacao:
                self.Falha("comparacao nao e associativa")
            OpTok = self.I
            self.I += 1
            Dir = self.ParseCast()
            Proxima = PRECEDENCIA.get(T[self.I])
            if Proxima is not None and Proxima > Prec:
                Dir = self.ParseBinario(Prec + 1, Dir)
            Esq = Binary(Esq.Start, OpTok, Op, Esq, Dir)
            UltimaComparacao = Prec == PREC_COMPARACAO

    def ParseCast(self):
        T = self.T
        Inicio = self.I
        Texto = T[Inicio]
        Classe = CLASSES.get(Texto) or ClassificarToken(Texto)
        if Classe == CLASSE_INTEIRO and T[Inicio + 1] != ".":
            self.I = Inicio + 1
            Expr = IntLit(Inicio, Texto)
        elif Classe == CLASSE_NOME and T[Inicio + 1] not in NOME_ESPECIAL:
            self.I = Inicio + 1
            Expr = Name(Inicio, Texto)
        else:
            Expr = self.ParseUnario()
        while T[self.I] == "as":
            self.I += 1
            TipoTok, TipoNome = self.EsperarNome()
            Expr = Cast(Expr.Start, Expr, TipoTok, TipoNome)
        return Expr

    def ParseUnario(self):
        Inicio = self.I
        Texto = self.T[Inicio]
        if Texto == "-" or Texto == "not" or Texto == "&":
            self.I = Inicio + 1
            return Unary(Inicio, Texto, self.ParseUnario())
        Expr = self.ParsePrimario()
        while self.T[self.I] == ".":
            self.I += 1
            CampoTok, Campo = self.EsperarNome()
            Expr = Field(Expr.Start, Expr, CampoTok, Campo)
        return Expr

    def ParsePrimario(self):
        Inicio = self.I
        T = self.T
        Texto = T[Inicio]
        if Texto[0].isdigit():
            self.I = Inicio + 1
            return RealLit(Inicio, Texto) if "." in Texto else IntLit(Inicio, Texto)
        if Texto == "(":
            self.I = Inicio + 1
            Interna = self.ParseExpr()
            self.Esperar(")")
            Interna.Start = Inicio
            return Interna
        if Texto == "true" or Texto == "false":
            self.I = Inicio + 1
            return BoolLit(Inicio, Texto == "true")
        if not self.EhNome(Inicio):
            self.Falha("esperada uma expressao")
        Seguinte = T[Inicio + 1]
        if Seguinte == "(":
            return self.ParseChamada()
        if Seguinte == "{" and not self.SemStruct and Texto[0].isupper():
            return self.ParseLiteralStruct()
        self.I = Inicio + 1
        return Name(Inicio, Texto)

    def ParseChamada(self) -> Call:
        Inicio = self.I
        Nome = self.T[Inicio]
        self.I = Inicio + 2
        Args = []
        if self.T[self.I] != ")":
            while True:
                Args.append(self.ParseArgumento())
                if self.T[self.I] != ",":
                    break
                self.I += 1
        self.Esperar(")")
        return Call(Inicio, Nome, Args)

    def ParseArgumento(self) -> tuple[int, str, object]:
        Atual = self.I
        if self.EhNome(Atual) and self.T[Atual + 1] == "=":
            self.I = Atual + 2
            return Atual, self.T[Atual], self.ParseExpr()
        return -1, "", self.ParseExpr()

    def ParseLiteralStruct(self) -> StructLit:
        Inicio = self.I
        Nome = self.T[Inicio]
        self.I = Inicio + 2
        Inits = []
        if self.T[self.I] != "}":
            while True:
                CampoTok, Campo = self.EsperarNome()
                self.Esperar("=")
                Inits.append((CampoTok, Campo, self.ParseExpr()))
                if self.T[self.I] != ",":
                    break
                self.I += 1
        self.Esperar("}")
        return StructLit(Inicio, Nome, Inits)


def ParsearArquivo(Tokens: list[str]) -> list:
    """Entrada: tokens do lexico (terminados em EOF). Saida: declaracoes na ordem do fonte."""
    return Parser(Tokens).ParseArquivo()
