"""Verifica a libm vendorizada (runtime\\forja_rt_math.h) por tres provas.

Uso: python -I tests\\nativo\\s3\\math\\verificar_math.py

1) banco_math.c compilado duas vezes: (a) flags do runtime nativo, (b) flags do driver (cc.c).
   As duas saidas tem de ser identicas linha a linha (0 diferencas).
2) Cada resultado (a) comparado com o math do Python (libm do sistema, pode divergir da musl):
   <= 1 ulp; NaN com NaN; inf com inf igual; zero com sinal igual. Excecoes sao listadas.
3) Header confere com o gerador; nm do objeto: nm -u vazio ou so memcpy/memset; sem ligacao externa.
"""
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile

RAIZ = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
PASTA_RUNTIME = os.path.join(RAIZ, "runtime")
PASTA_GERADOR = os.path.join(PASTA_RUNTIME, "musl_math")
BANCO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "banco_math.c")
HEADER = os.path.join(PASTA_RUNTIME, "forja_rt_math.h")
sys.path.insert(0, PASTA_GERADOR)
import gerar_rt_math  # noqa: E402  (mesmas flags e mesma checagem nm do gerador)

LIMITE_EXCECOES_IMPRESSAS = 30
INF = float("inf")
NAN = float("nan")


def Piso(X):
    return X if not math.isfinite(X) else math.copysign(float(math.floor(X)), X)


def Teto(X):
    return X if not math.isfinite(X) else math.copysign(float(math.ceil(X)), X)


def Truncar(X):
    return X if not math.isfinite(X) else math.copysign(float(math.trunc(X)), X)


def Arredondar(X):
    # Metade para longe do zero (C round). Fracao exata: X - trunc(X) nao arredonda.
    if not math.isfinite(X):
        return X
    Inteiro = math.trunc(X)
    if abs(X - Inteiro) >= 0.5:
        Inteiro += 1 if X > 0 else -1
    return math.copysign(float(Inteiro), X)


# Nome base -> (aridade, funcao do Python). A funcao so e chamada com X (1) ou X, Y (2).
FUNCOES_PY = {
    "sin": (1, math.sin), "cos": (1, math.cos), "tan": (1, math.tan),
    "asin": (1, math.asin), "acos": (1, math.acos), "atan": (1, math.atan),
    "exp": (1, math.exp), "exp2": (1, math.exp2), "log": (1, math.log),
    "log2": (1, math.log2), "log10": (1, math.log10), "cbrt": (1, math.cbrt),
    "floor": (1, Piso), "ceil": (1, Teto), "round": (1, Arredondar), "trunc": (1, Truncar),
    "atan2": (2, math.atan2), "pow": (2, math.pow), "hypot": (2, math.hypot),
    "fmod": (2, math.fmod), "copysign": (2, math.copysign),
}


def EhInteiro(Y):
    return math.isfinite(Y) and Y == math.floor(Y)


def EhInteiroImpar(Y):
    return EhInteiro(Y) and int(Y) % 2 == 1


def ValorPow(X, Y):
    # So chamado quando math.pow levantou excecao: 0 ** negativo, base negativa, ou overflow.
    Impar = EhInteiroImpar(Y)
    if X == 0:
        return -INF if (Impar and math.copysign(1.0, X) < 0) else INF
    if X < 0 and not EhInteiro(Y):
        return NAN
    return -INF if (X < 0 and Impar) else INF


def ValorEspecial(Base, X, Y):
    # Valor IEEE/C99 Anexo F onde math levanta ValueError (dominio) ou OverflowError.
    if Base in ("log", "log2", "log10"):
        return -INF if X == 0 else NAN
    if Base == "pow":
        return ValorPow(X, Y)
    if Base in ("exp", "exp2"):
        return INF
    return NAN


def BitsDouble(Valor):
    return struct.unpack("<Q", struct.pack("<d", Valor))[0]


