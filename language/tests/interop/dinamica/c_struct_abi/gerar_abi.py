# -*- coding: utf-8 -*-
"""gerar_abi.py - gerador MINIMO do caso c_struct_abi (P19i-a): N assinaturas de C com structs por valor, escalares e retorno por valor.

Gera abi.c (a lib), caso.fj (os structs, o foreign e as chamadas) e esperado.txt. O esperado NAO sai do compilador nem da DLL: sai de um modelo
em Python da mesma regra (oraculo independente). Regra de cada funcao abi_fN: s = soma dos valores das folhas (campos de struct e escalares,
na ordem dos parametros) vezes o peso 1, 2, 3...; retorno escalar T = (T)s; retorno struct = campo j recebe (Tj)(s + j). Inteiro trunca para zero e
da a volta no tamanho do tipo; real e multiplo de 0,25 (exato em f32/f64) e sai impresso como (x * 4) em inteiro.
O gerador completo (todas as classes Win64, 250 assinaturas, bordas) e do P20; este e a versao minima de 40 assinaturas.

Uso: python -I gerar_abi.py [--semente 1] [--quantidade 40] [--saida <pasta>]
Versao: 0.1.1
Changelog: 0.1.1 - 0.21.0 F3: padrao x64 na docstring de verificacao (--backend c trocado por x64).
           0.1.0 -M3 P19i-a: primeira versao (semente 1, 40 assinaturas, 10 structs de 1 a 24+ bytes).
Verificacao: python -I gerar_abi.py && python -I ..\\..\\verificar_interop.py --forja <copia> --backend x64 --pasta dinamica --caso c_struct_abi
"""

import argparse
import random
from pathlib import Path

VersaoGerador = "0.1.0"
# nome Forja -> (tipo C, bits, com sinal?); real tem bits 32/64 e sinal None
Tipos = {"i8": ("int8_t", 8, True), "u8": ("uint8_t", 8, False), "i16": ("int16_t", 16, True), "u16": ("uint16_t", 16, False),
         "i32": ("int32_t", 32, True), "u32": ("uint32_t", 32, False), "i64": ("int64_t", 64, True), "u64": ("uint64_t", 64, False),
         "f32": ("float", 32, None), "f64": ("double", 64, None)}
NomesTipos = list(Tipos)
# Formas fixas (cobrem 1, 3, 4, 8, 12 e 16 bytes); as demais structs saem do sorteio.
StructsFixos = [["i8"], ["u8", "u8", "u8"], ["i16", "i16"], ["i32", "f32"], ["f64"], ["i32", "i32", "i32"], ["f64", "i64"]]
QuantidadeStructs = 10
MaxParametros = 6


def SortearStructs(Sorteio):
    Formas = list(StructsFixos)
    while len(Formas) < QuantidadeStructs:
        Formas.append([Sorteio.choice(NomesTipos) for _ in range(Sorteio.randint(2, 4))])
    return [(f"Ab{I}", Forma) for I, Forma in enumerate(Formas)]


def SortearAssinaturas(Sorteio, Structs, Quantidade):
    """Cada assinatura: (parametros, retorno); parametro/retorno = ('t', tipo) ou ('s', indice do struct)."""
    Assinaturas = []
    for _ in range(Quantidade):
        Parametros = [SortearForma(Sorteio, Structs) for _ in range(Sorteio.randint(1, MaxParametros))]
        Assinaturas.append((Parametros, SortearForma(Sorteio, Structs)))
    return Assinaturas


def SortearForma(Sorteio, Structs):
    if Sorteio.random() < 0.5:
        return ("s", Sorteio.randrange(len(Structs)))
    return ("t", Sorteio.choice(NomesTipos))


def SortearValor(Sorteio, Tipo):
    _, _, ComSinal = Tipos[Tipo]
    if ComSinal is None:
        return Sorteio.randint(-80, 80) / 4.0
    return Sorteio.randint(-60, 60) if ComSinal else Sorteio.randint(0, 120)


def FolhasDaForma(Forma, Structs):
    """Tipos das folhas, em ordem: um escalar tem 1; um struct tem os campos dele."""
    return [Forma[1]] if Forma[0] == "t" else list(Structs[Forma[1]][1])


