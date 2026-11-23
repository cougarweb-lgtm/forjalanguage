"""Emissao da Forja: AST ja checada para texto C11 (stdint, stdbool, stdio).

Versao: 1.0.0
Changelog: 1.0.0 - consts como #define, structs como typedef, protótipos + definicoes, prefixos v_/f_/S_/m_/K_.
Verificacao: python C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/medir/rodar_suite.py --compilador "python C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/forja.py" --corpus C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/corpus/dourado --registro C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/_tentativas.jsonl
"""

from forja_ast import (
    CAT_BOOL, CAT_I32, CAT_I64, CAT_PTR, CAT_REAL, CAT_STRUCT,
    Assign, Binary, BoolLit, Call, CallStmt, Cast, ConstDecl, Field, For, If, IntLit, LetVar, Name,
    ProcDecl, RealLit, Return, StructDecl, StructLit, Tipo, Unary, While,
)

CABECALHO_C = "#include <stdint.h>\n#include <stdbool.h>\n#include <stdio.h>\n"
TIPOS_C = {CAT_I32: "int32_t", CAT_I64: "int64_t", CAT_BOOL: "bool", CAT_REAL: "float"}
ZEROS_C = {CAT_I32: "0", CAT_I64: "0LL", CAT_BOOL: "false", CAT_REAL: "0.0f"}
OPERADORES_C = {"and": "&&", "or": "||"}


def TipoC(Ty: Tipo) -> str:
    if Ty.Cat == CAT_STRUCT:
        return "S_" + Ty.Nome
    if Ty.Cat == CAT_PTR:
        return "S_" + Ty.Nome + " *"
    return TIPOS_C.get(Ty.Cat, "void")


