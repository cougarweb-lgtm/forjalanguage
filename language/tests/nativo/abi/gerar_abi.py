# -*- coding: utf-8 -*-
"""gerar_abi.py - gerador da bateria de ABI Win64 na fronteira (M3 P20; bateria 10.2 a do plano): N assinaturas sorteadas de C por semente.

Gera abi_lib_<s>.c (a DLL, compilada pelo construir_abi.py) e abi_chamar_<s>.fj (o programa Forja que a chama por foreign e imprime).
Cada assinatura tem 0 a 12 parametros: escalares i8..u64, f32, f64, ponteiro ^T (a DLL le e escreve por ele) e structs de 1 a 40 bytes
(todos os tamanhos 1..13, 15, 16, 17, 20, 24, 25, 31, 32, 33 e 40; mistura inteiro/real; campo [N]T). O retorno e escalar ou struct.
Cada funcao abi_fN calcula um Hash64 (FNV de 64 bits sobre as folhas dos argumentos em ordem), depois ESTRAGA a propria copia dos structs por valor
(1a e ultima folha = 77) e incrementa o que os ponteiros apontam; devolve o hash convertido ao tipo de retorno (struct: folha k = hash + k).
O programa imprime o retorno, a 1a e a ultima folha de cada struct passado (a copia do chamador NAO pode mudar) e o alvo de cada ponteiro (mudou +1).
O oraculo e o backend C (mesmo .fj, mesma DLL): o verificar_abi.py compara stdout, stderr e saida do x64 com o do C, em dev e release.
Real so em multiplos de 0,25 (exato em f32/f64); impresso como (x * 4) inteiro.

Variante indireta (M3 P21v-c; --indireto): as MESMAS 200 assinaturas e os mesmos valores, mas o .fj nao declara as funcoes em foreign: abre a DLL por LoadLibraryA e cada abi_fN vira um
proc(p0: T, ...) -> R obtido de GetProcAddress por 'as' (IR_CHAMADA_IND). A semantica recusa struct com campo array no tipo proc (E0909): na variante indireta cada campo [N]T vira N campos T
(mesmo layout, mesmo Tam), a DLL e propria (abi_ind_<s>.c/.dll) e o programa e abi_chamar_ind_<s>.fj.

Uso: python -I gerar_abi.py [--semente 1] [--quantidade 200] [--saida <pasta>] [--indireto]
Lado export (M3 P22b; --lado export): a Forja EXPORTA as mesmas 200 assinaturas numa DLL (abi_exp_<s>.fj, --modo dll --backend x64) e abi_exp_<s>.c (gcc) e o programa
que a chama e confere contra uma referencia em C (ver GerarExport).

Versao: 0.3.0
Changelog: 0.3.0 - M3 P22b: --lado export (GerarExport: DLL Forja + programa C com ref_fN); o lado import gera os mesmos bytes de antes.
           0.2.0 - M3 P21v-c: variante indireta (Gerar(..., Indireto), --indireto, ProcIndireto, AchatarArrays, NomeDaLib); a variante direta gera os mesmos bytes de antes.
           0.1.0 - M3 P20: primeira versao (parte do gerador minimo de 40 assinaturas do tests\\interop\\dinamica\\c_struct_abi; esse segue como esta).
Verificacao: python -I gerar_abi.py --saida %TEMP%\\abi_g && python -I verificar_abi.py --forja <copia> --sementes 1,2,3
"""

import argparse
import random
from dataclasses import dataclass
from pathlib import Path

VersaoGerador = "0.1.0"  # entra no cabecalho do abi_lib_<s>.c (hash versionado): so muda se o .c mudar
# nome Forja -> (tipo C, bytes, classe: i com sinal, u sem sinal, f real)
Tipos = {"i8": ("int8_t", 1, "i"), "u8": ("uint8_t", 1, "u"), "i16": ("int16_t", 2, "i"), "u16": ("uint16_t", 2, "u"),
         "i32": ("int32_t", 4, "i"), "u32": ("uint32_t", 4, "u"), "i64": ("int64_t", 8, "i"), "u64": ("uint64_t", 8, "u"),
         "f32": ("float", 4, "f"), "f64": ("double", 8, "f")}
NomesTipos = list(Tipos)
Faixas = {"i8": (-127, 127), "u8": (0, 255), "i16": (-32000, 32000), "u16": (0, 65535), "i32": (-2000000000, 2000000000),
          "u32": (0, 4200000000), "i64": (-9000000000000000000, 9000000000000000000), "u64": (0, 9000000000000000000)}
TamanhosForcados = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 20, 24, 25, 31, 32, 33, 40]
# Structs de forma fixa: real sozinho em 4 e 8 bytes, dois reais, real + inteiro, so inteiros pequenos (campos: (tipo, n); n 0 = escalar).
FormasFixas = [[("f32", 0)], [("f64", 0)], [("f32", 0), ("f32", 0)], [("f32", 0), ("i32", 0)], [("i16", 0)] * 4, [("f64", 0), ("f64", 0)],
               [("i64", 0)], [("u8", 3)], [("f32", 3)], [("i16", 5), ("u8", 0)], [("f64", 2), ("i32", 0)]]