def Embrulhar(Valor, Tipo):
    """Converte o double s para o tipo (trunca para zero e da a volta no tamanho do inteiro)."""
    _, Bits, ComSinal = Tipos[Tipo]
    if ComSinal is None:
        return float(Valor)
    Inteiro = int(Valor) % (1 << Bits)
    return Inteiro - (1 << Bits) if ComSinal and Inteiro >= (1 << (Bits - 1)) else Inteiro


def LiteralFj(Valor, Tipo):
    return repr(float(Valor)) if Tipos[Tipo][2] is None else str(Valor)


def TextoFolha(Expressao, Tipo):
    """Como a Forja imprime uma folha: inteiro direto; real como (x * 4) em inteiro."""
    return Expressao if Tipos[Tipo][2] is not None else f"(({Expressao} as f64) * 4.0) as int"


def SaidaFolha(Valor, Tipo):
    return str(Valor) if Tipos[Tipo][2] is not None else str(int(Valor * 4))


def EmitirStructs(Structs):
    Fj = [f"struct {N} {{\n" + "".join(f"    f{I}: {T}\n" for I, T in enumerate(Fs)) + "}\n" for N, Fs in Structs]
    C = ["typedef struct {\n" + "".join(f"    {Tipos[T][0]} f{I};\n" for I, T in enumerate(Fs)) + f"}} {N};\n" for N, Fs in Structs]
    return "\n".join(Fj), "\n".join(C)


def NomeCFj(Forma, Structs):
    return Structs[Forma[1]][0] if Forma[0] == "s" else None


def TipoC(Forma, Structs):
    return NomeCFj(Forma, Structs) or Tipos[Forma[1]][0]


def TipoFj(Forma, Structs):
    return NomeCFj(Forma, Structs) or Forma[1]


def CorpoC(Indice, Assinatura, Structs):
    Parametros, Retorno = Assinatura
    Corpo, Peso = ["    double s = 0.0;"], 1
    for K, Forma in enumerate(Parametros):
        for J, Tipo in enumerate(FolhasDaForma(Forma, Structs)):
            Alvo = f"p{K}" if Forma[0] == "t" else f"p{K}.f{J}"
            Corpo.append(f"    s += (double){Alvo} * {Peso};")
            Peso += 1
    Lista = ", ".join(f"{TipoC(F, Structs)} p{K}" for K, F in enumerate(Parametros))
    return f"ABI {TipoC(Retorno, Structs)} abi_f{Indice}({Lista})\n{{\n" + "\n".join(Corpo + CorpoRetorno(Retorno, Structs)) + "\n}\n"


def CastC(Tipo, Expressao):
    return f"({Tipos[Tipo][0]}){Expressao}" if Tipos[Tipo][2] is None else f"({Tipos[Tipo][0]})(int64_t)({Expressao})"


def CorpoRetorno(Retorno, Structs):
    if Retorno[0] == "t":
        return [f"    return {CastC(Retorno[1], 's')};"]
    Nome, Campos = Structs[Retorno[1]]
    Linhas = [f"    {Nome} r;"] + [f"    r.f{J} = {CastC(T, f's + {J}')};" for J, T in enumerate(Campos)]
    return Linhas + ["    return r;"]


def ChamadaFj(Indice, Assinatura, Structs, Sorteio):
    """Devolve (linhas Forja da chamada e dos prints, linhas do esperado)."""
    Parametros, Retorno = Assinatura
    Argumentos, S, Peso = [], 0.0, 1
    for K, Forma in enumerate(Parametros):
        Valores = [SortearValor(Sorteio, T) for T in FolhasDaForma(Forma, Structs)]
        for V in Valores:
            S += V * Peso
            Peso += 1
        Texto = ArgumentoFj(Forma, Valores, Structs)
        Argumentos.append(Texto if K == 0 else f"p{K} = {Texto}")
    return SaidaChamada(Indice, ", ".join(Argumentos), Retorno, Structs, S)


