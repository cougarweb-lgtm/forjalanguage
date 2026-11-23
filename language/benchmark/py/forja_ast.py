"""AST e tipos da Forja. Posicao de todo no = indice de token (a linha/coluna sai so no erro).

Versao: 1.0.0
Changelog: 1.0.0 - nos de expressao, comando e declaracao com __slots__; tipos internados.
Verificacao: python C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/medir/rodar_suite.py --compilador "python C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/forja.py" --corpus C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/corpus/dourado --registro C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/_tentativas.jsonl
"""

from __future__ import annotations

CAT_I32 = "i32"
CAT_I64 = "int"
CAT_BOOL = "bool"
CAT_REAL = "real"
CAT_STRUCT = "struct"
CAT_PTR = "ptr"
CAT_VOID = "void"
CATEGORIAS_INTEIRAS = (CAT_I32, CAT_I64)
CATEGORIAS_NUMERICAS = (CAT_I32, CAT_I64, CAT_REAL)


class Tipo:
    """Tipo internado: comparar sempre com `is`. Dim so vale para REAL; Nome para STRUCT e PTR."""

    __slots__ = ("Cat", "Dim", "Nome")

    def __init__(self, Cat: str, Dim: tuple = (), Nome: str = "") -> None:
        self.Cat = Cat
        self.Dim = Dim
        self.Nome = Nome


TIPO_I32 = Tipo(CAT_I32)
TIPO_I64 = Tipo(CAT_I64)
TIPO_BOOL = Tipo(CAT_BOOL)
TIPO_VOID = Tipo(CAT_VOID)
TIPO_F32 = Tipo(CAT_REAL)
_REAIS: dict[tuple, Tipo] = {(): TIPO_F32}
_STRUCTS: dict[str, Tipo] = {}
_PONTEIROS: dict[str, Tipo] = {}


def TipoReal(Dim: tuple) -> Tipo:
    Achado = _REAIS.get(Dim)
    if Achado is None:
        Achado = _REAIS[Dim] = Tipo(CAT_REAL, Dim)
    return Achado


def TipoStruct(Nome: str) -> Tipo:
    Achado = _STRUCTS.get(Nome)
    if Achado is None:
        Achado = _STRUCTS[Nome] = Tipo(CAT_STRUCT, (), Nome)
    return Achado


def TipoPonteiro(Nome: str) -> Tipo:
    Achado = _PONTEIROS.get(Nome)
    if Achado is None:
        Achado = _PONTEIROS[Nome] = Tipo(CAT_PTR, (), Nome)
    return Achado


# ---------------------------------------------------------------- expressoes
class IntLit:
    __slots__ = ("Start", "Texto", "Ty")

    def __init__(self, Start: int, Texto: str) -> None:
        self.Start = Start
        self.Texto = Texto
        self.Ty = None


class RealLit:
    __slots__ = ("Start", "Texto", "Ty")

    def __init__(self, Start: int, Texto: str) -> None:
        self.Start = Start
        self.Texto = Texto
        self.Ty = None


class BoolLit:
    __slots__ = ("Start", "Valor", "Ty")

    def __init__(self, Start: int, Valor: bool) -> None:
        self.Start = Start
        self.Valor = Valor
        self.Ty = None


class Name:
    __slots__ = ("Start", "Texto", "Ty", "CNome")

    def __init__(self, Start: int, Texto: str) -> None:
        self.Start = Start
        self.Texto = Texto
        self.Ty = None
        self.CNome = ""


class Unary:
    __slots__ = ("Start", "Op", "Operando", "Ty")

    def __init__(self, Start: int, Op: str, Operando) -> None:
        self.Start = Start
        self.Op = Op
        self.Operando = Operando
        self.Ty = None


class Binary:
    __slots__ = ("Start", "OpTok", "Op", "Esq", "Dir", "Ty")

    def __init__(self, Start: int, OpTok: int, Op: str, Esq, Dir) -> None:
        self.Start = Start
        self.OpTok = OpTok
        self.Op = Op
        self.Esq = Esq
        self.Dir = Dir
        self.Ty = None


class Cast:
    __slots__ = ("Start", "Operando", "TipoTok", "TipoNome", "Ty")

    def __init__(self, Start: int, Operando, TipoTok: int, TipoNome: str) -> None:
        self.Start = Start
        self.Operando = Operando
        self.TipoTok = TipoTok
        self.TipoNome = TipoNome
        self.Ty = None


class Field:
    __slots__ = ("Start", "Objeto", "NomeTok", "Nome", "Ty")

    def __init__(self, Start: int, Objeto, NomeTok: int, Nome: str) -> None:
        self.Start = Start
        self.Objeto = Objeto
        self.NomeTok = NomeTok
        self.Nome = Nome
        self.Ty = None


class Call:
    """Args: lista de (NomeTok, Nome, Expr); NomeTok = -1 quando posicional."""

    __slots__ = ("Start", "Nome", "Args", "Ty", "Proc", "Ligados")

    def __init__(self, Start: int, Nome: str, Args: list) -> None:
        self.Start = Start
        self.Nome = Nome
        self.Args = Args
        self.Ty = None
        self.Proc = None
        self.Ligados = None