MaxParametros = 12
MaxArray = 4
ChanceArray = 0.15
ProbabilidadeStruct = 0.40
ProbabilidadePonteiro = 0.10
ProbabilidadeRetornoStruct = 0.40
SentinelaMexida = 77
FolgaHash = 0x14650FB0739D0383
GrupoDeChamadas = 10


@dataclass
class Campo:
    Nome: str
    Tipo: str
    N: int  # 0 = escalar; senao [N]Tipo


@dataclass
class Estrutura:
    Nome: str
    Campos: list
    Tam: int

    def Folhas(self):
        """(sufixo de acesso, tipo) de cada folha escalar, em ordem de memoria."""
        Saida = []
        for C in self.Campos:
            Saida += [(f".{C.Nome}", C.Tipo)] if C.N == 0 else [(f".{C.Nome}[{I}]", C.Tipo) for I in range(C.N)]
        return Saida

    def FolhasDeBorda(self):
        """A 1a e a ultima folha (uma so se o struct tem uma folha)."""
        Folhas = self.Folhas()
        return [Folhas[0]] if len(Folhas) == 1 else [Folhas[0], Folhas[-1]]


def Alinhar(Valor, Alinhamento):
    return (Valor + Alinhamento - 1) // Alinhamento * Alinhamento


def Candidatos(Restante, Deslocamento, MaxAlinh):
    """Campos (tipo, n, deslocamento depois dele) que cabem a partir do deslocamento sem passar do alvo e sem alinhamento acima do permitido."""
    Achados = []
    for Tipo in NomesTipos:
        Bytes = Tipos[Tipo][1]
        Inicio = Alinhar(Deslocamento, Bytes)
        if Bytes <= MaxAlinh and Inicio + Bytes <= Deslocamento + Restante:
            Achados.append((Tipo, 0, Inicio + Bytes))
    return Achados


def SortearCampo(Sorteio, Alvo, Deslocamento, MaxAlinh):
    """Proximo campo (tipo, n, deslocamento depois dele); u8 sempre cabe, entao o laco anda."""
    Tipo, _, Fim = Sorteio.choice(Candidatos(Alvo - Deslocamento, Deslocamento, MaxAlinh))
    if Sorteio.random() < ChanceArray:
        N = Sorteio.randint(2, MaxArray)
        Inicio = Alinhar(Deslocamento, Tipos[Tipo][1])
        if Inicio + N * Tipos[Tipo][1] <= Alvo:
            return Tipo, N, Inicio + N * Tipos[Tipo][1]
    return Tipo, 0, Fim


def MontarComTamanho(Sorteio, Alvo):
    """Campos (tipo, n) cujo layout de C soma exatamente Alvo bytes (alinhamento maximo = maior potencia de 2 que divide Alvo, ate 8)."""
    MaxAlinh = min(8, Alvo & -Alvo)
    Formas, Deslocamento = [], 0
    while Deslocamento < Alvo:
        Tipo, N, Deslocamento = SortearCampo(Sorteio, Alvo, Deslocamento, MaxAlinh)
        Formas.append((Tipo, N))
    return Formas


def TamanhoDoLayout(Formas):
    Deslocamento, Alinh = 0, 1
    for Tipo, N in Formas:
        Bytes = Tipos[Tipo][1]
        Deslocamento = Alinhar(Deslocamento, Bytes) + Bytes * max(N, 1)
        Alinh = max(Alinh, Bytes)
    return Alinhar(Deslocamento, Alinh)


def SortearEstruturas(Sorteio):
    Formas = [MontarComTamanho(Sorteio, T) for T in TamanhosForcados] + [list(F) for F in FormasFixas]
    Formas += [MontarComTamanho(Sorteio, Sorteio.randint(1, 40)) for _ in range(4)]
    Estruturas = []
    for I, Forma in enumerate(Formas):
        Campos = [Campo(f"f{J}", Tipo, N) for J, (Tipo, N) in enumerate(Forma)]
        Estruturas.append(Estrutura(f"Ab{I}", Campos, TamanhoDoLayout(Forma)))
    return Estruturas


def SortearForma(Sorteio, Estruturas, Retorno=False):
    """('t', tipo) escalar | ('p', tipo) ponteiro (so parametro) | ('s', indice) struct."""
    Sorte = Sorteio.random()
    if Sorte < (ProbabilidadeRetornoStruct if Retorno else ProbabilidadeStruct):
        return ("s", Sorteio.randrange(len(Estruturas)))
    if not Retorno and Sorte > 1.0 - ProbabilidadePonteiro:
        return ("p", Sorteio.choice(NomesTipos))
    return ("t", Sorteio.choice(NomesTipos))


