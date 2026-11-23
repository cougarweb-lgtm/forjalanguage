"""Gera runtime\\forja_rt_ryu.h: o Ryu d2s/f2s (third\\ryu, Boost/Apache) mais o texto Display de runtime\\nativo\\rt_real_curto.c.

Uso:
  python -I runtime\\gerar_rt_ryu.py             grava runtime\\forja_rt_ryu.h
  python -I runtime\\gerar_rt_ryu.py --selftest  gera em memoria, compila com gcc -Wall -Wextra -Werror e confere textos conhecidos

O backend C (--modo c / release) imprime real por forja_fmt_f64_curto/forja_fmt_f32_curto deste header; o backend nativo usa o MESMO rt_real_curto.c: o texto sai igual por construcao.
O vendor e o rt_real_curto.c nao sao editados: os ajustes (static, nomes, assert) ficam neste gerador.
Verificacao: python -I runtime\\gerar_rt_ryu.py --selftest

Versao: 0.1.1
Changelog: 0.1.1 - Ryu d2fixed P4: forja_curto_texto_f64 (rt_real_curto.c 0.4.0) tambem vira static no header. 0.1.0 - Ryu d2fixed P1: primeira versao (substitui o laco %.*e + strtod do forja_rt.h).
"""
import os
import re
import subprocess
import sys
import tempfile

VERSAO = "0.1.1"
PASTA_AQUI = os.path.dirname(os.path.abspath(__file__))
PASTA_RYU = os.path.normpath(os.path.join(PASTA_AQUI, "..", "third", "ryu"))
FONTE_CURTO = os.path.join(PASTA_AQUI, "nativo", "rt_real_curto.c")
SAIDA = os.path.join(PASTA_AQUI, "forja_rt_ryu.h")
# Ordem de inclusao do Ryu (a mesma que o d2s.c e o f2s.c pedem): comum, tabela de digitos, d2s e f2s.
PECAS_RYU = [
    ("common.h", None),
    ("digit_table.h", None),
    ("d2s_intrinsics.h", None),
    ("d2s_full_table.h", None),
    ("d2s.c", "d2s"),
    ("f2s_intrinsics.h", None),
    ("f2s.c", "f2s"),
]
INCLUDE_LOCAL = re.compile(r'^\s*#\s*include\s+"ryu/[^"]+"\s*$')
INCLUDE_ASSERT = re.compile(r"^\s*#\s*include\s+<assert\.h>\s*$")
FUNCOES_PUBLICAS = re.compile(r"^(int|void|char\*) ((?:d2s|f2s)(?:_buffered_n|_buffered)?)\(", re.M)
MARCA_INICIO_CURTO = "#define RT_CURTO_DIGITOS_MAX"


def LerTexto(Caminho: str) -> str:
    with open(Caminho, encoding="utf-8", newline="") as Arq:
        return Arq.read().replace("\r\n", "\n")


def PreparaPecaRyu(Nome: str, Sufixo: str | None) -> str:
    """Tira os #include locais e de assert, deixa as funcoes publicas static e separa to_chars e DIGIT_TABLE por peca."""
    Linhas = [L for L in LerTexto(os.path.join(PASTA_RYU, Nome)).split("\n")
              if not INCLUDE_LOCAL.match(L) and not INCLUDE_ASSERT.match(L)]
    Texto = "\n".join(Linhas)
    Texto = re.sub(r"\bDIGIT_TABLE\b", "FORJA_RYU_DIGIT_TABLE", Texto)
    if Sufixo is not None:
        Texto = re.sub(r"\bto_chars\b", "to_chars_" + Sufixo, Texto)
        Texto = FUNCOES_PUBLICAS.sub(r"static FORJA_SU \1 \2(", Texto)
    return "/* ---- third/ryu/" + Nome + " ---- */\n" + Texto + "\n"


def PreparaTextoCurto() -> str:
    """A parte do rt_real_curto.c depois dos #include do Ryu; as 2 funcoes publicas viram static."""
    Texto = LerTexto(FONTE_CURTO)
    Posicao = Texto.find(MARCA_INICIO_CURTO)
    if Posicao < 0:
        raise SystemExit("gerar_rt_ryu: marca '" + MARCA_INICIO_CURTO + "' nao achada em " + FONTE_CURTO)
    Texto = Texto[Posicao:]
    Texto, Trocas = re.subn(r"^void (forja_fmt_f(?:32|64)_curto)\(", r"static FORJA_SU void \1(", Texto, flags=re.M)
    if Trocas != 2:
        raise SystemExit("gerar_rt_ryu: esperava 2 funcoes forja_fmt_f*_curto em rt_real_curto.c, achei " + str(Trocas))
    Texto, Trocas = re.subn(r"^int64_t (forja_curto_texto_f64)\(", r"static FORJA_SU int64_t \1(", Texto, flags=re.M)
    if Trocas != 1:
        raise SystemExit("gerar_rt_ryu: esperava 1 funcao forja_curto_texto_f64 em rt_real_curto.c, achei " + str(Trocas))
    return "/* ---- runtime/nativo/rt_real_curto.c (Display: o mais curto que volta igual, sempre posicional) ---- */\n" + Texto


