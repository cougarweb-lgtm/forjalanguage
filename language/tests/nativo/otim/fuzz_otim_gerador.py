# -*- coding: utf-8 -*-
"""Gerador deterministico de programas .fj para o fuzzer diferencial do otimizador x64 (M7a F9).

GerarPrograma(Semente) -> (texto .fj, meta). O programa e valido por construcao e termina: aritmetica int/i32/uint com valores
limitados por modulo, lacos com limite constante e variavel, arrays/listas/soa indexados por i e i*C+k, structs por valor (ate 56 bytes),
procs que chamam procs, floats f64/f32 limitados por clamp, prints no meio e, as vezes, um trap planejado DEPOIS de um print.
meta = {"trap": None|"oob_*"|"nulo"|"div0"|"wrap", "oob": bool (sem --checagem o release le/grava fora ou desreferencia nulo: nao comparar)}.

Modo dirigido (1.2.0): GerarPrograma(Semente, Dirigido="abcdefgh") forca os recursos a..h depois do sorteio padrao. Sem Dirigido, nada novo
e sorteado: o programa sai byte a byte igual ao de 1.1.0 (o congelado de 1..2400 continua valido).
Versao: 1.6.0
Changelog: 1.6.0 - o recurso i foi movido para fuzz_otim_gerador_i.py (mixin DirigidoSlp; limite de 800 linhas) e o grupo por ponteiro ganhou uso externo e splat redefinido (so com recurso i: sem Dirigido a saida nao muda).
           1.5.0 - S4 (SIMD): recurso dirigido i (slp): grupos de 4 gravacoes f32 (2 f64) contiguas com arvore ADD/SUB/MUL/DIV isomorfa (cargas contiguas, splat, constante, carga repetida) em array/struct local e por ponteiro, valores especiais nas lanes
           (0, -0, inf, denormal, extremos; NaN so em uma das fontes e impresso como classe), ^[8] sobrepostos nos dois sentidos, carga/escrita de fora, CHAMA no meio, uso externo, splat redefinido, 3 gravacoes e lanes nao contiguas, e trap depois de print.
           So no modo dirigido (i e a ultima letra: o sorteio dos recursos a..h nao muda): sem Dirigido o programa sai igual ao de 1.4.0 (o congelado de 1..2400 continua valido).
           1.4.0- R6a: recurso dirigido h (licm): laco com subexpressao invariante (k = P * 3 + Q) e ramo que a redefine (vivo depois do laco), e vreg lido antes de ser redefinido no laco. So no modo dirigido: sem Dirigido o programa sai igual ao de 1.3.0.
           1.3.0 - R5 (SIMD): recurso dirigido g (losango): min/max/clamp/abs e dois valores no mesmo if em int e i32 (cond ? a : b), min/max real f64/f32 no padrao minss/maxss com NaN, +-0 e inf nas lanes,
           e (metade dos programas) braco com print e indice que estoura DEPOIS do print. So no modo dirigido: sem Dirigido o programa sai igual ao de 1.2.0 (o congelado de 1..2400 continua valido).
           1.2.0 - D0701: modo dirigido com 6 recursos: a zero voltas com divisao e indice dentro (nao roda), b min/if com ramo de trap nunca tomado,
           c alias por ponteiro e indices i/i%3 que coincidem, d subexpressao repetida (/ e % do mesmo operando), e fl_soma (folha com laco
           chamada em laco) + fibd (recursao dupla) com trap de indice em profundidade depois de um print (metade dos programas), f else-if,
           return cedo, break e continue. Divisao por zero na Forja vale 0 (nao e trap): o trap planejado usa indice fora do limite.
           1.1.0 - M7b F3: FeatureSroa, structs locais (Loc com i32/i64/f32/f64, V3 f32, Q4 f64) copiadas, passadas por valor e lidas em laco; [4]f32 com indice constante.
           1.0.0 - M7a F9: primeira versao.
Verificacao: python -I fuzz_otim.py --selftest
"""

import random

from fuzz_otim_gerador_i import DirigidoSlp

MODULO_INT = 1000003
MODULO_I32 = 30011
TAMANHOS = (32, 64)
OPS_SINAL = ("+", "-", "*", "/", "%")
OPS_UINT = ("+", "-", "*", "/", "%", "&", "|", "~", "<<", ">>")
DIVISORES = (3, 5, 7, 9, 16)
TIPOS_REAIS = ("f64", "f32")
TAM_CX = 16
TRAPS_FORA = ("oob_const", "oob_var", "oob_afim", "oob_lista", "nulo")

CABECALHO = """struct Caixa {
    v: int = 0
    w: int = 0
}

struct Grande {
    a: int = 0
    b: int = 0
    c: i32 = 0
    d: int = 0
    e: int = 0
    f: int = 0
    g: int = 0
}

struct Pt {
    x: f64 = 0.0
    y: f64 = 0.0
}

"""

PROC_FAZER = """proc fazer(k: int) -> Grande {
    return Grande{a = k, b = k * 2, c = (k % 50) as i32, g = k % 7}
}
"""

