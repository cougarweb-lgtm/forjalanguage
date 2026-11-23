# -*- coding: utf-8 -*-
"""Recurso dirigido i (passe `slp`) do gerador do fuzzer diferencial do otimizador: texto .fj, arvores e os metodos DirSlp (mixin de Gerador).

Movido de fuzz_otim_gerador.py 1.5.0 para cumprir <=800 linhas por arquivo. Sem recurso i a saida do gerador nao muda.
Versao: 1.1.1
Changelog: 1.1.1 - splat redefinido ENTRE as gravacoes (kv = kv*0.5+1.0 depois de uma lane, as outras leem o valor novo): a forma "contas antes" era equivalente (o vetor sai antes da redefinicao).
           1.1.0 - provas de sensibilidade do slp_seguro.c: CHAMA sl_le (le o destino por ponteiro logo depois da 1a gravacao e imprime: apanha BIR_CHAMA fora das barreiras) e print do splat
           redefinido (kv) depois do grupo (a redefinicao fica viva: apanha PendentesIguais sempre true). Sem recurso i a saida do gerador nao muda.
           1.0.0 - modulo proprio do recurso i. Novidade sobre o corpo da 1.5.0: o grupo por ponteiro (^[8], que o sroa nao escalariza e o slp vetoriza)
           ganha as variantes uso externo (valor de uma lane lido depois do grupo: uso_externo) e splat redefinido antes das gravacoes (valor_redefinido),
           porque o grupo local, agora recusado antes por slot_escalarizavel, nao chegava mais a esses motivos.
Verificacao: python -I fuzz_otim.py --selftest
"""

MODULO_INT = 1000003  # igual ao de fuzz_otim_gerador.py

# ---- grupos SLP; sl_cls* imprime os bits, mas NaN vira 7 (o escalar release troca operandos de ADD/MUL, o vetor nao: o payload do NaN pode diferir) ----
STRUCTS_SLP = """struct SlV4 {
    x: f32 = 0.0
    y: f32 = 0.0
    z: f32 = 0.0
    w: f32 = 0.0
}

struct SlV2 {
    x: f64 = 0.0
    y: f64 = 0.0
}
"""
PROCS_SLP = """proc sl_cls32(v: f32) -> int {
    if v != v {
        return 7
    }
    return transmute(v, to = u32) as int
}

proc sl_cls64(v: f64) -> int {
    if v != v {
        return 7
    }
    return transmute(v, to = u64) as int
}

proc sl_pega32(v: []f32, k: int) -> f32 {
    return v[k]
}

proc sl_pega64(v: []f64, k: int) -> f64 {
    return v[k]
}

proc sl_ruido32(x: f32) {
    print(sl_cls32(x * 2.0))
}

proc sl_ruido64(x: f64) {
    print(sl_cls64(x * 2.0))
}

proc sl_le32(d: ^[8]f32, j: int) {
    print(sl_cls32(d^[j]))
}

proc sl_le64(d: ^[8]f64, j: int) {
    print(sl_cls64(d^[j]))
}
"""
OPS_SLP = ("+", "-", "*", "/")
CAMPOS_SLP = ("x", "y", "z", "w")
SUFIXO_SLP = {"f32": "32", "f64": "64"}
LARGURA_SLP = {"f32": 4, "f64": 2}
DESLOCAMENTOS_SOBREPOSTOS = ((0, 1), (1, 0), (0, 2), (2, 1), (0, 0), (1, 1))
LANES_NEGATIVAS = {"f32": ([0, 1, 2], [0, 2, 4, 6], [0, 1, 3]), "f64": ([0, 1, 2], [0, 2])}
POOL_A_ESPECIAL = ("1.5", "{z}", "-{z}", "1.0 / {z}", "(0.0 - 1.0) / {z}", "{z} / {z}", "{d}", "{g}")  # a unica fonte com NaN
POOL_B_ESPECIAL = ("2.5", "{z}", "-{z}", "1.0 / {z}", "(0.0 - 1.0) / {z}", "0.5", "{d}", "3.0")
POOL_A_NORMAL = ("1.5", "-2.5", "3.0", "0.5", "2.0", "-0.75", "4.0", "1.25")
POOL_B_NORMAL = ("2.5", "1.5", "0.5", "3.0", "-1.25", "2.0", "0.75", "4.0")
SLP_VEZES_DENORMAL = {"f32": 135, "f64": 1070}
SLP_VEZES_EXTREMO = {"f32": 126, "f64": 1000}


