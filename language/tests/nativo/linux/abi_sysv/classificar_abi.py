#!/usr/bin/env python3
"""classificar_abi.py - tabela ABI SysV: 60 assinaturas compiladas pelo gcc do alpine-build e classificadas pelo objdump.

Objetivo: P32(d) do contrato_m5_resto (D-R5/D-R9). Gera assinaturas.c (60 funcoes: escalares, 6/8/7/9 args, structs de 1..32 B
misturando int e float, retorno em memoria, make de struct, variadicas), compila com gcc -O0 no WSL alpine-build, le o
objdump e compara o que o gcc FEZ (registradores de entrada pelos stores do prologo; registradores de retorno pelo epilogo)
com a classificacao SysV calculada pelo algoritmo. O resultado vai para abi_sysv.json (PASSOU/FALHOU por assinatura).
Uso: python -I classificar_abi.py            (gera, compila no WSL, classifica, grava abi_sysv.json)
     python -I classificar_abi.py --selftest (so o algoritmo esperado, sem WSL)
Versao: 0.1.0
Changelog:
  0.1.0 (14/11/2026) P32d: 60 assinaturas; esperado SysV x observado do objdump -d; limitacao: retorno lido das ultimas 25 instrucoes.
Depende de: gcc e objdump no WSL alpine-build (wsl.exe -d alpine-build). Python 3.13 stdlib.
"""
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PASTA = Path(__file__).resolve().parent
C_SAIDA = PASTA / "assinaturas.c"
JSON_SAIDA = PASTA / "abi_sysv.json"
DISTRO = "alpine-build"

TIPOS = {  # nome: (tamanho, classe, nome em C)
    "i8": (1, "INT", "signed char"), "i16": (2, "INT", "short"), "i32": (4, "INT", "int"),
    "i64": (8, "INT", "long"), "ptr": (8, "INT", "void *"), "f32": (4, "SSE", "float"), "f64": (8, "SSE", "double"),
}
REGS_INT = ["rdi", "rsi", "rdx", "rcx", "r8", "r9"]
REGS_SSE = ["xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"]
ALIAS_REG = {"eax": "rax", "ax": "rax", "al": "rax", "edx": "rdx", "dx": "rdx", "dl": "rdx",
             "edi": "rdi", "di": "rdi", "dil": "rdi", "esi": "rsi", "si": "rsi", "sil": "rsi",
             "ecx": "rcx", "cx": "rcx", "cl": "rcx", "r8d": "r8", "r8w": "r8", "r8b": "r8",
             "r9d": "r9", "r9w": "r9", "r9b": "r9"}
REGS_RETORNO = {"rax", "rdx", "xmm0", "xmm1"}


def EhEstrutura(Tipo: str) -> bool:
    return Tipo.startswith("S")


def LayoutEstrutura(Campos: list) -> tuple:
    """(tamanho, [(offset, tipo)]) do struct com alinhamento natural de C."""
    Offset, Alinha, Layout = 0, 1, []
    for Campo in Campos:
        Tam = TIPOS[Campo][0]
        Offset = (Offset + Tam - 1) // Tam * Tam
        Layout.append((Offset, Campo))
        Offset += Tam
        Alinha = max(Alinha, Tam)
    return (Offset + Alinha - 1) // Alinha * Alinha, Layout


ESTRUTURAS = {  # 23 structs de 1 a 32 B: int e float misturados
    "S01": ["i8"], "S02": ["i16"], "S03": ["i8", "i8", "i8"], "S04": ["i32"], "S05": ["f32"],
    "S06": ["i8"] * 5, "S07": ["i16"] * 3, "S08": ["i32", "i32"], "S09": ["f32", "f32"],
    "S10": ["i32", "f32"], "S11": ["f32", "i32"], "S12": ["i64", "i8"], "S13": ["i32", "i32", "f32"],
    "S14": ["f32", "f32", "f32"], "S15": ["i64", "f64"], "S16": ["f64", "i64"], "S17": ["f32"] * 4,
    "S18": ["i64", "i64"], "S19": ["i32"] * 5, "S20": ["i64", "f64", "i64"], "S21": ["i64"] * 4,
    "S22": ["f64", "i32"], "S23": ["i8", "f64"],
}


