"""Gera as tabelas do porte pow/powf (musl 1.2.6) como constantes de Forja com os BITS crus (u64/u32).

Le third\\musl_math\\pow_data.c, exp_data.c, powf_data.c e exp2f_data.c. Cada numero C (hex-float, decimal ou produto
simples como `0x1.8p+52 / N` e `x * POWF_SCALE`) vira bits IEEE 754 com float.fromhex/struct, que e exato nas potencias
de 2 usadas. O modulo runtime\\wasm\\rt_wasm_pow.fj le as tabelas com transmute, sem conversao em runtime.

Uso:
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\tabelas\\gerar_tabelas_pow.py                      imprime as constantes (para colar no .fj)
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\tabelas\\gerar_tabelas_pow.py --conferir <rt_wasm_pow.fj>  compara o .fj com a fonte C (0 = igual)
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\tabelas\\gerar_tabelas_pow.py --selftest             confere contagens e um valor conhecido
Versao: 0.1.0 (sem dependencias: so stdlib)
"""
import os
import re
import struct
import sys

PASTA_AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ_LANGUAGE = os.path.normpath(os.path.join(PASTA_AQUI, "..", "..", "..", "..", ".."))
PASTA_MUSL = os.path.join(RAIZ_LANGUAGE, "third", "musl_math")
FJ_PADRAO = os.path.join(RAIZ_LANGUAGE, "runtime", "wasm", "rt_wasm_pow.fj")

COMENTARIO_BLOCO = re.compile(r"/\*.*?\*/", re.S)
COMENTARIO_LINHA = re.compile(r"//[^\n]*")
LINHA_PREPROCESSOR = re.compile(r"^\s*#.*$", re.M)
HEX_FLOAT = re.compile(r"0x[0-9a-fA-F]*\.?[0-9a-fA-F]*[pP][+-]?[0-9]+")
HEX_INTEIRO = re.compile(r"0x[0-9a-fA-F]+")

# constantes de bits que o modulo usa: (nome, tipo, expressao C); u32 = f32 (o sufixo f da expressao e tirado)
ESPECIAIS = [
    ("POW_BITS_UM", "u64", "1.0"),
    ("POW_BITS_2P52", "u64", "0x1p52"),
    ("POW_BITS_2P1009", "u64", "0x1p1009"),
    ("POW_BITS_2P_1022", "u64", "0x1p-1022"),
    ("POW_BITS_2P769", "u64", "0x1p769"),
    ("POW_BITS_2P_767", "u64", "0x1p-767"),
    ("POW_BITS_126", "u64", "126.0"),
    ("POWF_LIMITE_YLOGX", "u64", "0x1.fffffffd1d571p+6"),
    ("POWF_BITS_2P23", "u32", "0x1p23f"),
    ("POWF_BITS_2P97", "u32", "0x1p97f"),
    ("POWF_BITS_2P_95", "u32", "0x1p-95f"),
]


def LerSemComentario(Nome):
    """Texto do .c sem comentarios e sem linhas de preprocessador (os dois ramos do #if ficam; quem consome escolhe)."""
    with open(os.path.join(PASTA_MUSL, Nome), encoding="utf-8") as Arq:
        Texto = Arq.read()
    Texto = COMENTARIO_BLOCO.sub("", Texto)
    Texto = COMENTARIO_LINHA.sub("", Texto)
    return LINHA_PREPROCESSOR.sub("", Texto)


def BlocoApos(Texto, Marca):
    """Conteudo entre as chaves que abrem em `Marca` (a marca termina com `{`); casa as chaves internas."""
    Inicio = Texto.index(Marca)
    Abre = Texto.index("{", Inicio)
    Profundidade = 0
    for Posicao in range(Abre, len(Texto)):
        if Texto[Posicao] == "{":
            Profundidade += 1
        elif Texto[Posicao] == "}":
            Profundidade -= 1
            if Profundidade == 0:
                return Texto[Abre + 1:Posicao]
    raise ValueError("chave sem fechamento depois de " + Marca)


def Campo(Texto, Nome, Padrao=None):
    """Expressao C atribuida a `.Nome = ...,`; com `Padrao` procura o trecho exato (escolhe o ramo certo de um #if)."""
    if Padrao is not None:
        Achado = re.search(r"\.%s\s*=\s*(%s)\s*," % (Nome, re.escape(Padrao)), Texto)
    else:
        Achado = re.search(r"\.%s\s*=\s*([^,]+)," % Nome, Texto)
    if Achado is None:
        raise ValueError("campo nao achado: " + Nome)
    return Achado.group(1).strip()


def ItensVirgula(Conteudo):
    """Itens de uma lista C separada por virgula (vazios descartados)."""
    return [Item.strip() for Item in Conteudo.split(",") if Item.strip()]


