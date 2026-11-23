"""Gera o texto Forja das constantes do grupo exp (exp, exp2, log, log2, log10 e as f32) a partir do C do musl.

Uso (imprime na tela; copie para um arquivo em %TEMP% e cole no .fj com Edit/Write):
    python -X utf8 -I tests\\nativo\\wasm\\math_bits\\tabelas\\gerar_tabelas_exp.py > %TEMP%\\tabelas_exp.txt

Saida: `const NOME: u64 = 0x...` (bits de f64/f32) e `const NOME: [N]u64 = {...}` (tabelas).
Le third\\musl_math\\*_data.c, log10.c e log10f.c; nao escreve nenhum .fj.
Depende de: so a biblioteca padrao do Python.
Versao: 1.0.0 (changelog: 1.0.0 gerador inicial do grupo exp, 21/11/2026)
"""
import os
import re
import struct
import sys
from fractions import Fraction

AQUI = os.path.dirname(os.path.abspath(__file__))
LANG = os.path.abspath(os.path.join(AQUI, "..", "..", "..", "..", ".."))
MUSL = os.path.join(LANG, "third", "musl_math")
HEX_COM_P = re.compile(r"0x[0-9a-fA-F]*\.?[0-9a-fA-F]*p[+-]?\d+")


def LerC(NomeArquivo):
    """Texto do arquivo C sem comentarios (os comentarios tem numeros que nao sao dados)."""
    with open(os.path.join(MUSL, NomeArquivo), encoding="utf-8") as Arq:
        Texto = Arq.read()
    Texto = re.sub(r"/\*.*?\*/", "", Texto, flags=re.S)
    return re.sub(r"//[^\n]*", "", Texto)


def Bloco(Texto, NomeCampo):
    """Conteudo entre as chaves de `.NomeCampo = {` (casa as chaves aninhadas)."""
    Casa = re.search(r"\." + NomeCampo + r"\s*=\s*\{", Texto)
    assert Casa, NomeCampo
    Inicio = Casa.end()
    Prof, Pos = 1, Inicio
    while Prof:
        Prof += {"{": 1, "}": -1}.get(Texto[Pos], 0)
        Pos += 1
    return Texto[Inicio:Pos - 1]


def Campo(Texto, NomeCampo):
    """Lista das expressoes de `.NomeCampo = expr,` (o `#if` de exp_data.c tem 2 ocorrencias de shift)."""
    return [Trecho.strip() for Trecho in re.findall(r"\." + NomeCampo + r"\s*=\s*([^,\n]+)", Texto)]


def Itens(Inner):
    """Itens planos (`a, b, c`) ou grupos (`{a, b}, {c, d}`) como lista de listas de expressoes."""
    if "{" in Inner:
        return [[Trecho.strip() for Trecho in Grupo.split(",") if Trecho.strip()]
                for Grupo in re.findall(r"\{([^{}]*)\}", Inner)]
    return [[Trecho.strip() for Trecho in Inner.split(",") if Trecho.strip()]]


def Valor(Expr, N):
    """Avalia a expressao C de constante (hex-float, `* N`, `/ N`) com float exato."""
    Texto = HEX_COM_P.sub(lambda Casa: repr(float.fromhex(Casa.group(0))), Expr)
    # lint-ok: iaf-simbolo-dinamico expressao fixa lida do musl (arquivo local confiavel), sem entrada de usuario
    return eval(Texto, {"__builtins__": {}}, {"N": N})


def BitsF64(Valor_):
    """Bits de f64; inteiro (uint64 da tabela) passa direto."""
    if isinstance(Valor_, int):
        return Valor_
    return struct.unpack("<Q", struct.pack("<d", Valor_))[0]


def BitsF32Exato(Valor_):
    """Bits de f32 de um valor que ja e exatamente f32 (hex-float curto)."""
    assert struct.unpack("<f", struct.pack("<f", Valor_))[0] == Valor_, Valor_
    return struct.unpack("<I", struct.pack("<f", Valor_))[0]


def F32DeDecimal(Texto):
    """Arredondamento correto do decimal para f32 (round-half-even), sem passar por f64."""
    Q = Fraction(Texto)
    Sinal = -1 if Q < 0 else 1
    Q = abs(Q)
    E = Q.numerator.bit_length() - Q.denominator.bit_length()
    while Fraction(2) ** E > Q:
        E -= 1
    while Fraction(2) ** (E + 1) <= Q:
        E += 1
    Passo = Fraction(2) ** (max(E, -126) - 23)
    return Sinal * float(round(Q / Passo) * Passo)


