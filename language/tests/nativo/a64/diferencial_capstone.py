#!/usr/bin/env python3
"""Diferencial do encoder A64 (contrato_a64.txt §5 A1.1): encoder Forja x capstone 5.0.7.

Versao: 0.1.0
Changelog: 0.1.0 - A1.1: roda tests\\nativo\\a64\\gerar_vetores.fj (forja run); para cada vetor monta o texto ESPERADO
           pelo proprio Python a partir dos campos (layout de campos independente do encoder, §2.4) e compara com o
           desmonte do capstone (CS_ARCH_ARM64) depois de normalizar: aliases fixados (mov/mvn/cmp/cmn/tst/neg/cset/
           cinc/csetm/cinv/cneg/lsl/lsr/asr/ubfx/ubfiz/sbfx/sbfiz/bfi/bfxil/uxt*/sxt*/mul/mneg/ret) voltam ao canonico;
           imediato em inteiro (decimal ou hex, com sinal de 64 bits) e real em float. mov imm (MOVZ/MOVN/ORR imm)
           compara o VALOR. M (mascara logica), W (mov imm) e Q (fmov imm8) tem checagem semantica propria.
           --mutantes planta 3 bits trocados (ADD imm, LDR escalado, BL) e exige que todos sejam pegos.
           --sincronizar refaz a copia do encoder dentro do gerador (bloco entre os marcadores).
Verificacao: python -I tests\\nativo\\a64\\diferencial_capstone.py ; python -I ... --mutantes
Depende de: capstone 5.0.7 (pip).
"""

import argparse
import re
import site
import struct
import subprocess
import sys
from pathlib import Path

# python -I nao inclui o site do usuario, onde o capstone foi instalado (pip --user): reinclui-o aqui
sys.path.append(site.getusersitepackages())

from capstone import CS_ARCH_ARM64, CS_MODE_ARM, Cs  # noqa: E402

RAIZ = Path(__file__).resolve().parents[3]
FORJA = RAIZ / "compiler" / "forja.exe"
ENCODER = RAIZ / "forja_fj" / "nativo" / "a64" / "a64_codificar.fj"
GERADOR = RAIZ / "tests" / "nativo" / "a64" / "gerar_vetores.fj"
MARCA_TESTES = "// ==== TESTES ===="
MARCA_ENC_INI = "// ==== ENCODER (copia de nativo\\a64\\a64_codificar.fj) ===="
MARCA_GER = "// ==== GERADOR (so neste arquivo) ===="

# Layout de campos por posicao (contrato §2.4). Nomes = argumentos do vetor, na ordem do gerador.
LAYOUT_ARGS = {
    "ADDI": ["sf", "sh", "imm12", "rn", "rd"],
    "ADDR": ["sf", "sh", "rm", "imm6", "rn", "rd"],
    "ADDX": ["sf", "rm", "opt", "imm3", "rn", "rd"],
    "LOGR": ["sf", "sh", "n", "rm", "imm6", "rn", "rd"],
    "LOGI": ["sf", "n", "immr", "imms", "rn", "rd"],
    "BITF": ["sf", "immr", "imms", "rn", "rd"],
    "MOVW": ["sf", "hw", "imm16", "rd"],
    "DP3": ["sf", "rm", "rn", "rd"],
    "MADD": ["sf", "rm", "ra", "rn", "rd"],
    "MUL3": ["rm", "rn", "rd"],
    "CSEL": ["sf", "rm", "cond", "rn", "rd"],
    "B26": ["imm26"],
    "BCOND": ["imm19", "cond"],
    "CBZ": ["sf", "imm19", "rt"],
    "TBZ": ["b5", "b40", "imm14", "rt"],
    "RN": ["rn"],
    "IMM16": ["imm16"],
    "NADA": [],
    "ADR": ["immlo", "immhi", "rd"],
    "LDST": ["imm12", "rn", "rt"],
    "LDUR": ["imm9", "rn", "rt"],
    "LDP": ["imm7", "rt2", "rn", "rt"],
    "FP2": ["tipo", "rm", "rn", "rd"],
    "FP1": ["tipo", "rn", "rd"],
    "FCMP": ["tipo", "rm", "rn", "zero"],
    "RNRD": ["rn", "rd"],
    "FPINT": ["sf", "tipo", "rn", "rd"],
    "FMOVI": ["tipo", "imm8", "rd"],
    "FCSEL": ["tipo", "rm", "cond", "rn", "rd"],
}