class StructLit:
    """Inits: lista de (NomeTok, Nome, Expr)."""

    __slots__ = ("Start", "Nome", "Inits", "Ty", "Struct")

    def __init__(self, Start: int, Nome: str, Inits: list) -> None:
        self.Start = Start
        self.Nome = Nome
        self.Inits = Inits
        self.Ty = None
        self.Struct = None


# ------------------------------------------------------------------ comandos
class LetVar:
    __slots__ = ("NomeTok", "Nome", "TipoTok", "TipoNome", "Expr", "Ty", "CNome")

    def __init__(self, NomeTok: int, Nome: str, TipoTok: int, TipoNome: str, Expr) -> None:
        self.NomeTok = NomeTok
        self.Nome = Nome
        self.TipoTok = TipoTok
        self.TipoNome = TipoNome
        self.Expr = Expr
        self.Ty = None
        self.CNome = ""


class Assign:
    __slots__ = ("Alvo", "Op", "Expr")

    def __init__(self, Alvo, Op: str, Expr) -> None:
        self.Alvo = Alvo
        self.Op = Op
        self.Expr = Expr


class If:
    """Else: lista de comandos, outro If (else if) ou None."""

    __slots__ = ("Cond", "Then", "Else")

    def __init__(self, Cond, Then: list, Else) -> None:
        self.Cond = Cond
        self.Then = Then
        self.Else = Else


class While:
    __slots__ = ("Cond", "Corpo")

    def __init__(self, Cond, Corpo: list) -> None:
        self.Cond = Cond
        self.Corpo = Corpo


class For:
    __slots__ = ("Nome", "Inicio", "Fim", "Corpo", "Ty", "CNome", "CLimite")

    def __init__(self, Nome: str, Inicio, Fim, Corpo: list) -> None:
        self.Nome = Nome
        self.Inicio = Inicio
        self.Fim = Fim
        self.Corpo = Corpo
        self.Ty = None
        self.CNome = ""
        self.CLimite = ""


class Return:
    __slots__ = ("Tok", "Expr")

    def __init__(self, Tok: int, Expr) -> None:
        self.Tok = Tok
        self.Expr = Expr


class CallStmt:
    __slots__ = ("Call",)

    def __init__(self, Call) -> None:
        self.Call = Call


# ---------------------------------------------------------------- declaracoes
class UnitDecl:
    """Derivada quando Op e '*' ou '/'; base quando Op e ''."""

    __slots__ = ("Nome", "Op", "A", "ATok", "B", "BTok")

    def __init__(self, Nome: str, Op: str, A: str, ATok: int, B: str, BTok: int) -> None:
        self.Nome = Nome
        self.Op = Op
        self.A = A
        self.ATok = ATok
        self.B = B
        self.BTok = BTok


class ConstDecl:
    __slots__ = ("Nome", "TipoTok", "TipoNome", "Expr", "Ty")

    def __init__(self, Nome: str, TipoTok: int, TipoNome: str, Expr) -> None:
        self.Nome = Nome
        self.TipoTok = TipoTok
        self.TipoNome = TipoNome
        self.Expr = Expr
        self.Ty = None


class FieldDecl:
    """Faixa: (Min, Max, Inclusiva) ou None; Default: expressao ou None."""

    __slots__ = ("Nome", "TipoTok", "TipoNome", "Faixa", "Default", "Ty", "DefaultC")

    def __init__(self, Nome: str, TipoTok: int, TipoNome: str, Faixa, Default) -> None:
        self.Nome = Nome
        self.TipoTok = TipoTok
        self.TipoNome = TipoNome
        self.Faixa = Faixa
        self.Default = Default
        self.Ty = None
        self.DefaultC = ""


class StructDecl:
    __slots__ = ("Nome", "Campos", "Mapa")

    def __init__(self, Nome: str, Campos: list) -> None:
        self.Nome = Nome
        self.Campos = Campos
        self.Mapa = {}


class ParamDecl:
    __slots__ = ("Nome", "Ponteiro", "TipoTok", "TipoNome", "Default", "Ty", "DefaultC")

    def __init__(self, Nome: str, Ponteiro: bool, TipoTok: int, TipoNome: str, Default) -> None:
        self.Nome = Nome
        self.Ponteiro = Ponteiro
        self.TipoTok = TipoTok
        self.TipoNome = TipoNome
        self.Default = Default
        self.Ty = None
        self.DefaultC = ""


class ProcDecl:
    __slots__ = ("Nome", "Params", "RetTok", "RetNome", "Corpo", "FimTok", "Ret", "Indice")

    def __init__(self, Nome: str, Params: list, RetTok: int, RetNome: str, Corpo: list, FimTok: int) -> None:
        self.Nome = Nome
        self.Params = Params
        self.RetTok = RetTok
        self.RetNome = RetNome
        self.Corpo = Corpo
        self.FimTok = FimTok
        self.Ret = TIPO_VOID
        self.Indice = {}
