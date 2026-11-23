# -*- coding: utf-8 -*-
"""verificar_parse_float.py - prova do arredondamento correto de text.parse_float (G3-L3): textos reais x o float() do Python e x o strtod do C.

O que faz: gera N textos reais (determinista pela semente), compila um programa Forja (release) que le o arquivo, chama text.parse_float em cada linha e grava
uma linha por texto (16 digitos hex dos bits do f64, ou O = overflow, I = invalid, E = empty); compara linha a linha com o float() do Python (inf = overflow).
Com gcc no PATH tambem compila um leitor em C com strtod (o mesmo do compilador: ir\\lower_const.c e semantic\\sem_matematica.c) e conta as divergencias dele
contra o Python (so relata: o C so passa a usar o algoritmo da std se o strtod divergir).
Mistura do corpus: inteiros e decimais curtos, 15 a 17 digitos (repr e %.17g de bits aleatorios), 16 a 25 digitos, 19+ digitos com expoente -400..400, subnormais,
meio-ponto exato entre vizinhos (e um passo acima e abaixo, ate passar de 800 digitos), potencias de 2 e de 5 exatas, 2^53 + 1, os limites do f64 e as strings
dificeis conhecidas (Nigel Tao / fast_float), zeros e -0, formas de escrita (+, E, zeros a esquerda, ponto no fim) e textos invalidos (esperado I/E).

Uso: python -I verificar_parse_float.py [--forja <forja.exe>] [--n <qtd>] [--curto] [--semente S] [--sem-c] [--manter]
     python -I verificar_parse_float.py --selftest
--n padrao 1000000; --curto = 20000 (a versao da bateria). Saida: contagens e os 10 primeiros casos divergentes; exit 0 se 0 diferencas contra o Python.
Versao: 1.0.0
Changelog: 1.0.0 - L3 do marco G3: primeira versao.
Verificacao: python -I verificar_parse_float.py --selftest   |   python -I verificar_parse_float.py --curto
"""

import argparse
import math
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from decimal import Decimal, getcontext
from pathlib import Path

PastaAqui = Path(__file__).resolve().parent
PastaLanguage = PastaAqui.parent.parent
ForjaPadrao = PastaLanguage / "compiler" / "forja.exe"
NPadrao = 1000000
NCurto = 20000
SementePadrao = 20261119
MostrarDivergencias = 10
TimeoutSegundos = 900

# Programa Forja que roda o parse_float: argv[1] = arquivo de textos (um por linha), argv[2] = arquivo de saida.
ProgramaForja = """import "std:text"
import "std:os"
import "std:file"

// Uma linha de saida por linha de entrada: 16 digitos hex dos bits do f64, ou O (overflow), I (invalid), E (empty).
proc anotar(linha: str, saida: ^[dynamic]u8) {
    switch r in text.parse_float(linha) {
    case f64:
        let bits: u64 = transmute(r, to = u64)
        format(f"{bits:016x}\\n", into = saida)
    case text.ParseError:
        switch r {
        case text.ParseError.overflow:
            append(saida, item = "O\\n")
        case text.ParseError.invalid:
            append(saida, item = "I\\n")
        case text.ParseError.empty:
            append(saida, item = "E\\n")
        }
    }
}

proc main() -> ! file.FileError {
    let argumentos = os.args()
    if len(argumentos) < 3 {
        os.exit(2)
    }
    var linhas = try file.read_lines(argumentos[1], alloc = heap)
    defer delete(linhas.items)
    defer delete(linhas.data)
    var saida = make([dynamic]u8, alloc = heap)
    defer delete(saida)
    for linha in linhas.items {
        anotar(linha, &saida)
    }
    let erro = file.write_all(argumentos[2], data = saida[..])
    if erro != none {
        os.exit(3)
    }
}
"""