def ConstantesLog10():
    """log10.c (f64, decimal) e log10f.c (f32: decimal arredondado e hex-float exato)."""
    Saida = []
    for NomeArquivo, Tipo, Corte, Prefixo in (("log10.c", "double", "double log10(", "LOG10"),
                                              ("log10f.c", "float", "float log10f(", "LOG10F")):
        Texto = LerC(NomeArquivo)
        Cabeca = Texto[:Texto.index(Corte)]
        for Nome, Expr in re.findall(r"(\w+)\s*=\s*([^,;]+)[,;]", Cabeca):
            Expr = Expr.strip()
            Fonte = f"{NomeArquivo} {Nome} = {Expr}"
            if Tipo == "double":
                Saida.append((f"{Prefixo}_{Nome.upper()}", "u64", BitsF64(float(Expr)), Fonte))
            elif Expr.startswith("0x"):
                Saida.append((f"{Prefixo}_{Nome.upper()}", "u32", BitsF32Exato(float.fromhex(Expr)), Fonte))
            else:
                Saida.append((f"{Prefixo}_{Nome.upper()}", "u32", BitsF32Exato(F32DeDecimal(Expr)), Fonte))
    return Saida


def SaidaTabela(NomeConst, Valores, Largura=4):
    """`const NOME: [K]u64 = {...}` com Largura valores por linha."""
    Linhas = [f"const {NomeConst}: [{len(Valores)}]u64 = {{"]
    Pedacos = [f"0x{Valor_:016x}" for Valor_ in Valores]
    for Inicio in range(0, len(Pedacos), Largura):
        Fim = ", " if Inicio + Largura < len(Pedacos) else ""
        Linhas.append("    " + ", ".join(Pedacos[Inicio:Inicio + Largura]) + Fim)
    Linhas.append("}")
    return "\n".join(Linhas)


def Pares(Texto, NomeCampo, N):
    """Tabela de pares {a, b} (double) achatada em [a0, b0, a1, b1, ...] como bits."""
    Achatado = []
    for Grupo in Itens(Bloco(Texto, NomeCampo)):
        assert len(Grupo) == 2, (NomeCampo, Grupo)
        Achatado += [BitsF64(Valor(Expr, N)) for Expr in Grupo]
    return Achatado


def Plano(Texto, NomeCampo, N):
    """Itens planos de um bloco de numeros (uint64 ou double) como bits."""
    return [BitsF64(Valor(Expr, N)) for Grupo in Itens(Bloco(Texto, NomeCampo)) for Expr in Grupo]