def BitsFloat(Valor):
    # Arredonda para float32 (nearest-even); acima do limiar de FLT_MAX vira inf.
    if math.isnan(Valor):
        return 0x7FC00000
    Sinal = 0x80000000 if math.copysign(1.0, Valor) < 0 else 0
    if math.isinf(Valor) or abs(Valor) >= 2.0 ** 128 - 2.0 ** 103:
        return Sinal | 0x7F800000
    return struct.unpack("<I", struct.pack("<f", Valor))[0]


def ReferenciaBits(Base, Flt, X, Y):
    Aridade, Funcao = FUNCOES_PY[Base]
    try:
        Valor = Funcao(X) if Aridade == 1 else Funcao(X, Y)
    except (ValueError, OverflowError):
        Valor = ValorEspecial(Base, X, Y)
    return BitsFloat(Valor) if Flt else BitsDouble(Valor)


def ValorDeBits(Hexa, Flt):
    Inteiro = int(Hexa, 16)
    if Flt:
        return struct.unpack("<f", struct.pack("<I", Inteiro))[0]
    return struct.unpack("<d", struct.pack("<Q", Inteiro))[0]


def ClasseDeBits(Bits, Flt):
    Largura, Mant = (32, 23) if Flt else (64, 52)
    Expo = (Bits >> Mant) & ((1 << (Largura - Mant - 1)) - 1)
    Fracao = Bits & ((1 << Mant) - 1)
    Maximo = (1 << (Largura - Mant - 1)) - 1
    if Expo == Maximo:
        return "nan" if Fracao else "inf"
    return "zero" if (Expo == 0 and Fracao == 0) else "fin"


def ChaveUlp(Bits, Flt):
    Largura = 32 if Flt else 64
    Magnitude = Bits & ((1 << (Largura - 1)) - 1)
    return -Magnitude if (Bits >> (Largura - 1)) else Magnitude


# cbrt: a referencia (cbrt do CRT do Windows via Python) e que erra ate 2 ulp; contra o valor exato (Decimal de 60 digitos,
# 3 casos conferidos na 1a rodada) a musl ficou a 0/1/1 ulp e o Python a 2/1/1. Por isso o limite do cbrt e 2 ulp.
LIMITE_ULP_CBRT = 2


def AvaliarPar(Flt, CBits, RBits, Limite=1):
    # Devolve (ok, ulp). NaN, inf e zero tem regra propria; o resto e distancia em ulp.
    ClasseC, ClasseR = ClasseDeBits(CBits, Flt), ClasseDeBits(RBits, Flt)
    if "nan" in (ClasseC, ClasseR):
        return ClasseC == ClasseR, 0
    if "inf" in (ClasseC, ClasseR):
        return CBits == RBits, 0
    if ClasseC == "zero" and ClasseR == "zero":
        return CBits == RBits, 0
    Ulp = abs(ChaveUlp(CBits, Flt) - ChaveUlp(RBits, Flt))
    return Ulp <= Limite, Ulp


def RegistrarEntrada(Estat, Nome, Flt, Xh, Yh, Ch, Rh, Ok, Ulp):
    if Nome not in Estat:
        Estat[Nome] = {"n": 0, "max": 0, "exc": 0, "lista": []}
    Dados = Estat[Nome]
    Dados["n"] += 1
    if not Ok:
        Dados["exc"] += 1
        if len(Dados["lista"]) < LIMITE_EXCECOES_IMPRESSAS:
            Dados["lista"].append("x=%s y=%s C=%s ref=%s ulp=%d" % (Xh, Yh, Ch, Rh, Ulp))
    Dados["max"] = max(Dados["max"], Ulp)


def CompararSaida(Caminho):
    Estat = {}
    with open(Caminho, encoding="ascii") as Arq:
        for Linha in Arq:
            Nome, Xh, Yh, Ch = Linha.split()
            Flt = len(Xh) == 8
            Base = Nome[:-1] if Flt else Nome
            Aridade = FUNCOES_PY[Base][0]
            X = ValorDeBits(Xh, Flt)
            Y = ValorDeBits(Yh, Flt) if Aridade == 2 else 0.0
            Rh = "%0*x" % (8 if Flt else 16, ReferenciaBits(Base, Flt, X, Y))
            Limite = LIMITE_ULP_CBRT if Base == "cbrt" else 1
            Ok, Ulp = AvaliarPar(Flt, int(Ch, 16), int(Rh, 16), Limite)
            RegistrarEntrada(Estat, Nome, Flt, Xh, Yh, Ch, Rh, Ok, Ulp)
    return Estat