def Gerar() -> str:
    Partes = [
        "/* forja_rt_ryu.h - GERADO por runtime\\gerar_rt_ryu.py " + VERSAO + ". NAO EDITE: mude o gerador, third\\ryu ou runtime\\nativo\\rt_real_curto.c e gere de novo.\n"
        " * Ryu d2s/f2s (Ulf Adams; Boost Software License 1.0 / Apache-2.0, ver third\\ryu\\LICENSE-Boost) + o texto Display do print de real (forja_fmt_f64_curto e forja_fmt_f32_curto).\n"
        " * Entra no C emitido so quando o programa imprime real (IrPrograma.usa_real_curto); precisa de forja_rt.h (forja_fmt_alinhado) e FORJA_SU antes. */\n"
        "#include <stdint.h>\n#include <stdbool.h>\n#include <string.h>\n#include <stdlib.h>\n#include <inttypes.h>\n"
        '#pragma push_macro("assert")\n#undef assert\n#define assert(x) ((void)0)\n',
    ]
    for Nome, Sufixo in PECAS_RYU:
        Partes.append(PreparaPecaRyu(Nome, Sufixo))
    Partes.append(PreparaTextoCurto())
    Partes.append('#pragma pop_macro("assert")\n')
    return "\n".join(Partes)


CASOS_SELFTEST = [("1e15", "1000000000000000"), ("1e16", "10000000000000000"), ("0.1+0.2", "0.30000000000000004"),
                  ("0.5", "0.5"), ("3.0", "3"), ("5e-324", "0." + "0" * 323 + "5"), ("1e21", "1" + "0" * 21)]
HARNESS = r"""
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#define FORJA_SU __attribute__((unused))
static void forja_fmt_alinhado(const char *p, int64_t n, uint32_t fmt) { (void)fmt; fwrite(p, 1, (size_t)n, stdout); putchar('\n'); }
#include "forja_rt_ryu.h"
int main(void)
{
    forja_fmt_f64_curto(1e15, 0);
    forja_fmt_f64_curto(1e16, 0);
    forja_fmt_f64_curto(0.1 + 0.2, 0);
    forja_fmt_f64_curto(0.5, 0);
    forja_fmt_f64_curto(3.0, 0);
    forja_fmt_f64_curto(4.9406564584124654e-324, 0);
    forja_fmt_f64_curto(1e21, 0);
    forja_fmt_f32_curto((double)3.4028235e38f, 0);
    return 0;
}
"""


def Selftest() -> int:
    Esperado = [E for _, E in CASOS_SELFTEST] + ["340282350000000000000000000000000000000"]
    with tempfile.TemporaryDirectory() as Pasta:
        with open(os.path.join(Pasta, "forja_rt_ryu.h"), "w", encoding="utf-8", newline="\n") as Arq:
            Arq.write(Gerar())
        with open(os.path.join(Pasta, "t.c"), "w", encoding="utf-8") as Arq:
            Arq.write(HARNESS)
        Exe = os.path.join(Pasta, "t.exe")
        Compila = subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-o", Exe, os.path.join(Pasta, "t.c")], capture_output=True, text=True)
        if Compila.returncode != 0:
            print("selftest: gcc falhou\n" + Compila.stderr)
            return 1
        Saida = subprocess.run([Exe], capture_output=True, text=True).stdout.split()
    if Saida != Esperado:
        print("selftest: texto diferente\n obtido:   " + repr(Saida) + "\n esperado: " + repr(Esperado))
        return 1
    print("selftest: ok (" + str(len(Esperado)) + " textos)")
    return 0


def Principal() -> int:
    if "--selftest" in sys.argv[1:]:
        return Selftest()
    with open(SAIDA, "w", encoding="utf-8", newline="\n") as Arq:
        Arq.write(Gerar())
    print("gravado " + SAIDA)
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
