# -*- coding: utf-8 -*-
"""Gerador de kernels de micro-benchmark para as fatias de otimizacao do M7b (F0c).

Objetivo: cada kernel e um programa .fj pequeno e determinista que isola um idioma de uma fatia (F1..F8 de
MEM-LJ-OTIM-IDIOMAS). Laco quente com N fixo (~200 ms), imprime um checksum. Os .esperado.txt de gerados\\ vieram do
backend C (gcc -O2) antes da 0.21.0; o C saiu e o --gerar foi aposentado (D0730 congelou o binario gcc de referencia).
O runner rodar_kernels.py (mesma pasta) confere o x64 contra o .esperado e mede a razao com o binario congelado.

Uso:
    python -I gerar_kernels.py --selftest [--forja CAMINHO]
    python -I gerar_kernels.py --listar
    python -I gerar_kernels.py --gerar <pasta>   (aposentado na 0.21.0 F3: sai com exit 2)

Versao: 1.1.0
Changelog: 1.1.0 - 0.21.0 F3: --gerar aposentado (exit 2 com a mensagem do motivo); CompilarC, GerarUm e RodarGerar saem; --selftest segue sem gcc.
           1.0.0 - M7b F0c: 15 kernels (F1..F8), --selftest (catalogo, determinismo, forja check), --listar,
           --gerar (grava .fj e .esperado.txt; aborta se o x64 release diferir do backend C).
Verificacao: python -I gerar_kernels.py --selftest
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
from collections import namedtuple
from pathlib import Path

VERSAO = "1.1.0"
MENSAGEM_GERAR_APOSENTADO = ("ERRO: --gerar foi aposentado na 0.21.0 F3: o gerador do C saiu com o backend C (D0730 congelou o binario gcc de referencia). "
                             "Kernel novo precisa de .esperado.txt de fonte independente, nunca do proprio x64.")
FORJA_PADRAO = "C:/Users/Cougar/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/compiler/forja.exe"
MARCA_N = "@N@"
FATIAS_ESPERADAS = ("F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8")

Kernel = namedtuple("Kernel", "nome fatia nome_fatia descricao n fonte")


class ErroKernel(Exception):
    """Falha de geracao, compilacao ou conferencia de um kernel; a mensagem diz qual e onde."""


# Trechos repetidos: o xorshift32/64 e a fonte de valores pseudoaleatorios (sem sinal, sem UB).
XS32 = (
    "        x = x ~ (x << 13)\n"
    "        x = x ~ (x >> 17)\n"
    "        x = x ~ (x << 5)\n"
)
XS64 = (
    "        y = y ~ (y << 13)\n"
    "        y = y ~ (y >> 7)\n"
    "        y = y ~ (y << 17)\n"
)

CATALOGO = (
    Kernel("f1_div_i32_const", "F1", "divisao e modulo por constante",
           "i32 com sinal: / e % por 3, 7, 10, 1000, -3, -7 (sem -1: MIN / -1 da R0002)",
           20000000, """proc main() {
    var x: u32 = 2463534242
    var acc: int = 0
    for i in 0..<@N@ {
@XS32@        let v = ((x & 2147483647) as i32) - 1073741824
        acc = wrapping_add(acc, (v / 3) as int)
        acc = wrapping_add(acc, (v % 7) as int)
        acc = wrapping_add(acc, (v / 10) as int)
        acc = wrapping_add(acc, (v % 1000) as int)
        acc = wrapping_add(acc, (v / -3) as int)
        acc = wrapping_add(acc, (v % -7) as int)
    }
    print(acc)
}
"""),
    Kernel("f1_div_i32_pot2", "F1", "divisao e modulo por constante",
           "i32 com sinal: / e % por potencias de 2 (2, 8, 1024, 65536) e -8",
           20000000, """proc main() {
    var x: u32 = 2463534242
    var acc: int = 0
    for i in 0..<@N@ {
@XS32@        let v = ((x & 2147483647) as i32) - 1073741824
        acc = wrapping_add(acc, (v / 2) as int)
        acc = wrapping_add(acc, (v / 8) as int)
        acc = wrapping_add(acc, (v % 8) as int)
        acc = wrapping_add(acc, (v / 1024) as int)
        acc = wrapping_add(acc, (v % 65536) as int)
        acc = wrapping_add(acc, (v / -8) as int)
    }
    print(acc)
}
"""),
    Kernel("f1_div_i32_unidade_min", "F1", "divisao e modulo por constante",
           "i32 com sinal: / e % por 1, -1 e MIN (dividendo nunca MIN, para nao dar R0002)",
           27000000, """proc main() {
    var x: u32 = 2463534242
    var acc: int = 0
    for i in 0..<@N@ {
@XS32@        let v = ((x & 2147483647) as i32) - 1073741824
        acc = wrapping_add(acc, (v / 1) as int)
        acc = wrapping_add(acc, (v / -1) as int)
        acc = wrapping_add(acc, (v % 1) as int)
        acc = wrapping_add(acc, (v / (-2147483647 - 1)) as int)
        acc = wrapping_add(acc, (v % (-2147483647 - 1)) as int)
    }
    print(acc)
}
"""),
    Kernel("f1_div_i64_const", "F1", "divisao e modulo por constante",
           "int (i64) com sinal: / e % por 3, 10, 1000, 1024, -3, -1024",
           14000000, """proc main() {
    var y: u64 = 88172645463325252
    var acc: int = 0
    for i in 0..<@N@ {
@XS64@        let v = ((y >> 2) as int) - 2305843009213693952
        acc = wrapping_add(acc, v / 3)
        acc = wrapping_add(acc, v % 10)
        acc = wrapping_add(acc, v / 1000)
        acc = wrapping_add(acc, v % 1024)
        acc = wrapping_add(acc, v / -3)
        acc = wrapping_add(acc, v % -1024)
    }
    print(acc)
}
"""),
    Kernel("f1_div_u32_const", "F1", "divisao e modulo por constante",
           "u32 sem sinal: / e % por 3, 7, 10, 1000, 8 (dividendo na faixa inteira)",
           27000000, """proc main() {
    var x: u32 = 2463534242
    var acc: int = 0
    for i in 0..<@N@ {
@XS32@        acc = wrapping_add(acc, (x / 3) as int)
        acc = wrapping_add(acc, (x % 7) as int)
        acc = wrapping_add(acc, (x / 10) as int)
        acc = wrapping_add(acc, (x % 1000) as int)
        acc = wrapping_add(acc, (x / 8) as int)
    }
    print(acc)
}
"""),
    Kernel("f2_folha_laco", "F2", "chamada de folha em laco",
           "duas folhas minusculas (passo, mistura) chamadas a cada volta; o custo e a chamada",
           40000000, """proc passo(a: int) -> int {
    return wrapping_add(a, 7)
}