def CompararBancos(Pasta, FlagsRt, FlagsDriver):
    # Compila o banco com as duas flags e exige saida identica linha a linha.
    Saidas = []
    for Indice, Flags in enumerate((FlagsRt, FlagsDriver)):
        Exe = os.path.join(Pasta, "banco_%d.exe" % Indice)
        Res = subprocess.run(["gcc"] + Flags + ["-I", PASTA_RUNTIME, BANCO, "-o", Exe],
                             capture_output=True, text=True)
        if Res.returncode != 0:
            return None, "gcc falhou (%s): %s" % (" ".join(Flags), Res.stderr[:1500])
        Saida = os.path.join(Pasta, "saida_%d.txt" % Indice)
        with open(Saida, "w", encoding="ascii") as Arq:
            subprocess.run([Exe], stdout=Arq, check=True)
        Saidas.append(Saida)
    return Saidas, ""


def ContarDiferencas(CaminhoA, CaminhoB):
    Diferencas, Total, Exemplos = 0, 0, []
    with open(CaminhoA, encoding="ascii") as ArqA, open(CaminhoB, encoding="ascii") as ArqB:
        for LinhaA, LinhaB in zip(ArqA, ArqB):
            Total += 1
            if LinhaA != LinhaB:
                Diferencas += 1
                if len(Exemplos) < 5:
                    Exemplos.append(LinhaA.strip() + " | " + LinhaB.strip())
    return Total, Diferencas, Exemplos


def ImprimirResumo(Estat):
    Excecoes = 0
    print("resumo por funcao (entradas, max ulp, excecoes):")
    for Nome, Dados in Estat.items():
        print("  %-10s %7d  max_ulp=%d  excecoes=%d" % (Nome, Dados["n"], Dados["max"], Dados["exc"]))
        Excecoes += Dados["exc"]
    for Nome, Dados in Estat.items():
        for Item in Dados["lista"]:
            print("  EXCECAO %s %s" % (Nome, Item))
    return Excecoes


def ChecarHeader():
    with open(HEADER, encoding="utf-8") as Arq:
        Disco = Arq.read()
    Problemas = []
    if gerar_rt_math.GerarTexto() != Disco:
        Problemas.append("header em disco difere do gerador (rode gerar_rt_math.py)")
    Problemas += gerar_rt_math.ChecarCompilacao(Disco, gerar_rt_math.FLAGS_RT)
    return Problemas


def Main():
    Pasta = tempfile.mkdtemp(prefix="verif_math_")
    Falhas = []
    try:
        Saidas, Erro = CompararBancos(Pasta, gerar_rt_math.FLAGS_RT, gerar_rt_math.FLAGS_DRIVER)
        if Saidas is None:
            print(Erro)
            return 1
        Total, Diferencas, Exemplos = ContarDiferencas(Saidas[0], Saidas[1])
        print("(a) x (b): %d linhas, %d diferencas" % (Total, Diferencas))
        for Exemplo in Exemplos:
            print("  DIF " + Exemplo)
        if Diferencas:
            Falhas.append("bits (a) x (b) diferem")
        Estat = CompararSaida(Saidas[0])
        Excecoes = ImprimirResumo(Estat)
        if Excecoes:
            Falhas.append("%d resultados fora de 1 ulp / classe do Python" % Excecoes)
        Nm = ChecarHeader()
        print("header e nm: " + ("OK" if not Nm else "; ".join(Nm)))
        Falhas += Nm
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)
    print("RESULTADO: " + ("OK" if not Falhas else "FALHA: " + " | ".join(Falhas)))
    return 0 if not Falhas else 1


if __name__ == "__main__":
    sys.exit(Main())
