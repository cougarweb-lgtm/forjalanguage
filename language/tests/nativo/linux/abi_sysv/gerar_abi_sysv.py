# -*- coding: utf-8 -*-
"""gerar_abi_sysv.py - gerador da bateria ABI SysV no call site do foreign (M5b P37c; bateria 60/60 do P37).

Para cada uma das 60 assinaturas de abi_sysv.json gera casos/sysv_NN_<nome>.fj: um programa Forja que declara a funcao C em foreign
(lib libabi_sysv.a, compilada de assinaturas.c), chama com valores deterministicos e imprime o retorno. O oraculo e casos/oraculo_sysv.c:
a MESMA chamada em C (#include de assinaturas.c, compilado no alpine), um caso por indice (argv[1]). Os valores saem de um unico plano
(PlanejarAssinatura) e viram literal nos dois lados, entao o esperado nao depende do compilador da Forja.
Valores: inteiro = (ordem * 37 + 11) % 61 - 30; real = ((ordem * 5 + 2) % 17 - 6) * 0.25 (exato em f32/f64, impresso como x * 4 inteiro);
ponteiro = endereco de uma variavel i32 local (a funcao devolve o ponteiro e o programa imprime o inteiro apontado).
Variadica: a Forja nao declara '...' em foreign; as duas pontas chamam so com os parametros fixos.
Structs: layout lido dos typedefs de assinaturas.c (campos f0..fn, tipos C mapeados para a Forja).

Uso: python -I gerar_abi_sysv.py
Versao: 0.1.1
Changelog: 0.1.1 (15/11/2026) M5b P37c - var_p recebe ponteiro NULL (o retorno e o endereco truncado, nao reproduzivel entre processos).
           0.1.0 (15/11/2026) M5b P37c - primeira versao (60 .fj + oraculo_sysv.c; plano unico para os dois lados).
Verificacao: python -I gerar_abi_sysv.py && python -I verificar_abi_sysv.py --forja <copia do forja.exe>
"""
import itertools
import json
import re
from pathlib import Path

PASTA = Path(__file__).resolve().parent
JSON_ABI = PASTA / "abi_sysv.json"
C_ASSINATURAS = PASTA / "assinaturas.c"
PASTA_CASOS = PASTA / "casos"
ORACULO_C = PASTA_CASOS / "oraculo_sysv.c"
BIBLIOTECA = "abi_sysv"
VersaoGerador = "0.1.0"

ESCALAR_PARA_C = {"i8": "signed char", "i16": "short", "i32": "int", "i64": "long", "f32": "float", "f64": "double", "ptr": "void *"}
CAMPO_C_PARA_FJ = {"signed char": "i8", "short": "i16", "int": "i32", "long": "i64", "float": "f32", "double": "f64"}
REAIS = ("f32", "f64")
FORMATO_C = '"%lld\\n"'
REGEX_STRUCT = re.compile(r"typedef struct (S\d+) \{([^}]*)\}")
# var_p devolve (int) do endereco do ponteiro: o endereco muda de um processo para o outro, entao o argumento e NULL (retorno 0).
# O 0 ainda prova o registrador: lixo em rdi daria outro numero.
PONTEIRO_NULO = {"var_p"}


def CarregarStructs(TextoC: str) -> dict:
    """Nome do struct -> [(campo, tipo Forja)], lido dos typedefs de assinaturas.c (a fonte do layout)."""
    Structs = {}
    for Nome, Corpo in REGEX_STRUCT.findall(TextoC):
        Campos = []
        for Parte in Corpo.split(";"):
            if Parte.strip():
                TipoC, Campo = Parte.strip().rsplit(" ", 1)
                Campos.append((Campo, CAMPO_C_PARA_FJ[TipoC]))
        Structs[Nome] = Campos
    return Structs


def NovoValor(Tipo: str, Ordem: int):
    if Tipo in REAIS:
        return ((Ordem * 5 + 2) % 17 - 6) * 0.25
    return (Ordem * 37 + 11) % 61 - 30


def LiteralDe(Valor, Tipo: str) -> str:
    """Mesmo texto para a Forja e para o C: real sempre com duas casas (exato, multiplo de 0,25)."""
    return f"{Valor:.2f}" if Tipo in REAIS else str(Valor)