def Gerar():
    Saida = []

    def Esc(NomeConst, Expr, N, Fonte):
        Bits = BitsF64(Valor(Expr, N))
        Saida.append(f"const {NomeConst}: u64 = 0x{Bits:016x}  // {Fonte}: {Expr}")

    # ---- exp_data.c (exp, exp2) ----
    Texto = LerC("exp_data.c")
    N = 128
    Esc("EXP_INVLN2N", Campo(Texto, "invln2N")[0], N, "exp_data.c invln2N")
    Esc("EXP_SHIFT", Campo(Texto, "shift")[-1], N, "exp_data.c shift (ramo #else)")
    Esc("EXP_NEGLN2HIN", Campo(Texto, "negln2hiN")[0], N, "exp_data.c negln2hiN")
    Esc("EXP_NEGLN2LON", Campo(Texto, "negln2loN")[0], N, "exp_data.c negln2loN")
    for Indice, Expr in enumerate(Itens(Bloco(Texto, "poly"))[0]):
        Esc(f"EXP_C{Indice + 2}", Expr, N, f"exp_data.c poly[{Indice}]")
    Esc("EXP_EXP2_SHIFT", Campo(Texto, "exp2_shift")[0], N, "exp_data.c exp2_shift")
    for Indice, Expr in enumerate(Itens(Bloco(Texto, "exp2_poly"))[0]):
        Esc(f"EXP_EXP2_C{Indice + 1}", Expr, N, f"exp_data.c exp2_poly[{Indice}]")
    Tab = Plano(Texto, "tab", N)
    assert len(Tab) == 256, len(Tab)
    Saida.append(SaidaTabela("EXP_TAB", Tab))

    # ---- exp2f_data.c (expf, exp2f) ----
    Texto = LerC("exp2f_data.c")
    N = 32
    Esc("EXP2F_SHIFT_SCALED", Campo(Texto, "shift_scaled")[0], N, "exp2f_data.c shift_scaled")
    for Indice, Expr in enumerate(Itens(Bloco(Texto, "poly"))[0]):
        Esc(f"EXP2F_P{Indice}", Expr, N, f"exp2f_data.c poly[{Indice}]")
    Esc("EXP2F_SHIFT", Campo(Texto, "shift")[0], N, "exp2f_data.c shift")
    Esc("EXP2F_INVLN2_SCALED", Campo(Texto, "invln2_scaled")[0], N, "exp2f_data.c invln2_scaled")
    for Indice, Expr in enumerate(Itens(Bloco(Texto, "poly_scaled"))[0]):
        Esc(f"EXP2F_PS{Indice}", Expr, N, f"exp2f_data.c poly_scaled[{Indice}]")
    Tab = Plano(Texto, "tab", N)
    assert len(Tab) == 32, len(Tab)
    Saida.append(SaidaTabela("EXP2F_TAB", Tab))

    # ---- log_data.c (log) ----
    Texto = LerC("log_data.c")
    N = 128
    Esc("LOG_LN2HI", Campo(Texto, "ln2hi")[0], N, "log_data.c ln2hi")
    Esc("LOG_LN2LO", Campo(Texto, "ln2lo")[0], N, "log_data.c ln2lo")
    Poly1 = Itens(Bloco(Texto, "poly1"))[0]
    assert len(Poly1) == 11, len(Poly1)
    for Indice, Expr in enumerate(Poly1):
        Esc(f"LOG_B{Indice}", Expr, N, f"log_data.c poly1[{Indice}]")
    Poly = Itens(Bloco(Texto, "poly"))[0]
    assert len(Poly) == 5, len(Poly)
    for Indice, Expr in enumerate(Poly):
        Esc(f"LOG_A{Indice}", Expr, N, f"log_data.c poly[{Indice}]")
    Tab = Pares(Texto, "tab", N)
    assert len(Tab) == 256, len(Tab)
    Saida.append(SaidaTabela("LOG_TAB", Tab))
    Tab2 = Pares(Texto, "tab2", N)
    assert len(Tab2) == 256, len(Tab2)
    Saida.append(SaidaTabela("LOG_TAB2", Tab2))

    # ---- log2_data.c (log2) ----
    Texto = LerC("log2_data.c")
    N = 64
    Esc("LOG2_INVLN2HI", Campo(Texto, "invln2hi")[0], N, "log2_data.c invln2hi")
    Esc("LOG2_INVLN2LO", Campo(Texto, "invln2lo")[0], N, "log2_data.c invln2lo")
    Poly = Itens(Bloco(Texto, "poly"))[0]
    assert len(Poly) == 6, len(Poly)
    for Indice, Expr in enumerate(Poly):
        Esc(f"LOG2_A{Indice}", Expr, N, f"log2_data.c poly[{Indice}]")
    Poly1 = Itens(Bloco(Texto, "poly1"))[0]
    assert len(Poly1) == 10, len(Poly1)
    for Indice, Expr in enumerate(Poly1):
        Esc(f"LOG2_B{Indice}", Expr, N, f"log2_data.c poly1[{Indice}]")
    Tab = Pares(Texto, "tab", N)
    assert len(Tab) == 128, len(Tab)
    Saida.append(SaidaTabela("LOG2_TAB", Tab))
    Tab2 = Pares(Texto, "tab2", N)
    assert len(Tab2) == 128, len(Tab2)
    Saida.append(SaidaTabela("LOG2_TAB2", Tab2))

    # ---- logf_data.c (logf) e log2f_data.c (log2f) ----
    Texto = LerC("logf_data.c")
    Esc("LOGF_LN2", Campo(Texto, "ln2")[0], 16, "logf_data.c ln2")
    Poly = Itens(Bloco(Texto, "poly"))[0]
    assert len(Poly) == 3, len(Poly)
    for Indice, Expr in enumerate(Poly):
        Esc(f"LOGF_A{Indice}", Expr, 16, f"logf_data.c poly[{Indice}]")
    Tab = Pares(Texto, "tab", 16)
    assert len(Tab) == 32, len(Tab)
    Saida.append(SaidaTabela("LOGF_TAB", Tab))

    Texto = LerC("log2f_data.c")
    Poly = Itens(Bloco(Texto, "poly"))[0]
    assert len(Poly) == 4, len(Poly)
    for Indice, Expr in enumerate(Poly):
        Esc(f"LOG2F_A{Indice}", Expr, 16, f"log2f_data.c poly[{Indice}]")
    Tab = Pares(Texto, "tab", 16)
    assert len(Tab) == 32, len(Tab)
    Saida.append(SaidaTabela("LOG2F_TAB", Tab))

    # ---- log10.c e log10f.c (constantes decimais, sem tabela) ----
    for NomeConst, Tipo, Bits, Fonte in ConstantesLog10():
        Largura = 8 if Tipo == "u32" else 16
        Saida.append(f"const {NomeConst}: {Tipo} = 0x{Bits:0{Largura}x}  // {Fonte}")
    return "\n".join(Saida)


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stdout.write(Gerar() + "\n")