proc mistura(a: int, b: int) -> int {
    return wrapping_add(wrapping_mul(a, 31), b)
}

proc main() {
    var acc: int = 1
    for i in 0..<@N@ {
        acc = mistura(acc, passo(i))
        acc = passo(acc)
    }
    print(acc)
}
"""),
    Kernel("f3_struct_local", "F3", "struct local pequena (SROA)",
           "struct de 3 campos int passada por valor a uma folha e atualizada campo a campo",
           44000000, """struct Estado {
    a: int = 0
    b: int = 0
    c: int = 0
}

proc avanca(e: Estado, k: int) -> Estado {
    return Estado{a = wrapping_add(e.a, k), b = e.c, c = wrapping_mul(e.b, 3)}
}

proc main() {
    var s = Estado{a = 1, b = 2, c = 3}
    for i in 0..<@N@ {
        s = avanca(s, i)
        s.a = wrapping_add(s.a, s.b)
    }
    print(wrapping_add(wrapping_add(s.a, s.b), s.c))
}
"""),
    Kernel("f4_minmax_abs_clamp", "F4", "min/max/abs/clamp e bits",
           "min, max, abs e clamp sobre int (cmp+cmov); dividendo na faixa sem MIN",
           35000000, """proc main() {
    var x: u32 = 2463534242
    var acc: int = 0
    for i in 0..<@N@ {
@XS32@        let v = ((x & 2147483647) as int) - 1073741824
        acc = wrapping_add(acc, min(v, 1000))
        acc = wrapping_add(acc, max(v, -1000))
        acc = wrapping_add(acc, abs(v))
        acc = wrapping_add(acc, clamp(v, -5000, 5000))
    }
    print(acc)
}
"""),
    Kernel("f4_minmax_real", "F4", "min/max/abs/clamp e bits",
           "min, max, abs e clamp sobre f64 com valores inteiros (somas exatas, sem reassociar)",
           15000000, """proc main() {
    var x: u32 = 2463534242
    var acc: f64 = 0.0
    for i in 0..<@N@ {
@XS32@        let v = ((x & 65535) as f64) - 32768.0
        acc += min(v, 100.0)
        acc += max(v, -100.0)
        acc += abs(v)
        acc += clamp(v, -20000.0, 20000.0)
    }
    print(acc as int)
}
"""),
    Kernel("f4_bits_rot_popcount", "F4", "min/max/abs/clamp e bits",
           "rotacao (shl|shr) e popcount SWAR escritos a mao em u32",
           67000000, """proc main() {
    var x: u32 = 2463534242
    var acc: int = 0
    for i in 0..<@N@ {
@XS32@        let r = (x << 7) | (x >> 25)
        var v = wrapping_sub(r, (r >> 1) & 1431655765)
        v = (v & 858993459) + ((v >> 2) & 858993459)
        v = (v + (v >> 4)) & 252645135
        let c = wrapping_mul(v, 16843009) >> 24
        acc = wrapping_add(acc, c as int)
    }
    print(acc)
}
"""),
    Kernel("f5_expr_redundante_sccp", "F5", "expressoes redundantes e constantes",
           "subexpressoes iguais (GVN), constantes dobradas (SCCP) e ramo sempre verdadeiro",
           96000000, """proc main() {
    var x: u32 = 2463534242
    var acc: int = 0
    for i in 0..<@N@ {
@XS32@        let a = (x & 65535) as int
        let b = wrapping_mul(a, 3) + 7
        let c = wrapping_mul(a, 3) + 7
        let k = 3 * 1000 + 7
        if 2 > 1 {
            acc = wrapping_add(acc, b + c)
        }
        acc = wrapping_add(acc, k * 2 + a * 0)
    }
    print(acc)
}
"""),
    Kernel("f6_licm_dse", "F6", "carga invariante e store morto",
           "carga de tab[5] e tab[2] dentro do laco (LICM) e gravacao de out[1] sobrescrita (DSE)",
           95000000, """proc main() {
    let tab: [8]int = {3, 1, 4, 1, 5, 9, 2, 6}
    var out: [4]int = {0, 0, 0, 0}
    var x: u32 = 2463534242
    var acc: int = 0
    for i in 0..<@N@ {
@XS32@        let base = tab[5] * 13 + tab[2]
        out[1] = base
        out[1] = wrapping_add((x & 1023) as int, base)
        acc = wrapping_add(acc, out[1])
    }
    print(acc)
}
"""),
    Kernel("f7_selecao_condicional", "F7", "selecao condicional (cmov)",
           "maximo e escolha por bit com ramo imprevisivel (candidatos a cmov)",
           30000000, """proc main() {
    var x: u32 = 2463534242
    var acc: int = 0
    for i in 0..<@N@ {
@XS32@        let a = (x & 65535) as int
        let b = (x >> 16) as int
        var r: int = a
        if b > r {
            r = b
        }
        var s: int = b
        if (x & 1) == 1 {
            s = a
        }
        acc = wrapping_add(acc, r - s)
    }
    print(acc)
}
"""),
    Kernel("f8_laco_curto", "F8", "laco curto desenrolavel",
           "laco interno de 4 voltas com limite constante dentro de laco externo",
           48000000, """proc main() {
    var acc: int = 0
    for i in 0..<@N@ {
        for j in 0..<4 {
            acc = wrapping_add(acc, wrapping_mul(i, 4) + j)
        }
    }
    print(acc)
}
"""),
    Kernel("f8_recursao_mutua", "F8", "recursao mutua pequena",
           "par e impar em recursao mutua de profundidade 8, chamados a cada volta",
           8500000, """proc par(n: int, k: int) -> int {
    if n == 0 {
        return k
    }
    return impar(n - 1, wrapping_add(k, 1))
}