# Leitor em C: uma linha por texto, strtod, 16 digitos hex dos bits ou O se deu infinito. Linhas invalidas nao entram na comparacao.
ProgramaC = r"""#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
static char Linha[1 << 16];
int main(int argc, char **argv) {
    FILE *entrada = fopen(argv[1], "rb");
    FILE *saida = fopen(argv[2], "wb");
    if (!entrada || !saida) return 2;
    while (fgets(Linha, sizeof Linha, entrada)) {
        size_t n = strlen(Linha);
        while (n > 0 && (Linha[n - 1] == '\n' || Linha[n - 1] == '\r')) Linha[--n] = 0;
        double v = strtod(Linha, NULL);
        uint64_t bits;
        memcpy(&bits, &v, 8);
        if (isinf(v)) fputs("O\n", saida); else fprintf(saida, "%016llx\n", (unsigned long long)bits);
    }
    fclose(saida);
    return 0;
}
"""

# Strings dificeis conhecidas e limites do f64; o esperado vem do float() do Python.
Especiais = [
    "0", "-0", "+0", "0.0", "-0.0", "0e5", "0.000e-400", "00000", "0.", ".0", "1", "-1", "+1", "1.", ".5", "-.5", "5e-324", "4.9406564584124654e-324",
    "2.4703282292062327e-324", "2.4703282292062328e-324", "2.4703282292062329e-324", "2.2250738585072011e-308", "2.2250738585072012e-308",
    "2.2250738585072014e-308", "2.225073858507201e-308", "1.7976931348623157e308", "1.7976931348623158e308", "1.7976931348623159e308", "1.797693134862315807e308",
    "1.7976931348623158079372897140530341507993413271003782693617377898044496829276475094664901797758720709633028641669288791094655554785194040263065748867150582068190890200070838367627385484581771153176447573027006985557136695962284291481986083493647529271907416844436551070434271155969950934e308",
    "9007199254740992", "9007199254740993", "9007199254740994", "9007199254740995", "9007199254740991", "18014398509481985", "18014398509481986", "36028797018963969",
    "1.00000000000000011102230246251565404236316680908203125", "1.00000000000000011102230246251565404236316680908203126",
    "1.00000000000000011102230246251565404236316680908203124", "0.9999999999999999444888487687421729788184165954589843750",
    "8.5070591730234616e+37", "7.2057594037927933e16", "1e23", "8.41e21", "8.5e-5", "7e-310", "1e-310", "1e-320", "1e-323", "1e-324", "1e-400", "1e400", "1e309",
    "4.35e-325", "1e22", "1e23", "123456789012345678901234567890", "0.000000000000000000000000000001", "00001.5000", "1E5", "1e+5", "1E-5", "+1.5e+3",
    "9223372036854775807", "9223372036854775808", "18446744073709551615", "18446744073709551616", "10000000000000000000", "9999999999999999999e-1",
    "1.5e-323", "2.5e-323", "3e-323", "6.9533558078350043e-310", "2.1219957915e-314", "5e-325", "2.4703282292062328e-324",
    "9.999999999999999e22", "9.9999999999999999e22", "1.0000000000000001e23", "8.988465674311580536566680e307", "1.797693134862315708145274237317043567981e308",
    "0.1", "0.2", "0.3", "3.14159265358979323846264338327950288419716939937510", "2.718281828459045", "123e-20", "5e-1", "1e0", "1e-0", "1e+0",
]
Invalidos = ["abc", "1e", "1e+", ".", "+", "-", "e5", "1 ", " 1", "1.2.3", "1e5.5", "--1", "+-1", "0x10", "inf", "nan", "1,5", "1_000", ".e5", "1e5e5", "1d5"]
VazioEsperado = ""


def BitsDoFloat(Valor):
    return struct.unpack("<Q", struct.pack("<d", Valor))[0]


def FloatDosBits(Bits):
    return struct.unpack("<d", struct.pack("<Q", Bits))[0]


def EsperadoPython(Texto):
    # Linha esperada para o texto: 16 hex, ou O se o float() deu infinito. Invalidos e vazio tem o esperado explicito em GerarCasos.
    Valor = float(Texto)
    if math.isinf(Valor):
        return "O"
    return "%016x" % BitsDoFloat(Valor)