def ClassesDosOitoBytes(Campos: list) -> list:
    """Classe SysV de cada eightbyte: INT se algum campo int cai nele, senao SSE; >16 B = MEMORY."""
    Tamanho, Layout = LayoutEstrutura(Campos)
    if Tamanho > 16:
        return ["MEMORY"]
    Classes = []
    for Inicio in range(0, Tamanho, 8):
        Dentro = [T for Off, T in Layout if Inicio <= Off < Inicio + 8]
        Classes.append("INT" if any(TIPOS[T][1] == "INT" for T in Dentro) else "SSE")
    return Classes


def EsperadoSysV(Assinatura: dict) -> dict:
    """Registradores que a ABI SysV usa para os argumentos e para o retorno, mais se algum vai para a pilha."""
    Ints, Sses, Pilha, RetRegs, Args = list(REGS_INT), list(REGS_SSE), False, [], []
    Ret = Assinatura["ret"]
    if EhEstrutura(Ret) and ClassesDosOitoBytes(ESTRUTURAS[Ret]) == ["MEMORY"]:
        Args.append(Ints.pop(0))  # ponteiro oculto ocupa rdi
        RetRegs = ["rax"]
    elif EhEstrutura(Ret):
        ProximosInt, ProximosSse = ["rax", "rdx"], ["xmm0", "xmm1"]
        for Classe in ClassesDosOitoBytes(ESTRUTURAS[Ret]):
            RetRegs.append(ProximosInt.pop(0) if Classe == "INT" else ProximosSse.pop(0))
    else:
        RetRegs = ["xmm0"] if TIPOS[Ret][1] == "SSE" else ["rax"]
    for Param in Assinatura["params"]:
        if EhEstrutura(Param):
            Classes = ClassesDosOitoBytes(ESTRUTURAS[Param])
            PrecisaInt, PrecisaSse = Classes.count("INT"), Classes.count("SSE")
            if Classes == ["MEMORY"] or PrecisaInt > len(Ints) or PrecisaSse > len(Sses):
                Pilha = True
                continue
            for Classe in Classes:
                Args.append(Ints.pop(0) if Classe == "INT" else Sses.pop(0))
        else:
            Lista = Sses if TIPOS[Param][1] == "SSE" else Ints
            if Lista:
                Args.append(Lista.pop(0))
            else:
                Pilha = True
    return {"args": Args, "ret": sorted(set(RetRegs)), "pilha": Pilha}