def PlanejarArg(Tipo: str, Structs: dict, Ordem, Indice: int) -> dict:
    if Tipo in Structs:
        Campos = [(Campo, TipoCampo, NovoValor(TipoCampo, next(Ordem))) for Campo, TipoCampo in Structs[Tipo]]
        return {"tipo": Tipo, "campos": Campos}
    return {"tipo": Tipo, "nome": f"p{Indice}", "valor": NovoValor(Tipo, next(Ordem))}


def PlanejarAssinatura(Assinatura: dict, Structs: dict) -> list:
    """Plano unico por assinatura: a Forja e o oraculo C leem a mesma lista de argumentos."""
    Ordem = itertools.count()
    Args = [PlanejarArg(Tipo, Structs, Ordem, Indice) for Indice, Tipo in enumerate(Assinatura["params"])]
    if Assinatura["nome"] in PONTEIRO_NULO:
        for Arg in Args:
            Arg["nulo"] = Arg["tipo"] == "ptr"
    return Args


def Locais(Args: list) -> list:
    """Ponteiros que apontam para uma variavel local (os nulos nao precisam dela)."""
    return [A for A in Args if A["tipo"] == "ptr" and not A.get("nulo")]


def TipoFj(Tipo: str) -> str:
    return "rawptr" if Tipo == "ptr" else Tipo


def ExprFj(Arg: dict) -> str:
    if "campos" in Arg:
        Pares = ", ".join(f"{Campo} = {LiteralDe(Valor, Tipo)}" for Campo, Tipo, Valor in Arg["campos"])
        return f"{Arg['tipo']}{{{Pares}}}"
    if Arg["tipo"] == "ptr":
        return "none" if Arg.get("nulo") else f"(&{Arg['nome']} as rawptr)"
    return LiteralDe(Arg["valor"], Arg["tipo"])


def ExprC(Arg: dict) -> str:
    if "campos" in Arg:
        Pares = ", ".join(f".{Campo} = {LiteralDe(Valor, Tipo)}" for Campo, Tipo, Valor in Arg["campos"])
        return f"({Arg['tipo']}){{ {Pares} }}"
    if Arg["tipo"] == "ptr":
        return "(void *)0" if Arg.get("nulo") else f"(void *)&{Arg['nome']}"
    return LiteralDe(Arg["valor"], Arg["tipo"])


def ImprimirFj(Expressao: str, Tipo: str) -> str:
    if Tipo in REAIS:
        return f"print((({Expressao} as f64) * 4.0) as i64)"
    if Tipo == "ptr":
        return f"print((({Expressao} as ^i32)^) as i64)"
    return f"print({Expressao} as i64)"


def ImprimirC(Expressao: str, Tipo: str) -> str:
    if Tipo in REAIS:
        return f"printf({FORMATO_C}, (long long)((double)({Expressao}) * 4.0));"
    if Tipo == "ptr":
        return f"printf({FORMATO_C}, (long long)*(int *)({Expressao}));"
    return f"printf({FORMATO_C}, (long long)({Expressao}));"


def ImpressoesDoRetorno(Retorno: str, Structs: dict, Imprimir) -> list:
    """Escalar: uma linha. Struct: uma linha por campo (f0, f1, ...), na ordem do typedef."""
    if Retorno in Structs:
        return [Imprimir(f"r.{Campo}", Tipo) for Campo, Tipo in Structs[Retorno]]
    return [Imprimir("r", Retorno)]


def DeclaracoesFj(Assinatura: dict, Structs: dict) -> list:
    Usados = [Tipo for Tipo in Assinatura["params"] + [Assinatura["ret"]] if Tipo in Structs]
    Saida = []
    for Nome in dict.fromkeys(Usados):
        Saida.append(f"struct {Nome} {{")
        Saida += [f"    {Campo}: {Tipo}" for Campo, Tipo in Structs[Nome]]
        Saida += ["}", ""]
    return Saida


def ParametrosFj(Args: list) -> str:
    return ", ".join(f"a{Indice}: {TipoFj(Arg['tipo'])}" for Indice, Arg in enumerate(Args))


