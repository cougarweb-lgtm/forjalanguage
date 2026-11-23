"""Checagem da Forja: nomes, tipos, unidades/dimensoes, faixa constante e regras de argumento.

Versao: 1.0.0
Changelog: 1.0.0 - passe A (unit, const, struct, assinaturas) e passe B (corpos); para no 1o diagnostico.
Verificacao: python C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/medir/rodar_suite.py --compilador "python C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/forja.py" --corpus C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/corpus/dourado --registro C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/_tentativas.jsonl
"""

from forja_ast import (
    CAT_BOOL, CAT_I32, CAT_I64, CAT_PTR, CAT_REAL, CAT_STRUCT, CATEGORIAS_INTEIRAS, CATEGORIAS_NUMERICAS,
    TIPO_BOOL, TIPO_F32, TIPO_I32, TIPO_I64, TIPO_VOID,
    Assign, Binary, BoolLit, Call, CallStmt, Cast, ConstDecl, Field, For, If, IntLit, LetVar, Name,
    ProcDecl, RealLit, Return, StructDecl, StructLit, Tipo, TipoPonteiro, TipoReal, TipoStruct, Unary,
    UnitDecl, While,
)
from forja_erro import ForjaErro, PosicaoFixaErro

TIPOS_EMBUTIDOS = {"i32": TIPO_I32, "int": TIPO_I64, "f32": TIPO_F32, "bool": TIPO_BOOL}
CATEGORIAS_IGUALDADE = (CAT_I32, CAT_I64, CAT_REAL, CAT_BOOL)
OPERADORES_IGUALDADE = ("==", "!=")
OPERADORES_ADITIVOS = ("+", "-")
OPERADORES_COMPARACAO = ("==", "!=", "<", "<=", ">", ">=")
OPERADORES_LOGICOS = ("and", "or")


def NomeDoTipo(Ty: Tipo) -> str:
    if Ty.Cat == CAT_REAL:
        return "f32" if not Ty.Dim else f"unidade{Ty.Dim}"
    if Ty.Cat == CAT_STRUCT:
        return Ty.Nome
    return f"^{Ty.Nome}" if Ty.Cat == CAT_PTR else Ty.Cat


def EhLiteral(No) -> bool:
    Classe = No.__class__
    if Classe is IntLit or Classe is RealLit:
        return True
    return Classe is Unary and No.Op == "-" and EhLiteral(No.Operando)


def ValorLiteralInteiro(No) -> int | None:
    if No.__class__ is IntLit:
        return int(No.Texto)
    if No.__class__ is Unary and No.Op == "-" and No.Operando.__class__ is IntLit:
        return -int(No.Operando.Texto)
    return None


def AdotarEmProduto(Ty: Tipo) -> Tipo | None:
    """Dica para o literal do outro lado de * e /: inteiro adota o tipo, real vira adimensional."""
    if Ty.Cat == CAT_I32 or Ty.Cat == CAT_I64:
        return Ty
    return TIPO_F32 if Ty.Cat == CAT_REAL else None