def Assinaturas() -> list:
    """As 60 assinaturas: 7 identidades escalares, 10 mistas, 23 identidades de struct, 4 com struct e escalar, 4 variadicas, 12 make de struct."""
    Lista = []
    for Tipo in ["i8", "i16", "i32", "i64", "f32", "f64", "ptr"]:
        Lista.append({"nome": f"id_{Tipo}", "ret": Tipo, "params": [Tipo], "variadica": False, "tipo": "escalar"})
    Mistas = [(["i32", "f64"], "f64"), (["f64", "i32"], "i32"), (["i64", "f32"], "f32"),
              (["i64"] * 6, "i64"), (["f64"] * 8, "f64"), (["i64"] * 7, "i64"), (["f64"] * 9, "f64"),
              (["i8", "f32", "i16", "f64", "i32", "f32"], "i32"), (["ptr", "ptr"], "ptr"), (["i32"] * 3, "i32")]
    for N, (Params, Ret) in enumerate(Mistas):
        Lista.append({"nome": f"mista_{N:02d}", "ret": Ret, "params": Params, "variadica": False, "tipo": "escalar"})
    for Estrutura in ESTRUTURAS:
        Lista.append({"nome": f"id_{Estrutura}", "ret": Estrutura, "params": [Estrutura], "variadica": False, "tipo": "struct"})
    for Estrutura in ["S10", "S13", "S15", "S20"]:
        Lista.append({"nome": f"mix_{Estrutura}", "ret": "i64", "params": ["i32", Estrutura, "f64"], "variadica": False, "tipo": "struct+escalar"})
    Lista += [
        {"nome": "var_i", "ret": "i32", "params": ["i32"], "variadica": True, "tipo": "variadica"},
        {"nome": "var_d", "ret": "f64", "params": ["f64"], "variadica": True, "tipo": "variadica"},
        {"nome": "var_p", "ret": "i32", "params": ["ptr"], "variadica": True, "tipo": "variadica"},
        {"nome": "var_l", "ret": "i64", "params": ["i64", "i64"], "variadica": True, "tipo": "variadica"},
    ]
    for Estrutura in ["S08", "S09", "S10", "S11", "S12", "S13", "S14", "S15", "S16", "S17", "S19", "S21"]:
        Lista.append({"nome": f"make_{Estrutura}", "ret": Estrutura, "params": ESTRUTURAS[Estrutura], "variadica": False, "tipo": "make"})
    return Lista


def FonteC(Lista: list) -> str:
    Linhas = ["/* assinaturas.c - gerado por classificar_abi.py (0.1.0). Nao editar a mao. */", "#include <stdarg.h>", ""]
    for Nome, Campos in ESTRUTURAS.items():
        Corpo = " ".join(f"{TIPOS[T][2]} f{I};" for I, T in enumerate(Campos))
        Linhas.append(f"typedef struct {Nome} {{ {Corpo} }} {Nome};")
    Linhas.append("")
    for Func in Lista:
        Linhas.append(Declaracao(Func))
    return "\n".join(Linhas) + "\n"


def TipoC(Tipo: str) -> str:
    return Tipo if EhEstrutura(Tipo) else TIPOS[Tipo][2]


def Declaracao(Func: dict) -> str:
    Nome, Ret, Params = Func["nome"], Func["ret"], Func["params"]
    if Func["variadica"]:
        Lista = [f"{TipoC(T)} a{I}" for I, T in enumerate(Params)]
        Corpo = f"va_list ap; va_start(ap, a{len(Params) - 1}); va_arg(ap, int); va_end(ap); return ({TipoC(Ret)})a0;"
        return f"{TipoC(Ret)} {Nome}({', '.join(Lista + ['...'])}) {{ {Corpo} }}"
    if Func["tipo"] == "make":
        Lista = [f"{TipoC(T)} a{I}" for I, T in enumerate(Params)]
        Atrib = " ".join(f"r.f{I} = a{I};" for I in range(len(Params)))
        return f"{Ret} {Nome}({', '.join(Lista)}) {{ {Ret} r; {Atrib} return r; }}"
    Lista = [f"{TipoC(T)} a{I}" for I, T in enumerate(Params)]
    if Func["tipo"] == "struct+escalar":
        Corpo = f"(void)a1; return (long)a0 + (long)a2 + (long)a1.f0;"
    elif Func["tipo"] == "struct" or Func["tipo"] == "escalar" and EhEstrutura(Ret):
        Corpo = "return a0;"
    else:
        Corpo = f"return ({TipoC(Ret)})a{len(Params) - 1};"
    return f"__attribute__((noinline)) {TipoC(Ret)} {Nome}({', '.join(Lista)}) {{ {Corpo} }}"


ATRIBUTO_LIMPO = re.compile(r"#.*$")
PADRAO_INSTR = re.compile(r"^\s*[0-9a-f]+:\s+(\S+)\s*(.*)$")
PADRAO_FUNC = re.compile(r"^[0-9a-f]+ <(\w+)>:$")
PADRAO_POS_RBP = re.compile(r"(?<![-\w])0x[0-9a-f]+\(%rbp\)")