def AvaliaExpressao(Texto):
    """Avalia so produtos e quocientes de numeros (`a * b / c`), da esquerda para a direita, como o compilador C faz."""
    Partes = [Parte.strip() for Parte in re.split(r"([*/])", Texto)]
    Valor = float(Partes[0])
    for Indice in range(1, len(Partes), 2):
        Operador, Numero = Partes[Indice], float(Partes[Indice + 1])
        Valor = Valor * Numero if Operador == "*" else Valor / Numero
    return Valor


def ValorBits(Expressao, Subs):
    """Bits IEEE 754 (inteiro) do numero C; `Subs` troca nomes C (N, POWF_SCALE) pelo valor."""
    Texto = HEX_FLOAT.sub(lambda Casado: repr(float.fromhex(Casado.group(0))), Expressao)
    for NomeC, Valor in Subs.items():
        Texto = re.sub(r"\b%s\b" % NomeC, repr(float(Valor)), Texto)
    Valor = AvaliaExpressao(Texto)
    return struct.unpack("<Q", struct.pack("<d", Valor))[0]


def ValorInteiro(Token, Subs):
    """Inteiro hex da tabela (ja sao bits) ou expressao de double convertida para bits."""
    if HEX_INTEIRO.fullmatch(Token):
        return int(Token, 16)
    return ValorBits(Token, Subs)


def ValoresEspeciais():
    """Constantes de bits pedidas pelo modulo (potencias de 2, 1.0, 126.0 e o limite do powf)."""
    Registros = []
    for Nome, Tipo, Expressao in ESPECIAIS:
        if Tipo == "u32":
            Valor = struct.unpack("<I", struct.pack("<f", float.fromhex(Expressao.rstrip("f"))))[0]
        else:
            Valor = ValorBits(Expressao, {})
        Registros.append((Nome, Tipo, None, [Valor]))
    return Registros


def RegistrosPow():
    """Constantes do log e do exp que o pow usa (pow_data.c e exp_data.c)."""
    PowData = LerSemComentario("pow_data.c")
    ExpData = LerSemComentario("exp_data.c")
    SubsExp = {"N": 128}
    TabLog = re.findall(r"A\(\s*([^,]+?)\s*,\s*([^,]+?)\s*,\s*([^)]+?)\s*\)", BlocoApos(PowData, ".tab = {"))
    PolyLog = ItensVirgula(BlocoApos(PowData, ".poly = {"))
    PolyExp = ItensVirgula(BlocoApos(ExpData, ".poly = {"))
    TabExp = ItensVirgula(BlocoApos(ExpData, ".tab = {"))
    assert len(TabLog) == 128 and len(PolyLog) == 7, (len(TabLog), len(PolyLog))
    assert len(PolyExp) == 4 and len(TabExp) == 256, (len(PolyExp), len(TabExp))
    return [
        ("POW_LN2HI", "u64", None, [ValorBits(Campo(PowData, "ln2hi"), {})]),
        ("POW_LN2LO", "u64", None, [ValorBits(Campo(PowData, "ln2lo"), {})]),
        ("POW_LOG_POLY", "u64", 7, [ValorBits(Item, {}) for Item in PolyLog]),
        ("POW_LOG_INVC", "u64", 128, [ValorBits(Invc, {}) for Invc, _, _ in TabLog]),
        ("POW_LOG_LOGC", "u64", 128, [ValorBits(Logc, {}) for _, Logc, _ in TabLog]),
        ("POW_LOG_LOGCTAIL", "u64", 128, [ValorBits(Cauda, {}) for _, _, Cauda in TabLog]),
        ("POW_EXP_INVLN2N", "u64", None, [ValorBits(Campo(ExpData, "invln2N"), SubsExp)]),
        ("POW_EXP_SHIFT", "u64", None, [ValorBits(Campo(ExpData, "shift", "0x1.8p52"), {})]),
        ("POW_EXP_NEGLN2HI", "u64", None, [ValorBits(Campo(ExpData, "negln2hiN"), {})]),
        ("POW_EXP_NEGLN2LO", "u64", None, [ValorBits(Campo(ExpData, "negln2loN"), {})]),
        ("POW_EXP_POLY", "u64", 4, [ValorInteiro(Item, {}) for Item in PolyExp]),
        ("POW_EXP_TAB", "u64", 256, [ValorInteiro(Item, {}) for Item in TabExp]),
    ]