# Formas, na mesma ordem de A64F_* do encoder: (nome, layout)
FORMAS = [
    ("ADD_IMM", "ADDI"), ("ADDS_IMM", "ADDI"), ("SUB_IMM", "ADDI"), ("SUBS_IMM", "ADDI"),
    ("ADD_REG", "ADDR"), ("ADDS_REG", "ADDR"), ("SUB_REG", "ADDR"), ("SUBS_REG", "ADDR"),
    ("ADD_EXT", "ADDX"),
    ("AND_REG", "LOGR"), ("ORR_REG", "LOGR"), ("EOR_REG", "LOGR"), ("ANDS_REG", "LOGR"),
    ("AND_IMM", "LOGI"), ("ORR_IMM", "LOGI"), ("EOR_IMM", "LOGI"), ("ANDS_IMM", "LOGI"),
    ("MOVN", "MOVW"), ("MOVZ", "MOVW"), ("MOVK", "MOVW"),
    ("SBFM", "BITF"), ("BFM", "BITF"), ("UBFM", "BITF"),
    ("LSLV", "DP3"), ("LSRV", "DP3"), ("ASRV", "DP3"), ("UDIV", "DP3"), ("SDIV", "DP3"),
    ("MADD", "MADD"), ("MSUB", "MADD"),
    ("SMULH", "MUL3"), ("UMULH", "MUL3"), ("SMULL", "MUL3"), ("UMULL", "MUL3"),
    ("CSEL", "CSEL"), ("CSINC", "CSEL"), ("CSINV", "CSEL"), ("CSNEG", "CSEL"),
    ("B", "B26"), ("BL", "B26"), ("B_COND", "BCOND"),
    ("CBZ", "CBZ"), ("CBNZ", "CBZ"), ("TBZ", "TBZ"), ("TBNZ", "TBZ"),
    ("BR", "RN"), ("BLR", "RN"), ("RET", "RN"),
    ("SVC", "IMM16"), ("BRK", "IMM16"), ("NOP", "NADA"), ("DMB_ISH", "NADA"),
    ("ADR", "ADR"), ("ADRP", "ADR"),
    ("STRB", "LDST"), ("LDRB", "LDST"), ("LDRSB_X", "LDST"), ("LDRSB_W", "LDST"),
    ("STRH", "LDST"), ("LDRH", "LDST"), ("LDRSH_X", "LDST"), ("LDRSH_W", "LDST"),
    ("STR_W", "LDST"), ("LDR_W", "LDST"), ("LDRSW", "LDST"), ("STR_X", "LDST"), ("LDR_X", "LDST"),
    ("STR_S", "LDST"), ("LDR_S", "LDST"), ("STR_D", "LDST"), ("LDR_D", "LDST"),
    ("STR_Q", "LDST"), ("LDR_Q", "LDST"),
    ("STURB", "LDUR"), ("LDURB", "LDUR"), ("LDURSB_X", "LDUR"), ("LDURSB_W", "LDUR"),
    ("STURH", "LDUR"), ("LDURH", "LDUR"), ("LDURSH_X", "LDUR"), ("LDURSH_W", "LDUR"),
    ("STUR_W", "LDUR"), ("LDUR_W", "LDUR"), ("LDURSW", "LDUR"), ("STUR_X", "LDUR"), ("LDUR_X", "LDUR"),
    ("STUR_S", "LDUR"), ("LDUR_S", "LDUR"), ("STUR_D", "LDUR"), ("LDUR_D", "LDUR"),
    ("STUR_Q", "LDUR"), ("LDUR_Q", "LDUR"),
    ("STP_X", "LDP"), ("LDP_X", "LDP"), ("STP_X_PRE", "LDP"), ("LDP_X_PRE", "LDP"),
    ("STP_X_POS", "LDP"), ("LDP_X_POS", "LDP"), ("STP_D", "LDP"), ("LDP_D", "LDP"), ("STP_W", "LDP"),
    ("FADD", "FP2"), ("FSUB", "FP2"), ("FMUL", "FP2"), ("FDIV", "FP2"),
    ("FSQRT", "FP1"), ("FNEG", "FP1"), ("FABS", "FP1"), ("FMOV_REG", "FP1"),
    ("FCMP", "FCMP"), ("FCVT_S_D", "RNRD"), ("FCVT_D_S", "RNRD"),
    ("SCVTF", "FPINT"), ("UCVTF", "FPINT"), ("FCVTZS", "FPINT"), ("FCVTZU", "FPINT"),
    ("FMOV_XD_DN", "RNRD"), ("FMOV_DD_XN", "RNRD"), ("FMOV_WD_SN", "RNRD"), ("FMOV_SD_WN", "RNRD"),
    ("FMOV_IMM", "FMOVI"),
    ("FRINTM", "FP1"), ("FRINTP", "FP1"), ("FRINTZ", "FP1"), ("FRINTA", "FP1"),
    ("FCSEL", "FCSEL"),
]