def SortearAssinaturas(Sorteio, Estruturas, Quantidade):
    """(parametros, retorno) de cada funcao; a metade das contagens de parametros anda de 0 a 12 em ciclo, a outra metade sorteia."""
    Assinaturas = []
    for I in range(Quantidade):
        N = I % (MaxParametros + 1) if I % 2 == 0 else Sorteio.randint(1, MaxParametros)
        Parametros = [SortearForma(Sorteio, Estruturas) for _ in range(N)]
        Assinaturas.append((Parametros, SortearForma(Sorteio, Estruturas, Retorno=True)))
    return Assinaturas


def SortearValor(Sorteio, Tipo):
    if Tipos[Tipo][2] == "f":
        return Sorteio.randint(-4000, 4000) / 4.0
    Menor, Maior = Faixas[Tipo]
    Sorte = Sorteio.random()
    if Sorte < 0.25:
        return Sorteio.randint(max(Menor, -100), min(Maior, 100))
    if Sorte < 0.35:
        return Sorteio.choice([Menor, Maior])
    return Sorteio.randint(Menor, Maior)


def Literal(Valor, Tipo):
    return repr(float(Valor)) if Tipos[Tipo][2] == "f" else str(Valor)


# ---- lado C ----

def TipoC(Forma, Estruturas):
    if Forma[0] == "s":
        return Estruturas[Forma[1]].Nome
    return Tipos[Forma[1]][0] + ("*" if Forma[0] == "p" else "")


def FolhasC(Forma, Nome, Estruturas):
    """(expressao C, tipo Forja) de cada folha de um parametro."""
    if Forma[0] == "t":
        return [(Nome, Forma[1])]
    if Forma[0] == "p":
        return [(f"(*{Nome})", Forma[1])]
    return [(Nome + Sufixo, Tipo) for Sufixo, Tipo in Estruturas[Forma[1]].Folhas()]


def HashDe(Expressao, Tipo):
    return f"h = mix_{Tipos[Tipo][2]}(h, {Expressao});"


def ConverterRetorno(Tipo, Expressao):
    if Tipos[Tipo][2] != "f":
        return f"({Tipos[Tipo][0]})({Expressao})"
    return f"({Tipos[Tipo][0]})(((int)(({Expressao}) & 0x3FFF)) - 8192) / 4"


def CorpoRetorno(Retorno, Estruturas):
    if Retorno[0] == "t":
        return [f"    return {ConverterRetorno(Retorno[1], 'h')};"]
    Estrutura = Estruturas[Retorno[1]]
    Linhas = [f"    {Estrutura.Nome} r = {{0}};"]
    for K, (Sufixo, Tipo) in enumerate(Estrutura.Folhas()):
        Linhas.append(f"    r{Sufixo} = {ConverterRetorno(Tipo, f'h + {K}')};")
    return Linhas + ["    return r;"]


def CorpoEstragos(Assinatura, Estruturas):
    """Depois do hash: a DLL estraga a propria copia dos structs (1a e ultima folha) e incrementa o que os ponteiros apontam."""
    Linhas = []
    for K, Forma in enumerate(Assinatura[0]):
        if Forma[0] == "s":
            Linhas += [f"    p{K}{Sufixo} = {SentinelaMexida};" for Sufixo, _ in Estruturas[Forma[1]].FolhasDeBorda()]
        if Forma[0] == "p":
            Linhas.append(f"    *p{K} = ({Tipos[Forma[1]][0]})(*p{K} + 1);")
    return Linhas


def FuncaoC(Indice, Assinatura, Estruturas, Semente):
    Parametros, Retorno = Assinatura
    Lista = ", ".join(f"{TipoC(F, Estruturas)} p{K}" for K, F in enumerate(Parametros)) or "void"
    Corpo = [f"    uint64_t h = 0x{FolgaHash ^ (Semente << 20) ^ Indice:016X}ULL;"]
    for K, Forma in enumerate(Parametros):
        Corpo += [f"    {HashDe(Expr, Tipo)}" for Expr, Tipo in FolhasC(Forma, f"p{K}", Estruturas)]
    Corpo += CorpoEstragos(Assinatura, Estruturas) + CorpoRetorno(Retorno, Estruturas)
    return f"ABI {TipoC(Retorno, Estruturas)} abi_f{Indice}({Lista})\n{{\n" + "\n".join(Corpo) + "\n}\n"


def StructC(Estrutura):
    Linhas = [f"    {Tipos[C.Tipo][0]} {C.Nome}" + (f"[{C.N}]" if C.N else "") + ";" for C in Estrutura.Campos]
    Tamanho = f"_Static_assert(sizeof({Estrutura.Nome}) == {Estrutura.Tam}, \"layout\");"
    return "typedef struct {\n" + "\n".join(Linhas) + f"\n}} {Estrutura.Nome};\n{Tamanho}\n"


CabecalhoC = """#include <stdint.h>
#include <string.h>

#define ABI __declspec(dllexport)

static uint64_t passo(uint64_t h, uint64_t v) { return (h ^ v) * 0x100000001B3ULL; }
static uint64_t mix_i(uint64_t h, int64_t v) { return passo(h, (uint64_t)v); }
static uint64_t mix_u(uint64_t h, uint64_t v) { return passo(h, v); }
static uint64_t mix_f(uint64_t h, double v)
{
    uint64_t bits;
    memcpy(&bits, &v, sizeof bits);
    return passo(h, bits);
}
"""