def Operandos(Texto: str) -> list:
    """Divide operandos AT&T por virgula fora de parenteses."""
    Partes, Atual, Prof = [], "", 0
    for C in Texto:
        if C == "(":
            Prof += 1
        elif C == ")":
            Prof -= 1
        if C == "," and Prof == 0:
            Partes.append(Atual.strip())
            Atual = ""
        else:
            Atual += C
    if Atual.strip():
        Partes.append(Atual.strip())
    return Partes


def Canonico(Reg: str) -> str:
    Nome = Reg.lstrip("%")
    return ALIAS_REG.get(Nome, Nome)


def EhRegistrador(Op: str) -> bool:
    return Op.startswith("%") and "(" not in Op


def InstrucoesDaFuncao(Linhas: list) -> list:
    Instr = []
    for Linha in Linhas:
        Casou = PADRAO_INSTR.match(ATRIBUTO_LIMPO.sub("", Linha).rstrip())
        if Casou:
            Instr.append((Casou.group(1), Operandos(Casou.group(2))))
    return Instr


def ObservadoDoObjdump(Instr: list) -> dict:
    """Registradores de entrada (stores do prologo, na ordem) e de retorno (epilogo) lidos do objdump."""
    Alias, Args, Pilha = {}, [], False
    for Mn, Ops in Instr:
        if any(PADRAO_POS_RBP.search(O) for O in Ops):
            Pilha = True
    for Mn, Ops in Instr:
        if Mn.startswith("ret"):
            break
        if len(Ops) != 2 or not Mn.startswith("mov"):
            continue
        Origem, Destino = Ops
        if EhRegistrador(Destino):
            Alias[Canonico(Destino)] = Alias.get(Canonico(Origem), Canonico(Origem)) if EhRegistrador(Origem) else None
        elif EhRegistrador(Origem) and "(%rbp)" in Destino:
            Real = Alias.get(Canonico(Origem), Canonico(Origem))
            if Real in REGS_INT + REGS_SSE and Real not in Args:
                Args.append(Real)
    Fim = max((I for I, (Mn, _) in enumerate(Instr) if Mn.startswith("ret")), default=0)
    Ultimas = Instr[max(0, Fim - 25):Fim]
    Retorno = sorted({Canonico(O[-1]) for _, O in Ultimas if O and EhRegistrador(O[-1]) and Canonico(O[-1]) in REGS_RETORNO})
    return {"args": Args, "ret": Retorno, "pilha": Pilha}


def Comparar(Esperado: dict, Observado: dict, Variadica: bool) -> dict:
    """Ordem dos args nao conta (o -O0 grava na ordem que quer); retorno precisa ter os registros esperados.
    Variadica: o prologo salva todos os registros de argumento, entao so se exige que os esperados estejam la e a pilha nao entra."""
    if Variadica:
        ArgsOk = set(Esperado["args"]) <= set(Observado["args"])
        PilhaOk = True
    else:
        ArgsOk = sorted(Esperado["args"]) == sorted(Observado["args"])
        PilhaOk = Observado["pilha"] == Esperado["pilha"]
    RetOk = set(Esperado["ret"]) <= set(Observado["ret"])
    Sobra = sorted(set(Observado["ret"]) - set(Esperado["ret"]))
    return {"ok": ArgsOk and RetOk and PilhaOk, "sobra_ret": Sobra}


def CaminhoWsl(Caminho: Path) -> str:
    Texto = str(Caminho).replace("\\", "/")
    Letra, Resto = Texto[0].lower(), Texto[2:]
    return f"/mnt/{Letra}{Resto}"