def DoubleAleatorio(Rng, Subnormal=False):
    # Bits aleatorios de um f64 finito positivo (ou subnormal); o sinal vem de Sinal().
    if Subnormal:
        return FloatDosBits(Rng.getrandbits(52) or 1)
    while True:
        Valor = FloatDosBits(Rng.getrandbits(63))
        if math.isfinite(Valor) and Valor != 0.0:
            return Valor


def Sinal(Rng):
    return "-" if Rng.random() < 0.3 else ("+" if Rng.random() < 0.05 else "")


def Montar(Rng, Digitos, Exp10):
    # Texto de Digitos (so algarismos) vezes 10^Exp10 numa das formas de escrita: sem ponto (dddde-X), cientifica (d.ddde+Y) ou posicional (ddd.ddd).
    Letra = "e" if Rng.random() < 0.8 else "E"
    Forma = Rng.randrange(3)
    if Forma == 0:
        return "%s%s%s%d" % (Digitos, Letra, "+" if Exp10 >= 0 and Rng.random() < 0.2 else "", Exp10)
    if Forma == 1 and len(Digitos) > 1:
        return "%s.%s%s%d" % (Digitos[0], Digitos[1:], Letra, Exp10 + len(Digitos) - 1)
    PosPonto = len(Digitos) + Exp10
    if -30 <= PosPonto <= 60:
        if PosPonto <= 0:
            return "0." + "0" * (-PosPonto) + Digitos
        if PosPonto >= len(Digitos):
            return Digitos + "0" * (PosPonto - len(Digitos))
        return Digitos[:PosPonto] + "." + Digitos[PosPonto:]
    return "%s%s%d" % (Digitos, Letra, Exp10)


def DigitosExatos(Valor):
    # (algarismos, expoente) EXATOS do float positivo Valor: Valor = inteiro * 2^e, em decimal sem perda.
    Mantissa, Expoente = math.frexp(Valor)
    Inteiro = int(Mantissa * (1 << 53))
    Exp2 = Expoente - 53
    if Exp2 >= 0:
        return str(Inteiro << Exp2), 0
    return str(Inteiro * 5 ** (-Exp2)), Exp2


def PontoMedio(Bits):
    # (algarismos, expoente) EXATOS do ponto medio entre o f64 de bits Bits e o seguinte: (2f + 1) * 2^(k - 1).
    Campo = Bits >> 52
    Fracao = Bits & ((1 << 52) - 1)
    K = -1074
    if Campo > 0:
        Fracao |= 1 << 52
        K = Campo - 1075
    Inteiro = 2 * Fracao + 1
    if K - 1 >= 0:
        return str(Inteiro << (K - 1)), 0
    return str(Inteiro * 5 ** (1 - K)), K - 1


def CasoCurto(Rng):
    Escolha = Rng.randrange(3)
    if Escolha == 0:
        return Sinal(Rng) + str(Rng.randrange(10 ** Rng.randint(1, 15)))
    Digitos = str(Rng.randrange(1, 10 ** Rng.randint(1, 15)))
    return Sinal(Rng) + Montar(Rng, Digitos, Rng.randint(-22, 22) - (0 if Escolha == 1 else Rng.randint(0, 6)))


def CasoRepr(Rng):
    Valor = DoubleAleatorio(Rng, Subnormal=Rng.random() < 0.1)
    Forma = Rng.randrange(4)
    if Forma == 0:
        return Sinal(Rng) + repr(Valor)
    if Forma == 1:
        return Sinal(Rng) + "%.17g" % Valor
    if Forma == 2:
        return Sinal(Rng) + "%.*e" % (Rng.randint(14, 24), Valor)
    return Sinal(Rng) + "%.*g" % (Rng.randint(15, 20), Valor)


def CasoLongo(Rng):
    Quantos = Rng.choice([19, 19, 20, 21, 25, 30, 40, 60, 100, 200])
    Digitos = str(Rng.randrange(1, 10)) + "".join(Rng.choice("0123456789") for _ in range(Quantos - 1))
    return Sinal(Rng) + Montar(Rng, Digitos, Rng.randint(-400, 400) - Quantos)


