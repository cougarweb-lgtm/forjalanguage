"""Compara, no proprio x64, o porte Forja de um grupo da libm (runtime\\wasm\\rt_wasm_<grupo>.fj) com math.* (musl em C do x64).

E o laco rapido de desenvolvimento dos modulos std:wasm_math/trig/atan/exp/pow da 0.31.0: nao precisa recompilar o compilador nem rodar no Node.
Monta um programa unico = `import "std:math"` + o texto do modulo + um teste que chama math_X (porte) e math.X (musl) com as mesmas entradas
(LCG de gerar_math_bits.py: bits crus, [-20,20], [-1000,1000], [-2,2], mais especiais) e compara os BITS (NaN por classe). Funcoes que o modulo
ainda nao tem sao puladas (listadas).

Uso:
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\comparar_x64.py <math|trig|atan|exp|pow> [--forja <forja.exe>] [--release] [--passos N] [--funcao nome]
  python -X utf8 -I tests\\nativo\\wasm\\math_bits\\comparar_x64.py --selftest
Saida: uma linha por funcao (`ok`/`DIVERGE n`) e o total; exit 0 so se 0 divergencias e nada pulado alem do declarado em --aceitar-pulo.
Versao: 0.1.0
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile

PASTA_AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, PASTA_AQUI)
import gerar_math_bits as Gerador  # noqa: E402

RAIZ_LANGUAGE = os.path.normpath(os.path.join(PASTA_AQUI, "..", "..", "..", ".."))
FORJA_PADRAO = os.path.join(RAIZ_LANGUAGE, "compiler", "forja.exe")
# Desde a 0.40.0 estas viram opcode f64/f32 no wasm (sem porte em software); o oraculo delas e verificar_wasm math_g1 (bits).
OPCODE_NO_WASM = {"floor", "ceil", "trunc", "round"}
GRUPO_PARA_FIXTURE = {"math": "math_g1", "trig": "math_g2", "atan": "math_g3", "exp": "math_g4", "pow": "math_g5"}

AUXILIARES = """
proc t_proximo(estado: u64) -> u64 {
    return wrapping_add(wrapping_mul(estado, 6364136223846793005), 1442695040888963407)
}
proc t_entrada64(estado: u64, k: int) -> f64 {
    let sel: int = k % 4
    if sel == 0 {
        return transmute(estado, to = f64)
    }
    let u: f64 = ((estado >> 11) as f64) / 9007199254740992.0
    if sel == 1 {
        return (u - 0.5) * 40.0
    }
    if sel == 2 {
        return (u - 0.5) * 2000.0
    }
    return (u - 0.5) * 4.0
}
proc t_entrada32(estado: u64, k: int) -> f32 {
    let sel: int = k % 4
    if sel == 0 {
        return transmute((estado >> 32) as u32, to = f32)
    }
    return t_entrada64(estado, k) as f32
}
proc t_bits64(r: f64) -> u64 {
    if r != r {
        return 0x7ff8000000000000
    }
    return transmute(r, to = u64)
}
proc t_bits32(r: f32) -> u32 {
    if r != r {
        return 0x7fc00000
    }
    return transmute(r, to = u32)
}
proc t_confere64(porte: f64, musl: f64, nome: str, x: f64, y: f64) -> int {
    if t_bits64(porte) == t_bits64(musl) {
        return 0
    }
    print(f"  DIFF {nome} x={t_bits64(x)} y={t_bits64(y)} porte={t_bits64(porte)} musl={t_bits64(musl)}")
    return 1
}
proc t_confere32(porte: f32, musl: f32, nome: str, x: f32, y: f32) -> int {
    if t_bits32(porte) == t_bits32(musl) {
        return 0
    }
    print(f"  DIFF {nome} x={t_bits32(x)} y={t_bits32(y)} porte={t_bits32(porte)} musl={t_bits32(musl)}")
    return 1
}
"""


def LerFuncoesDoModulo(TextoModulo):
    """Nomes dos procs pub math_* do modulo (o proc math_X atende forja_math_X)."""
    return set(re.findall(r"^pub proc (math_\w+)\(", TextoModulo, re.M))


def BlocoUnaria(Nome, Tipo):
    """Teste de uma funcao de 1 argumento: porte math_<Nome>[f] contra math.<Nome>."""
    Sufixo = "" if Tipo == "f64" else "f"
    Especiais = "ESPECIAIS64" if Tipo == "f64" else "ESPECIAIS32"
    Entrada = "t_entrada64" if Tipo == "f64" else "t_entrada32"
    Confere = "t_confere64" if Tipo == "f64" else "t_confere32"
    Zero = "0.0"
    return f"""
