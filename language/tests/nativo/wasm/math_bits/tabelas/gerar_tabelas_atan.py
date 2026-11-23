"""Gera as tabelas de bits do grupo atan (asin, acos, atan2) da libm do musl 1.2.6 para runtime\\wasm\\rt_wasm_atan.fj.

Uso:
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\tabelas\\gerar_tabelas_atan.py            (imprime o texto Forja das tabelas)
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\tabelas\\gerar_tabelas_atan.py --selftest  (confere os bits com os 0x dos comentarios do C)
Entrada: os decimais e os 0x dos fontes third\\musl_math\\{atan,asin,acos,atan2}[f].c (copiados aqui, sem editar o C).
f64: float() do Python arredonda certo. f32: o decimal vai para o Fraction exato e o vizinho mais proximo do float32 vence
(o gcc arredonda o literal float direto do decimal, nao passa por double).
Versao: 0.1.0
"""
import struct
import sys
from decimal import Decimal
from fractions import Fraction

# (decimal do C, 0x do comentario do C ou None)
ATAN_HI64 = [("4.63647609000806093515e-01", 0x3FDDAC670561BB4F), ("7.85398163397448278999e-01", 0x3FE921FB54442D18),
             ("9.82793723247329054082e-01", 0x3FEF730BD281F69B), ("1.57079632679489655800e+00", 0x3FF921FB54442D18)]
ATAN_LO64 = [("2.26987774529616870924e-17", 0x3C7A2B7F222F65E2), ("3.06161699786838301793e-17", 0x3C81A62633145C07),
             ("1.39033110312309984516e-17", 0x3C7007887AF0CBBD), ("6.12323399573676603587e-17", 0x3C91A62633145C07)]
ATAN_T64 = [("3.33333333333329318027e-01", 0x3FD555555555550D), ("-1.99999999998764832476e-01", 0xBFC999999998EBC4),
            ("1.42857142725034663711e-01", 0x3FC24924920083FF), ("-1.11111104054623557880e-01", 0xBFBC71C6FE231671),
            ("9.09088713343650656196e-02", 0x3FB745CDC54C206E), ("-7.69187620504482999495e-02", 0xBFB3B0F2AF749A6D),
            ("6.66107313738753120669e-02", 0x3FB10D66A0D03D51), ("-5.83357013379057348645e-02", 0xBFADDE2D52DEFD9A),
            ("4.97687799461593236017e-02", 0x3FA97B4B24760DEB), ("-3.65315727442169155270e-02", 0xBFA2B4442C6A6C2F),
            ("1.62858201153657823623e-02", 0x3F90AD3AE322DA11)]
# asin.c/acos.c: pS0..pS5 (R_P64) e qS1..qS4 (R_Q64)
R_P64 = [("1.66666666666666657415e-01", 0x3FC5555555555555), ("-3.25565818622400915405e-01", 0xBFD4D61203EB6F7D),
         ("2.01212532134862925881e-01", 0x3FC9C1550E884455), ("-4.00555345006794114027e-02", 0xBFA48228B5688F3B),
         ("7.91534994289814532176e-04", 0x3F49EFE07501B288), ("3.47933107596021167570e-05", 0x3F023DE10DFDF709)]
R_Q64 = [("-2.40339491173441421878e+00", 0xC0033A271C8A2D4B), ("2.02094576023350569471e+00", 0x40002AE59C598AC8),
         ("-6.88283971605453293030e-01", 0xBFE6066C1B8D0159), ("7.70381505559019352791e-02", 0x3FB3B8C5B12E9282)]
# escalares f64: pio2_hi/pio2_lo (asin, acos), pi/pi_lo (atan2); asin.c usa pio2 = 1.570796326794896558e+00 (= pio2_hi)
PIO2_HI64 = ("1.57079632679489655800e+00", 0x3FF921FB54442D18)
PIO2_LO64 = ("6.12323399573676603587e-17", 0x3C91A62633145C07)
PI64 = ("3.1415926535897931160E+00", 0x400921FB54442D18)
PI_LO64 = ("1.2246467991473531772E-16", 0x3CA1A62633145C07)

# f32: atanf.c, asinf.c, acosf.c, atan2f.c (0x dos comentarios so onde o C traz)
ATAN_HI32 = [("4.6364760399e-01", 0x3EED6338), ("7.8539812565e-01", 0x3F490FDA), ("9.8279368877e-01", 0x3F7B985E),
             ("1.5707962513e+00", 0x3FC90FDA)]
ATAN_LO32 = [("5.0121582440e-09", 0x31AC3769), ("3.7748947079e-08", 0x33222168), ("3.4473217170e-08", 0x33140FB4),
             ("7.5497894159e-08", 0x33A22168)]
ATAN_T32 = [("3.3333328366e-01", None), ("-1.9999158382e-01", None), ("1.4253635705e-01", None),
            ("-1.0648017377e-01", None), ("6.1687607318e-02", None)]
# asinf.c e acosf.c usam os mesmos pS0..pS2 e qS1 (conferido no C)
R_P32 = [("1.6666586697e-01", None), ("-4.2743422091e-02", None), ("-8.6563630030e-03", None)]
R_Q32 = [("-7.0662963390e-01", None)]
PIO2_HI32 = ("1.5707962513e+00", 0x3FC90FDA)  # acosf.c
PIO2_LO32 = ("7.5497894159e-08", 0x33A22168)  # acosf.c
PI32 = ("3.1415927410e+00", 0x40490FDB)  # atan2f.c
PI_LO32 = ("-8.7422776573e-08", 0xB3BBBD2E)  # atan2f.c