def CompilarNoWsl(Pasta_Trabalho: Path) -> Path:
    """gcc -O0 do alpine-build sobre assinaturas.c e objdump -d para um .dis dentro da pasta de trabalho."""
    Fonte = CaminhoWsl(C_SAIDA)
    Obj = CaminhoWsl(Pasta_Trabalho / "assinaturas.o")
    Dis = CaminhoWsl(Pasta_Trabalho / "assinaturas.dis")
    Comando = (f"gcc -O0 -fno-asynchronous-unwind-tables -fno-stack-protector -fno-pic -c {Fonte} -o {Obj}"
               f" && objdump -d --no-show-raw-insn {Obj} > {Dis}")
    Proc = subprocess.run(["wsl.exe", "-d", DISTRO, "--", "sh", "-c", Comando], capture_output=True, timeout=600)
    Saida = (Proc.stdout + Proc.stderr).decode("utf-8", errors="replace").replace("\x00", "")
    if Proc.returncode != 0:
        raise RuntimeError(f"gcc/objdump falhou: {Saida[-600:]}")
    return Pasta_Trabalho / "assinaturas.dis"


def SepararFuncoes(TextoDis: str) -> dict:
    Funcoes, Atual = {}, None
    for Linha in TextoDis.splitlines():
        Casou = PADRAO_FUNC.match(Linha)
        if Casou:
            Atual = Casou.group(1)
            Funcoes[Atual] = []
        elif Atual is not None:
            Funcoes[Atual].append(Linha)
    return Funcoes


def Classificar(Lista: list, TextoDis: str) -> dict:
    Funcoes = SepararFuncoes(TextoDis)
    Registros, Passou = [], 0
    for Func in Lista:
        Esperado = EsperadoSysV(Func)
        Observado = ObservadoDoObjdump(InstrucoesDaFuncao(Funcoes.get(Func["nome"], [])))
        Veredito = Comparar(Esperado, Observado, Func["variadica"])
        Passou += Veredito["ok"]
        Registros.append({"nome": Func["nome"], "tipo": Func["tipo"], "ret": Func["ret"], "params": Func["params"],
                          "esperado": Esperado, "observado": Observado, **Veredito})
    return {"total": len(Lista), "passou": Passou, "falhou": len(Lista) - Passou, "assinaturas": Registros}


def SelfTest() -> int:
    Casos = [
        ({"ret": "S15", "params": []}, {"args": [], "ret": ["rax", "xmm0"], "pilha": False}),
        ({"ret": "S20", "params": []}, {"args": ["rdi"], "ret": ["rax"], "pilha": False}),
        ({"ret": "i64", "params": ["i64"] * 7}, {"args": ["rdi", "rsi", "rdx", "rcx", "r8", "r9"], "ret": ["rax"], "pilha": True}),
        ({"ret": "i64", "params": ["S13"]}, {"args": ["rdi", "xmm0"], "ret": ["rax"], "pilha": False}),
    ]
    for Assinatura, Esperado in Casos:
        Obtido = EsperadoSysV(Assinatura)
        if Obtido != Esperado:
            print(f"FALHOU selftest: {Assinatura} -> {Obtido}")
            return 1
    print("PASSOU selftest classificar_abi (algoritmo SysV)")
    return 0


def Principal() -> int:
    if "--selftest" in sys.argv:
        return SelfTest()
    Lista = Assinaturas()
    assert len(Lista) == 60, f"esperado 60 assinaturas, achei {len(Lista)}"
    C_SAIDA.write_text(FonteC(Lista), encoding="utf-8")
    Pasta = Path(tempfile.mkdtemp(prefix="abi_sysv_"))
    Dis = CompilarNoWsl(Pasta)
    Relatorio = Classificar(Lista, Dis.read_text(encoding="utf-8", errors="replace"))
    JSON_SAIDA.write_text(json.dumps(Relatorio, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"total={Relatorio['total']} PASSOU={Relatorio['passou']} FALHOU={Relatorio['falhou']} saida={JSON_SAIDA}")
    return 0 if Relatorio["falhou"] == 0 else 1


if __name__ == "__main__":
    sys.exit(Principal())