def Rotulo(Semente, Quantidade):
    return f"gerado por gerar_abi.py {VersaoGerador} (semente {Semente}, {Quantidade} assinaturas); nao edite a mao"


def GerarC(Semente, Estruturas, Assinaturas, Nome):
    TextoStructs = "\n".join(StructC(E) for E in Estruturas)
    Funcoes = "\n".join(FuncaoC(I, A, Estruturas, Semente) for I, A in enumerate(Assinaturas))
    Titulo = f"/* {Nome}.c - {Rotulo(Semente, len(Assinaturas))}.\n * Versao: {VersaoGerador}\n */"
    return f"{Titulo}\n{CabecalhoC}\n{TextoStructs}\n{Funcoes}"


# ---- lado Forja ----

def TipoFj(Forma, Estruturas):
    if Forma[0] == "s":
        return Estruturas[Forma[1]].Nome
    return ("^" if Forma[0] == "p" else "") + Forma[1]


def StructFj(Estrutura):
    Linhas = [f"    {C.Nome}: " + (f"[{C.N}]" if C.N else "") + C.Tipo for C in Estrutura.Campos]
    return f"struct {Estrutura.Nome} {{\n" + "\n".join(Linhas) + "\n}\n"


def LiteralStruct(Estrutura, Sorteio):
    Partes = []
    for C in Estrutura.Campos:
        if C.N == 0:
            Partes.append(f"{C.Nome} = {Literal(SortearValor(Sorteio, C.Tipo), C.Tipo)}")
        else:
            Itens = ", ".join(Literal(SortearValor(Sorteio, C.Tipo), C.Tipo) for _ in range(C.N))
            Partes.append(f"{C.Nome} = {{{Itens}}}")
    return f"{Estrutura.Nome}{{{', '.join(Partes)}}}"


def Imprimir(Expressao, Tipo):
    if Tipos[Tipo][2] != "f":
        return f"    print({Expressao})"
    return f"    print(((({Expressao}) as f64) * 4.0) as int)"


def AchatarArrays(Estruturas):
    """Cada campo [N]T vira N campos T seguidos (mesmo layout de C, mesmo Tam): a semantica nao deixa tipo proc(...) levar struct com array (E0909)."""
    Achatadas = []
    for E in Estruturas:
        Campos = []
        for C in E.Campos:
            Campos += [Campo(C.Nome, C.Tipo, 0)] if C.N == 0 else [Campo(f"{C.Nome}_{I}", C.Tipo, 0) for I in range(C.N)]
        Achatadas.append(Estrutura(E.Nome, Campos, E.Tam))
    return Achatadas


def NomeDaLib(Semente, Indireto):
    """Nome (sem extensao) da DLL da semente: a variante indireta tem DLL propria porque os structs saem sem array."""
    return f"abi_ind_{Semente}" if Indireto else f"abi_lib_{Semente}"


def ProcIndireto(Indice, Assinatura, Estruturas):
    """Variante indireta (P21v-c): o endereco de abi_fN vem de GetProcAddress, vira proc(p0: T, ...) -> R por 'as' e o local tem o nome da funcao (a chamada e a mesma da direta)."""
    Tipo = "proc(" + ", ".join(f"p{K}: {TipoFj(F, Estruturas)}" for K, F in enumerate(Assinatura[0])) + f") -> {TipoFj(Assinatura[1], Estruturas)}"
    return f'    let abi_f{Indice} = GetProcAddress(biblioteca, nome = "abi_f{Indice}") as {Tipo}'


class ChamadaFj:
    """Monta as linhas Forja de uma chamada: preparo dos argumentos, a chamada e os prints."""

    def __init__(self, Indice, Assinatura, Estruturas, Sorteio, Indireto=False):
        self.Indice, self.Estruturas, self.Sorteio = Indice, Estruturas, Sorteio
        self.Preparo, self.Argumentos, self.Depois = [], [], []
        for K, Forma in enumerate(Assinatura[0]):
            self.Argumento(K, Forma)
        self.Retorno = Assinatura[1]
        if Indireto:
            self.Preparo.insert(0, ProcIndireto(Indice, Assinatura, Estruturas))

    def Argumento(self, K, Forma):
        Nome = f"a{self.Indice}_{K}"
        if Forma[0] == "t":
            Texto = Literal(SortearValor(self.Sorteio, Forma[1]), Forma[1])
        elif Forma[0] == "p":
            Texto = f"&{Nome}"
            self.Preparo.append(f"    var {Nome}: {Forma[1]} = {Literal(SortearValor(self.Sorteio, Forma[1]), Forma[1])}")
            self.Depois.append(Imprimir(Nome, Forma[1]))
        else:
            Estrutura = self.Estruturas[Forma[1]]
            Texto = Nome
            self.Preparo.append(f"    let {Nome} = {LiteralStruct(Estrutura, self.Sorteio)}")
            self.Depois += [Imprimir(Nome + Sufixo, Tipo) for Sufixo, Tipo in Estrutura.FolhasDeBorda()]
        self.Argumentos.append(Texto if K == 0 else f"p{K} = {Texto}")

    def Linhas(self):
        Chamada = f"abi_f{self.Indice}({', '.join(self.Argumentos)})"
        if self.Retorno[0] == "t":
            return self.Preparo + [Imprimir(Chamada, self.Retorno[1])] + self.Depois
        Estrutura = self.Estruturas[self.Retorno[1]]
        Nome = f"r{self.Indice}"
        Impressoes = [Imprimir(Nome + Sufixo, Tipo) for Sufixo, Tipo in Estrutura.Folhas()]
        return self.Preparo + [f"    let {Nome} = {Chamada}"] + Impressoes + self.Depois