PROC_REC = """proc rec(n: int, k: int) -> int {
    if n < 2 {
        return k % 1000003
    }
    return (rec(n - 1, k + n) * 3 + n) % 1000003
}
"""

PROC_MOV = """proc mov(p: Pt, k: f64) -> Pt {
    return Pt{x = clamp(p.y * 0.5 + k, -1000.0, 1000.0), y = clamp(p.x - k * 0.25, -1000.0, 1000.0)}
}
"""


STRUCTS_SROA = """struct Loc {
    a: i32 = 0
    b: i64 = 0
    c: f32 = 0.0
    d: f64 = 0.0
}

struct V3 {
    x: f32 = 0.0
    y: f32 = 0.0
    z: f32 = 0.0
}

struct Q4 {
    x: f64 = 0.0
    y: f64 = 0.0
    z: f64 = 0.0
    w: f64 = 1.0
}
"""

PROC_LOC_MIX = """proc mistura(p: Loc, k: int) -> Loc {
    return Loc{a = (p.a + (k as i32)) % 30011, b = (p.b * 3 + (k as i64)) % 1000003, c = clamp(p.c * 0.5 + 1.0, -1000.0, 1000.0), d = clamp(p.d * 0.25 + (k as f64), -1000.0, 1000.0)}
}
"""

PROC_SOMA_V3 = """proc somav(a: V3, b: V3) -> V3 {
    return V3{x = a.x + b.x, y = a.y + b.y, z = clamp(a.z * 0.5 + b.z, -1000.0, 1000.0)}
}
"""

PROC_GIRA_Q4 = """proc gira(q: Q4, k: f64) -> Q4 {
    return Q4{x = clamp(q.x + k, -1000.0, 1000.0), y = clamp(q.y - k * 0.5, -1000.0, 1000.0), z = clamp(q.z * 0.5, -1000.0, 1000.0), w = clamp(q.w + q.x * 0.25, -1000.0, 1000.0)}
}
"""

# ---- modo dirigido (1.2.0): so entram no programa quando o recurso e pedido ----
RECURSOS_DIRIGIDOS = "abcdefghi"
RECURSOS_ANTIGOS = "abcdefgh"  # o que "todos" gera: os programas de "todos" ja registrados em achados continuam reproduziveis
# marcadores de cada recurso no codigo gerado (o fuzz conta os programas que os contem para a cobertura)
MARCADORES_DIRIGIDO = {"a": ("zl_n",), "b": ("sel_m",), "c": ("alias_g",), "d": ("sub_x",), "e": ("fl_soma(", "fibd("), "f": ("fl_busca(",), "g": ("ls_marca(",), "h": ("li_inv",), "i": ("var sl_pa",)}
# (1.3.0, recurso g) losango real no padrao minss/maxss; ls_marca mostra +0/-0 (1/x) e NaN (7.0) no print
PROC_LOSANGO = """proc ls_min(a: f64, b: f64) -> f64 {
    var m = b
    if a < b {
        m = a
    }
    return m
}

proc ls_max(a: f64, b: f64) -> f64 {
    var m = b
    if a > b {
        m = a
    }
    return m
}

proc ls_min32(a: f32, b: f32) -> f32 {
    var m: f32 = 0.0
    if a < b {
        m = a
    } else {
        m = b
    }
    return m
}

proc ls_marca(x: f64) -> f64 {
    if x != x {
        return 7.0
    }
    if x == 0.0 {
        return 1.0 / x
    }
    return x
}
"""
PROC_FOLHA_SOMA = """proc fl_soma(n: int, k: int) -> int {
    var s = 0
    for j in 0..<n {
        s = (s + j * k) % 1000003
    }
    return s
}
"""
PROC_FIBD = """proc fibd(n: int, k: int, t: int) -> int {
    if n < 2 {
        if t == 1 {
            print(n + 100)
            let tb: [3]int = {}
            return tb[n + 2]
        }
        return k % 1000003
    }
    return (fibd(n - 1, k, t) + fibd(n - 2, k, t)) % 1000003
}
"""
PROC_BUSCA = """proc fl_busca(n: int, k: int) -> int {
    var s = 0
    for i in 0..<n {
        if (i * k) % 7 == 3 {
            return i + s
        } else if i % 2 == 0 {
            continue
        } else if i > 9 {
            break
        }
        s = (s + i * k) % 1000003
    }
    return s
}
"""