def FolhaSlp(R):
    Tipo = R.choice(("A", "A", "B", "B", "S", "C"))
    return ("C", R.choice(("2.5", "0.5", "3.0", "1.25"))) if Tipo == "C" else (Tipo,)


def ArvoreSlp(R, Prof):
    """Arvore isomorfa: no = (op, esq, dir), folha = ("A",) carga da fonte A, ("B",) da B, ("S",) splat, ("C", literal)."""
    if Prof <= 0 or R.random() < 0.25:
        return FolhaSlp(R)
    return (R.choice(OPS_SLP), ArvoreSlp(R, Prof - 1), ArvoreSlp(R, Prof - 1))


def TemFolhaSlp(Arv, Tipo):
    if len(Arv) == 3:
        return TemFolhaSlp(Arv[1], Tipo) or TemFolhaSlp(Arv[2], Tipo)
    return Arv[0] == Tipo


def ArvoreSlpCom(R, Exigidas):
    """Arvore com raiz em no e pelo menos uma folha de cada tipo exigido (A sempre)."""
    Arv = ArvoreSlp(R, 2)
    if len(Arv) != 3:
        Arv = (R.choice(OPS_SLP), Arv, FolhaSlp(R))
    for Tipo in ("A",) + tuple(Exigidas):
        if not TemFolhaSlp(Arv, Tipo):
            Arv = (R.choice(OPS_SLP), Arv, (Tipo,))
    return Arv


def RenderSlp(Arv, Folha):
    if len(Arv) == 3:
        return f"({RenderSlp(Arv[1], Folha)} {Arv[0]} {RenderSlp(Arv[2], Folha)})"
    return Folha(*Arv)


def LinhasPoolSlp(P, Tipos, Especial, Trap):
    """Pools de valores por tipo: sl_pa (com NaN quando Especial) e sl_pb (sem NaN); extremos e denormal calculados em runtime a partir de um zero opaco.
    sl_acc e var so com trap (so ele o altera; var nunca alterada e E0707)."""
    Linhas = [f"    {'var' if Trap else 'let'} sl_acc: int = 0"]
    for T in sorted(Tipos):
        W = SUFIXO_SLP[T]
        Nomes = {"z": f"sl_z{W}", "d": f"sl_d{W}", "g": f"sl_g{W}"}
        if Especial:  # z/d/g so existem (e so sao lidos) quando os pools especiais os usam: let/var sem leitura e E0708
            Linhas += [f"    let sl_z{W}: {T} = ({P} % 1) as {T}", f"    var sl_d{W}: {T} = 1.0", f"    for sl_h in 0..<{SLP_VEZES_DENORMAL[T]} {{", f"        sl_d{W} = sl_d{W} * 0.5", "    }",
                       f"    var sl_g{W}: {T} = 1.0", f"    for sl_h in 0..<{SLP_VEZES_EXTREMO[T]} {{", f"        sl_g{W} = sl_g{W} * 2.0", "    }"]
        for Pool, Esp, Normal in (("sl_pa", POOL_A_ESPECIAL, POOL_A_NORMAL), ("sl_pb", POOL_B_ESPECIAL, POOL_B_NORMAL)):
            Linhas.append(f"    var {Pool}{W}: [8]{T} = {{}}")
            Linhas += [f"    {Pool}{W}[{K}] = {(Esp if Especial else Normal)[K].format(**Nomes)}" for K in range(8)]
    return Linhas