def RegistrosPowf():
    """Constantes do log2 e do exp2 que o powf usa (powf_data.c e exp2f_data.c); POWF_SCALE = 1.0 (TOINT_INTRINSICS=0)."""
    PowfData = LerSemComentario("powf_data.c")
    Exp2fData = LerSemComentario("exp2f_data.c")
    SubsEscala = {"POWF_SCALE": 1.0}
    TabLog2 = re.findall(r"\{\s*([^{}]+?)\s*,\s*([^{}]+?)\s*\}", BlocoApos(PowfData, ".tab = {"))
    PolyLog2 = ItensVirgula(BlocoApos(PowfData, ".poly = {"))
    TabExp2 = ItensVirgula(BlocoApos(Exp2fData, ".tab = {"))
    PolyExp2 = ItensVirgula(BlocoApos(Exp2fData, ".poly = {"))
    assert len(TabLog2) == 16 and len(PolyLog2) == 5, (len(TabLog2), len(PolyLog2))
    assert len(TabExp2) == 32 and len(PolyExp2) == 3, (len(TabExp2), len(PolyExp2))
    return [
        ("POWF_LOG2_INVC", "u64", 16, [ValorBits(Invc, SubsEscala) for Invc, _ in TabLog2]),
        ("POWF_LOG2_LOGC", "u64", 16, [ValorBits(Logc, SubsEscala) for _, Logc in TabLog2]),
        ("POWF_LOG2_POLY", "u64", 5, [ValorBits(Item, SubsEscala) for Item in PolyLog2]),
        ("POWF_EXP2_TAB", "u64", 32, [ValorInteiro(Item, {}) for Item in TabExp2]),
        ("POWF_EXP2_SHIFT_SCALED", "u64", None, [ValorBits(Campo(Exp2fData, "shift_scaled"), {"N": 32})]),
        ("POWF_EXP2_POLY", "u64", 3, [ValorInteiro(Item, {}) for Item in PolyExp2]),
    ]


def Registros():
    """Todas as constantes do porte, na ordem em que o .fj as declara: (nome, tipo, tamanho ou None, valores)."""
    return RegistrosPow() + RegistrosPowf() + ValoresEspeciais()


def FormataRegistro(Nome, Tipo, Tamanho, Valores):
    """Texto Forja de uma constante: escalar numa linha; vetor com 4 valores por linha."""
    Digitos = 16 if Tipo == "u64" else 8
    Hexa = ["0x%0*x" % (Digitos, Valor) for Valor in Valores]
    if Tamanho is None:
        return "const %s: %s = %s\n" % (Nome, Tipo, Hexa[0])
    Linhas = [", ".join(Hexa[Indice:Indice + 4]) for Indice in range(0, len(Hexa), 4)]
    Corpo = ",\n".join("    " + Linha for Linha in Linhas)
    return "const %s: [%d]%s = {\n%s\n}\n" % (Nome, Tamanho, Tipo, Corpo)


def ConfereFj(Caminho):
    """Compara as constantes do .fj com a fonte C (mesmos bits e tamanho); devolve 0 se tudo bate."""
    with open(Caminho, encoding="utf-8") as Arq:
        Texto = Arq.read()
    Divergencias = 0
    Todas = Registros()
    for Nome, Tipo, Tamanho, Valores in Todas:
        if Tamanho is None:
            Achado = re.search(r"^const %s: %s = (0x[0-9a-fA-F]+)" % (Nome, Tipo), Texto, re.M)
            Obtido = [int(Achado.group(1), 16)] if Achado else None
        else:
            Achado = re.search(r"^const %s: \[%d\]%s = \{([^}]*)\}" % (Nome, Tamanho, Tipo), Texto, re.M)
            Obtido = [int(Hex, 16) for Hex in HEX_INTEIRO.findall(Achado.group(1))] if Achado else None
        if Obtido != Valores:
            Divergencias += 1
            print("DIVERGE " + Nome)
    print("conferidas %d constantes, divergencias %d" % (len(Todas), Divergencias))
    return 0 if Divergencias == 0 else 1


def Selftest():
    """Contagens das tabelas e valores conhecidos (ln2hi do musl: 0x1.62e42fefa3800p-1)."""
    Valores = {Nome: Lista for Nome, _, _, Lista in Registros()}
    assert len(Valores["POW_LOG_INVC"]) == 128 and len(Valores["POW_EXP_TAB"]) == 256
    assert len(Valores["POWF_LOG2_INVC"]) == 16 and len(Valores["POWF_EXP2_TAB"]) == 32
    Ln2hi = struct.unpack("<Q", struct.pack("<d", float.fromhex("0x1.62e42fefa3800p-1")))[0]
    assert Valores["POW_LN2HI"][0] == Ln2hi, hex(Valores["POW_LN2HI"][0])
    assert Valores["POW_BITS_2P52"][0] == 0x4330000000000000
    assert Valores["POWF_BITS_2P23"][0] == 0x4B000000
    print("selftest OK")
    return 0


def Principal(Argumentos):
    if "--selftest" in Argumentos:
        return Selftest()
    if "--conferir" in Argumentos:
        Indice = Argumentos.index("--conferir")
        Caminho = Argumentos[Indice + 1] if Indice + 1 < len(Argumentos) else FJ_PADRAO
        return ConfereFj(Caminho)
    for Nome, Tipo, Tamanho, Valores in Registros():
        sys.stdout.write(FormataRegistro(Nome, Tipo, Tamanho, Valores) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