def ProcessosDeChamadas(Chamadas, Biblioteca=None):
    """Grupos de 10 chamadas por proc (um main gigante nao ajuda ninguem); main chama os grupos em ordem. Biblioteca (variante indireta): cada grupo abre a DLL pelo nome."""
    Corpos, Nomes = [], []
    for G in range(0, len(Chamadas), GrupoDeChamadas):
        Nome = f"grupo_{G // GrupoDeChamadas}"
        Linhas = [L for Chamada in Chamadas[G:G + GrupoDeChamadas] for L in Chamada.Linhas()]
        if Biblioteca is not None:
            Linhas.insert(0, f'    let biblioteca = LoadLibraryA("{Biblioteca}")')
        Corpos.append(f"proc {Nome}() {{\n" + "\n".join(Linhas) + "\n}\n")
        Nomes.append(Nome)
    Main = "proc main() {\n" + "\n".join(f"    {N}()" for N in Nomes) + "\n}\n"
    return "\n".join(Corpos) + "\n" + Main


def ForeignDireto(Semente, Estruturas, Assinaturas):
    Prototipos = [f"    proc abi_f{I}(" + ", ".join(f"p{K}: {TipoFj(F, Estruturas)}" for K, F in enumerate(A[0]))
                  + f") -> {TipoFj(A[1], Estruturas)}" for I, A in enumerate(Assinaturas)]
    return f'foreign {{\n    link "{NomeDaLib(Semente, False)}"\n' + "\n".join(Prototipos) + "\n}\n"


# Variante indireta: so o carregador; as funcoes chegam por GetProcAddress (nao ha foreign delas).
ForeignIndireto = """foreign {
    link "kernel32"
    proc LoadLibraryA(nome: cstr) -> rawptr
    proc GetProcAddress(modulo: rawptr, nome: cstr) -> rawptr
}
"""


def GerarFj(Semente, Estruturas, Assinaturas, Sorteio, Indireto=False):
    Chamadas = [ChamadaFj(I, A, Estruturas, Sorteio, Indireto) for I, A in enumerate(Assinaturas)]
    Foreign = ForeignIndireto if Indireto else ForeignDireto(Semente, Estruturas, Assinaturas)
    Structs = "\n".join(StructFj(E) for E in Estruturas)
    Variante = ", chamada indireta por GetProcAddress" if Indireto else ""
    Titulo = f"// {NomeDoFj(Semente, Indireto)}.fj - {Rotulo(Semente, len(Assinaturas))}{Variante}.\n// Versao: {VersaoGerador}"
    Biblioteca = f"{NomeDaLib(Semente, True)}.dll" if Indireto else None
    return f"{Titulo}\n\n{Structs}\n{Foreign}\n{ProcessosDeChamadas(Chamadas, Biblioteca)}"


def NomeDoFj(Semente, Indireto):
    return f"abi_chamar_ind_{Semente}" if Indireto else f"abi_chamar_{Semente}"


def Gerar(Semente, Quantidade, Indireto=False):
    """(texto da DLL em C, texto do programa .fj); o mesmo (semente, quantidade, variante) gera sempre os mesmos bytes. Indireto: as mesmas assinaturas e valores, structs sem
    campo array (AchatarArrays), DLL propria (abi_ind_<s>) e o .fj chama por proc obtido de GetProcAddress."""
    Sorteio = random.Random(Semente)
    Estruturas = SortearEstruturas(Sorteio)
    Assinaturas = SortearAssinaturas(Sorteio, Estruturas, Quantidade)
    if Indireto:
        Estruturas = AchatarArrays(Estruturas)
    return GerarC(Semente, Estruturas, Assinaturas, NomeDaLib(Semente, Indireto)), GerarFj(Semente, Estruturas, Assinaturas, Sorteio, Indireto)


# ---- lado export (M3 P22b): a Forja EXPORTA as 200 funcoes numa DLL (--modo dll --backend x64) e um programa C (gcc) chama e confere ----
# O hash do lado export e outro (rotacao e xor, sem multiplicacao): em dev o inteiro da Forja trava no estouro e so inteiro sem sinal tem operador de bits.
# O programa C leva a implementacao de referencia de cada funcao (ref_fN, o mesmo calculo em C) e compara retorno, structs passados e alvo de ponteiros.