class DirigidoSlp:
    """Metodos do recurso i; usa self.R, self.Pool, self.Procs, self.Corpo, self.Check e self.Meta do Gerador."""

    def DirSlp(self):
        """(i) passe `slp` (S4): 2 a 4 grupos de gravacoes contiguas com arvore isomorfa (array/struct local ou por ponteiro) e as variantes que ele recusa; em 40% dos programas o ultimo
        grupo e local e leva um trap de indice DEPOIS do print no mesmo bloco. NaN so na fonte A e impresso como classe (sl_cls)."""
        P, Especial, Trap, Total, Tipos, Grupos = self.R.choice(self.Pool["int"]), self.R.random() < 0.7, self.R.random() < 0.4, self.R.randint(2, 4), set(), []
        self.Procs += [STRUCTS_SLP, PROCS_SLP]
        for Id in range(Total):
            T = self.R.choice(("f32", "f32", "f64"))
            Tipos.add(T)
            ComTrap = Trap and Id == Total - 1
            Grupos += self.SlpLocal(Id, T, ComTrap) if ComTrap or self.R.random() < 0.3 else self.SlpPonteiro(Id, T)
        self.Corpo += LinhasPoolSlp(P, Tipos, Especial, Trap) + Grupos
        self.Check.append("    print(sl_acc)")
        self.Check += [f"    print(sl_cls{SUFIXO_SLP[T]}(sl_pa{SUFIXO_SLP[T]}[1] + sl_pb{SUFIXO_SLP[T]}[2]))" for T in sorted(Tipos)]  # garante a leitura dos dois pools (E0708)
        if Trap:
            self.Meta["oob"] = True
            self.Meta["slp_trap"] = True
            if self.Meta["trap"] is None:
                self.Meta["trap"] = "slp_oob"

    def LanesSlp(self, T, Negativa):
        """Lanes gravadas: as contiguas de 0 a N-1 (grupo cheio) ou, como negativo, 3 gravacoes ou lanes nao contiguas."""
        return list(self.R.choice(LANES_NEGATIVAS[T])) if Negativa else list(range(LARGURA_SLP[T]))

    def SlpLocal(self, Id, T, ComTrap):
        """Grupo em array [8] ou struct local (SlV4/SlV2): fontes sl_a/sl_b copiadas do pool, splat sl_sp, destino sl_r. Variantes: uso externo, splat redefinido, CHAMA no meio."""
        R, W, Ind = self.R, SUFIXO_SLP[T], "        "
        Lanes = self.LanesSlp(T, not ComTrap and R.random() < 0.12)
        Estr = not ComTrap and Lanes == list(range(LARGURA_SLP[T])) and R.random() < 0.35
        Uso, Redef, Chama = R.random() < 0.15, R.random() < 0.15, R.random() < 0.15
        Arv, Desloc, Cls = ArvoreSlpCom(R, ("S",) if Redef else ()), R.randint(0, 5), "SlV4" if T == "f32" else "SlV2"
        Ref = (lambda N, J: f"{N}.{CAMPOS_SLP[J]}") if Estr else (lambda N, J: f"{N}[{J}]")
        Linhas = [f"    for sl_i in 0..<{10 if ComTrap else 4} {{"]
        for Fonte, Mais in (("a", 0), ("b", 1)):
            Pool = f"sl_p{Fonte}{W}"
            if not TemFolhaSlp(Arv, Fonte.upper()):  # variavel sem leitura e E0708
                continue
            if Estr:
                Campos = ", ".join(f"{CAMPOS_SLP[J]} = {Pool}[(sl_i + {J + Desloc + Mais}) % 8]" for J in Lanes)
                Linhas.append(f"{Ind}let sl_{Fonte} = {Cls}{{{Campos}}}")
            else:
                Linhas.append(f"{Ind}var sl_{Fonte}: [8]{T} = {{}}")
                Linhas += [f"{Ind}sl_{Fonte}[{J}] = {Pool}[(sl_i + {J + Desloc + Mais}) % 8]" for J in Lanes]
        Linhas.append(f"{Ind}var sl_r = {Cls}{{}}" if Estr else f"{Ind}var sl_r: [8]{T} = {{}}")
        if Chama or TemFolhaSlp(Arv, "S"):
            Linhas.append(f"{Ind}{'var' if Redef else 'let'} sl_sp: {T} = sl_pb{W}[(sl_i * 3 + 1) % 8]")
        PosUso = R.randrange(len(Lanes))
        Textos = [RenderSlp(Arv, lambda Tp, Lit=None, J=J: {"A": Ref("sl_a", J), "B": Ref("sl_b", J), "S": "sl_sp", "C": Lit}[Tp]) for J in Lanes]
        if Redef:  # as contas saem todas antes e o splat e redefinido ANTES das gravacoes: o vetor leria o valor novo (valor_redefinido)
            Linhas += [f"{Ind}let sl_t{J} = {Texto}" for J, Texto in zip(Lanes, Textos)] + [f"{Ind}sl_sp = sl_sp * 0.5 + 1.0"]
            Textos = [f"sl_t{J}" for J in Lanes]
        for Pos, (J, Texto) in enumerate(zip(Lanes, Textos)):
            if Uso and Pos == PosUso:
                Linhas += [f"{Ind}let sl_x = {Texto}", f"{Ind}{Ref('sl_r', J)} = sl_x"]
            else:
                Linhas.append(f"{Ind}{Ref('sl_r', J)} = {Texto}")
            if Chama and Pos == min(1, len(Lanes) - 1):
                Linhas.append(f"{Ind}sl_ruido{W}(sl_sp)")
        Linhas += [f"{Ind}print(sl_cls{W}({Ref('sl_r', J)}))" for J in Lanes]
        if Uso:
            Linhas.append(f"{Ind}print(sl_cls{W}(sl_x))")
        if ComTrap:
            Linhas.append(f"{Ind}sl_acc = (sl_acc + sl_cls{W}(sl_pega{W}(sl_r[..], sl_i))) % {MODULO_INT}")
        return Linhas + ["    }"]

    def FolhaPonteiro(self, J, Soff, Lets, Antes, *, Splat="k"):
        """Folha por lane de um grupo por ponteiro: com Antes cada carga vira um `let` (todas as cargas antes das gravacoes), senao fica na propria conta. Splat e o nome do valor difundido."""
        Contador = [0]

        def Folha(Tp, Lit=None):
            if Tp in ("C", "S"):
                return Lit if Tp == "C" else Splat
            Base = f"s^[{Soff + J}]" if Tp == "A" else f"t^[{J}]"
            if not Antes:
                return Base
            Nome = f"sl_l{Contador[0]}_{J}"
            Contador[0] += 1
            Lets.append(f"    let {Nome} = {Base}")
            return Nome
        return Folha

    def SlpPonteiro(self, Id, T):
        """Grupo em proc com ^[8]: origem, segunda fonte e destino, que as vezes sao o mesmo array deslocado (alias dentro do grupo). Variantes: cargas antes das gravacoes, carga/escrita de fora,
        CHAMA no meio, uso externo (lane lida depois do grupo) e splat redefinido antes das gravacoes."""
        R, W = self.R, SUFIXO_SLP[T]
        Lanes = self.LanesSlp(T, R.random() < 0.12)
        Antes, Sobrepoe, Fora, Chama = R.random() < 0.5, R.random() < 0.5, R.random() < 0.2, R.random() < 0.2
        Uso, Redef = R.random() < 0.25, R.random() < 0.25
        Soff, Doff = R.choice(DESLOCAMENTOS_SOBREPOSTOS) if Sobrepoe else (R.randint(0, 1), R.randint(0, 1))
        if max(Lanes) + max(Soff, Doff) > 7:  # lanes 0,2,4,6 com deslocamento 2 sairiam do [8] (E0421)
            Soff, Doff = min(Soff, 1), min(Doff, 1)
        Arv, Lets, Fim = ArvoreSlpCom(R, ("S",) if Redef else ()), [], []
        Splat = "kv" if Redef else "k"
        if Redef:
            Lets.append(f"    var kv: {T} = k")
        Contas = [RenderSlp(Arv, self.FolhaPonteiro(J, Soff, Lets, Antes, Splat=Splat)) for J in Lanes]
        PosUso = R.randrange(len(Lanes))
        Entre = Redef and R.random() < 0.6  # splat redefinido ENTRE as gravacoes: o mesmo vreg tem valores diferentes por lane e o vetor nao pode usar um so (apanha PendentesIguais)
        if Redef and not Entre:  # contas antes, splat redefinido, so entao as gravacoes (valor_redefinido)
            Gravas = [f"    let sl_t{J} = {Conta}" for J, Conta in zip(Lanes, Contas)] + ["    kv = kv * 0.5 + 1.0"] + [f"    d^[{Doff + J}] = sl_t{J}" for J in Lanes]
            if Uso:
                Fim.append(f"    print(sl_cls{W}(sl_t{Lanes[PosUso]}))")
        else:
            Gravas = [f"    d^[{Doff + J}] = {Conta}" for J, Conta in zip(Lanes, Contas)]
            if Uso:
                Gravas[PosUso] = f"    let sl_x = {Contas[PosUso]}\n    d^[{Doff + Lanes[PosUso]}] = sl_x"
                Fim.append(f"    print(sl_cls{W}(sl_x))")
            if Entre:
                Gravas.insert(1 + R.randrange(len(Lanes) - 1), "    kv = kv * 0.5 + 1.0")
        if Fora:
            if R.random() < 0.5:
                Gravas.insert(0 if Antes else 1, "    d^[7] = k")
            else:
                Gravas.insert(0 if Antes else 1, "    let sl_fx = s^[7]")
                Fim.append(f"    print(sl_cls{W}(sl_fx))")
        if Redef:  # le o splat redefinido DEPOIS do grupo: a redefinicao fica viva (o DCE nao a remove) e o vetor que lesse o valor novo aparece na saida
            Fim.append(f"    print(sl_cls{W}(kv))")
        if Chama and R.random() < 0.5:  # CHAMA que le o destino logo depois da 1a gravacao: mover a gravacao para depois da chamada muda a saida
            Gravas.insert(1 + next(N for N, Linha in enumerate(Gravas) if Linha.lstrip().startswith("d^[")), f"    sl_le{W}(d, {Doff + Lanes[0]})")
        elif Chama:
            Gravas.insert(min(1, len(Gravas)), f"    sl_ruido{W}(k)")
        self.Procs.append(f"proc sl_p{Id}(s: ^[8]{T}, t: ^[8]{T}, d: ^[8]{T}, k: {T}) {{\n" + "\n".join(Lets + Gravas + Fim) + "\n}\n")
        V, Alvo = f"sl_v{Id}", f"&sl_v{Id}" if Sobrepoe else f"&sl_o{Id}"
        Segunda = f"&sl_v{Id}" if Sobrepoe and R.random() < 0.3 else f"&sl_w{Id}"
        Outros = [Nome for Nome in (f"sl_w{Id}", f"sl_o{Id}") if f"&{Nome}" in (Segunda, Alvo)]  # so os arrays usados na chamada (var sem leitura e E0708)
        return [f"    var {V}: [8]{T} = {{}}"] + [f"    var {Nome}: [8]{T} = {{}}" for Nome in Outros] + ["    for sl_k in 0..<8 {",
                f"        {V}[sl_k] = sl_pa{W}[(sl_k + {R.randint(0, 5)}) % 8]"] + [f"        {Nome}[sl_k] = sl_pb{W}[(sl_k + {R.randint(0, 5)}) % 8]" for Nome in Outros] + ["    }", "    for sl_i in 0..<3 {",
                f"        sl_p{Id}(&{V}, {Segunda}, {Alvo}, sl_pb{W}[(sl_i + 1) % 8])", f"        for sl_j in 0..<8 {{", f"            print(sl_cls{W}({Alvo[1:]}[sl_j]))", "        }", "    }"]