def ArgumentoFj(Forma, Valores, Structs):
    if Forma[0] == "t":
        return LiteralFj(Valores[0], Forma[1])
    Nome, Campos = Structs[Forma[1]]
    return Nome + "{" + ", ".join(f"f{J} = {LiteralFj(V, T)}" for J, (V, T) in enumerate(zip(Valores, Campos))) + "}"


def SaidaChamada(Indice, Argumentos, Retorno, Structs, S):
    Chamada = f"abi_f{Indice}({Argumentos})"
    if Retorno[0] == "t":
        return [f"    print({TextoFolha(Chamada, Retorno[1])})"], [SaidaFolha(Embrulhar(S, Retorno[1]), Retorno[1])]
    Campos = Structs[Retorno[1]][1]
    Fj = [f"    let r{Indice} = {Chamada}"] + [f"    print({TextoFolha(f'r{Indice}.f{J}', T)})" for J, T in enumerate(Campos)]
    return Fj, [SaidaFolha(Embrulhar(S + J, T), T) for J, T in enumerate(Campos)]


def Gerar(Semente, Quantidade, Saida):
    Sorteio = random.Random(Semente)
    Structs = SortearStructs(Sorteio)
    Assinaturas = SortearAssinaturas(Sorteio, Structs, Quantidade)
    StructsFj, StructsC = EmitirStructs(Structs)
    Prototipos, Chamadas, Esperado = [], [], []
    for I, Assinatura in enumerate(Assinaturas):
        Parametros, Retorno = Assinatura
        Prototipos.append(f"    proc abi_f{I}(" + ", ".join(f"p{K}: {TipoFj(F, Structs)}" for K, F in enumerate(Parametros))
                          + f") -> {TipoFj(Retorno, Structs)}")
        Fj, Saidas = ChamadaFj(I, Assinatura, Structs, Sorteio)
        Chamadas += Fj
        Esperado += Saidas
    Rotulo = f"gerado por gerar_abi.py {VersaoGerador} (semente {Semente}, {Quantidade} assinaturas); nao edite a mao"
    Cabecalho = "Versao: 0.1.0 (primeira versao, M3 P19i-a). Verificacao: python -I gerar_abi.py e depois o verificar_interop (caso c_struct_abi)."
    (Saida / "abi.c").write_text(MontarC(Rotulo, Cabecalho, StructsC, Assinaturas, Structs), encoding="utf-8", newline="\n")
    (Saida / "caso.fj").write_text(MontarFj(Rotulo, Cabecalho, StructsFj, Prototipos, Chamadas), encoding="utf-8", newline="\n")
    (Saida / "esperado.txt").write_text("\n".join(Esperado) + "\n", encoding="utf-8", newline="\n")
    print(f"gerado: {Quantidade} assinaturas, {len(Structs)} structs, {len(Esperado)} linhas de esperado (semente {Semente})")


def MontarC(Rotulo, Cabecalho, StructsC, Assinaturas, Structs):
    Corpos = "\n".join(CorpoC(I, A, Structs) for I, A in enumerate(Assinaturas))
    return (f"/* abi.c - {Rotulo}.\n * {Cabecalho}\n */\n#include <stdint.h>\n\n#define ABI __declspec(dllexport)\n\n{StructsC}\n{Corpos}")


def MontarFj(Rotulo, Cabecalho, StructsFj, Prototipos, Chamadas):
    Foreign = 'foreign {\n    link "abi"\n' + "\n".join(Prototipos) + "\n}\n"
    return (f"// caso.fj - P19i-a, caso c_struct_abi: {Rotulo}.\n// {Cabecalho}\n\n{StructsFj}\n{Foreign}\n"
            + "proc main() {\n" + "\n".join(Chamadas) + "\n}\n")


def Principal():
    Analise = argparse.ArgumentParser()
    Analise.add_argument("--semente", type=int, default=1)
    Analise.add_argument("--quantidade", type=int, default=40)
    Analise.add_argument("--saida", default=str(Path(__file__).resolve().parent))
    Opcoes = Analise.parse_args()
    Gerar(Opcoes.semente, Opcoes.quantidade, Path(Opcoes.saida))


if __name__ == "__main__":
    Principal()