def CasoExatoDeDouble(Rng):
    # Decimal exato de um double (ou de uma potencia de 2 / 5): o valor e representavel, o texto e longo.
    Tipo = Rng.randrange(4)
    if Tipo == 0:
        Valor = math.ldexp(1.0, Rng.randint(-1074, 1023))
    elif Tipo == 1:
        Valor = DoubleAleatorio(Rng, Subnormal=Rng.random() < 0.4)
    elif Tipo == 2:
        Valor = float(5 ** Rng.randint(0, 440)) if Rng.random() < 0.5 else FloatDosBits(Rng.getrandbits(52) + (Rng.randint(0, 2046) << 52) or 1)
    else:
        Valor = float(2 ** 53 + Rng.randint(-5, 5))
    if not math.isfinite(Valor) or Valor == 0.0:
        Valor = 1.0
    Digitos, Exp10 = DigitosExatos(Valor)
    return Sinal(Rng) + Montar(Rng, Digitos, Exp10)


def CasoMeioPonto(Rng):
    # Meio-ponto exato entre vizinhos (empate: o par), ou um passo (10^-m) acima ou abaixo, ate passar de 800 digitos.
    Perto = Rng.random()
    if Perto < 0.1:
        Bits = Rng.choice([0, 1, 2, (1 << 52) - 1, 1 << 52, (1 << 52) + 1, 0x7FEFFFFFFFFFFFFE, 0x7FEFFFFFFFFFFFFF])
    elif Perto < 0.4:
        Bits = Rng.getrandbits(52)
    else:
        Bits = Rng.getrandbits(63)
        if Bits >= 0x7FF0000000000000:
            Bits = 0x7FEFFFFFFFFFFFFF - (Bits & 0xFFFF)
    Digitos, Exp10 = PontoMedio(Bits)
    Variante = Rng.randrange(4)
    if Variante != 0:
        Casas = Rng.choice([1, 2, 5, 12, 40, 90, 400]) if Variante == 3 else Rng.randint(1, 40)
        Inteiro = int(Digitos) * 10 ** Casas + (1 if Variante == 1 else -1 if Variante == 2 else 1)
        Digitos, Exp10 = str(Inteiro), Exp10 - Casas
    if Rng.random() < 0.15:
        Digitos = Digitos.rstrip("0") or "0"
        Exp10 = Exp10 + 0
    return Sinal(Rng) + Montar(Rng, Digitos, Exp10)


def CasoEscrita(Rng):
    # Formas de escrita: zeros a esquerda e a direita, ponto no fim, ponto no inicio, mantissa gigante com expoente compensando.
    Tipo = Rng.randrange(4)
    Digitos = str(Rng.randrange(1, 10 ** Rng.randint(1, 17)))
    if Tipo == 0:
        return Sinal(Rng) + "0" * Rng.randint(1, 30) + Digitos + "." + "0" * Rng.randint(0, 30) + "e" + str(Rng.randint(-30, 30))
    if Tipo == 1:
        return Sinal(Rng) + "." + "0" * Rng.randint(0, 400) + Digitos + "e" + str(Rng.randint(0, 100))
    if Tipo == 2:
        Zeros = Rng.randint(50, 1200)
        return Sinal(Rng) + Digitos + "0" * Zeros + "e-" + str(Zeros + Rng.randint(-5, 5))
    return Sinal(Rng) + Digitos + "." + "".join(Rng.choice("0123456789") for _ in range(Rng.randint(1, 1000))) + "e" + str(Rng.randint(-1200, 100))


CategoriasPesos = (
    ("curto", CasoCurto, 13), ("repr", CasoRepr, 27), ("longo", CasoLongo, 10), ("exato", CasoExatoDeDouble, 10), ("meio", CasoMeioPonto, 30), ("escrita", CasoEscrita, 10),
)