VersaoExport = "0.1.0"  # entra no cabecalho do abi_exp_<s>.c e do .fj (hash versionado): so muda se o texto gerado mudar
SemSinal = {"i8": "u8", "i16": "u16", "i32": "u32", "i64": "u64"}
MascaraSemSinal = {"u8": 255, "u16": 65535, "u32": 4294967295}
MascaraReal = 16383  # 0x3FFF: o real do retorno sai de ((h & MascaraReal) - 8192) / 4


def NomeDoExport(Semente):
    return f"abi_exp_{Semente}"


CabecalhoExportC = """#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t passo(uint64_t h, uint64_t v) { return ((h << 13) | (h >> 51)) ^ v; }
static uint64_t mix_i(uint64_t h, int64_t v) { return passo(h, (uint64_t)v); }
static uint64_t mix_u(uint64_t h, uint64_t v) { return passo(h, v); }
static uint64_t mix_f(uint64_t h, double v)
{
    uint64_t bits;
    memcpy(&bits, &v, sizeof bits);
    return passo(h, bits);
}

static int g_ruins;
#define CONFERE(cond, n, texto) do { if (!(cond)) { printf("abi_f%d: %s\\n", (n), (texto)); g_ruins++; } } while (0)
"""


def RetornoExportC(Retorno, Estruturas):
    if Retorno[0] == "t":
        return [f"    return {ConverterRetorno(Retorno[1], 'h')};"]
    Estrutura = Estruturas[Retorno[1]]
    Linhas = [f"    {Estrutura.Nome} r = {{0}};"]
    for K, (Sufixo, Tipo) in enumerate(Estrutura.Folhas()):
        Linhas.append(f"    r{Sufixo} = {ConverterRetorno(Tipo, f'h ^ {K}')};")
    return Linhas + ["    return r;"]


def EstragoPonteiroC(K, Tipo):
    Operacao = "+ 1" if Tipos[Tipo][2] == "f" else "^ 1"
    return f"    *p{K} = ({Tipos[Tipo][0]})(*p{K} {Operacao});"


def FuncaoReferenciaC(Indice, Assinatura, Estruturas, Semente):
    """ref_fN: o calculo que a DLL da Forja tem de fazer (hash das folhas, alvo dos ponteiros mexido, retorno montado do hash)."""
    Parametros, Retorno = Assinatura
    Lista = ", ".join(f"{TipoC(F, Estruturas)} p{K}" for K, F in enumerate(Parametros)) or "void"
    Corpo = [f"    uint64_t h = 0x{FolgaHash ^ (Semente << 20) ^ Indice:016X}ULL;"]
    for K, Forma in enumerate(Parametros):
        Corpo += [f"    {HashDe(Expr, Tipo)}" for Expr, Tipo in FolhasC(Forma, f"p{K}", Estruturas)]
    Corpo += [EstragoPonteiroC(K, F[1]) for K, F in enumerate(Parametros) if F[0] == "p"] + RetornoExportC(Retorno, Estruturas)
    return f"static {TipoC(Retorno, Estruturas)} ref_f{Indice}({Lista})\n{{\n" + "\n".join(Corpo) + "\n}\n"


def LiteralC(Valor, Tipo):
    if Tipos[Tipo][2] == "f":
        return repr(float(Valor)) + ("f" if Tipo == "f32" else "")
    if Tipo == "i64":
        return f"INT64_C({Valor})"
    if Tipo == "u64":
        return f"UINT64_C({Valor})"
    return f"({Tipos[Tipo][0]})({Valor})"


def LiteralStructC(Estrutura, Sorteio):
    Partes = []
    for C in Estrutura.Campos:
        if C.N == 0:
            Partes.append(f".{C.Nome} = {LiteralC(SortearValor(Sorteio, C.Tipo), C.Tipo)}")
        else:
            Itens = ", ".join(LiteralC(SortearValor(Sorteio, C.Tipo), C.Tipo) for _ in range(C.N))
            Partes.append(f".{C.Nome} = {{{Itens}}}")
    return "{ " + ", ".join(Partes) + " }"


def ComparacoesStructC(Estrutura, Esq, Dir, Indice, Rotulo):
    return [f'    CONFERE({Esq}{Sufixo} == {Dir}{Sufixo}, {Indice}, "{Rotulo}{Sufixo}");' for Sufixo, _ in Estrutura.Folhas()]