# Load/store escalado: nome da forma -> (mnemonico, bytes do acesso, classe do registrador)
LDST_INFO = {
    "STRB": ("strb", 1, "w"), "LDRB": ("ldrb", 1, "w"), "LDRSB_X": ("ldrsb", 1, "x"), "LDRSB_W": ("ldrsb", 1, "w"),
    "STRH": ("strh", 2, "w"), "LDRH": ("ldrh", 2, "w"), "LDRSH_X": ("ldrsh", 2, "x"), "LDRSH_W": ("ldrsh", 2, "w"),
    "STR_W": ("str", 4, "w"), "LDR_W": ("ldr", 4, "w"), "LDRSW": ("ldrsw", 4, "x"),
    "STR_X": ("str", 8, "x"), "LDR_X": ("ldr", 8, "x"), "STR_S": ("str", 4, "s"), "LDR_S": ("ldr", 4, "s"),
    "STR_D": ("str", 8, "d"), "LDR_D": ("ldr", 8, "d"), "STR_Q": ("str", 16, "q"), "LDR_Q": ("ldr", 16, "q"),
}
CONDS = ["eq", "ne", "hs", "lo", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le", "al", "nv"]
EXTS = ["uxtb", "uxth", "uxtw", "uxtx", "sxtb", "sxth", "sxtw", "sxtx"]
SHIFTS = ["lsl", "lsr", "asr", "ror"]
INV_LOGI = {"and": "bic", "orr": "orn", "eor": "eon", "ands": "bics"}
MASCARA64 = (1 << 64) - 1
MASCARA32 = (1 << 32) - 1
RE_INT = re.compile(r"#(-?0x[0-9a-f]+|-?[0-9]+)(?![.\w])")
RE_FLT = re.compile(r"#(-?[0-9]+\.[0-9]+)")
CS_MOTOR = Cs(CS_ARCH_ARM64, CS_MODE_ARM)


# ----------------------------------------------------------------------------------------------
# Modelo independente: imediato logico (ARM ARM DecodeBitMasks) e VFPExpandImm
# ----------------------------------------------------------------------------------------------

def DecodificaMascara(N, Immr, Imms):
    """Valor de 64 bits do imediato logico (N, immr, imms); None se o triplo for reservado."""
    Cod = (N << 6) | ((~Imms) & 63)
    if Cod == 0 or Cod.bit_length() - 1 < 1:
        return None
    Esize = 1 << (Cod.bit_length() - 1)
    Niveis = Esize - 1
    S = Imms & Niveis
    if S == Niveis:
        return None
    R = Immr & Niveis
    Elem = (1 << (S + 1)) - 1
    if R:
        Elem = ((Elem >> R) | (Elem << (Esize - R))) & ((1 << Esize) - 1)
    Valor = 0
    for Pos in range(0, 64, Esize):
        Valor |= Elem << Pos
    return Valor


def ImediatosValidos():
    """Conjuntos dos valores de 64 bits e de 32 bits representaveis por imediato logico."""
    V64, V32 = set(), set()
    for N in (0, 1):
        for Immr in range(64):
            for Imms in range(64):
                Valor = DecodificaMascara(N, Immr, Imms)
                if Valor is None:
                    continue
                V64.add(Valor)
                if N == 0:
                    V32.add(Valor & MASCARA32)
    return V64, V32


def VfpExpandir(Imm8, Dupla):
    A = (Imm8 >> 7) & 1
    B = (Imm8 >> 6) & 1
    Cd = (Imm8 >> 4) & 3
    Efgh = Imm8 & 15
    if Dupla:
        Expo = ((1 - B) << 10) | ((B * 255) << 2) | Cd
        return (A << 63) | (Expo << 52) | (Efgh << 48)
    Expo = ((1 - B) << 7) | ((B * 31) << 2) | Cd
    return (A << 31) | (Expo << 23) | (Efgh << 19)


def ComSinal(Valor, Bits):
    Valor &= (1 << Bits) - 1
    return Valor - (1 << Bits) if Valor >> (Bits - 1) else Valor


VALIDOS64, VALIDOS32 = ImediatosValidos()
VFP_TODOS = {
    True: {VfpExpandir(Ind, True) for Ind in range(256)},
    False: {VfpExpandir(Ind, False) for Ind in range(256)},
}


# ----------------------------------------------------------------------------------------------
# Texto canonico: normalizacao comum ao esperado (Python) e ao desmonte (capstone)
# ----------------------------------------------------------------------------------------------

def NormInt(Tok):
    Neg = Tok.startswith("-")
    Valor = int(Tok, 16) if "0x" in Tok else int(Tok)
    if Neg:
        Valor = -abs(Valor)
    elif (1 << 63) <= Valor <= MASCARA64:
        Valor -= 1 << 64
    return f"#{Valor}"


def NormOp(Op):
    Op = re.sub(r"\s+", " ", Op).strip()
    Op = RE_FLT.sub(lambda m: f"#{float(m.group(1))!r}", Op)
    Op = RE_INT.sub(lambda m: NormInt(m.group(1)), Op)
    return re.sub(r"\[([^\]]*?), #0\]", r"[\1]", Op)


def DividaOps(Texto):
    Partes, Atual, Prof = [], "", 0
    for Ch in Texto:
        if Ch == "[":
            Prof += 1
        elif Ch == "]":
            Prof -= 1
        if Ch == "," and Prof == 0:
            Partes.append(Atual.strip())
            Atual = ""
        else:
            Atual += Ch
    if Atual.strip():
        Partes.append(Atual.strip())
    return Partes


def Norm(Texto):
    Texto = Texto.strip().lower()
    Mnem, _, Resto = Texto.partition(" ")
    return Mnem, [NormOp(Op) for Op in DividaOps(Resto)]


def RegInfo(Nome):
    Casa = re.fullmatch(r"([xw])(\d+)", Nome)
    if Casa:
        return Casa.group(1), int(Casa.group(2))
    if Nome in ("sp", "wsp"):
        return ("x" if Nome == "sp" else "w"), "sp"
    if Nome in ("xzr", "wzr"):
        return ("x" if Nome == "xzr" else "w"), "zr"
    return None


def LarguraDe(Ops):
    for Op in Ops:
        Info = RegInfo(Op)
        if Info:
            return Info[0]
    return "x"


def Zr(W):
    return W + "zr"


def Inverte(Cond):
    return CONDS[CONDS.index(Cond) ^ 1]


def Expande(Mnem, Ops):
    """Aliases do capstone -> forma canonica (so os fixados em tabela)."""
    W = LarguraDe(Ops)
    Bits = 64 if W == "x" else 32
    if Mnem == "mov" and len(Ops) == 2 and RegInfo(Ops[0]) and RegInfo(Ops[1]):
        if "sp" in Ops[0] or "sp" in Ops[1] or "wsp" in Ops:
            return "add", [Ops[0], Ops[1], "#0"]
        return "orr", [Ops[0], Zr(W), Ops[1]]
    if Mnem == "mvn":
        return "orn", [Ops[0], Zr(W)] + Ops[1:]
    if Mnem in ("cmp", "cmn", "tst"):
        Trad = {"cmp": "subs", "cmn": "adds", "tst": "ands"}[Mnem]
        return Trad, [Zr(LarguraDe(Ops[:1])), Ops[0]] + Ops[1:]
    if Mnem in ("neg", "negs"):
        return ("sub" if Mnem == "neg" else "subs"), [Ops[0], Zr(W)] + Ops[1:]
    if Mnem in ("cset", "csetm"):
        Trad = "csinc" if Mnem == "cset" else "csinv"
        return Trad, [Ops[0], Zr(W), Zr(W), Inverte(Ops[1])]
    if Mnem in ("cinc", "cinv", "cneg"):
        Trad = {"cinc": "csinc", "cinv": "csinv", "cneg": "csneg"}[Mnem]
        return Trad, [Ops[0], Ops[1], Ops[1], Inverte(Ops[2])]
    if Mnem in ("lsl", "lsr", "asr") and len(Ops) == 3:
        if RegInfo(Ops[2]):
            return {"lsl": "lslv", "lsr": "lsrv", "asr": "asrv"}[Mnem], Ops
        Sh = int(Ops[2][1:])
        if Mnem == "lsl":
            return "ubfm", [Ops[0], Ops[1], f"#{(-Sh) % Bits}", f"#{Bits - 1 - Sh}"]
        return ("ubfm" if Mnem == "lsr" else "sbfm"), [Ops[0], Ops[1], f"#{Sh}", f"#{Bits - 1}"]
    if Mnem in ("ubfx", "sbfx", "bfxil"):
        Lsb, Larg = int(Ops[2][1:]), int(Ops[3][1:])
        Trad = {"ubfx": "ubfm", "sbfx": "sbfm", "bfxil": "bfm"}[Mnem]
        return Trad, [Ops[0], Ops[1], f"#{Lsb}", f"#{Lsb + Larg - 1}"]
    if Mnem in ("ubfiz", "sbfiz", "bfi"):
        Lsb, Larg = int(Ops[2][1:]), int(Ops[3][1:])
        Trad = {"ubfiz": "ubfm", "sbfiz": "sbfm", "bfi": "bfm"}[Mnem]
        return Trad, [Ops[0], Ops[1], f"#{(-Lsb) % Bits}", f"#{Larg - 1}"]
    if Mnem in ("uxtb", "uxth", "sxtb", "sxth", "sxtw"):
        Fim = {"uxtb": 7, "uxth": 15, "sxtb": 7, "sxth": 15, "sxtw": 31}[Mnem]
        Trad = "ubfm" if Mnem.startswith("u") else "sbfm"
        Rn = Ops[1]
        if RegInfo(Rn) and RegInfo(Ops[0])[0] == "x":
            Rn = "x" + Rn[1:]
        return Trad, [Ops[0], Rn, "#0", f"#{Fim}"]
    if Mnem == "mul" and len(Ops) == 3:
        return "madd", Ops + [Zr(W)]
    if Mnem == "mneg" and len(Ops) == 3:
        return "msub", Ops + [Zr(W)]
    if Mnem == "ret" and not Ops:
        return "ret", ["x30"]
    if Mnem == "bfc" and len(Ops) == 3:
        Lsb, Larg = int(Ops[1][1:]), int(Ops[2][1:])
        return "bfm", [Ops[0], Zr(W), f"#{(-Lsb) % Bits}", f"#{Larg - 1}"]
    return Mnem, Ops


def Desmonta(Palavra):
    Insns = list(CS_MOTOR.disasm(Palavra.to_bytes(4, "little"), 0))
    if len(Insns) != 1:
        return None
    return f"{Insns[0].mnemonic} {Insns[0].op_str}".strip()


def Compara(TextoEsperado, ValorEsperado, Sf, Palavra, Forma):
    """(ok, desmontado): o desmonte da palavra bate com o texto esperado depois de normalizar."""
    Desm = Desmonta(Palavra)
    if Desm is None:
        return False, "capstone recusou a palavra"
    MnemA, OpsA = Norm(Desm)
    MnemE, OpsE = Norm(TextoEsperado)
    if ValorEsperado is not None and MnemA == "mov" and len(OpsA) == 2 and OpsA[1].startswith("#"):
        Mascara = MASCARA64 if Sf else MASCARA32
        ValorA = int(OpsA[1][1:]) & Mascara
        return OpsA[0] == OpsE[0] and ValorA == (ValorEsperado & Mascara), Desm
    if Forma == "ADD_EXT" and MnemA in ("add", "adds", "sub", "subs") and len(OpsA) == 4 and OpsA[3].startswith("lsl"):
        OpsA = OpsA[:3] + [OpsA[3].replace("lsl", "uxtx", 1)]
    MnemA, OpsA = Expande(MnemA, OpsA)
    return (MnemA, OpsA) == (MnemE, OpsE), Desm


# ----------------------------------------------------------------------------------------------
# Renderizacao canonica por forma (texto esperado) a partir dos campos
# ----------------------------------------------------------------------------------------------

def Reg(N, W, Sp=False):
    if W in ("x", "w"):
        if N == 31:
            if Sp:
                return "sp" if W == "x" else "wsp"
            return Zr(W)
    return f"{W}{N}"


def FpClasse(Tipo):
    return "s" if Tipo == 0 else "d"


def Sombra(Sh, Imm):
    if Sh or Imm:
        return f", {SHIFTS[Sh]} #{Imm}"
    return ""


def ClasseLdp(Nome):
    if Nome.endswith("_D"):
        return "D"
    if Nome.endswith("_W"):
        return "W"
    return "X"


def BaseLdst(Nome):
    if Nome.startswith("STUR"):
        return "STR" + Nome[4:]
    if Nome.startswith("LDUR"):
        return "LDR" + Nome[4:]
    return Nome


def RnRd(Nome, A):
    if Nome == "FCVT_S_D":
        return f"fcvt d{A['rd']}, s{A['rn']}"
    if Nome == "FCVT_D_S":
        return f"fcvt s{A['rd']}, d{A['rn']}"
    if Nome == "FMOV_XD_DN":
        return f"fmov {Reg(A['rd'], 'x')}, d{A['rn']}"
    if Nome == "FMOV_DD_XN":
        return f"fmov d{A['rd']}, {Reg(A['rn'], 'x')}"
    if Nome == "FMOV_WD_SN":
        return f"fmov {Reg(A['rd'], 'w')}, s{A['rn']}"
    return f"fmov s{A['rd']}, {Reg(A['rn'], 'w')}"


def Render(Nome, Lay, A):
    """(texto canonico esperado, valor esperado ou None). Texto None = palavra reservada (fora da classe)."""
    Sf = A.get("sf", 1)
    W = "x" if Sf else "w"
    Mn = Nome.split("_")[0].lower()
    if Lay == "ADDI":
        Txt = f"{Mn} {Reg(A['rd'], W, Mn in ('add', 'sub'))}, {Reg(A['rn'], W, True)}, #{A['imm12']}"
        return Txt + (", lsl #12" if A["sh"] else ""), None
    if Lay == "ADDR":
        return f"{Mn} {Reg(A['rd'], W)}, {Reg(A['rn'], W)}, {Reg(A['rm'], W)}" + Sombra(A["sh"], A["imm6"]), None
    if Lay == "ADDX":
        RmW = "x" if (Sf and A["opt"] in (3, 7)) else "w"
        Txt = f"{Mn} {Reg(A['rd'], W, True)}, {Reg(A['rn'], W, True)}, {Reg(A['rm'], RmW)}, {EXTS[A['opt']]}"
        return Txt + (f" #{A['imm3']}" if A["imm3"] else ""), None
    if Lay == "LOGR":
        if A["n"]:
            Mn = INV_LOGI[Mn]
        return f"{Mn} {Reg(A['rd'], W)}, {Reg(A['rn'], W)}, {Reg(A['rm'], W)}" + Sombra(A["sh"], A["imm6"]), None
    if Lay == "LOGI":
        if not Sf and A["n"]:
            return None, None
        Valor = DecodificaMascara(A["n"], A["immr"], A["imms"])
        if Valor is None:
            return None, None
        if not Sf:
            Valor &= MASCARA32
        return f"{Mn} {Reg(A['rd'], W, Mn != 'ands')}, {Reg(A['rn'], W)}, #{Valor}", Valor
    if Lay == "BITF":
        return f"{Nome.lower()} {Reg(A['rd'], W)}, {Reg(A['rn'], W)}, #{A['immr']}, #{A['imms']}", None
    if Lay == "MOVW":
        S = 16 * A["hw"]
        Valor = None
        if Nome == "MOVZ":
            Valor = A["imm16"] << S
        elif Nome == "MOVN":
            Valor = (~(A["imm16"] << S)) & (MASCARA64 if Sf else MASCARA32)
        Txt = f"{Nome.lower()} {Reg(A['rd'], W)}, #{A['imm16']}" + (f", lsl #{S}" if S else "")
        return Txt, Valor
    if Lay == "DP3":
        return f"{Nome.lower()} {Reg(A['rd'], W)}, {Reg(A['rn'], W)}, {Reg(A['rm'], W)}", None
    if Lay == "MADD":
        return f"{Nome.lower()} {Reg(A['rd'], W)}, {Reg(A['rn'], W)}, {Reg(A['rm'], W)}, {Reg(A['ra'], W)}", None
    if Lay == "MUL3":
        if Nome in ("SMULL", "UMULL"):
            return f"{Nome.lower()} {Reg(A['rd'], 'x')}, {Reg(A['rn'], 'w')}, {Reg(A['rm'], 'w')}", None
        return f"{Nome.lower()} {Reg(A['rd'], 'x')}, {Reg(A['rn'], 'x')}, {Reg(A['rm'], 'x')}", None
    if Lay == "CSEL":
        return f"{Nome.lower()} {Reg(A['rd'], W)}, {Reg(A['rn'], W)}, {Reg(A['rm'], W)}, {CONDS[A['cond']]}", None
    if Lay == "B26":
        return f"{Nome.lower()} #{ComSinal(A['imm26'], 26) * 4}", None
    if Lay == "BCOND":
        return f"b.{CONDS[A['cond']]} #{ComSinal(A['imm19'], 19) * 4}", None
    if Lay == "CBZ":
        return f"{Nome.lower().replace('_', '')} {Reg(A['rt'], W)}, #{ComSinal(A['imm19'], 19) * 4}", None
    if Lay == "TBZ":
        Bit = A["b5"] * 32 + A["b40"]
        Mn = "tbz" if Nome == "TBZ" else "tbnz"
        return f"{Mn} {Reg(A['rt'], 'x' if A['b5'] else 'w')}, #{Bit}, #{ComSinal(A['imm14'], 14) * 4}", None
    if Lay == "RN":
        return f"{Nome.lower()} {Reg(A['rn'], 'x')}", None
    if Lay == "IMM16":
        return f"{Nome.lower()} #{A['imm16']}", None
    if Lay == "NADA":
        return ("nop" if Nome == "NOP" else "dmb ish"), None
    if Lay == "ADR":
        Imm21 = ComSinal((A["immhi"] << 2) | A["immlo"], 21)
        if Nome == "ADR":
            return f"adr {Reg(A['rd'], 'x')}, #{Imm21}", Imm21
        return f"adrp {Reg(A['rd'], 'x')}, #{Imm21 << 12}", Imm21 << 12
    if Lay in ("LDST", "LDUR"):
        Base = BaseLdst(Nome)
        Mn, Tam, Rc = LDST_INFO[Base]
        if Lay == "LDUR":
            Mn = Mn.replace("ldr", "ldur", 1).replace("str", "stur", 1)
        Off = A["imm12"] * Tam if Lay == "LDST" else ComSinal(A["imm9"], 9)
        return f"{Mn} {Reg(A['rt'], Rc)}, [{Reg(A['rn'], 'x', True)}, #{Off}]", None
    if Lay == "LDP":
        # CONSTRAINED UNPREDICTABLE na ARM ARM: LDP com Rt == Rt2, ou writeback com base igual a um Rt
        if (Nome.startswith("LDP") and A["rt"] == A["rt2"]) or (
                (Nome.endswith("_PRE") or Nome.endswith("_POS")) and A["rn"] != 31 and A["rn"] in (A["rt"], A["rt2"])):
            return None, None
        Rc = {"X": "x", "D": "d", "W": "w"}[ClasseLdp(Nome)]
        Tam = {"x": 8, "d": 8, "w": 4}[Rc]
        Off = ComSinal(A["imm7"], 7) * Tam
        Mn = "ldp" if Nome.startswith("LDP") else "stp"
        Base = Reg(A["rn"], "x", True)
        if Nome.endswith("_PRE"):
            Mem = f"[{Base}, #{Off}]!"
        elif Nome.endswith("_POS"):
            Mem = f"[{Base}], #{Off}"
        else:
            Mem = f"[{Base}, #{Off}]"
        return f"{Mn} {Reg(A['rt'], Rc)}, {Reg(A['rt2'], Rc)}, {Mem}", None
    if Lay == "FP2":
        C = FpClasse(A["tipo"])
        return f"{Nome.lower()} {C}{A['rd']}, {C}{A['rn']}, {C}{A['rm']}", None
    if Lay == "FP1":
        C = FpClasse(A["tipo"])
        Mn = "fmov" if Nome == "FMOV_REG" else Nome.lower()
        return f"{Mn} {C}{A['rd']}, {C}{A['rn']}", None
    if Lay == "FCMP":
        C = FpClasse(A["tipo"])
        return f"fcmp {C}{A['rn']}, " + ("#0.0" if A["zero"] else f"{C}{A['rm']}"), None
    if Lay == "RNRD":
        return RnRd(Nome, A), None
    if Lay == "FPINT":
        C = FpClasse(A["tipo"])
        Gp = "x" if A["sf"] else "w"
        if Nome in ("SCVTF", "UCVTF"):
            return f"{Nome.lower()} {C}{A['rd']}, {Reg(A['rn'], Gp)}", None
        return f"{Nome.lower()} {Reg(A['rd'], Gp)}, {C}{A['rn']}", None
    if Lay == "FCSEL":
        C = FpClasse(A["tipo"])
        return f"fcsel {C}{A['rd']}, {C}{A['rn']}, {C}{A['rm']}, {CONDS[A['cond']]}", None
    if Lay == "FMOVI":
        Valor = VfpExpandir(A["imm8"], A["tipo"] == 1)
        if A["tipo"] == 1:
            Real = struct.unpack("<d", struct.pack("<Q", Valor))[0]
        else:
            Real = struct.unpack("<f", struct.pack("<I", Valor))[0]
        return f"fmov {FpClasse(A['tipo'])}{A['rd']}, #{Real!r}", None
    raise ValueError(f"layout sem renderizacao: {Lay}")


# ----------------------------------------------------------------------------------------------
# Checagem por linha de vetor
# ----------------------------------------------------------------------------------------------

def ChecaForma(Nome, Lay, Campos, Palavra):
    Texto, Valor = Render(Nome, Lay, Campos)
    if Texto is None:
        return "reservado", None
    Ok, Desm = Compara(Texto, Valor, Campos.get("sf", 1), Palavra, Nome)
    return ("ok" if Ok else "falha"), (Nome, Palavra, Texto, Desm)


def ChecaAdr(Nome, Imm21, Rd, Palavra):
    Campos = {"immlo": Imm21 & 3, "immhi": (Imm21 & 0x1FFFFF) >> 2, "rd": Rd}
    return ChecaForma(Nome, "ADR", Campos, Palavra)


def ChecaMascara(Sf, Rn, Rd, Valor, Flag, Palavra):
    W = "x" if Sf else "w"
    Valido = (Valor in VALIDOS64) if Sf else (Valor in VALIDOS32)
    if Flag == 0:
        return ("falha" if Valido else "ok"), ("M", Palavra, "nao representavel", "representavel")
    if not Valido:
        return "falha", ("M", Palavra, "representavel", "fora do conjunto")
    Texto = f"and {Reg(Rd, W, True)}, {Reg(Rn, W)}, #{Valor}"
    Ok, Desm = Compara(Texto, None, Sf, Palavra, "AND_IMM")
    return ("ok" if Ok else "falha"), ("M", Palavra, Texto, Desm)


def ChecaMovImm(Sfx, Valor, Palavras):
    Mascara = MASCARA64 if Sfx else MASCARA32
    Atual = 0
    for Indice, Palavra in enumerate(Palavras):
        Desm = Desmonta(Palavra)
        if Desm is None:
            return "falha", ("W", Palavra, "palavra", "capstone recusou")
        Mnem, Ops = Norm(Desm)
        if Mnem == "mov" and len(Ops) == 2 and Indice == 0:
            Atual = int(Ops[1][1:]) & Mascara
            continue
        Imm = int(Ops[1][1:]) if len(Ops) > 1 else 0
        Sh = int(Ops[2].split("#")[1]) if len(Ops) > 2 and Ops[2].startswith("lsl") else 0
        if Mnem == "movz" and Indice == 0:
            Atual = (Imm << Sh) & Mascara
        elif Mnem == "movn" and Indice == 0:
            Atual = (~(Imm << Sh)) & Mascara
        elif Mnem == "movk" and Indice > 0:
            Atual = ((Atual & ~(0xFFFF << Sh)) | (Imm << Sh)) & Mascara
        else:
            return "falha", ("W", Palavra, "sequencia", Desm)
    Ok = Atual == (Valor & Mascara)
    return ("ok" if Ok else "falha"), ("W", Palavras, f"valor {Valor}", f"simulado {Atual}")


def ChecaFmov(Dupla, Bits, Flag, Imm8):
    Mascara = MASCARA64 if Dupla else MASCARA32
    Bits &= Mascara
    if Flag == 1:
        Ok = VfpExpandir(Imm8, Dupla) == Bits
    else:
        Ok = Bits not in VFP_TODOS[Dupla]
    return ("ok" if Ok else "falha"), ("Q", Bits, Flag, Imm8)


def ChecaLinha(Linha):
    """(status, detalhe). status: ok | falha | reservado."""
    T = Linha.split()
    Tag = T[0]
    if Tag == "F":
        Idx = int(T[1])
        Nome, Lay = FORMAS[Idx]
        Nomes = LAYOUT_ARGS[Lay]
        Valores = [int(Xv) for Xv in T[2:2 + len(Nomes)]]
        return ChecaForma(Nome, Lay, dict(zip(Nomes, Valores)), int(T[2 + len(Nomes)]))
    if Tag == "A":
        return ChecaAdr(FORMAS[int(T[1])][0], int(T[2]), int(T[3]), int(T[4]))
    if Tag == "M":
        return ChecaMascara(int(T[1]), int(T[2]), int(T[3]), int(T[4]), int(T[5]), int(T[6]))
    if Tag == "W":
        N = int(T[4])
        return ChecaMovImm(int(T[1]), int(T[3]), [int(Xv) for Xv in T[5:5 + N]])
    if Tag == "Q":
        return ChecaFmov(int(T[1]), int(T[2]), int(T[3]), int(T[4]))
    raise ValueError(f"linha desconhecida: {Linha[:60]}")


# ----------------------------------------------------------------------------------------------
# Copia do encoder dentro do gerador, execucao e CLI
# ----------------------------------------------------------------------------------------------

def CopiaDoEncoder():
    Texto = ENCODER.read_text(encoding="utf-8")
    return Texto[: Texto.index(MARCA_TESTES)].strip("\n")


def BlocoNoGerador(Texto):
    return Texto.index(MARCA_ENC_INI), Texto.index(MARCA_GER)


def Sincronizar():
    Texto = GERADOR.read_text(encoding="utf-8")
    Ini, Fim = BlocoNoGerador(Texto)
    Novo = Texto[: Ini + len(MARCA_ENC_INI)] + "\n\n" + CopiaDoEncoder() + "\n\n" + Texto[Fim:]
    GERADOR.write_text(Novo, encoding="utf-8", newline="\n")
    print(f"copia sincronizada em {GERADOR}")


def CopiaEmDia():
    Texto = GERADOR.read_text(encoding="utf-8")
    Ini, Fim = BlocoNoGerador(Texto)
    return Texto[Ini + len(MARCA_ENC_INI): Fim].strip("\n") == CopiaDoEncoder()


def RodaGerador():
    Proc = subprocess.run([str(FORJA), "run", str(GERADOR)], capture_output=True, timeout=300)
    if Proc.returncode != 0:
        print(Proc.stdout.decode("utf-8", "replace")[:800])
        print(Proc.stderr.decode("utf-8", "replace")[:800])
        raise SystemExit(f"gerar_vetores.fj saiu com {Proc.returncode}")
    return Proc.stdout.decode("utf-8").splitlines()


def Mutantes(Linhas):
    """Planta 3 bits trocados (ADD imm bit 10, LDR escalado bit 10, BL bit 0); todos tem de ser pegos."""
    Pegos = 0
    for NomeAlvo, Bit in (("ADD_IMM", 10), ("LDR_X", 10), ("BL", 0)):
        Idx = [Ind for Ind, (Nome, _) in enumerate(FORMAS) if Nome == NomeAlvo][0]
        Lay = FORMAS[Idx][1]
        Linha = next(Xv for Xv in Linhas if Xv.startswith(f"F {Idx} "))
        T = Linha.split()
        Nomes = LAYOUT_ARGS[Lay]
        Valores = [int(Xv) for Xv in T[2:2 + len(Nomes)]]
        Status, _ = ChecaForma(NomeAlvo, Lay, dict(zip(Nomes, Valores)), int(T[2 + len(Nomes)]) ^ (1 << Bit))
        print(f"  mutante {NomeAlvo} bit {Bit}: {Status}")
        if Status == "falha":
            Pegos += 1
    print(f"{Pegos}/3 mutantes detectados")
    return 0 if Pegos == 3 else 1


def Main():
    Ap = argparse.ArgumentParser(description="Diferencial do encoder A64 contra o capstone")
    Ap.add_argument("--mutantes", action="store_true", help="planta 3 mutantes e exige deteccao")
    Ap.add_argument("--sincronizar", action="store_true", help="refaz a copia do encoder dentro do gerador")
    Args = Ap.parse_args()
    if Args.sincronizar:
        Sincronizar()
        return 0
    if not CopiaEmDia():
        print("gerar_vetores.fj com copia do encoder DESATUALIZADA; rode com --sincronizar")
        return 3
    Linhas = RodaGerador()
    if Args.mutantes:
        return Mutantes(Linhas)
    Contagem = {"ok": 0, "falha": 0, "reservado": 0}
    Falhas = []
    for Linha in Linhas:
        if not Linha.strip():
            continue
        Status, Detalhe = ChecaLinha(Linha)
        Contagem[Status] += 1
        if Status == "falha":
            Falhas.append(Detalhe)
    Total = Contagem["ok"] + Contagem["falha"]
    print(f"diferencial_capstone: {Contagem['ok']}/{Total} vetores ok")
    print(f"pulados (imediato logico reservado ou LDP/STP CONSTRAINED UNPREDICTABLE, sem palavra valida): {Contagem['reservado']}")
    Resumo = {}
    for Detalhe in Falhas:
        Resumo[Detalhe[0]] = Resumo.get(Detalhe[0], 0) + 1
    print(f"divergencias por forma: {Resumo}")
    for Detalhe in Falhas[:40]:
        print("  DIVERGE:", Detalhe)
    return 0 if not Falhas else 1


if __name__ == "__main__":
    sys.exit(Main())