def TextoFj(Numero: int, Assinatura: dict, Args: list, Structs: dict) -> str:
    Nome, Retorno = Assinatura["nome"], Assinatura["ret"]
    Linhas = [f"// sysv_{Numero:02d}_{Nome}.fj - M5b P37c: {Nome} (lib {BIBLIOTECA}, ABI SysV) chamada pelo foreign; "
              f"gerado por gerar_abi_sysv.py {VersaoGerador}, nao edite a mao.", ""]
    Linhas += DeclaracoesFj(Assinatura, Structs)
    Linhas += ["foreign {", f'    link "{BIBLIOTECA}"', f"    proc {Nome}({ParametrosFj(Args)}) -> {TipoFj(Retorno)}", "}", "", "proc main() {"]
    Linhas += [f"    var {A['nome']}: i32 = {A['valor']}" for A in Locais(Args)]
    Linhas.append(f"    let r = {Nome}({', '.join(ExprFj(A) for A in Args)})")
    Linhas += ["    " + L for L in ImpressoesDoRetorno(Retorno, Structs, ImprimirFj)]
    Linhas += ["}", ""]
    return "\n".join(Linhas)


def CasoC(Numero: int, Assinatura: dict, Args: list, Structs: dict) -> str:
    Nome, Retorno = Assinatura["nome"], Assinatura["ret"]
    Linhas = [f"    case {Numero}: {{"]
    Linhas += [f"        int {A['nome']} = {A['valor']};" for A in Locais(Args)]
    Linhas.append(f"        {ESCALAR_PARA_C.get(Retorno, Retorno)} r = {Nome}({', '.join(ExprC(A) for A in Args)});")
    Linhas += ["        " + L for L in ImpressoesDoRetorno(Retorno, Structs, ImprimirC)]
    Linhas += ["        break;", "    }"]
    return "\n".join(Linhas)


def TextoOraculo(Casos: list) -> str:
    """Casos: [(numero, assinatura, args, structs)]. O oraculo roda UM caso por argv[1]."""
    Linhas = ["/* oraculo_sysv.c - oraculo C da bateria ABI SysV (M5b P37c): a mesma chamada de cada sysv_NN_<nome>.fj, compilada no alpine com assinaturas.c.",
              " * Gerado por gerar_abi_sysv.py " + VersaoGerador + ". Nao edite a mao. Uso: oraculo_sysv <indice>.",
              " */", "#include <stdio.h>", "#include <stdlib.h>", '#include "../assinaturas.c"', "",
              "int main(int argc, char **argv)", "{", "    if (argc < 2) return 3;", "    switch (atoi(argv[1])) {"]
    Linhas += [CasoC(Numero, Assinatura, Args, Structs) for Numero, Assinatura, Args, Structs in Casos]
    Linhas += ["    default:", "        return 2;", "    }", "    return 0;", "}", ""]
    return "\n".join(Linhas)


def Gerar(Pasta: Path = PASTA_CASOS) -> int:
    """Escreve os .fj e o oraculo. Devolve a quantidade de assinaturas."""
    Assinaturas = json.loads(JSON_ABI.read_text(encoding="utf-8"))["assinaturas"]
    Structs = CarregarStructs(C_ASSINATURAS.read_text(encoding="utf-8"))
    Pasta.mkdir(parents=True, exist_ok=True)
    Casos = []
    for Numero, Assinatura in enumerate(Assinaturas):
        Args = PlanejarAssinatura(Assinatura, Structs)
        Casos.append((Numero, Assinatura, Args, Structs))
        Nome = Assinatura["nome"]
        (Pasta / f"sysv_{Numero:02d}_{Nome}.fj").write_text(TextoFj(Numero, Assinatura, Args, Structs), encoding="utf-8", newline="\n")
    ORACULO_C.write_text(TextoOraculo(Casos), encoding="utf-8", newline="\n")
    return len(Assinaturas)


def NomesDosCasos() -> list:
    """Nome de cada caso (sem .fj), na ordem do abi_sysv.json: sysv_NN_<assinatura>."""
    Assinaturas = json.loads(JSON_ABI.read_text(encoding="utf-8"))["assinaturas"]
    return [f"sysv_{Numero:02d}_{Assinatura['nome']}" for Numero, Assinatura in enumerate(Assinaturas)]


def Principal() -> int:
    Quantidade = Gerar()
    print(f"gerar_abi_sysv: {Quantidade} casos em {PASTA_CASOS} + oraculo_sysv.c")
    return 0


if __name__ == "__main__":
    raise SystemExit(Principal())
