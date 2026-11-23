# gerar_e_rodar.py - prova o texto de real do runtime wasm (runtime/wasm/rt_wasm_real.fj) contra o do x64.
# Uso: python gerar_e_rodar.py   (sai 0 so com "OK <n> casos, 0 diverge" em dev e em release)
# Versao: 0.1.3
# Changelog:
#   0.1.3 - contagem em struct passada por ponteiro (Forja nao tem var de topo).
#   0.1.2 - a conferencia usa o buffer do teste (texto_f64_fixo, curto_texto_f64/f32), sem estado global.
#   0.1.1 - nomes em CamelCase (PY-R01) e remocao do import nao usado.
#   0.1.0 - primeira versao.
# Como funciona: copia rt_wasm_real.fj trocando o bloco PLATAFORMA (import do std:wasm_rt e emite_texto) por um emite_texto
#   vazio do x64, acrescenta a bateria (bordas + LCG de f64/f32) e roda forja run dev/release. A referencia e o
#   format(f"...") do proprio x64, byte a byte. Gera teste_real_x64.fj ao lado deste arquivo.
# Depende de: nada alem do forja.exe (language\compiler\forja.exe).
import struct
import subprocess
import sys
from pathlib import Path

LANGUAGE = Path(__file__).resolve().parents[4]
MODULO = LANGUAGE / "runtime" / "wasm" / "rt_wasm_real.fj"
FORJA = LANGUAGE / "compiler" / "forja.exe"
SAIDA = Path(__file__).resolve().parent / "teste_real_x64.fj"
MARCA_INI = "// PLATAFORMA-INICIO"
MARCA_FIM = "// PLATAFORMA-FIM"
# format(f"{x:.N}") do x64 aceita N 0..17 (E0315); 18 e 30 vao so nas bordas, contra o '%.Nf' exato do python.
CASAS = [0, 1, 2, 3, 6, 17]
CASAS_LONGAS = [18, 30]
LCG_MULT = 6364136223846793005
LCG_SOMA = 1442695040888963407

BLOCO_X64 = """// PLATAFORMA x64 (teste): o texto e conferido pelo buffer do proprio teste; emite_texto so existe para o modulo compilar
proc emite_texto(p: ^u8, n: int, fmt: u32) {
    let _tamanho = n
}
"""


def ValoresDecimais():
    """Valores de borda em decimal; viram bits IEEE no python (exato)."""
    Base = [0.0, -0.0, 5e-324, 1.7976931348623157e308, 2.2250738585072014e-308,
            0.1, 0.3, 1 / 3, 123456789.125, 0.5, 1.5, 100.0, 9007199254740993.0,
            2.675, 1.005, 4.35, 8.5, 0.000001234, -2.5, -0.001]
    Potencias = [10.0 ** Expoente for Expoente in range(-10, 23)]
    return Base + Potencias


def BitsDeF64(Valor):
    return struct.unpack("<Q", struct.pack("<d", Valor))[0]


def Literal64(Bits):
    """u64 como expressao Forja: literal de 64 bits passa de int (i64), entao sai em duas metades de 32 bits."""
    return "((0x%08X as u64) << 32) | (0x%08X as u64)" % (Bits >> 32, Bits & 0xFFFFFFFF)


def GerarBordasF64():
    Linhas = []
    # nan, +inf e -inf: o -inf sai de OR do bit de sinal (literal 0xFFF... passaria de i64)
    Especiais = [(float("nan"), "0x7FF8000000000000 as u64"),
                 (float("inf"), "0x7FF0000000000000 as u64"),
                 (float("-inf"), "((1 as u64) << 63) | (0x7FF0000000000000 as u64)")]
    Pares = [(Valor, Literal64(BitsDeF64(Valor))) for Valor in ValoresDecimais()] + Especiais
    for Valor, Expressao in Pares:
        Linhas.append("    testa_f64(transmute(%s, to = f64), %s, c)" % (Expressao, Expressao))
        Linhas.append("    testa_longa_f64(transmute(%s, to = f64), %s, \"%s\", \"%s\", c)"
                      % (Expressao, Expressao, "%.18f" % Valor, "%.30f" % Valor))
    return "proc testa_bordas_f64(c: ^Contagem) {\n%s\n}\n" % "\n".join(Linhas)


def GerarBordasF32():
    Padroes = [0x00000000, 0x80000000, 0x00000001, 0x7F7FFFFF, 0x00800000, 0x3DCCCCCD,
               0x3EAAAAAB, 0x7FC00000, 0x7F800000, 0xFF800000, 0x4B000000, 0x42C80000]
    Linhas = []
    for Bits in Padroes:
        Linhas.append("    testa_f32(transmute(0x%08X as u32, to = f32) as f64, 0x%08X as u64, c)" % (Bits, Bits))
    return "proc testa_bordas_f32(c: ^Contagem) {\n%s\n}\n" % "\n".join(Linhas)


def GerarPrecisoes(Nome, Tipo):
    """Um bloco por N de precisao: texto do modulo (buffer) contra f"{v:.N}" do x64."""
    Linhas = []
    for Casas in CASAS:
        Linhas.append("    let n%d: int = texto_f64_fixo(v, prec_fmt(%d), buf[..])" % (Casas, Casas))
        Linhas.append("    let r%d = format(f\"{%s:.%d}\", alloc = heap)" % (Casas, Tipo, Casas))
        Linhas.append("    registra(iguais(r%d, buf[..], n%d), \"%s .%d\", bits, c)" % (Casas, Casas, Nome, Casas))
        Linhas.append("    delete(r%d)" % Casas)
    return Linhas