def GerarCasos(N, Semente):
    # Lista de (texto, esperado, categoria) com N itens: primeiro os especiais e os invalidos, depois o sorteio pelas categorias.
    Rng = random.Random(Semente)
    Casos = [(Texto, EsperadoPython(Texto), "especial") for Texto in Especiais]
    Casos += [(Texto, "I", "invalido") for Texto in Invalidos] + [(VazioEsperado, "E", "invalido")]
    Pesos = [Peso for _, _, Peso in CategoriasPesos]
    while len(Casos) < N:
        Nome, Funcao, _ = Rng.choices(CategoriasPesos, weights=Pesos)[0]
        Texto = Funcao(Rng)
        Casos.append((Texto, EsperadoPython(Texto), Nome))
    return Casos[:N]


def Divergencias(Casos, Obtido, Fonte):
    # Lista de (indice, texto, esperado, obtido) onde Obtido (linhas) difere do esperado; faltar linha tambem e divergencia.
    Lista = []
    for Indice, (Texto, Esperado, Categoria) in enumerate(Casos):
        if Categoria == "invalido" and Fonte == "c":
            continue
        Linha = Obtido[Indice] if Indice < len(Obtido) else "(sem linha)"
        if Linha != Esperado:
            Lista.append((Indice, Texto, Esperado, Linha))
    return Lista


def Resumo(Texto):
    return Texto if len(Texto) <= 120 else Texto[:60] + "..." + Texto[-40:] + " (%d chars)" % len(Texto)


def Relatar(Nome, Lista, Total):
    print("%s: %d divergencias em %d textos" % (Nome, len(Lista), Total))
    for Indice, Texto, Esperado, Obtido in Lista[:MostrarDivergencias]:
        print("  #%d %s  esperado=%s obtido=%s" % (Indice, Resumo(Texto), Esperado, Obtido))


def Rodar(Comando, Timeout=TimeoutSegundos):
    return subprocess.run(Comando, capture_output=True, text=True, timeout=Timeout)


def LerLinhas(Caminho):
    return Caminho.read_text(encoding="ascii").split("\n")[:-1]


def CompilarForja(Forja, Pasta):
    Fonte = Pasta / "parse_float_prova.fj"
    Fonte.write_text(ProgramaForja, encoding="utf-8", newline="\n")
    Exe = Pasta / "parse_float_prova.exe"
    Resultado = Rodar([str(Forja), "build", "--release", str(Fonte), "-o", str(Exe)])
    if Resultado.returncode != 0 or not Exe.exists():
        raise RuntimeError("forja build falhou: " + (Resultado.stdout + Resultado.stderr)[:600])
    return Exe


def CompilarC(Pasta):
    Gcc = shutil.which("gcc")
    if Gcc is None:
        return None
    Fonte = Pasta / "strtod_ref.c"
    Fonte.write_text(ProgramaC, encoding="ascii", newline="\n")
    Exe = Pasta / "strtod_ref.exe"
    Resultado = Rodar([Gcc, "-O2", "-o", str(Exe), str(Fonte)])
    return Exe if Resultado.returncode == 0 else None


def RodarLeitor(Exe, Entrada, Pasta, Nome):
    Saida = Pasta / (Nome + ".saida.txt")
    Inicio = time.time()
    Resultado = Rodar([str(Exe), str(Entrada), str(Saida)])
    Segundos = time.time() - Inicio
    if Resultado.returncode != 0:
        raise RuntimeError("%s saiu com %d: %s" % (Nome, Resultado.returncode, (Resultado.stdout + Resultado.stderr)[:400]))
    return LerLinhas(Saida), Segundos