def ChamadaExportC(Indice, Assinatura, Estruturas, Sorteio):
    """chamar_N: sorteia os argumentos, chama a funcao da DLL e a ref_fN com copias iguais e confere retorno, structs passados e alvos de ponteiro."""
    Parametros, Retorno = Assinatura
    Tipo = TipoC(Retorno, Estruturas)
    Lista = ", ".join(TipoC(F, Estruturas) for F in Parametros) or "void"
    Corpo = [f"    typedef {Tipo} (*Funcao)({Lista});", f'    Funcao f = (Funcao)GetProcAddress(m, "abi_f{Indice}");',
             f'    if (f == NULL) {{ printf("abi_f{Indice}: export ausente\\n"); g_ruins++; return; }}']
    ArgsDll, ArgsRef, Depois = [], [], []
    for K, Forma in enumerate(Parametros):
        if Forma[0] == "t":
            Texto = LiteralC(SortearValor(Sorteio, Forma[1]), Forma[1])
            ArgsDll.append(Texto)
            ArgsRef.append(Texto)
        elif Forma[0] == "p":
            Corpo += [f"    {Tipos[Forma[1]][0]} a{K} = {LiteralC(SortearValor(Sorteio, Forma[1]), Forma[1])};", f"    {Tipos[Forma[1]][0]} b{K} = a{K};"]
            ArgsDll.append(f"&a{K}")
            ArgsRef.append(f"&b{K}")
            Depois.append(f'    CONFERE(a{K} == b{K}, {Indice}, "alvo do ponteiro p{K}");')
        else:
            Estrutura = Estruturas[Forma[1]]
            Corpo += [f"    {Estrutura.Nome} a{K} = {LiteralStructC(Estrutura, Sorteio)};", f"    {Estrutura.Nome} b{K} = a{K};"]
            ArgsDll.append(f"a{K}")
            ArgsRef.append(f"b{K}")
            Depois += ComparacoesStructC(Estrutura, f"a{K}", f"b{K}", Indice, f"struct p{K} mudou na ida")
    Corpo += [f"    {Tipo} got = f({', '.join(ArgsDll)});", f"    {Tipo} want = ref_f{Indice}({', '.join(ArgsRef)});"]
    if Retorno[0] == "s":
        Corpo += ComparacoesStructC(Estruturas[Retorno[1]], "got", "want", Indice, "retorno")
    else:
        Corpo.append(f'    CONFERE(got == want, {Indice}, "retorno");')
    return f"static void chamar_{Indice}(HMODULE m)\n{{\n" + "\n".join(Corpo + Depois) + "\n}\n"


def PrincipalExportC(Quantidade, Nome):
    Chamadas = "\n".join(f"    chamar_{I}(m);" for I in range(Quantidade))
    return f"""int main(int argc, char **argv)
{{
    const char *caminho = argc > 1 ? argv[1] : "{Nome}.dll";
    HMODULE m = LoadLibraryA(caminho);
    if (m == NULL) {{ printf("LoadLibrary(%s) falhou: %lu\\n", caminho, GetLastError()); return 2; }}
{Chamadas}
    printf("{Nome}: {Quantidade} chamadas, %d diferencas\\n", g_ruins);
    return g_ruins == 0 ? 0 : 1;
}}
"""


def FolhasFj(Forma, Nome, Estruturas):
    """(expressao Forja, tipo) de cada folha de um parametro; Estruturas ja sem campo array."""
    if Forma[0] == "t":
        return [(Nome, Forma[1])]
    if Forma[0] == "p":
        return [(f"{Nome}^", Forma[1])]
    return [(Nome + Sufixo, Tipo) for Sufixo, Tipo in Estruturas[Forma[1]].Folhas()]


def MisturaFj(Expressao, Tipo):
    Classe = Tipos[Tipo][2]
    if Classe == "f":
        return f"h = mix_f(h, v = {Expressao}{' as f64' if Tipo == 'f32' else ''})"
    if Classe == "i":
        return f"h = mix_i(h, v = {Expressao}{'' if Tipo == 'i64' else ' as i64'})"
    return f"h = passo(h, v = {Expressao}{'' if Tipo == 'u64' else ' as u64'})"


def ConverterRetornoFj(Tipo, Hash):
    """Hash (u64) -> Tipo, o mesmo valor do (T)(h) de C: inteiro pelos bits baixos, real por ((h & 0x3FFF) - 8192) / 4."""
    if Tipos[Tipo][2] == "f":
        return f"((((({Hash}) & {MascaraReal}) as int) - 8192) as {Tipo}) / 4.0"
    if Tipo == "u64":
        return Hash
    if Tipo in MascaraSemSinal:
        return f"(({Hash}) & {MascaraSemSinal[Tipo]}) as {Tipo}"
    if Tipo == "i64":
        return f"transmute({Hash}, to = i64)"
    Sem = SemSinal[Tipo]
    return f"transmute((({Hash}) & {MascaraSemSinal[Sem]}) as {Sem}, to = {Tipo})"


def EstragoPonteiroFj(K, Tipo):
    if Tipos[Tipo][2] == "f":
        return f"p{K}^ = p{K}^ + 1.0"
    if Tipos[Tipo][2] == "u":
        return f"p{K}^ = p{K}^ ~ 1"
    return f"p{K}^ = transmute(transmute(p{K}^, to = {SemSinal[Tipo]}) ~ 1, to = {Tipo})"


def RetornoExportFj(Retorno, Estruturas):
    if Retorno[0] == "t":
        return [f"    return {ConverterRetornoFj(Retorno[1], 'h')}"]
    Estrutura = Estruturas[Retorno[1]]
    Itens = [f"{Sufixo[1:]} = {ConverterRetornoFj(Tipo, f'h ~ {K}')}" for K, (Sufixo, Tipo) in enumerate(Estrutura.Folhas())]
    return [f"    return {Estrutura.Nome}{{" + ", ".join(Itens) + "}"]