def BitsF64(Texto):
    """Bits do double mais proximo do decimal (float() do Python e correto)."""
    return struct.unpack("<Q", struct.pack("<d", float(Texto)))[0]


def ValorF32(Bits):
    return struct.unpack("<f", struct.pack("<I", Bits))[0]


def BitsF32(Texto):
    """Bits do float32 mais proximo do decimal (Fraction exato; gcc arredonda literal float direto do decimal)."""
    Alvo = Fraction(Decimal(Texto))
    Base = struct.unpack("<I", struct.pack("<f", float(Texto)))[0]
    Melhor = None
    for Bits in range(Base - 2, Base + 3):
        Erro = abs(Fraction(ValorF32(Bits)) - Alvo)
        if Melhor is None or Erro < Melhor[0]:
            Melhor = (Erro, Bits)
    return Melhor[1]


def BitsDe64(Par):
    Texto, Hex = Par
    Bits = BitsF64(Texto)
    if Hex is not None and Bits != Hex:
        raise SystemExit(f"ERRO f64 {Texto}: decimal da {Bits:016X} != comentario {Hex:016X}")
    return Bits


def BitsDe32(Par):
    Texto, Hex = Par
    Bits = BitsF32(Texto)
    if Hex is not None and Bits != Hex:
        raise SystemExit(f"ERRO f32 {Texto}: decimal da {Bits:08X} != comentario {Hex:08X}")
    return Bits


def FormatarTabela(Nome, Tipo, Bits):
    """Texto `const NOME: [N]Tipo = {...}`, 4 valores por linha."""
    Largura = 16 if Tipo == "u64" else 8
    Linhas = []
    for Inicio in range(0, len(Bits), 4):
        Fatia = Bits[Inicio:Inicio + 4]
        Linhas.append("    " + ", ".join(f"0x{B:0{Largura}X}" for B in Fatia) + ",")
    return f"const {Nome}: [{len(Bits)}]{Tipo} = {{\n" + "\n".join(Linhas) + "\n}\n"


def FormatarEscalar(Nome, Tipo, Bits):
    Largura = 16 if Tipo == "u64" else 8
    return f"const {Nome}: {Tipo} = 0x{Bits:0{Largura}X}\n"


def MontarTabelas():
    """Lista de (nome, tipo, bits) na ordem do modulo; escalares de 1 valor saem como const simples."""
    Tabelas64 = [("ATAN_HI64", [BitsDe64(P) for P in ATAN_HI64]), ("ATAN_LO64", [BitsDe64(P) for P in ATAN_LO64]),
                 ("ATAN_T64", [BitsDe64(P) for P in ATAN_T64]), ("ATAN_R_P64", [BitsDe64(P) for P in R_P64]),
                 ("ATAN_R_Q64", [BitsDe64(P) for P in R_Q64])]
    Tabelas32 = [("ATAN_HI32", [BitsDe32(P) for P in ATAN_HI32]), ("ATAN_LO32", [BitsDe32(P) for P in ATAN_LO32]),
                 ("ATAN_T32", [BitsDe32(P) for P in ATAN_T32]), ("ATAN_R_P32", [BitsDe32(P) for P in R_P32]),
                 ("ATAN_R_Q32", [BitsDe32(P) for P in R_Q32])]
    Escalares64 = [("ATAN_PIO2_HI64", BitsDe64(PIO2_HI64)), ("ATAN_PIO2_LO64", BitsDe64(PIO2_LO64)),
                   ("ATAN_PI64", BitsDe64(PI64)), ("ATAN_PI_LO64", BitsDe64(PI_LO64))]
    Escalares32 = [("ATAN_PIO2_HI32", BitsDe32(PIO2_HI32)), ("ATAN_PIO2_LO32", BitsDe32(PIO2_LO32)),
                   ("ATAN_PI32", BitsDe32(PI32)), ("ATAN_PI_LO32", BitsDe32(PI_LO32))]
    return Tabelas64, Tabelas32, Escalares64, Escalares32


def Selftest():
    """Confere que os decimais batem com os 0x dos comentarios e que 2^-120 sai como o tiny do C."""
    MontarTabelas()
    assert struct.unpack("<Q", struct.pack("<d", 2.0 ** -120))[0] == 0x3870000000000000
    assert struct.unpack("<I", struct.pack("<f", 2.0 ** -120))[0] == 0x03800000
    print("selftest OK: decimais batem com os 0x dos comentarios do C; tiny f64 0x3870000000000000, f32 0x03800000")


def Principal():
    if "--selftest" in sys.argv:
        Selftest()
        return 0
    Tab64, Tab32, Esc64, Esc32 = MontarTabelas()
    Saida = ["// ---- tabelas do atan (gerar_tabelas_atan.py 0.1.0; bits do musl 1.2.6) ----"]
    Saida += [FormatarTabela(N, "u64", B) for N, B in Tab64]
    Saida += [FormatarTabela(N, "u32", B) for N, B in Tab32]
    Saida += [FormatarEscalar(N, "u64", B) for N, B in Esc64]
    Saida += [FormatarEscalar(N, "u32", B) for N, B in Esc32]
    print("\n".join(Saida))
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