def Verificar(Forja, N, Semente, ComC, Manter):
    Pasta = Path(tempfile.mkdtemp(prefix="parse_float_"))
    try:
        Inicio = time.time()
        Casos = GerarCasos(N, Semente)
        Entrada = Pasta / "entrada.txt"
        Entrada.write_text("\n".join(Texto for Texto, _, _ in Casos) + "\n", encoding="ascii", newline="\n")
        print("corpus: %d textos (semente %d) gerados em %.1f s" % (len(Casos), Semente, time.time() - Inicio))
        Obtido, Segundos = RodarLeitor(CompilarForja(Forja, Pasta), Entrada, Pasta, "forja")
        print("forja (release x64): %d linhas em %.1f s" % (len(Obtido), Segundos))
        ListaForja = Divergencias(Casos, Obtido, "forja")
        Relatar("forja x python float()", ListaForja, len(Casos))
        if ComC:
            ExeC = CompilarC(Pasta)
            if ExeC is None:
                print("strtod do C: sem gcc no PATH, nao conferido")
            else:
                ObtidoC, SegundosC = RodarLeitor(ExeC, Entrada, Pasta, "c")
                Relatar("strtod do C x python float()", Divergencias(Casos, ObtidoC, "c"), len(Casos))
        return 0 if not ListaForja else 1
    finally:
        if Manter:
            print("arquivos mantidos em", Pasta)
        else:
            shutil.rmtree(Pasta, ignore_errors=True)


def Selftest():
    # (a) determinismo e tamanho; (b) todas as categorias aparecem; (c) esperados conhecidos do Python; (d) a comparacao acusa diferenca plantada; (e) montagem do meio-ponto.
    A = GerarCasos(3000, 7)
    assert A == GerarCasos(3000, 7) and len(A) == 3000 and A != GerarCasos(3000, 8), "gerador nao e determinista"
    Nomes = {Categoria for _, _, Categoria in A}
    assert Nomes == {"especial", "invalido", "curto", "repr", "longo", "exato", "meio", "escrita"}, Nomes
    Mapa = {Texto: Esperado for Texto, Esperado, _ in A}
    assert Mapa["5e-324"] == "0000000000000001" and Mapa["1e400"] == "O" and Mapa["-0"] == "8000000000000000" and Mapa["1e-400"] == "0000000000000000"
    assert Mapa["9007199254740993"] == "4340000000000000" and Mapa["1.7976931348623157e308"] == "7fefffffffffffff"
    assert Mapa["1.7976931348623159e308"] == "O" and Mapa["2.2250738585072011e-308"] == "000fffffffffffff"
    Digitos, Exp10 = PontoMedio(0x3FF0000000000000)
    assert float(Digitos + "e" + str(Exp10)) == 1.0 and Digitos.startswith("1000000000000000111022302462515654"), "meio-ponto de 1.0 e o seguinte"
    assert float(DigitosExatos(0.1)[0] + "e" + str(DigitosExatos(0.1)[1])) == 0.1
    Casos = [("a", "1", "especial"), ("b", "2", "especial"), ("c", "I", "invalido")]
    Lista = Divergencias(Casos, ["1", "3", "x"], "forja")
    assert [Item[0] for Item in Lista] == [1, 2], Lista
    assert [Item[0] for Item in Divergencias(Casos, ["1", "3", "x"], "c")] == [1], "invalidos nao entram na comparacao com o C"
    assert [Item[0] for Item in Divergencias(Casos, ["1"], "forja")] == [1, 2], "linha faltando e divergencia"
    getcontext().prec = 50
    assert Decimal("1e-400") == Decimal("1e-400")
    print("selftest ok: gerador determinista, categorias, esperados do Python, divergencia plantada acusada")
    return 0


def Principal():
    Parser = argparse.ArgumentParser(description="parse_float x float() do Python e strtod do C")
    Parser.add_argument("--forja", default=str(ForjaPadrao))
    Parser.add_argument("--n", type=int, default=None)
    Parser.add_argument("--curto", action="store_true", help="20 mil textos (a versao da bateria)")
    Parser.add_argument("--semente", type=int, default=SementePadrao)
    Parser.add_argument("--sem-c", action="store_true", help="nao conferir o strtod do C")
    Parser.add_argument("--manter", action="store_true", help="nao apagar a pasta temporaria")
    Parser.add_argument("--selftest", action="store_true")
    Args = Parser.parse_args()
    if Args.selftest:
        return Selftest()
    N = Args.n if Args.n is not None else (NCurto if Args.curto else NPadrao)
    return Verificar(Path(Args.forja), N, Args.semente, not Args.sem_c, Args.manter)


if __name__ == "__main__":
    sys.exit(Principal())