class Emissor:
    def __init__(self) -> None:
        self.Linhas: list[str] = []
        self.Expressoes = {
            IntLit: self.EmitirInt, RealLit: self.EmitirReal, BoolLit: self.EmitirBool,
            Name: self.EmitirNome, Unary: self.EmitirUnario, Binary: self.EmitirBinario,
            Cast: self.EmitirCast, Field: self.EmitirCampo, Call: self.EmitirChamada,
            StructLit: self.EmitirLiteralStruct,
        }
        self.Comandos = {
            LetVar: self.EmitirLet, Assign: self.EmitirAtribuicao, If: self.EmitirIf,
            While: self.EmitirWhile, For: self.EmitirFor, Return: self.EmitirReturn,
            CallStmt: self.EmitirComandoChamada,
        }

    # -------------------------------------------------------------- programa
    def EmitirPrograma(self, Decls: list) -> str:
        Saida = self.Linhas
        Saida.append(CABECALHO_C)
        Procs = [Decl for Decl in Decls if Decl.__class__ is ProcDecl]
        for Decl in Decls:
            if Decl.__class__ is ConstDecl:
                Saida.append(f"#define K_{Decl.Nome} (({TipoC(Decl.Ty)})({self.Expr(Decl.Expr)}))")
            elif Decl.__class__ is StructDecl:
                self.EmitirStruct(Decl)
        Assinaturas = [self.Assinatura(Proc) for Proc in Procs]
        for Assinatura in Assinaturas:
            Saida.append(Assinatura + ";")
        for Proc, Assinatura in zip(Procs, Assinaturas):
            self.EmitirProc(Proc, Assinatura)
        Saida.append("int main(void) {\nf_main();\nreturn 0;\n}\n")
        return "\n".join(Saida)

    def EmitirStruct(self, Decl: StructDecl) -> None:
        Saida = self.Linhas
        Saida.append("typedef struct S_" + Decl.Nome + " {")
        for Campo in Decl.Campos:
            Saida.append(f"{TipoC(Campo.Ty)} m_{Campo.Nome};")
            if Campo.Default is None:
                Campo.DefaultC = ZEROS_C[Campo.Ty.Cat]
            else:
                Campo.DefaultC = self.Expr(Campo.Default)
        Saida.append(f"}} S_{Decl.Nome};")

    def Assinatura(self, Proc: ProcDecl) -> str:
        Partes = []
        for Param in Proc.Params:
            Partes.append(f"{TipoC(Param.Ty)} v_{Param.Nome}")
            if Param.Default is not None:
                Param.DefaultC = self.Expr(Param.Default)
        return f"static {TipoC(Proc.Ret)} f_{Proc.Nome}({', '.join(Partes) or 'void'})"

    def EmitirProc(self, Proc: ProcDecl, Assinatura: str) -> None:
        self.Linhas.append(Assinatura + " {")
        self.EmitirBloco(Proc.Corpo)
        self.Linhas.append("}")

    # ------------------------------------------------------------- comandos
    def EmitirBloco(self, Comandos: list) -> None:
        Tabela = self.Comandos
        for Comando in Comandos:
            Tabela[Comando.__class__](Comando)

    def EmitirLet(self, Cmd: LetVar) -> None:
        self.Linhas.append(f"{TipoC(Cmd.Ty)} {Cmd.CNome} = {self.Expr(Cmd.Expr)};")

    def EmitirAtribuicao(self, Cmd: Assign) -> None:
        self.Linhas.append(f"{self.Expr(Cmd.Alvo)} {Cmd.Op} {self.Expr(Cmd.Expr)};")

    def EmitirIf(self, Cmd: If) -> None:
        Saida = self.Linhas
        Saida.append(f"if ({self.Expr(Cmd.Cond)}) {{")
        while True:
            self.EmitirBloco(Cmd.Then)
            Senao = Cmd.Else
            if Senao is None:
                Saida.append("}")
                return
            if Senao.__class__ is If:
                Saida.append(f"}} else if ({self.Expr(Senao.Cond)}) {{")
                Cmd = Senao
                continue
            Saida.append("} else {")
            self.EmitirBloco(Senao)
            Saida.append("}")
            return

    def EmitirWhile(self, Cmd: While) -> None:
        self.Linhas.append(f"while ({self.Expr(Cmd.Cond)}) {{")
        self.EmitirBloco(Cmd.Corpo)
        self.Linhas.append("}")

    def EmitirFor(self, Cmd: For) -> None:
        Tipo_ = TipoC(Cmd.Ty)
        Inicio, Fim = self.Expr(Cmd.Inicio), self.Expr(Cmd.Fim)
        self.Linhas.append(
            f"for ({Tipo_} {Cmd.CNome} = {Inicio}, {Cmd.CLimite} = {Fim}; {Cmd.CNome} < {Cmd.CLimite}; ++{Cmd.CNome}) {{"
        )
        self.EmitirBloco(Cmd.Corpo)
        self.Linhas.append("}")

    def EmitirReturn(self, Cmd: Return) -> None:
        if Cmd.Expr is None:
            self.Linhas.append("return;")
        else:
            self.Linhas.append(f"return {self.Expr(Cmd.Expr)};")

    def EmitirComandoChamada(self, Cmd: CallStmt) -> None:
        Chamada = Cmd.Call
        if Chamada.Nome != "print":
            self.Linhas.append(self.Expr(Chamada) + ";")
            return
        Valor = Chamada.Args[0][2]
        Texto = self.Expr(Valor)
        Cat = Valor.Ty.Cat
        if Cat == CAT_I32:
            self.Linhas.append(f'printf("%d\\n", (int)({Texto}));')
        elif Cat == CAT_I64:
            self.Linhas.append(f'printf("%lld\\n", (long long)({Texto}));')
        else:
            self.Linhas.append(f'puts(({Texto}) ? "true" : "false");')

    # ----------------------------------------------------------- expressoes
    def Expr(self, No) -> str:
        Classe = No.__class__
        if Classe is Name:  # caminho quente: evita uma chamada por nome
            return No.CNome
        return self.Expressoes[Classe](No)

    def EmitirInt(self, No: IntLit) -> str:
        Valor = int(No.Texto)
        return str(Valor) if No.Ty.Cat == CAT_I32 else f"{Valor}LL"

    def EmitirReal(self, No: RealLit) -> str:
        return No.Texto + "f"

    def EmitirBool(self, No: BoolLit) -> str:
        return "true" if No.Valor else "false"

    def EmitirNome(self, No: Name) -> str:
        return No.CNome

    def EmitirUnario(self, No: Unary) -> str:
        Operador = "!" if No.Op == "not" else No.Op
        return f"({Operador}{self.Expr(No.Operando)})"

    def EmitirBinario(self, No: Binary) -> str:
        Operador = OPERADORES_C.get(No.Op, No.Op)
        return f"({self.Expr(No.Esq)} {Operador} {self.Expr(No.Dir)})"

    def EmitirCast(self, No: Cast) -> str:
        return f"(({TipoC(No.Ty)})({self.Expr(No.Operando)}))"

    def EmitirCampo(self, No: Field) -> str:
        Separador = "->" if No.Objeto.Ty.Cat == CAT_PTR else "."
        return f"{self.Expr(No.Objeto)}{Separador}m_{No.Nome}"

    def EmitirChamada(self, No: Call) -> str:
        Argumentos = []
        for Param, Valor in zip(No.Proc.Params, No.Ligados):
            Argumentos.append(Param.DefaultC if Valor is None else self.Expr(Valor))
        return f"f_{No.Nome}({', '.join(Argumentos)})"

    def EmitirLiteralStruct(self, No: StructLit) -> str:
        Valores = {Nome: self.Expr(Valor) for _, Nome, Valor in No.Inits}
        Partes = [f".m_{Campo.Nome} = {Valores.get(Campo.Nome, Campo.DefaultC)}" for Campo in No.Struct.Campos]
        return f"((S_{No.Nome}){{{', '.join(Partes)}}})"


def EmitirC(Decls: list) -> str:
    """Entrada: declaracoes checadas. Saida: texto do programa C."""
    return Emissor().EmitirPrograma(Decls)