class Checker:
    def __init__(self) -> None:
        self.Unidades: dict[str, Tipo] = {}
        self.Declarados: set[Tipo] = {TIPO_F32}
        self.ProximaDim = 0
        self.Consts: dict[str, ConstDecl] = {}
        self.Structs: dict[str, StructDecl] = {}
        self.Procs: dict[str, ProcDecl] = {}
        self.Env: dict[str, tuple[Tipo, str]] = {}
        self.Undo: list = []
        self.Ret = TIPO_VOID
        self.Contador = 0
        self.CacheProduto: dict = {}
        self.Expressoes = {
            IntLit: self.ChecarInt, RealLit: self.ChecarReal, BoolLit: self.ChecarBool,
            Name: self.ChecarNome, Unary: self.ChecarUnario, Binary: self.ChecarBinario,
            Cast: self.ChecarCast, Field: self.ChecarCampo, Call: self.ChecarChamada,
            StructLit: self.ChecarLiteralStruct,
        }
        self.Comandos = {
            LetVar: self.ChecarLet, Assign: self.ChecarAtribuicao, If: self.ChecarIf,
            While: self.ChecarWhile, For: self.ChecarFor, Return: self.ChecarReturn,
            CallStmt: self.ChecarChamadaComando,
        }
        self.Operadores = {Op: self.ChecarAditivo for Op in OPERADORES_ADITIVOS}
        self.Operadores.update({Op: self.ChecarProduto for Op in ("*", "/", "%")})
        self.Operadores.update({Op: self.ChecarComparacao for Op in OPERADORES_COMPARACAO})
        self.Operadores.update({Op: self.ChecarLogico for Op in OPERADORES_LOGICOS})

    # ------------------------------------------------------------ utilitarios
    def Erro(self, Codigo: str, Tok: int, Causa: str) -> None:
        raise ForjaErro(Codigo, Causa, Tok=Tok)

    def Conforme(self, Esperado: Tipo, Real: Tipo, No) -> None:
        if Esperado is Real:
            return
        Codigo = "E0412" if Esperado.Cat == CAT_REAL and Real.Cat == CAT_REAL else "E0301"
        self.Erro(Codigo, No.Start, f"esperado {NomeDoTipo(Esperado)}, encontrado {NomeDoTipo(Real)}")

    def TipoDeNome(self, Tok: int, Nome: str) -> Tipo:
        Achado = TIPOS_EMBUTIDOS.get(Nome) or self.Unidades.get(Nome)
        if Achado is None and Nome in self.Structs:
            Achado = TipoStruct(Nome)
        if Achado is None:
            self.Erro("E0201", Tok, f"tipo {Nome!r} nao declarado")
        return Achado

    def Expr(self, No, Dica: Tipo | None) -> Tipo:
        Classe = No.__class__
        if Classe is IntLit:  # caminho quente: evita uma chamada por literal
            Ty = TIPO_I32 if Dica is TIPO_I32 else TIPO_I64
        else:
            Ty = self.Expressoes[Classe](No, Dica)
        No.Ty = Ty
        return Ty

    def Declarar(self, Nome: str, Ty: Tipo) -> str:
        CNome = f"v_{Nome}"
        if Nome in self.Env:
            self.Contador += 1
            CNome = f"v_{Nome}_{self.Contador}"
        self.Undo.append((Nome, self.Env.get(Nome)))
        self.Env[Nome] = (Ty, CNome)
        return CNome

    def Desfazer(self, Marca: int) -> None:
        while len(self.Undo) > Marca:
            Nome, Antes = self.Undo.pop()
            if Antes is None:
                del self.Env[Nome]
            else:
                self.Env[Nome] = Antes

    # ------------------------------------------------------------- passe A
    def ChecarPrograma(self, Decls: list) -> None:
        Passe = {
            UnitDecl: self.DeclararUnidade, ConstDecl: self.DeclararConst,
            StructDecl: self.DeclararStruct, ProcDecl: self.DeclararAssinatura,
        }
        for Categoria in (UnitDecl, ConstDecl, StructDecl, ProcDecl):  # passe A por categoria (ADENDO 1.0.1)
            for Decl in Decls:
                if Decl.__class__ is Categoria:
                    Passe[Categoria](Decl)
        for Decl in Decls:
            if Decl.__class__ is ProcDecl:
                self.ChecarCorpo(Decl)
        Principal = self.Procs.get("main")
        if Principal is None or Principal.Params or Principal.Ret is not TIPO_VOID:
            raise PosicaoFixaErro("E0201", "proc main() sem parametros e sem retorno nao encontrado", 1, 1)

    def DeclararUnidade(self, Decl: UnitDecl) -> None:
        if not Decl.Op:
            Novo = TipoReal(((self.ProximaDim, 1),))
            self.ProximaDim += 1
        else:
            A = self.UnidadeDeNome(Decl.ATok, Decl.A)
            B = self.UnidadeDeNome(Decl.BTok, Decl.B)
            Novo = self.Combinar(A, B, Decl.Op == "/")
        self.Declarados.add(Novo)
        self.Unidades[Decl.Nome] = Novo

    def UnidadeDeNome(self, Tok: int, Nome: str) -> Tipo:
        Achado = self.Unidades.get(Nome)
        if Achado is None:
            self.Erro("E0201", Tok, f"unidade {Nome!r} nao declarada")
        return Achado

    def Combinar(self, A: Tipo, B: Tipo, Dividir: bool) -> Tipo:
        Chave = (A, B, Dividir)
        Achado = self.CacheProduto.get(Chave)
        if Achado is None:
            Expoentes = dict(A.Dim)
            for Base, Expoente in B.Dim:
                Expoentes[Base] = Expoentes.get(Base, 0) + (-Expoente if Dividir else Expoente)
            Dim = tuple(sorted((Base, Exp) for Base, Exp in Expoentes.items() if Exp != 0))
            Achado = self.CacheProduto[Chave] = TipoReal(Dim)
        return Achado

    def DeclararConst(self, Decl: ConstDecl) -> None:
        Decl.Ty = self.TipoDeNome(Decl.TipoTok, Decl.TipoNome)
        self.Conforme(Decl.Ty, self.Expr(Decl.Expr, Decl.Ty), Decl.Expr)
        self.Consts[Decl.Nome] = Decl

    def DeclararStruct(self, Decl: StructDecl) -> None:
        for Campo in Decl.Campos:
            Campo.Ty = self.TipoDeNome(Campo.TipoTok, Campo.TipoNome)
            if Campo.Default is not None:
                self.Conforme(Campo.Ty, self.Expr(Campo.Default, Campo.Ty), Campo.Default)
                self.ConferirFaixa(Campo, Campo.Default)
            Decl.Mapa[Campo.Nome] = Campo
        self.Structs[Decl.Nome] = Decl

    def ConferirFaixa(self, Campo, Valor) -> None:
        if Campo.Faixa is None or Campo.Ty.Cat not in CATEGORIAS_INTEIRAS:
            return
        Constante = ValorLiteralInteiro(Valor)
        if Constante is None:
            return
        Minimo, Maximo, Inclusiva = Campo.Faixa
        if Minimo <= Constante and (Constante <= Maximo if Inclusiva else Constante < Maximo):
            return
        Limite = f"{Minimo}..={Maximo}" if Inclusiva else f"{Minimo}..<{Maximo}"
        self.Erro("E0420", Valor.Start, f"valor {Constante} fora da faixa {Limite} do campo {Campo.Nome!r}")

    def DeclararAssinatura(self, Decl: ProcDecl) -> None:
        for Indice, Param in enumerate(Decl.Params):
            if Param.Ponteiro:
                if Param.TipoNome not in self.Structs:
                    self.Erro("E0201", Param.TipoTok, f"struct {Param.TipoNome!r} nao declarada")
                Param.Ty = TipoPonteiro(Param.TipoNome)
            else:
                Param.Ty = self.TipoDeNome(Param.TipoTok, Param.TipoNome)
            if Param.Default is not None:
                self.Conforme(Param.Ty, self.Expr(Param.Default, Param.Ty), Param.Default)
            Decl.Indice[Param.Nome] = Indice
        if Decl.RetNome:
            Decl.Ret = self.TipoDeNome(Decl.RetTok, Decl.RetNome)
        self.Procs[Decl.Nome] = Decl

    # ------------------------------------------------------------- passe B
    def ChecarCorpo(self, Decl: ProcDecl) -> None:
        self.Env = {}
        self.Undo = []
        self.Ret = Decl.Ret
        for Param in Decl.Params:
            self.Declarar(Param.Nome, Param.Ty)
        self.ChecarBloco(Decl.Corpo)
        if Decl.Ret is not TIPO_VOID and (not Decl.Corpo or Decl.Corpo[-1].__class__ is not Return):
            self.Erro("E0302", Decl.FimTok, f"proc {Decl.Nome!r} com retorno nao termina em return")

    def ChecarBloco(self, Comandos: list) -> None:
        Marca = len(self.Undo)
        Tabela = self.Comandos
        for Comando in Comandos:
            Tabela[Comando.__class__](Comando)
        self.Desfazer(Marca)

    def ChecarLet(self, Cmd: LetVar) -> None:
        if Cmd.TipoNome:
            Ty = self.TipoDeNome(Cmd.TipoTok, Cmd.TipoNome)
            self.Conforme(Ty, self.Expr(Cmd.Expr, Ty), Cmd.Expr)
        else:
            Ty = self.Expr(Cmd.Expr, None)
            if Ty is TIPO_VOID:
                self.Erro("E0301", Cmd.Expr.Start, "expressao sem valor no inicializador")
        Cmd.Ty = Ty
        Cmd.CNome = self.Declarar(Cmd.Nome, Ty)

    def ChecarAtribuicao(self, Cmd: Assign) -> None:
        AlvoTy = self.Expr(Cmd.Alvo, None)
        Ty = self.Expr(Cmd.Expr, AlvoTy)
        if Cmd.Op != "=" and AlvoTy.Cat not in CATEGORIAS_NUMERICAS:
            self.Erro("E0301", Cmd.Expr.Start, f"{Cmd.Op} exige alvo numerico, encontrado {NomeDoTipo(AlvoTy)}")
        self.Conforme(AlvoTy, Ty, Cmd.Expr)

    def ChecarCondicao(self, Cond) -> None:
        Ty = self.Expr(Cond, None)
        if Ty is not TIPO_BOOL:
            self.Erro("E0301", Cond.Start, f"condicao precisa ser bool, encontrado {NomeDoTipo(Ty)}")

    def ChecarIf(self, Cmd: If) -> None:
        self.ChecarCondicao(Cmd.Cond)
        self.ChecarBloco(Cmd.Then)
        if Cmd.Else is None:
            return
        if Cmd.Else.__class__ is If:
            self.ChecarIf(Cmd.Else)
        else:
            self.ChecarBloco(Cmd.Else)

    def ChecarWhile(self, Cmd: While) -> None:
        self.ChecarCondicao(Cmd.Cond)
        self.ChecarBloco(Cmd.Corpo)

    def ChecarFor(self, Cmd: For) -> None:
        Inicio, Fim = Cmd.Inicio, Cmd.Fim
        if EhLiteral(Inicio) and not EhLiteral(Fim):
            TyFim = self.Expr(Fim, None)
            TyInicio = self.Expr(Inicio, TyFim)
        else:
            TyInicio = self.Expr(Inicio, None)
            TyFim = self.Expr(Fim, TyInicio)
        if TyInicio.Cat not in CATEGORIAS_INTEIRAS:
            self.Erro("E0301", Inicio.Start, f"limite do for precisa ser inteiro, encontrado {NomeDoTipo(TyInicio)}")
        self.Conforme(TyInicio, TyFim, Fim)
        Marca = len(self.Undo)
        Cmd.Ty = TyInicio
        Cmd.CNome = self.Declarar(Cmd.Nome, TyInicio)
        self.Contador += 1
        Cmd.CLimite = f"l_{self.Contador}"
        self.ChecarBloco(Cmd.Corpo)
        self.Desfazer(Marca)

    def ChecarReturn(self, Cmd: Return) -> None:
        if Cmd.Expr is None:
            if self.Ret is not TIPO_VOID:
                self.Erro("E0301", Cmd.Tok, f"return sem valor em proc que retorna {NomeDoTipo(self.Ret)}")
        elif self.Ret is TIPO_VOID:
            self.Expr(Cmd.Expr, None)
            self.Erro("E0301", Cmd.Expr.Start, "return com valor em proc sem retorno")
        else:
            self.Conforme(self.Ret, self.Expr(Cmd.Expr, self.Ret), Cmd.Expr)

    def ChecarChamadaComando(self, Cmd: CallStmt) -> None:
        self.Expr(Cmd.Call, None)

    # ---------------------------------------------------------- expressoes
    def ChecarInt(self, No: IntLit, Dica: Tipo | None) -> Tipo:
        return TIPO_I32 if Dica is TIPO_I32 else TIPO_I64

    def ChecarReal(self, No: RealLit, Dica: Tipo | None) -> Tipo:
        return Dica if Dica is not None and Dica.Cat == CAT_REAL else TIPO_F32

    def ChecarBool(self, No: BoolLit, Dica: Tipo | None) -> Tipo:
        return TIPO_BOOL

    def ChecarNome(self, No: Name, Dica: Tipo | None) -> Tipo:
        Achado = self.Env.get(No.Texto)
        if Achado is not None:
            No.CNome = Achado[1]
            return Achado[0]
        Constante = self.Consts.get(No.Texto)
        if Constante is None:
            self.Erro("E0201", No.Start, f"nome {No.Texto!r} nao declarado")
        No.CNome = "K_" + No.Texto
        return Constante.Ty

    def ChecarUnario(self, No: Unary, Dica: Tipo | None) -> Tipo:
        if No.Op == "&":
            return self.ChecarEndereco(No)
        Ty = self.Expr(No.Operando, Dica if No.Op == "-" else None)
        if No.Op == "not":
            if Ty is not TIPO_BOOL:
                self.Erro("E0301", No.Start, f"'not' exige bool, encontrado {NomeDoTipo(Ty)}")
            return TIPO_BOOL
        if Ty.Cat not in CATEGORIAS_NUMERICAS:
            self.Erro("E0301", No.Start, f"'-' exige numero, encontrado {NomeDoTipo(Ty)}")
        return Ty

    def ChecarEndereco(self, No: Unary) -> Tipo:
        if No.Operando.__class__ is not Name:
            self.Erro("E0301", No.Start, "'&' so vale sobre o nome de uma variavel de struct")
        Ty = self.Expr(No.Operando, None)
        if Ty.Cat != CAT_STRUCT:
            self.Erro("E0301", No.Start, f"'&' exige variavel de struct, encontrado {NomeDoTipo(Ty)}")
        return TipoPonteiro(Ty.Nome)

    def ChecarBinario(self, No: Binary, Dica: Tipo | None) -> Tipo:
        return self.Operadores[No.Op](No, Dica)

    def ChecarOperandos(self, No: Binary, Dica: Tipo | None, Produto: bool) -> tuple[Tipo, Tipo]:
        Esq, Dir = No.Esq, No.Dir
        if EhLiteral(Esq) and not EhLiteral(Dir):
            TyDir = self.Expr(Dir, Dica)
            return self.Expr(Esq, AdotarEmProduto(TyDir) if Produto else TyDir), TyDir
        TyEsq = self.Expr(Esq, Dica)
        return TyEsq, self.Expr(Dir, AdotarEmProduto(TyEsq) if Produto else TyEsq)

    def ErroOperador(self, No: Binary, TyEsq: Tipo, TyDir: Tipo) -> None:
        Codigo = "E0412" if TyEsq.Cat == CAT_REAL and TyDir.Cat == CAT_REAL else "E0301"
        Causa = f"operador {No.Op!r} nao combina {NomeDoTipo(TyEsq)} com {NomeDoTipo(TyDir)}"
        self.Erro(Codigo, No.OpTok, Causa)

    def ChecarAditivo(self, No: Binary, Dica: Tipo | None) -> Tipo:
        TyEsq, TyDir = self.ChecarOperandos(No, Dica, False)
        if TyEsq is TyDir and TyEsq.Cat in CATEGORIAS_NUMERICAS:
            return TyEsq
        self.ErroOperador(No, TyEsq, TyDir)

    def ChecarProduto(self, No: Binary, Dica: Tipo | None) -> Tipo:
        Inteira = Dica if Dica is not None and Dica.Cat in CATEGORIAS_INTEIRAS else None
        TyEsq, TyDir = self.ChecarOperandos(No, Inteira, True)
        if TyEsq.Cat == CAT_REAL and TyDir.Cat == CAT_REAL and No.Op != "%":
            Resultado = self.Combinar(TyEsq, TyDir, No.Op == "/")
            if Resultado not in self.Declarados:
                self.Erro("E0412", No.OpTok, "a dimensao do resultado nao tem unidade declarada")
            return Resultado
        if TyEsq is TyDir and TyEsq.Cat in CATEGORIAS_INTEIRAS:
            return TyEsq
        self.ErroOperador(No, TyEsq, TyDir)

    def ChecarComparacao(self, No: Binary, Dica: Tipo | None) -> Tipo:
        TyEsq, TyDir = self.ChecarOperandos(No, None, False)
        if TyEsq is TyDir:
            Aceitas = CATEGORIAS_IGUALDADE if No.Op in OPERADORES_IGUALDADE else CATEGORIAS_NUMERICAS
            if TyEsq.Cat in Aceitas:
                return TIPO_BOOL
        self.ErroOperador(No, TyEsq, TyDir)

    def ChecarLogico(self, No: Binary, Dica: Tipo | None) -> Tipo:
        for Lado in (No.Esq, No.Dir):
            Ty = self.Expr(Lado, None)
            if Ty is not TIPO_BOOL:
                self.Erro("E0301", No.OpTok, f"{No.Op!r} exige bool, encontrado {NomeDoTipo(Ty)}")
        return TIPO_BOOL

    def ChecarCast(self, No: Cast, Dica: Tipo | None) -> Tipo:
        Origem = self.Expr(No.Operando, None)
        if Origem.Cat not in CATEGORIAS_NUMERICAS:
            self.Erro("E0301", No.Start, f"'as' nao converte de {NomeDoTipo(Origem)}")
        Alvo = self.TipoDeNome(No.TipoTok, No.TipoNome)
        if Alvo.Cat not in CATEGORIAS_NUMERICAS:
            self.Erro("E0301", No.Start, f"'as' nao converte para {NomeDoTipo(Alvo)}")
        return Alvo

    def ChecarCampo(self, No: Field, Dica: Tipo | None) -> Tipo:
        Dono = self.Expr(No.Objeto, None)
        if Dono.Cat != CAT_STRUCT and Dono.Cat != CAT_PTR:
            self.Erro("E0301", No.Start, f"acesso a campo em {NomeDoTipo(Dono)}, que nao e struct")
        Campo = self.Structs[Dono.Nome].Mapa.get(No.Nome)
        if Campo is None:
            self.Erro("E0201", No.NomeTok, f"campo {No.Nome!r} nao existe em {Dono.Nome}")
        return Campo.Ty

    def ChecarLiteralStruct(self, No: StructLit, Dica: Tipo | None) -> Tipo:
        Struct = self.Structs.get(No.Nome)
        if Struct is None:
            self.Erro("E0201", No.Start, f"struct {No.Nome!r} nao declarada")
        for CampoTok, Nome, Valor in No.Inits:
            Campo = Struct.Mapa.get(Nome)
            if Campo is None:
                self.Erro("E0504", CampoTok, f"campo {Nome!r} nao existe em {No.Nome}")
            self.Conforme(Campo.Ty, self.Expr(Valor, Campo.Ty), Valor)
            self.ConferirFaixa(Campo, Valor)
        No.Struct = Struct
        return TipoStruct(No.Nome)

    # ------------------------------------------------------------- chamadas
    def ChecarChamada(self, No: Call, Dica: Tipo | None) -> Tipo:
        if No.Nome == "print":
            self.ChecarPrint(No)
            return TIPO_VOID
        Proc = self.Procs.get(No.Nome)
        if Proc is None:
            self.Erro("E0201", No.Start, f"proc {No.Nome!r} nao declarado")
        Ligados = self.LigarArgumentos(No, Proc)
        for Param, Valor in zip(Proc.Params, Ligados):
            if Valor is not None:
                self.Conforme(Param.Ty, self.Expr(Valor, Param.Ty), Valor)
        for Param, Valor in zip(Proc.Params, Ligados):
            if Valor is None and Param.Default is None:
                self.Erro("E0503", No.Start, f"falta o argumento {Param.Nome!r} em {No.Nome}")
        No.Proc = Proc
        No.Ligados = Ligados
        return Proc.Ret

    def LigarArgumentos(self, No: Call, Proc: ProcDecl) -> list:
        Ligados = [None] * len(Proc.Params)
        Ultimo = -1
        for Posicao, (NomeTok, Nome, Valor) in enumerate(No.Args):
            if Posicao == 0:
                if NomeTok >= 0:
                    self.Erro("E0502", NomeTok, "o 1o argumento deve ser posicional")
                if not Ligados:
                    self.Erro("E0502", Valor.Start, f"{No.Nome} nao recebe argumentos")
                Ligados[0] = Valor
                Ultimo = 0
                continue
            if NomeTok < 0:
                self.Erro("E0502", Valor.Start, "so o 1o argumento pode ser posicional")
            Indice = Proc.Indice.get(Nome)
            if Indice is None:
                self.Erro("E0504", NomeTok, f"{No.Nome} nao tem o parametro {Nome!r}")
            if Indice <= Ultimo:
                self.Erro("E0501", NomeTok, f"argumento {Nome!r} fora da ordem declarada ou repetido")
            Ligados[Indice] = Valor
            Ultimo = Indice
        return Ligados

    def ChecarPrint(self, No: Call) -> None:
        if len(No.Args) != 1:
            self.Erro("E0301", No.Start, f"print recebe 1 argumento, recebeu {len(No.Args)}")
        NomeTok, _, Valor = No.Args[0]
        if NomeTok >= 0:
            self.Erro("E0301", NomeTok, "print recebe argumento posicional")
        Ty = self.Expr(Valor, None)
        if Ty.Cat not in (CAT_I32, CAT_I64, CAT_BOOL):
            self.Erro("E0301", Valor.Start, f"print nao imprime {NomeDoTipo(Ty)}")


def ChecarArquivo(Decls: list) -> None:
    """Entrada: declaracoes do parser. Efeito: anota tipos nos nos ou levanta ForjaErro/PosicaoFixaErro."""
    Checker().ChecarPrograma(Decls)