TESTES = """
struct Contagem {
    casos: int = 0
    diverge: int = 0
}

proc prec_fmt(casas: int) -> u32 {
    return ((casas + 1) as u32) << 8
}

proc iguais(ref: str, buf: []u8, n: int) -> bool {
    let b = as_bytes(ref)
    if len(b) != n {
        return false
    }
    var k: int = 0
    while k < n {
        if b[k] != buf[k] {
            return false
        }
        k += 1
    }
    return true
}

proc registra(ok: bool, rotulo: str, bits: u64, c: ^Contagem) {
    c.casos += 1
    if not ok {
        c.diverge += 1
        if c.diverge <= 20 {
            eprint(f"DIVERGE {rotulo} bits={bits:x}")
        }
    }
}

proc testa_f64(v: f64, bits: u64, c: ^Contagem) {
    var buf: [352]u8 = {}
    let n: int = curto_texto_f64(v, buf[..])
    let r = format(f"{v}", alloc = heap)
    registra(iguais(r, buf[..], n), "f64 curto", bits, c)
    delete(r)
%PREC_F64%}

proc testa_f32(v: f64, bits: u64, c: ^Contagem) {
    var buf: [352]u8 = {}
    let n: int = curto_texto_f32(v, buf[..])
    let f: f32 = v as f32
    let r = format(f"{f}", alloc = heap)
    registra(iguais(r, buf[..], n), "f32 curto", bits, c)
    delete(r)
%PREC_F32%}

proc testa_longa_f64(v: f64, bits: u64, esperado18: str, esperado30: str, c: ^Contagem) {
    var buf: [352]u8 = {}
    let n18: int = texto_f64_fixo(v, prec_fmt(18), buf[..])
    registra(iguais(esperado18, buf[..], n18), "f64 .18 python", bits, c)
    let n30: int = texto_f64_fixo(v, prec_fmt(30), buf[..])
    registra(iguais(esperado30, buf[..], n30), "f64 .30 python", bits, c)
}

proc testa_aleatorios(quantidade: int, c: ^Contagem) {
    var estado: u64 = 88172645463325252
    var k: int = 0
    while k < quantidade {
        estado = wrapping_add(wrapping_mul(estado, %MULT%), %SOMA%)
        let bits: u64 = estado
        if ((bits >> 52) & 2047) != 2047 {
            testa_f64(transmute(bits, to = f64), bits, c)
        }
        let meio: u64 = (((bits >> 40) % 60) + 993) << 52
        let bits_meio: u64 = (bits & 4503599627370495) | meio
        testa_f64(transmute(bits_meio, to = f64), bits_meio, c)
        estado = wrapping_add(wrapping_mul(estado, %MULT%), %SOMA%)
        let b32: u32 = (estado >> 32) as u32
        if ((b32 >> 23) & 255) != 255 {
            let f32_valor: f32 = transmute(b32, to = f32)
            testa_f32(f32_valor as f64, b32 as u64, c)
        }
        k += 1
    }
}

proc main() {
    var c = Contagem{}
    testa_bordas_f64(&c)
    testa_bordas_f32(&c)
    testa_aleatorios(20000, &c)
    if c.diverge == 0 {
        print(f"OK {c.casos} casos, 0 diverge")
    } else {
        print(f"{c.diverge} diverge de {c.casos} casos")
        panic("divergencia do texto de real")
    }
}
"""


def MontarTeste(TextoModulo):
    Ini = TextoModulo.index(MARCA_INI)
    Fim = TextoModulo.index(MARCA_FIM) + len(MARCA_FIM)
    Base = TextoModulo[:Ini] + BLOCO_X64.rstrip("\n") + TextoModulo[Fim:]
    Prec64 = "\n".join(GerarPrecisoes("f64", "v")) + "\n"
    Prec32 = "\n".join(GerarPrecisoes("f32", "f")) + "\n"
    Cauda = (TESTES.replace("%PREC_F64%", Prec64).replace("%PREC_F32%", Prec32)
             .replace("%MULT%", str(LCG_MULT)).replace("%SOMA%", str(LCG_SOMA)))
    Cauda = Cauda + "\n" + GerarBordasF64() + "\n" + GerarBordasF32()
    return Base + "\n" + Cauda


def RodarForja(Argumentos):
    Res = subprocess.run([str(FORJA)] + Argumentos, capture_output=True, text=True, encoding="utf-8",
                         errors="replace")
    return Res.returncode, (Res.stdout + Res.stderr).strip()


def Main():
    Texto = MODULO.read_text(encoding="utf-8")
    SAIDA.write_text(MontarTeste(Texto), encoding="utf-8", newline="\n")
    print("gerado:", SAIDA)
    Codigo, Saida = RodarForja(["check", str(SAIDA)])
    if Codigo != 0:
        print("forja check FALHOU:", Saida[-600:])
        return 1
    Ok = True
    for Nome, Extra in [("dev", []), ("release", ["--release"])]:
        Codigo, Saida = RodarForja(["run"] + Extra + [str(SAIDA)])
        print("[%s] exit=%d | %s" % (Nome, Codigo, Saida[-400:]))
        Ok = Ok and Codigo == 0 and "0 diverge" in Saida
    return 0 if Ok else 1


if __name__ == "__main__":
    sys.exit(Main())