def FuncaoExportFj(Indice, Assinatura, Estruturas, Semente):
    Parametros, Retorno = Assinatura
    Lista = ", ".join(f"p{K}: {TipoFj(F, Estruturas)}" for K, F in enumerate(Parametros))
    Declaracao = "var" if Parametros else "let"  # sem parametro nada altera o hash (E0707)
    Corpo = [f"    {Declaracao} h: u64 = 0x{FolgaHash ^ (Semente << 20) ^ Indice:016X}"]
    for K, Forma in enumerate(Parametros):
        Corpo += [f"    {MisturaFj(Expr, Tipo)}" for Expr, Tipo in FolhasFj(Forma, f"p{K}", Estruturas)]
    Corpo += [f"    {EstragoPonteiroFj(K, F[1])}" for K, F in enumerate(Parametros) if F[0] == "p"] + RetornoExportFj(Retorno, Estruturas)
    return f"pub export proc abi_f{Indice}({Lista}) -> {TipoFj(Retorno, Estruturas)} {{\n" + "\n".join(Corpo) + "\n}\n"


AuxiliaresExportFj = """proc passo(h: u64, v: u64) -> u64 {
    return ((h << 13) | (h >> 51)) ~ v
}

proc mix_i(h: u64, v: i64) -> u64 {
    return passo(h, v = transmute(v, to = u64))
}

proc mix_f(h: u64, v: f64) -> u64 {
    return passo(h, v = transmute(v, to = u64))
}
"""


def GerarExport(Semente, Quantidade):
    """(texto da DLL em Forja, texto do programa C): as MESMAS estruturas e assinaturas de Gerar(Semente); structs da DLL sem campo array (a Forja nao exporta array)."""
    Sorteio = random.Random(Semente)
    Estruturas = SortearEstruturas(Sorteio)
    Assinaturas = SortearAssinaturas(Sorteio, Estruturas, Quantidade)
    Achatadas = AchatarArrays(Estruturas)
    Nome = NomeDoExport(Semente)
    Rotulo = f"gerado por gerar_abi.py {VersaoExport} --lado export (semente {Semente}, {Quantidade} assinaturas); nao edite a mao"
    Funcoes = "\n".join(FuncaoExportFj(I, A, Achatadas, Semente) for I, A in enumerate(Assinaturas))
    Fj = (f"// {Nome}.fj - {Rotulo}.\n// Versao: {VersaoExport}\n\n" + "\n".join(StructFj(E) for E in Achatadas)
          + f"\n{AuxiliaresExportFj}\n{Funcoes}\nproc main() {{\n}}\n")
    Referencias = "\n".join(FuncaoReferenciaC(I, A, Estruturas, Semente) for I, A in enumerate(Assinaturas))
    Chamadas = "\n".join(ChamadaExportC(I, A, Estruturas, Sorteio) for I, A in enumerate(Assinaturas))
    C = (f"/* {Nome}.c - {Rotulo}.\n * Versao: {VersaoExport}\n */\n{CabecalhoExportC}\n"
         + "\n".join(StructC(E) for E in Estruturas) + f"\n{Referencias}\n{Chamadas}\n{PrincipalExportC(len(Assinaturas), Nome)}")
    return C, Fj


def Principal():
    Analise = argparse.ArgumentParser()
    Analise.add_argument("--semente", type=int, default=1)
    Analise.add_argument("--quantidade", type=int, default=200)
    Analise.add_argument("--saida", default=str(Path(__file__).resolve().parent))
    Analise.add_argument("--indireto", action="store_true")
    Analise.add_argument("--lado", choices=("import", "export"), default="import")
    Opcoes = Analise.parse_args()
    Saida = Path(Opcoes.saida)
    Saida.mkdir(parents=True, exist_ok=True)
    if Opcoes.lado == "export":
        TextoC, TextoFj = GerarExport(Opcoes.semente, Opcoes.quantidade)
        (Saida / f"{NomeDoExport(Opcoes.semente)}.c").write_text(TextoC, encoding="utf-8", newline="\n")
        (Saida / f"{NomeDoExport(Opcoes.semente)}.fj").write_text(TextoFj, encoding="utf-8", newline="\n")
        print(f"gerado (lado export): semente {Opcoes.semente}, {Opcoes.quantidade} assinaturas em {Saida}")
        return
    TextoC, TextoFj = Gerar(Opcoes.semente, Opcoes.quantidade, Opcoes.indireto)
    (Saida / f"{NomeDaLib(Opcoes.semente, Opcoes.indireto)}.c").write_text(TextoC, encoding="utf-8", newline="\n")
    (Saida / f"{NomeDoFj(Opcoes.semente, Opcoes.indireto)}.fj").write_text(TextoFj, encoding="utf-8", newline="\n")
    print(f"gerado: semente {Opcoes.semente}, {Opcoes.quantidade} assinaturas em {Saida}")


if __name__ == "__main__":
    Principal()