proc impar(n: int, k: int) -> int {
    if n == 0 {
        return k
    }
    return par(n - 1, wrapping_add(k, 3))
}

proc main() {
    var acc: int = 0
    for i in 0..<@N@ {
        acc = wrapping_add(acc, par(8, i))
    }
    print(acc)
}
"""),
)


def GerarFonte(Kernel):
    # Monta o .fj: cabecalho de 3 linhas + corpo com N e os trechos de xorshift trocados.
    Cabecalho = (
        f"// {Kernel.nome}.fj - kernel de micro-benchmark (M7b F0c, fatia {Kernel.fatia}: {Kernel.nome_fatia}).\n"
        f"// {Kernel.descricao}\n"
        f"// Gerado por gerar_kernels.py {VERSAO}; nao editar a mao. Saida = checksum deterministico.\n\n"
    )
    Corpo = Kernel.fonte.replace("@XS32@", XS32).replace("@XS64@", XS64).replace(MARCA_N, str(Kernel.n))
    return Cabecalho + Corpo


def Executar(Argumentos, Pasta):
    return subprocess.run([str(A) for A in Argumentos], capture_output=True, cwd=str(Pasta))


def Texto(Bytes):
    return Bytes.decode("utf-8", errors="replace")


def ExigirSucesso(Resultado, Nome, Etapa):
    if Resultado.returncode != 0:
        Causa = Texto(Resultado.stdout).strip()[:300] or Texto(Resultado.stderr).strip()[:300]
        raise ErroKernel(f"{Nome}: {Etapa} falhou (exit {Resultado.returncode}): {Causa}")


def CompilarX64(Forja, Fonte, Pasta):
    Saida = Pasta / (Fonte.stem + "_x64.exe")
    ExigirSucesso(Executar([Forja, "build", "--backend", "x64", "--release", Fonte, "-o", Saida], Pasta),
                  Fonte.stem, "forja build --backend x64 --release")
    return Saida


def RodarPrograma(Exe, Pasta):
    # Devolve a saida normalizada (LF); exit diferente de 0 e erro do kernel.
    Resultado = Executar([Exe], Pasta)
    if Resultado.returncode != 0:
        raise ErroKernel(f"{Exe.name}: exit {Resultado.returncode}: {Texto(Resultado.stderr).strip()[:300]}")
    return Texto(Resultado.stdout).replace("\r\n", "\n")


def ResolverForja(Caminho):
    Forja = Path(Caminho).resolve()
    if not Forja.is_file():
        raise ErroKernel(f"forja.exe nao achado em {Forja}")
    return Forja


def GravarTexto(Caminho, Conteudo):
    with open(Caminho, "w", encoding="utf-8", newline="\n") as Arq:
        Arq.write(Conteudo)


def VerificarCatalogo():
    Nomes = [K.nome for K in CATALOGO]
    if len(CATALOGO) < 8:
        raise ErroKernel(f"catalogo com {len(CATALOGO)} kernels; minimo 8")
    if len(set(Nomes)) != len(Nomes):
        raise ErroKernel("nomes de kernel repetidos")
    Faltando = [F for F in FATIAS_ESPERADAS if F not in {K.fatia for K in CATALOGO}]
    if Faltando:
        raise ErroKernel(f"fatias sem kernel: {', '.join(Faltando)}")
    for K in CATALOGO:
        Texto_ = GerarFonte(K)
        if GerarFonte(K) != Texto_:
            raise ErroKernel(f"{K.nome}: geracao nao e deterministica")
        if "proc main()" not in Texto_ or MARCA_N in Texto_ or "@XS" in Texto_:
            raise ErroKernel(f"{K.nome}: placeholder nao substituido ou sem proc main")


def VerificarSintaxe(Forja):
    # forja check em cada kernel (o compilador e a verdade sobre a sintaxe).
    PastaTemp = Path(tempfile.mkdtemp(prefix="kernels_check_"))
    try:
        for K in CATALOGO:
            Fonte = PastaTemp / f"{K.nome}.fj"
            GravarTexto(Fonte, GerarFonte(K))
            ExigirSucesso(Executar([Forja, "check", Fonte], PastaTemp), K.nome, "forja check")
    finally:
        shutil.rmtree(PastaTemp, ignore_errors=True)


def ListarCatalogo():
    print("nome | fatia | N | descricao")
    for K in CATALOGO:
        print(f"{K.nome} | {K.fatia} {K.nome_fatia} | {K.n} | {K.descricao}")


def RodarSelftest(Forja):
    VerificarCatalogo()
    print(f"catalogo OK: {len(CATALOGO)} kernels, fatias {', '.join(FATIAS_ESPERADAS)}, geracao deterministica")
    if Forja is None:
        print("forja.exe nao informado: forja check pulado")
        return 0
    VerificarSintaxe(Forja)
    print(f"forja check OK em {len(CATALOGO)} kernels")
    return 0


def MontarArgumentos():
    Ap = argparse.ArgumentParser(description="Gerador de kernels de micro-benchmark do M7b (F0c).")
    Grupo = Ap.add_mutually_exclusive_group(required=True)
    Grupo.add_argument("--selftest", action="store_true", help="confere catalogo, determinismo e forja check")
    Grupo.add_argument("--listar", action="store_true", help="lista os kernels")
    Grupo.add_argument("--gerar", metavar="PASTA", help="APOSENTADO na 0.21.0 F3: sai com erro (exit 2)")
    Ap.add_argument("--forja", default=FORJA_PADRAO, help="caminho absoluto do forja.exe")
    return Ap


def Principal(Argumentos=None):
    Ap = MontarArgumentos()
    Args = Ap.parse_args(Argumentos)
    if Args.gerar is not None:
        print(MENSAGEM_GERAR_APOSENTADO, file=sys.stderr)
        return 2
    try:
        if Args.listar:
            ListarCatalogo()
            return 0
        Forja = ResolverForja(Args.forja)
        return RodarSelftest(Forja)
    except ErroKernel as Erro:
        print(f"ERRO: {Erro}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(Principal())