class Gerador(DirigidoSlp):
    def __init__(self, Semente, Dirigido=()):
        Desconhecidos = set(Dirigido) - set(RECURSOS_DIRIGIDOS)
        if Desconhecidos:
            raise ValueError(f"recurso dirigido desconhecido: {sorted(Desconhecidos)} (validos: a..i)")
        self.Dirigido = frozenset(Dirigido)
        self.R = random.Random(Semente)
        self.Decl, self.Corpo, self.Check, self.Procs, self.Trap = [], [], [], [], []
        self.Pool = {"int": [], "i32": [], "uint": [], "f64": [], "f32": []}
        self.Arrays = []  # (nome, tipo)
        self.N = self.R.choice(TAMANHOS)
        self.Meta = {"trap": None, "oob": False}
        self.Contador = 0
        self.Var = "i"
        self.TemStructs = False

    # ---- expressoes -------------------------------------------------
    def Literal(self, T):
        if T in TIPOS_REAIS:
            return f"({self.R.choice(('0.5', '1.5', '2.0', '0.25', '3.0'))} as {T})"
        if T == "uint":  # literal puro seria int e `<<`/`&` so valem em sem sinal (E0303)
            return f"({self.R.randint(0, 99)} as uint)"
        return str(self.R.randint(0, 99))

    def Folha(self, T, Pool, Extra):
        Nomes = list(Pool.get(T, [])) + [E[0] for E in Extra if E[1] == T]
        if Nomes and self.R.random() < 0.8:
            return self.R.choice(Nomes)
        return self.Literal(T)

    def Divisor(self, T, Pool, Extra):
        if self.R.random() < 0.6:
            return str(-3 if T != "uint" and self.R.random() < 0.2 else self.R.choice(DIVISORES))
        return f"({self.Folha(T, Pool, Extra)} % 5 + {3 if T == 'uint' else 7})"

    def Expr(self, T, Pool, Extra=(), Prof=2, Mul=True):
        if Prof <= 0 or self.R.random() < 0.25:
            return self.Folha(T, Pool, Extra)
        if T in TIPOS_REAIS:
            Op = self.R.choice(("+", "-", "*", "/"))
        else:
            Op = self.R.choice(OPS_UINT if T == "uint" else OPS_SINAL)
        if Op == "*" and not Mul:
            Op = "+"
        Esq = self.Expr(T, Pool, Extra, Prof - 1, Mul and Op != "*")
        if Op in ("<<", ">>"):
            return f"({Esq} {Op} {self.R.randint(0, 9)})"
        if Op == "/" and T in TIPOS_REAIS:
            return f"({Esq} / {self.R.choice(('3.0', '7.0', '0.5'))})"
        if Op in ("/", "%"):
            return f"({Esq} {Op} {self.Divisor(T, Pool, Extra)})"
        return f"({Esq} {Op} {self.Expr(T, Pool, Extra, Prof - 1, Mul and Op != '*')})"

    def Mistura(self, T, Pool, Extra):
        """Termo de OUTRO tipo convertido com `as` (limitado antes do cast); '' quando nao ha."""
        if self.R.random() > 0.3:
            return ""
        if T == "int" and Pool.get("i32"):
            return f" + (({self.Folha('i32', Pool, Extra)} % 1000) as int)"
        if T == "i32" and Pool.get("int"):
            return f" + (({self.Folha('int', Pool, Extra)} % 1000) as i32)"
        if T == "uint" and Pool.get("int"):
            return f" + (({self.Folha('int', Pool, Extra)} % 1000 + 1000) as uint)"
        if T == "f64" and Pool.get("int"):
            return f" + (({self.Folha('int', Pool, Extra)} % 100) as f64)"
        if T == "f64" and Pool.get("f32"):
            return f" + ({self.Folha('f32', Pool, Extra)} as f64)"
        if T == "f32" and Pool.get("f64"):
            return f" + ({self.Folha('f64', Pool, Extra)} as f32)"
        return ""

    def Atrib(self, T, Alvo, Pool, Extra=()):
        """`Alvo = ...` com o resultado sempre limitado (o programa nunca estoura sem querer)."""
        E = f"({self.Expr(T, Pool, Extra)}{self.Mistura(T, Pool, Extra)})"
        if T == "int":
            return f"{Alvo} = {E} % {MODULO_INT}"
        if T == "i32":
            return f"{Alvo} = {E} % {MODULO_I32}"
        if T == "uint":
            return f"{Alvo} = {E}"
        return f"{Alvo} = clamp({E}, -1000.0, 1000.0)"

    def Cond(self, Pool, Extra=()):
        T = self.R.choice([K for K in ("int", "i32", "uint") if Pool.get(K)] or ["int"])
        Op = self.R.choice(("<", ">", "<=", ">=", "==", "!="))
        Esq, Dir = self.Expr(T, Pool, Extra, 1), self.Expr(T, Pool, Extra, 1)
        if Esq == Dir:  # o backend C recusa `x > x` (-Werror=tautological-compare): achado do fuzzer em 17/11, fora do escopo do otimizador
            Dir = f"({Dir} + 1)"
        return f"{Esq} {Op} {Dir}"

    def Imprimir(self, T, Expr):
        if T == "i32":
            return f"print({Expr} as int)"
        if T == "uint":
            return f"print(({Expr} % {MODULO_INT}) as int)"
        return f"print({Expr})"

    # ---- escalares ---------------------------------------------------
    def NovoEscalar(self, T, Prefixo):
        Nome = f"{Prefixo}{len(self.Pool[T])}"
        self.Decl.append(f"    var {Nome}: {T} = {self.Literal(T)}")
        self.Pool[T].append(Nome)
        self.Check.append(f"    {self.Imprimir(T, Nome)}")
        if T == "uint":
            self.Check.append(f"    print(({Nome} >> 32) as int)")
        return Nome

    def FeatureEscalares(self):
        for T, Pref in (("int", "n"), ("i32", "s"), ("uint", "u"), ("f64", "f"), ("f32", "g")):
            if T != "int" and self.R.random() < 0.3:
                continue
            for _ in range(self.R.randint(1, 3)):
                Nome = self.NovoEscalar(T, Pref)
                self.Corpo.append(f"    {self.Atrib(T, Nome, self.Pool)}")
        for _ in range(self.R.randint(2, 6)):
            self.CorpoEscalar("    ", ())

    def CorpoEscalar(self, Ind, Extra):
        T = self.R.choice([K for K in ("int", "i32", "uint", "f64") if self.Pool[K]])
        Alvo = self.R.choice(self.Pool[T])
        if self.R.random() < 0.3:
            self.Corpo.append(f"{Ind}if {self.Cond(self.Pool, Extra)} {{")
            self.Corpo.append(f"{Ind}    {self.Atrib(T, Alvo, self.Pool, Extra)}")
            self.Corpo.append(f"{Ind}}} else {{")
            self.Corpo.append(f"{Ind}    {self.Atrib(T, Alvo, self.Pool, Extra)}")
            self.Corpo.append(f"{Ind}}}")
        else:
            self.Corpo.append(f"{Ind}{self.Atrib(T, Alvo, self.Pool, Extra)}")
        if self.R.random() < 0.12:
            self.Corpo.append(f"{Ind}{self.Imprimir(T, Alvo)}")

    # ---- arrays e lacos -----------------------------------------------
    def Indice(self, Limite):
        """Texto do indice sempre dentro de [0, N) para a variavel do laco em 0..<Limite: i ou i*C+k."""
        V = self.Var
        if self.R.random() < 0.4:
            return V
        C, K = self.R.choice((1, 2, 3)), self.R.randint(0, 3)
        while (Limite - 1) * C + K >= self.N:
            C, K = max(1, C - 1), 0
        return V if (C, K) == (1, 0) else f"{V} * {C} + {K}"

    def FeatureArrays(self):
        for Nome, T in (("arr_i", "int"), ("arr_s", "i32")):
            if Nome == "arr_s" and self.R.random() < 0.4:
                continue
            Cast = " as i32" if T == "i32" else ""
            self.Decl.append(f"    var {Nome}: [{self.N}]{T} = {{}}")
            self.Corpo += [f"    for i in 0..<{self.N} {{", f"        {Nome}[i] = ((i * {self.R.randint(1, 9)} + {self.R.randint(0, 9)}) % 1000){Cast}", "    }"]
            Soma = f"ac_{Nome}"
            Elem = f"({Nome}[i] as int)" if T == "i32" else f"{Nome}[i]"
            self.Check += [f"    var {Soma} = 0", f"    for i in 0..<{self.N} {{", f"        {Soma} = {Soma} + {Elem}", "    }", f"    print({Soma})"]
            self.Arrays.append((Nome, T))
        for _ in range(self.R.randint(2, 5)):
            self.Laco()

    def Laco(self):
        Tipo = self.R.choice(("const", "const", "var", "while", "aninhado"))
        Limite = self.R.randint(2, min(self.N // 2, 20))
        self.Var = "i"
        if Tipo == "const":
            self.Corpo.append(f"    for i in 0..<{Limite} {{")
        elif Tipo == "var":
            self.Corpo += [f"    let lim{self.Contador} = ({self.R.choice(self.Pool['int'])} % 5) + 6", f"    for i in 0..<lim{self.Contador} {{"]
            Limite = 10
        elif Tipo == "while":
            self.Var = f"w{self.Contador}"
            self.Corpo += [f"    var {self.Var} = 0", f"    while {self.Var} < {Limite} {{"]
        else:
            Limite = min(Limite, 8)
            self.Corpo.append(f"    for i in 0..<{Limite} {{")
        self.Contador += 1
        self.CorpoLaco(Limite, Tipo)

    def CorpoLaco(self, Limite, Tipo):
        Ind = "        "
        self.Corpo.append(f"{Ind}{self.Atrib('int', f'arr_i[{self.Var}]', self.Pool)}")
        for _ in range(self.R.randint(1, 4)):
            Extra = [(f"{N}[{self.Indice(Limite)}]", T) for N, T in self.Arrays]
            Nome, T = self.R.choice(self.Arrays)
            if self.R.random() < 0.5:
                self.Corpo.append(f"{Ind}{self.Atrib(T, f'{Nome}[{self.Indice(Limite)}]', self.Pool, Extra)}")
            else:
                self.CorpoEscalar(Ind, Extra)
        if Tipo == "aninhado":
            self.Corpo += ["        for j in 0..<4 {", f"            arr_i[i * 4 + j] = (arr_i[i * 4 + j] + i * j) % {MODULO_INT}", "        }"]
        if Tipo == "while":
            self.Corpo.append(f"{Ind}{self.Var} = {self.Var} + 1")
        self.Corpo.append("    }")

    # ---- structs, procs e listas ---------------------------------------
    def ProcsStructs(self):
        P1 = {"int": ["a.v", "a.w", "k"]}
        P2 = {"int": ["b.a", "b.b", "b.d", "b.e", "b.f", "b.g", "k"], "i32": ["b.c"]}
        P3 = {"int": ["b.a", "b.g", "k"]}
        self.Procs += [
            PROC_FAZER,
            f"proc le(a: Caixa, k: int) -> int {{\n    return {self.Expr('int', P1)} % {MODULO_INT}\n}}\n",
            f"proc somag(b: Grande, k: int) -> int {{\n    return ({self.Expr('int', P2)} + (b.c as int)) % {MODULO_INT}\n}}\n",
            f"proc viaptr(b: ^Grande, k: int) -> int {{\n    b.a = ({self.Expr('int', P3)}) % 9973\n    b.g = (b.g + k) % 9973\n    return b.a + b.g\n}}\n",
        ]

    def FeatureStructs(self):
        self.TemStructs = True
        self.ProcsStructs()
        self.Decl += [f"    var cx: [{TAM_CX}]Caixa = {{}}", "    var bx = Caixa{v = 1, w = 2}", "    var gr = Grande{a = 1, b = 2, c = 3}",
                      "    var lg = make([dynamic]Grande, alloc = heap)", "    defer delete(lg)"]
        self.Corpo += ["    for k in 0..<6 {", "        append(&lg, item = fazer(k * 3))", "    }", "    bx.w = (bx.w * 2) % 100", "    gr.d = gr.d + 5"]
        for _ in range(self.R.randint(1, 3)):
            self.CorpoStruct()
        self.Check += ["    print(bx.v + bx.w)", "    print(gr.a + gr.b + gr.d + gr.g + (gr.c as int))", f"    print(cx[0].v + cx[{TAM_CX - 1}].w)", "    print(lg[0].a + lg[5].g)"]

    def CorpoStruct(self):
        N = self.R.choice(self.Pool["int"])
        Linhas = [f"{N} = ({N} + le(cx[i], i)) % {MODULO_INT}",
                  f"{N} = ({N} + le(bx, {N} % 7)) % {MODULO_INT}",
                  f"{N} = ({N} + somag(gr, i) + somag(lg[i % 6], 2)) % {MODULO_INT}",
                  f"{N} = ({N} + somag(fazer(i), {N} % 5)) % {MODULO_INT}",
                  f"{N} = ({N} + viaptr(&gr, i)) % {MODULO_INT}",
                  f"bx.v = (bx.v + i + {N} % 3) % 100",
                  "lg[i % 6].a = (lg[i % 6].a + i) % 9973"]
        Sel = self.R.sample(Linhas, self.R.randint(2, 5))
        self.Corpo.append(f"    for i in 0..<{self.R.choice((6, 12, TAM_CX))} {{")
        self.Corpo.append(f"        cx[i] = Caixa{{v = i * {self.R.randint(1, 5)}, w = ({N} + i) % 9}}")
        self.Corpo += [f"        {L}" for L in Sel]
        if self.R.random() < 0.3:
            self.Corpo.append(f"        print({N})")
        self.Corpo.append("    }")

    def FeatureProcs(self):
        N = self.R.choice(self.Pool["int"])
        self.Procs.append(PROC_REC)
        self.Corpo.append(f"    {N} = ({N} + rec({self.R.randint(2, 6)}, {N} % 100)) % {MODULO_INT}")

    # ---- reais: soa, arrays e struct de f64 ---------------------------
    def FeatureReais(self):
        if not self.Pool["f64"]:
            Nome = self.NovoEscalar("f64", "f")
            self.Corpo.append(f"    {self.Atrib('f64', Nome, self.Pool)}")
        self.Procs.append(PROC_MOV)
        F = self.R.choice(self.Pool["f64"])
        N = self.R.choice(self.Pool["int"])
        self.Decl += ["    var sf: soa[8]Pt = {}", "    var pt = Pt{x = 1.0, y = 2.0}", "    var ar_f: [8]f64 = {}"]
        self.Corpo += ["    for i in 0..<8 {", "        sf.x[i] = (i as f64) * 0.5",
                       f"        sf.y[i] = clamp(sf.x[i] * 3.0 - {F}, -1000.0, 1000.0)",
                       f"        ar_f[i] = clamp((sf.y[i] + sf.x[i]) / 7.0 + {F}, -1000.0, 1000.0)",
                       "        pt = mov(pt, ar_f[i] * 0.125)",
                       f"        {N} = ({N} + (clamp(ar_f[i], -1000.0, 1000.0) as int)) % {MODULO_INT}", "    }"]
        self.Check += ["    print(sf.y[3])", "    print(ar_f[7])", "    print(pt.x + pt.y)"]

    # ---- structs locais e Vec3/quat por valor (alvo do passe sroa) ----------
    def FeatureSroa(self):
        """Structs locais copiadas, passadas por valor e lidas em laco; [4]f32 so com indice constante. Sempre dentro de limites."""
        self.Procs += [STRUCTS_SROA, PROC_LOC_MIX, PROC_SOMA_V3, PROC_GIRA_Q4]
        N = self.R.choice(self.Pool["int"])
        self.Decl += ["    var lc = Loc{a = 1, b = 2, c = 1.5, d = 0.25}", "    var lc2 = lc", "    var v3 = V3{x = 1.0, y = 2.0, z = 3.0}",
                      "    var q4 = Q4{w = 1.0}", "    var ar4: [4]f32 = {1.0, 2.0, 3.0, 4.0}"]
        self.Corpo += ["    for i in 0..<12 {", "        lc = mistura(lc, i)", "        lc2.a = (lc2.a + lc.a + (i as i32)) % 30011",
                       "        v3 = somav(v3, V3{x = 0.5, y = 0.25, z = (i as f32) * 0.125})", "        q4 = gira(q4, (i as f64) * 0.125)",
                       "        ar4[0] = clamp(ar4[0] + v3.z * 0.25, -1000.0, 1000.0)", "        ar4[3] = clamp(ar4[3] * 0.5 + v3.x, -1000.0, 1000.0)",
                       f"        {N} = ({N} + (lc2.a as int)) % {MODULO_INT}", "    }"]
        self.Check += ["    print((lc.a + lc2.a) as int)", "    print((lc.c * 100.0) as int)", "    print((lc2.d * 100.0) as int)", "    print((lc.b % 1000003) as int)",
                       "    print((v3.z * 100.0) as int)", "    print((q4.x * 100.0) as int)", "    print((q4.w * 100.0) as int)", "    print((ar4[0] * 100.0) as int)", "    print((ar4[3] * 100.0) as int)"]

    # ---- traps planejados (sempre depois de um print) -----------------------
    def Elemento(self, Arr, Idx):
        T = dict(self.Arrays)[Arr]
        return f"({Idx} * 3) as i32" if T == "i32" else f"{Idx} * 3"

    def CodigoTrap(self, Tipo):
        N, Arr = self.N, self.R.choice(self.Arrays)[0]
        N0 = self.Pool["int"][0]
        if Tipo == "oob_const":
            return [f"for i in 0..<{N + self.R.randint(1, 4)} {{", "    print(i)", f"    {Arr}[i] = {self.Elemento(Arr, 'i')}", "}"]
        if Tipo == "oob_var":
            return ["var j = 0", f"while j < {N + 3} {{", "    print(j)", f"    {Arr}[j] = {self.Elemento(Arr, 'j')}", "    j = j + 1", "}"]
        if Tipo == "oob_afim":
            return [f"for i in 0..<{N // 2 + 2} {{", "    print(i)", f"    {Arr}[i * 2 + 1] = {self.Elemento(Arr, 'i')}", "}"]
        if Tipo == "oob_lista":
            return ["for i in 0..<9 {", "    print(i)", f"    {N0} = ({N0} + lg[i].a) % {MODULO_INT}", "}"]
        if Tipo == "nulo":
            return ["var p: ?^Caixa = none", "for i in 0..<10 {", "    if i < 3 {", "        p = &cx[0]", "    } else {", "        p = none", "    }",
                    "    print(i)", "    let q = p as ^Caixa", f"    {N0} = ({N0} + q.v * 7 + i) % {MODULO_INT}", "}"]
        if Tipo == "div0":
            return [f"let z = {N0} % 1", f"print({N0} + 1)", f"print({N0} / z)", f"print({N0} % z)"]
        return ["var k: i32 = 2147483000", "for i in 0..<5 {", "    print(i)", "    k = k + 300", "}", "print(k)"]

    def FeatureTrap(self):
        Tipo = self.R.choice(("oob_const", "oob_var", "oob_afim", "oob_lista", "nulo", "div0", "wrap"))
        if Tipo in ("oob_lista", "nulo") and not self.TemStructs:
            Tipo = "oob_const"
        self.Trap = ["    " + L for L in self.CodigoTrap(Tipo)]
        self.Meta["trap"] = Tipo
        self.Meta["oob"] = Tipo in TRAPS_FORA

    # ---- modo dirigido (1.2.0): recursos forcados, sem sorteio quando Dirigido e vazio ----
    def FeaturesDirigidas(self):
        """Forca os recursos pedidos depois do sorteio padrao (cada um com a sua garantia de determinismo e de termino)."""
        Metodos = {"a": self.DirZeroVoltas, "b": self.DirSelecao, "c": self.DirAlias, "d": self.DirSubexpressao, "e": self.DirFolhaRec, "f": self.DirControle,
                   "g": self.DirLosango, "h": self.DirInvariante, "i": self.DirSlp}
        for Letra in RECURSOS_DIRIGIDOS:
            if Letra in self.Dirigido:
                Metodos[Letra]()

    def DirZeroVoltas(self):
        """(a) laco de zero voltas: o limite vale 0 em runtime, entao a divisao e o indice fora do limite do corpo nunca rodam."""
        P = self.R.choice(self.Pool["int"])
        self.Corpo += [f"    let zl_n = ({P} % 1)", "    for i in 0..<zl_n {", f"        {P} = {P} / zl_n", f"        arr_i[i + {self.N + 2}] = 1", "    }"]

    def DirSelecao(self):
        """(b) min pelo if/else; ramo de indice fora que nunca e tomado (valores < 1000003) e divisao por zero no outro ramo (vale 0)."""
        P, Q = self.R.choice(self.Pool["int"]), self.R.choice(self.Pool["int"])
        if Q == P:  # o backend C recusa `x < x` (-Werror=tautological-compare): mesmo achado do Cond(), 17/11
            Q = f"({P} + 1)"
        self.Corpo += ["    var sel_m: int = 0", f"    if {P} < {Q} {{", f"        sel_m = {P}", "    } else {", f"        sel_m = {Q}", "    }",
                       "    if sel_m > 2000000 {", "        sel_m = arr_i[sel_m]", "    } else {", f"        sel_m = sel_m / ({Q} % 1)", "    }"]
        self.Check.append("    print(sel_m)")

    def DirAlias(self):
        """(c) escrita por um nome e leitura por outro: store->carga por ponteiro; os indices i e i % 3 coincidem para i < 3."""
        P = self.R.choice(self.Pool["int"])
        self.Corpo += ["    var alias_a: [8]int = {}", "    var alias_g = Grande{a = 1, b = 2}", "    var alias_p: ^Grande = &alias_g",
                       "    for i in 0..<6 {", "        let alias_j = i % 3", f"        alias_a[i] = {P} + i", "        alias_a[alias_j] = alias_a[alias_j] + 5",
                       "        alias_p.a = (alias_p.a + alias_g.b + i) % 9973", f"        {P} = ({P} + alias_a[i] + alias_g.a) % {MODULO_INT}", "    }"]
        self.Check += ["    print(alias_g.a)", "    print(alias_a[1] + alias_a[4])"]

    def DirSubexpressao(self):
        """(d) a mesma conta, com / e % do mesmo operando, em dois ramos e depois do if; divisor constante nao zero."""
        P, Q, R = self.R.choice(self.Pool["int"]), self.R.choice(self.Pool["int"]), self.R.choice(self.Pool["int"])
        D = self.R.choice(DIVISORES)
        Conta = f"({P} * {Q} + {R})"
        Cmp = Q if Q != P else f"({P} + 1)"  # gcc recusa `x < x` (tautological-compare), igual ao Cond()
        self.Corpo += ["    var sub_x: int = 0", f"    if {P} < {Cmp} {{", f"        sub_x = ({Conta} / {D}) % {MODULO_INT}", "    } else {",
                       f"        sub_x = (({Conta} % {D}) + ({Conta} / {D})) % {MODULO_INT}", "    }",
                       f"    sub_x = (sub_x + {Conta} / {D} + {Conta} % {D}) % {MODULO_INT}"]
        self.Check.append("    print(sub_x)")

    def DirFolhaRec(self):
        """(e) folha com laco chamada dentro de laco; recursao dupla; em metade dos programas, trap de indice fora DEPOIS de um print, no fim."""
        P = self.R.choice(self.Pool["int"])
        self.Procs += [PROC_FOLHA_SOMA, PROC_FIBD]
        self.Corpo += ["    for i in 0..<4 {", "        for j in 0..<3 {", f"            {P} = ({P} + fl_soma(j + i, i)) % {MODULO_INT}", "        }", "    }",
                       f"    {P} = ({P} + fibd({self.R.randint(4, 7)}, {P} % 50, 0)) % {MODULO_INT}"]
        if self.R.random() < 0.5:
            self.Trap.append(f"    print(fibd(3, {P} % 50, 1))")
            self.Meta["oob"] = True
            if self.Meta["trap"] is None:
                self.Meta["trap"] = "fibd_prof"

    def DirControle(self):
        """(f) else-if em cadeia, return cedo dentro de laco (fl_busca), break e continue num laco de main."""
        P = self.R.choice(self.Pool["int"])
        self.Procs.append(PROC_BUSCA)
        self.Corpo += [f"    {P} = ({P} + fl_busca(12, {P} % 50)) % {MODULO_INT}", "    for i in 0..<12 {", "        if i == 5 {", "            continue",
                       "        } else if i > 9 {", "            break", "        }", f"        {P} = ({P} + i) % {MODULO_INT}", "    }",
                       f"    if {P} % 3 == 0 {{", f"        {P} = ({P} + 2) % {MODULO_INT}", f"    }} else if {P} % 3 == 1 {{",
                       f"        {P} = ({P} - 1) % {MODULO_INT}", "    } else {", f"        {P} = ({P} + 7) % {MODULO_INT}", "    }"]

    def DirLosango(self):
        """(g) losango puro (passe `selecao`, R5): min/max/clamp/abs e dois valores no mesmo if em inteiro (int e i32); min/max real com NaN e +-0 nas lanes
        (f64 e f32); em metade dos programas um braco com print e indice que estoura (trap DEPOIS do print, o losango nao pode virar selecao)."""
        P, Q = self.R.choice(self.Pool["int"]), self.R.choice(self.Pool["int"])
        Cmp = Q if Q != P else f"({P} + 1)"  # gcc recusa `x < x` (tautological-compare), igual ao Cond()
        self.Procs.append(PROC_LOSANGO)
        self.Corpo += ["    var ls_max: int = 0", f"    if {P} > {Cmp} {{", f"        ls_max = {P}", "    } else {", f"        ls_max = {Cmp}", "    }",
                       f"    var ls_clamp: int = {P}", f"    if ls_clamp < {self.R.randint(0, 40)} {{", "        ls_clamp = 17", "    }",
                       f"    if ls_clamp > {self.R.randint(500, 900)} {{", "        ls_clamp = 333", "    }",
                       f"    var ls_abs: int = {P} - {Cmp}", "    if ls_abs < 0 {", "        ls_abs = 0 - ls_abs", "    }",
                       f"    var ls_lo: int = {P}", f"    var ls_hi: int = {Cmp}", f"    if {P} > {Cmp} {{", f"        ls_lo = {Cmp}", f"        ls_hi = {P}", "    }"]
        if self.Pool["i32"]:
            S = self.R.choice(self.Pool["i32"])
            self.Corpo += [f"    var ls_s: i32 = {S}", f"    if ls_s > {self.R.randint(0, 900)} {{", "        ls_s = 5", "    }"]
            self.Check.append("    print(ls_s as int)")
        self.Corpo += [f"    let ls_z: f64 = ({P} % 1) as f64", "    var ls_v: [6]f64 = {}", "    ls_v[0] = 1.5", "    ls_v[1] = -2.5", "    ls_v[2] = ls_z",
                       "    ls_v[3] = -ls_z", "    ls_v[4] = ls_z / ls_z", "    ls_v[5] = 1.0 / ls_z"]
        self.Check += ["    print(ls_max + ls_clamp + ls_abs + ls_lo * 3 + ls_hi)", "    for ls_i in 0..<6 {", "        for ls_j in 0..<6 {",
                       "            print(ls_marca(ls_min(ls_v[ls_i], ls_v[ls_j])))", "            print(ls_marca(ls_max(ls_v[ls_i], ls_v[ls_j])))",
                       "            print(ls_marca(ls_min32(ls_v[ls_i] as f32, ls_v[ls_j] as f32) as f64))", "        }", "    }"]
        if self.R.random() < 0.5:
            self.Trap += [f"    for ls_k in 0..<{self.N + 3} {{", "        var ls_r = 0", f"        if ls_k < {self.N + 1} {{", "            print(ls_k)",
                          "            ls_r = arr_i[ls_k + 1]", "        } else {", "            ls_r = ls_k", "        }", f"        {P} = ({P} + ls_r) % {MODULO_INT}", "    }"]
            self.Meta["oob"] = True
            if self.Meta["trap"] is None:
                self.Meta["trap"] = "losango_oob"

    def DirInvariante(self):
        """(h) passe `licm` (R6a): subexpressao pura invariante do laco (li_inv, li_k), um ramo que redefine li_t (vivo depois do laco) e li_u, lido antes de ser redefinido."""
        P, Q = self.R.choice(self.Pool["int"]), self.R.choice(self.Pool["int"])
        Voltas = self.R.randint(0, 6)
        self.Corpo += ["    var li_t: int = 5", "    var li_u: int = 9", "    var li_acc: int = 0", f"    for li_i in 0..<{Voltas} {{", f"        let li_inv = {P} * 3 + {Q}",
                       "        let li_k = li_inv * 7 + 1", f"        li_acc = (li_acc + li_k + li_u + li_i) % {MODULO_INT}", f"        li_u = {P} * 2 + 1",
                       "        if li_i == 3 {", "            li_t = li_k + 1", "        }", "    }"]
        self.Check += ["    print(li_acc)", "    print(li_t)", "    print(li_u)"]

    # ---- montagem --------------------------------------------------------
    def Montar(self):
        self.FeatureEscalares()
        self.FeatureArrays()
        if self.R.random() < 0.75:
            self.FeatureStructs()
            if self.R.random() < 0.6:
                self.FeatureProcs()
        if self.R.random() < 0.5:
            self.FeatureReais()
        if self.R.random() < 0.5:
            self.FeatureSroa()
        if self.R.random() < 0.5:
            self.FeatureTrap()
        self.FeaturesDirigidas()
        Cabeca = CABECALHO + "\n".join(self.Procs) + ("\n" if self.Procs else "")
        Main = ["proc main() {"] + self.Decl + self.Corpo + self.Check + self.Trap + ["}", ""]
        return Cabeca + "\n".join(Main), dict(self.Meta)


def GerarPrograma(Semente, Dirigido=()):
    return Gerador(Semente, Dirigido).Montar()