proc t_{Nome}_{Tipo}(estado0: u64) -> int {{
    var erros: int = 0
    for e in 0..<len({Especiais}) {{
        let x: {Tipo} = transmute({Especiais}[e], to = {Tipo})
        erros += {Confere}(math_{Nome}{Sufixo}(x), math.{Nome}(x), "{Nome} {Tipo}", x, {Zero})
    }}
    var estado: u64 = estado0
    for i in 0..<PASSOS {{
        estado = t_proximo(estado)
        let x: {Tipo} = {Entrada}(estado, i)
        erros += {Confere}(math_{Nome}{Sufixo}(x), math.{Nome}(x), "{Nome} {Tipo}", x, {Zero})
    }}
    return erros
}}
"""


def BlocoBinaria(Nome, Tipo):
    """Teste de uma funcao de 2 argumentos."""
    Sufixo = "" if Tipo == "f64" else "f"
    Especiais = "ESPECIAIS64" if Tipo == "f64" else "ESPECIAIS32"
    Entrada = "t_entrada64" if Tipo == "f64" else "t_entrada32"
    Confere = "t_confere64" if Tipo == "f64" else "t_confere32"
    return f"""
proc t_{Nome}_{Tipo}(estado0: u64) -> int {{
    var erros: int = 0
    for e in 0..<len({Especiais}) {{
        for d in 0..<len({Especiais}) {{
            let x: {Tipo} = transmute({Especiais}[e], to = {Tipo})
            let y: {Tipo} = transmute({Especiais}[d], to = {Tipo})
            erros += {Confere}(math_{Nome}{Sufixo}(x, y), math.{Nome}(x, y), "{Nome} {Tipo}", x, y)
        }}
    }}
    var estado: u64 = estado0
    for i in 0..<PASSOS {{
        estado = t_proximo(estado)
        let x: {Tipo} = {Entrada}(estado, i)
        estado = t_proximo(estado)
        let y: {Tipo} = {Entrada}(estado, (i / 4) + 1)
        erros += {Confere}(math_{Nome}{Sufixo}(x, y), math.{Nome}(x, y), "{Nome} {Tipo}", x, y)
    }}
    return erros
}}
"""


def MontarPrograma(Grupo, TextoModulo, Passos, SoFuncao):
    """Texto do programa de comparacao e a lista de funcoes puladas por ainda nao existirem no modulo."""
    Unarias, Binarias = Gerador.GRUPOS[GRUPO_PARA_FIXTURE[Grupo]]
    Existentes = LerFuncoesDoModulo(TextoModulo)
    Texto = 'import "std:math" as math\n' + TextoModulo + f"\nconst PASSOS: int = {Passos}\n"
    Texto += f"const ESPECIAIS64: [{len(Gerador.ESPECIAIS_64)}]u64 = {{{', '.join(Gerador.ESPECIAIS_64)}}}\n"
    Texto += f"const ESPECIAIS32: [{len(Gerador.ESPECIAIS_32)}]u32 = {{{', '.join(Gerador.ESPECIAIS_32)}}}\n"
    Texto += AUXILIARES
    Chamadas = []
    Puladas = []
    for Tipo in ("f64", "f32"):
        Sufixo = "" if Tipo == "f64" else "f"
        for Nome, Fabrica in [(N, BlocoUnaria) for N in Unarias] + [(N, BlocoBinaria) for N in Binarias]:
            if SoFuncao and Nome != SoFuncao:
                continue
            if f"math_{Nome}{Sufixo}" not in Existentes:
                Puladas.append(f"{Nome} {Tipo}")
                continue
            Texto += Fabrica(Nome, Tipo)
            Chamadas.append((Nome, Tipo))
    Texto += "\nproc main() {\n    var total: int = 0\n"
    for Indice, (Nome, Tipo) in enumerate(Chamadas):
        Texto += f"    let e{Indice}: int = t_{Nome}_{Tipo}({88172645463325252 + Indice})\n"
        Texto += f'    print(f"{Nome} {Tipo}: {{e{Indice}}} divergencias")\n'
        Texto += f"    total += e{Indice}\n"
    Texto += '    print(f"TOTAL {total}")\n}\n'
    return Texto, Puladas, len(Chamadas)


def Rodar(Forja, Programa, Release):
    """Roda o programa no x64 e devolve (exit, stdout, stderr)."""
    with tempfile.TemporaryDirectory() as Pasta:
        Caminho = os.path.join(Pasta, "comparar.fj")
        with open(Caminho, "w", encoding="utf-8", newline="\n") as Arq:
            Arq.write(Programa)
        Comando = [Forja, "run"] + (["--release"] if Release else []) + [Caminho]
        Resultado = subprocess.run(Comando, capture_output=True, text=True, encoding="utf-8", errors="replace")
        return Resultado.returncode, Resultado.stdout, Resultado.stderr


def Selftest():
    """Monta o programa do grupo `math` com o modulo atual (so copysign existe no esqueleto) e confere que compara e passa."""
    Caminho = os.path.join(RAIZ_LANGUAGE, "runtime", "wasm", "rt_wasm_math.fj")
    with open(Caminho, encoding="utf-8") as Arq:
        TextoModulo = Arq.read()
    Programa, Puladas, Quantas = MontarPrograma("math", TextoModulo, 50, "copysign")
    assert Quantas == 2, Quantas
    Codigo, Saida, Erro = Rodar(FORJA_PADRAO, Programa, False)
    assert Codigo == 0, (Codigo, Saida[-400:], Erro[-400:])
    assert "TOTAL 0" in Saida, Saida[-400:]
    print("selftest OK: copysign f64 e f32 iguais ao musl")


def Principal():
    Analisador = argparse.ArgumentParser(description="porte Forja da libm x musl no x64")
    Analisador.add_argument("grupo", nargs="?", choices=sorted(GRUPO_PARA_FIXTURE))
    Analisador.add_argument("--forja", default=FORJA_PADRAO)
    Analisador.add_argument("--release", action="store_true")
    Analisador.add_argument("--passos", type=int, default=Gerador.PASSOS)
    Analisador.add_argument("--funcao", default="")
    Analisador.add_argument("--selftest", action="store_true")
    Argumentos = Analisador.parse_args()
    if Argumentos.selftest:
        Selftest()
        return 0
    if not Argumentos.grupo:
        Analisador.error("informe o grupo")
    Caminho = os.path.join(RAIZ_LANGUAGE, "runtime", "wasm", f"rt_wasm_{Argumentos.grupo}.fj")
    with open(Caminho, encoding="utf-8") as Arq:
        TextoModulo = Arq.read()
    Programa, Puladas, Quantas = MontarPrograma(Argumentos.grupo, TextoModulo, Argumentos.passos, Argumentos.funcao)
    if Quantas == 0:
        print("nada a comparar: o modulo ainda nao tem funcoes deste grupo; puladas:", ", ".join(Puladas))
        return 2
    Codigo, Saida, Erro = Rodar(Argumentos.forja, Programa, Argumentos.release)
    Linhas = Saida.rstrip().splitlines()
    Diferencas = [L for L in Linhas if L.startswith("  DIFF")]
    print("\n".join([L for L in Linhas if not L.startswith("  DIFF")] + Diferencas[:12]))
    if len(Diferencas) > 12:
        print(f"  ... e mais {len(Diferencas) - 12} divergencias (so as 12 primeiras)")
    if Codigo != 0:
        print("falha de compilacao ou execucao (exit %d):" % Codigo)
        print((Erro or Saida)[-1500:])
        return 1
    if Puladas:
        print("puladas (ainda sem porte):", ", ".join(Puladas))
    Inesperadas = [P for P in Puladas if P.split()[0] not in OPCODE_NO_WASM]
    return 0 if ("TOTAL 0" in Saida and not Inesperadas) else 1


if __name__ == "__main__":
    sys.exit(Principal())
