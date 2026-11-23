"""Gera runtime\\forja_rt_math.h a partir de third\\musl_math (musl 1.2.6, MIT).

Uso:
  python -I runtime\\musl_math\\gerar_rt_math.py             grava runtime\\forja_rt_math.h
  python -I runtime\\musl_math\\gerar_rt_math.py --selftest  gera em memoria e confere (gcc, nm)

Tudo sai static (nada com ligacao externa); nomes de libm, publicos e internos, viram
forja_math_* por #define. O vendor nao e editado: os ajustes ficam neste gerador.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

VERSAO = "0.1.2"
PASTA_AQUI = os.path.dirname(os.path.abspath(__file__))
PASTA_VENDOR = os.path.normpath(os.path.join(PASTA_AQUI, "..", "..", "third", "musl_math"))
SAIDA = os.path.normpath(os.path.join(PASTA_AQUI, "..", "forja_rt_math.h"))
FUNCOES_DOUBLE = ("sin cos tan asin acos atan atan2 exp exp2 log log2 log10 pow cbrt "
                  "hypot fmod floor ceil round trunc copysign").split()
FUNCOES_FLOAT = [Nome + "f" for Nome in FUNCOES_DOUBLE]
PUBLICAS = FUNCOES_DOUBLE + FUNCOES_FLOAT + ["sqrt", "sqrtf"]
CABECALHOS_VENDOR = ["fp_arch.h", "libm.h", "exp_data.h", "exp2f_data.h", "log_data.h",
                     "log2_data.h", "logf_data.h", "log2f_data.h", "pow_data.h", "powf_data.h"]
FLAGS_RT = ["-O2", "-ffreestanding", "-fno-builtin", "-mno-stack-arg-probe",
            "-Wall", "-Wextra", "-Werror", "-ffp-contract=off", "-fno-math-errno"]
FLAGS_DRIVER = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
# Avisos que o gcc acusa no vendor com -Wall -Wextra -Werror. So entram aqui; o vendor nao muda.
AVISOS_IGNORADOS = ["-Wunused-but-set-variable", "-Wparentheses", "-Wmaybe-uninitialized"]
PALAVRAS_CHAVE = {"static", "inline", "const", "volatile", "struct", "union", "enum", "hidden",
                  "extern", "unsigned", "signed", "int", "long", "short", "char", "float",
                  "double", "void", "typedef", "return", "sizeof", "if", "for", "while", "do",
                  "else", "switch", "case"}
TIPO_DECL = re.compile(r"(static|hidden|extern|const|volatile|inline|unsigned|signed|int|long|"
                       r"short|char|float|double|void|struct|union|uint\d+_t|int\d+_t|float_t|double_t)\b")
TIPO_DEFINICAO = re.compile(r"(struct|union|enum)\b[^(=;]*\{")
PADRAO_DEFINICAO = re.compile(r"^((?:static __attribute__\(\(unused\)\) |static |FORJA_MATH_PUBLICA )[^\n;{]*\))[ \t]*\n\{", re.M)
PADRAO_PROTOTIPO = re.compile(r"^(?:hidden|static __attribute__\(\(unused\)\)|FORJA_MATH_PUBLICA) [^\n;{=]*\)\s*;[ \t]*$", re.M)

PRELUDIO = """\
#define FLT_EVAL_METHOD __FLT_EVAL_METHOD__
#if FLT_EVAL_METHOD != 0
#error "forja_rt_math.h exige FLT_EVAL_METHOD 0 (x86-64 SSE)"
#endif
typedef float float_t;
typedef double double_t;
#define LDBL_MANT_DIG __LDBL_MANT_DIG__
#define LDBL_MAX_EXP __LDBL_MAX_EXP__
#define LDBL_EPSILON __LDBL_EPSILON__
#define DBL_EPSILON __DBL_EPSILON__
#define FLT_EPSILON __FLT_EPSILON__
#define __BYTE_ORDER __BYTE_ORDER__
#define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#define __BIG_ENDIAN __ORDER_BIG_ENDIAN__
#define INFINITY __builtin_inff()
#define HUGE_VAL __builtin_huge_val()
#define NAN __builtin_nanf("")
#define M_PI_2 1.57079632679489661923
#define isnan(x) __builtin_isnan(x)
#define hidden static __attribute__((unused))
"""

# FORJA_RT_MATH_EXPORTAR (rt_math.o, runtime nativo): as 44 funcoes publicas saem globais (ligacao externa).
# Sem a macro (C emitido pelo backend C) saem static, como antes: o programa C tem uma so unidade.
EXPORTACAO_TEXTO = """\
#ifdef FORJA_RT_MATH_EXPORTAR
#define FORJA_MATH_PUBLICA
#else
#define FORJA_MATH_PUBLICA static __attribute__((unused))
#endif
"""

SQRT_TEXTO = """\
/* sqrt/sqrtf sem libc: a libm do musl chama sqrt; SSE e IEEE correto (arredondamento nearest). */
FORJA_MATH_PUBLICA double forja_math_sqrt(double x)
{
	double r;
	__asm__("sqrtsd %1, %0" : "=x"(r) : "x"(x));
	return r;
}
FORJA_MATH_PUBLICA float forja_math_sqrtf(float x)
{
	float r;
	__asm__("sqrtss %1, %0" : "=x"(r) : "x"(x));
	return r;
}
"""


def CabecalhoTexto():
    return (
        "/*\n"
        " * forja_rt_math.h " + VERSAO + " - libm do musl 1.2.6 (double e float) numa unidade so.\n"
        " * GERADO por runtime\\musl_math\\gerar_rt_math.py a partir de third\\musl_math (MIT, ver LEIAME.txt).\n"
        " * Nao editar a mao: o ajuste vai no gerador. Tudo static (sem ligacao externa); nomes de libm\n"
        " * renomeados para forja_math_* por #define. Sem <math.h>/<stdint.h>/<errno.h> do sistema:\n"
        " * tipos e macros vem de __*__ do gcc. errno nao e usado pelo musl 1.2.6 (nada a silenciar).\n"
        " * Exige FLT_EVAL_METHOD 0 (x86-64 SSE) e nao usa FMA (__FP_FAST_FMA nao definido sem -mfma).\n"
        " * Changelog:\n"
        " *   0.1.2 (2026-11-13): -Wmaybe-uninitialized ignorado no vendor (__rem_pio2_large, com constprop no C emitido: falso positivo do gcc).\n"
        " *   0.1.1 (2026-11-13): FORJA_RT_MATH_EXPORTAR exporta as 44 publicas (rt_math.o); sem a macro, static como antes. L11a-2 (S3 P2).\n"
        " *   0.1.0 (2026-11-13): primeira versao, L11a-1 (S3 P1). 42 funcoes + sqrt/sqrtf.\n"
        " * Verificacao: python -I runtime\\musl_math\\gerar_rt_math.py --selftest\n"
        " *               python -I tests\\nativo\\s3\\math\\verificar_math.py\n"
        " */\n")


def PreludioInteiros():
    Linhas = []
    for Bits in (8, 16, 32, 64):
        Linhas.append("typedef __INT%d_TYPE__ forja_math_i%d;" % (Bits, Bits))
        Linhas.append("typedef __UINT%d_TYPE__ forja_math_u%d;" % (Bits, Bits))
        Linhas.append("#define int%d_t forja_math_i%d" % (Bits, Bits))
        Linhas.append("#define uint%d_t forja_math_u%d" % (Bits, Bits))
    return "\n".join(Linhas) + "\n"


def PragmasAvisos():
    Linhas = ["#pragma GCC diagnostic push"]
    Linhas += ['#pragma GCC diagnostic ignored "%s"' % Aviso for Aviso in AVISOS_IGNORADOS]
    return "\n".join(Linhas) + "\n"


def DefinesRenome(Nomes):
    Linhas = ["#define %s forja_math_%s" % (Nome, Nome) for Nome in Nomes]
    return "\n".join(Linhas) + "\n"


def ChecarVendor():
    Esperados = CABECALHOS_VENDOR + [Nome + ".c" for Nome in FUNCOES_DOUBLE + FUNCOES_FLOAT]
    Faltando = [Nome for Nome in Esperados if not os.path.isfile(os.path.join(PASTA_VENDOR, Nome))]
    if Faltando:
        raise SystemExit("vendor incompleto em " + PASTA_VENDOR + ": " + ", ".join(Faltando))


def FontesC():
    return sorted(Nome for Nome in os.listdir(PASTA_VENDOR) if Nome.endswith(".c"))


def LerVendor(Nome):
    with open(os.path.join(PASTA_VENDOR, Nome), encoding="utf-8") as Arq:
        return Arq.read()


def MascararComentarios(Texto):
    def Apagar(Casamento):
        return re.sub(r"[^\n]", " ", Casamento.group(0))
    Texto = re.sub(r"/\*.*?\*/", Apagar, Texto, flags=re.S)
    return re.sub(r"//[^\n]*", Apagar, Texto)


def NomeDaLinha(Linha):
    Achado = re.search(r"([A-Za-z_]\w*)\s*\(", Linha) or re.search(r"([A-Za-z_]\w*)\s*[=\[;]", Linha)
    return Achado.group(1) if Achado else None


def NomeDeLinhaSolta(Linha):
    Achado = re.match(r"([A-Za-z_]\w*)\s*=", Linha)
    return Achado.group(1) if Achado else None


def NomesDeclarados(Mascarado):
    Nomes = set()
    for Linha in Mascarado.split("\n"):
        Nome = None
        Fecho = re.match(r"\}\s*([A-Za-z_]\w*)\s*;", Linha)
        if Fecho:
            Nome = Fecho.group(1)
        elif TIPO_DECL.match(Linha) and not TIPO_DEFINICAO.match(Linha):
            Nome = NomeDaLinha(Linha)
        else:
            Nome = NomeDeLinhaSolta(Linha)
        if Nome and Nome not in PALAVRAS_CHAVE:
            Nomes.add(Nome)
    return Nomes


def NomesDoArquivo(Texto):
    return NomesDeclarados(MascararComentarios(Texto))


def ColisoesEntreFontes(Fontes):
    # Nome definido em dois .c (ex.: C1 em __cos.c e __cosdf.c): vira escopo por arquivo.
    Contagem = {}
    for Texto in Fontes.values():
        for Nome in NomesDoArquivo(Texto):
            Contagem[Nome] = Contagem.get(Nome, 0) + 1
    return {Nome for Nome, Vezes in Contagem.items() if Vezes > 1}


def NomesProtegidos(Textos):
    # Idioma "#ifndef X / #define X X / static ... X(...)" do vendor: X nao pode ser renomeado.
    Protegidos = set()
    for Texto in Textos:
        Protegidos |= set(re.findall(r"#\s*ifndef\s+(\w+)", Texto))
    return Protegidos


def NomesGlobais(Textos, Colisoes):
    # So nomes em caixa baixa viram renome global; constantes (C1, EPS) ficam no escopo do arquivo.
    Achados = NomesDoArquivo("\n".join(Textos))
    Candidatos = (Achados | set(PUBLICAS)) - Colisoes - NomesProtegidos(Textos)
    return sorted(Nome for Nome in Candidatos if re.fullmatch(r"[a-z_]\w*", Nome))


def PrefixarLinha(Original, Mascara):
    if re.match(r"extern\s+", Mascara):
        Original = re.sub(r"^extern\s+", "", Original)
        Mascara = re.sub(r"^extern\s+", "", Mascara)
    if not TIPO_DECL.match(Mascara) or TIPO_DEFINICAO.match(Mascara):
        return Original
    if re.match(r"(static|hidden)\b", Mascara):
        return Original
    if NomeDaLinha(Mascara) in PUBLICAS:
        return "FORJA_MATH_PUBLICA " + Original
    return "static __attribute__((unused)) " + Original


def PrefixarTexto(Texto):
    Mascarado = MascararComentarios(Texto)
    Saida = []
    for Original, Mascara in zip(Texto.split("\n"), Mascarado.split("\n")):
        if re.match(r"\s*#\s*include\b", Mascara):
            continue
        Saida.append(PrefixarLinha(Original, Mascara))
    return "\n".join(Saida)


def EnvolverFonte(Nome, Texto, Colisoes):
    # Escopo por arquivo: cada .c vira uma "unidade" de nomes e de #define (como TU separada).
    Stem = os.path.splitext(Nome)[0]
    Mascarado = MascararComentarios(Texto)
    Locais = sorted(NomesDeclarados(Mascarado) & Colisoes)
    Macros = sorted(set(re.findall(r"^\s*#\s*define\s+(\w+)", Mascarado, re.M)))
    Abre = ["#define %s forja_math_%s_%s" % (Local, Stem, Local) for Local in Locais]
    Prefixado = PrefixarTexto(Texto)
    Protos = ProtosDoArquivo(Prefixado, Colisoes, True)
    Fecha = ["#undef %s" % Macro for Macro in Locais + Macros]
    return "\n".join(Abre + Protos + [Prefixado] + Fecha)


def NomeDaCabeca(Cabeca):
    return NomeDaLinha(Cabeca.replace("__attribute__((unused))", ""))


def ProtosDoArquivo(Texto, Colisoes, EhLocal):
    # EhLocal=True: prototipos de nomes em colisao (vao no escopo do arquivo); False: os globais.
    Protos = []
    for Achado in PADRAO_DEFINICAO.finditer(Texto):
        Cabeca = Achado.group(1)
        if (NomeDaCabeca(Cabeca) in Colisoes) == EhLocal:
            Protos.append(Cabeca + ";")
    return Protos


def NomesDefinidos(Fontes):
    # Funcoes com corpo no bundle; prototipo sem corpo daria "declared static but never defined".
    Definidos = set()
    for Texto in Fontes.values():
        for Achado in PADRAO_DEFINICAO.finditer(PrefixarTexto(Texto)):
            Definidos.add(NomeDaCabeca(Achado.group(1)))
    return Definidos


def RemoverProtosOrfaos(Texto, Definidos):
    def Decidir(Achado):
        return Achado.group(0) if NomeDaCabeca(Achado.group(0)) in Definidos else ""
    return PADRAO_PROTOTIPO.sub(Decidir, Texto)


def PrototiposDasFontes(Fontes, Colisoes):
    # O vendor usa funcoes de arquivos lidos depois (ordem alfabetica): prototipo global antes do corpo.
    Protos = []
    for Texto in Fontes.values():
        Protos += ProtosDoArquivo(PrefixarTexto(Texto), Colisoes, False)
    return "\n".join(Protos) + "\n"


def GerarTexto():
    ChecarVendor()
    Cabecalhos = {Nome: LerVendor(Nome) for Nome in CABECALHOS_VENDOR}
    Fontes = {Nome: LerVendor(Nome) for Nome in FontesC()}
    Textos = list(Cabecalhos.values()) + list(Fontes.values())
    Colisoes = ColisoesEntreFontes(Fontes)
    Definidos = NomesDefinidos(Fontes)
    Corpo = [RemoverProtosOrfaos(PrefixarTexto(Texto), Definidos) for Texto in Cabecalhos.values()]
    Corpo += [EnvolverFonte(Nome, Texto, Colisoes) for Nome, Texto in Fontes.items()]
    Nomes = NomesGlobais(Textos, Colisoes)
    Partes = [
        CabecalhoTexto(),
        "#ifndef FORJA_RT_MATH_H\n#define FORJA_RT_MATH_H\n",
        PRELUDIO,
        EXPORTACAO_TEXTO,
        PreludioInteiros(),
        PragmasAvisos(),
        DefinesRenome(Nomes),
        SQRT_TEXTO,
        PrototiposDasFontes(Fontes, Colisoes),
        "\n".join(Corpo),
        "\n#pragma GCC diagnostic pop\n#endif /* FORJA_RT_MATH_H */\n",
    ]
    return "\n".join(Partes)


def TextoUso():
    Itens = ", ".join("(void *)forja_math_%s" % Nome for Nome in PUBLICAS)
    return '#include "forja_rt_math.h"\nvoid *const forja_math_uso[] = {%s};\n' % Itens


def AnalisarNm(Saida):
    Problemas = []
    for Linha in Saida.splitlines():
        Partes = Linha.split()
        if len(Partes) == 2 and Partes[0] == "U" and Partes[1] not in ("memcpy", "memset"):
            Problemas.append("nao permitido em nm -u: " + Partes[1])
        if len(Partes) == 3 and Partes[1] in "TDBRCGVWSI" and Partes[2] != "forja_math_uso":
            Problemas.append("simbolo com ligacao externa: " + Partes[2])
    return Problemas


def ChecarCompilacao(Texto, Flags):
    Pasta = tempfile.mkdtemp(prefix="forja_math_")
    try:
        with open(os.path.join(Pasta, "forja_rt_math.h"), "w", encoding="utf-8", newline="\n") as Arq:
            Arq.write(Texto)
        with open(os.path.join(Pasta, "uso.c"), "w", encoding="utf-8", newline="\n") as Arq:
            Arq.write(TextoUso())
        Cmd = ["gcc"] + Flags + ["-c", "uso.c", "-o", "uso.o"]
        Res = subprocess.run(Cmd, capture_output=True, text=True, cwd=Pasta)
        if Res.returncode != 0:
            return ["gcc falhou (" + " ".join(Flags) + "): " + Res.stderr[:1500]]
        Nm = subprocess.run(["nm", "uso.o"], capture_output=True, text=True, cwd=Pasta)
        if Nm.returncode != 0:
            return ["nm falhou: " + Nm.stderr[:500]]
        return AnalisarNm(Nm.stdout)
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def TextoExportacao():
    return '#define FORJA_RT_MATH_EXPORTAR\n#include "forja_rt_math.h"\n'


def AnalisarExportacao(Saida):
    # Com FORJA_RT_MATH_EXPORTAR o globais tem de ser exatamente as 44 publicas (nada interno vaza).
    Problemas = []
    Globais = set()
    for Linha in Saida.splitlines():
        Partes = Linha.split()
        if len(Partes) == 2 and Partes[0] == "U" and Partes[1] not in ("memcpy", "memset"):
            Problemas.append("nao permitido em nm -u: " + Partes[1])
        if len(Partes) == 3 and Partes[1] in "TDBRCGVWSI":
            Globais.add(Partes[2])
    Esperados = {"forja_math_" + Nome for Nome in PUBLICAS}
    if Globais != Esperados:
        Problemas.append("globais fora do esperado: sobram " + str(sorted(Globais - Esperados))
                         + " / faltam " + str(sorted(Esperados - Globais)))
    return Problemas


def ChecarExportacao(Texto):
    Pasta = tempfile.mkdtemp(prefix="forja_math_")
    try:
        with open(os.path.join(Pasta, "forja_rt_math.h"), "w", encoding="utf-8", newline="\n") as Arq:
            Arq.write(Texto)
        with open(os.path.join(Pasta, "exporta.c"), "w", encoding="utf-8", newline="\n") as Arq:
            Arq.write(TextoExportacao())
        Res = subprocess.run(["gcc"] + FLAGS_RT + ["-c", "exporta.c", "-o", "exporta.o"], capture_output=True, text=True, cwd=Pasta)
        if Res.returncode != 0:
            return ["gcc falhou (exportacao): " + Res.stderr[:1500]]
        Nm = subprocess.run(["nm", "exporta.o"], capture_output=True, text=True, cwd=Pasta)
        if Nm.returncode != 0:
            return ["nm falhou (exportacao): " + Nm.stderr[:500]]
        return AnalisarExportacao(Nm.stdout)
    finally:
        shutil.rmtree(Pasta, ignore_errors=True)


def Selftest():
    Primeiro = GerarTexto()
    Segundo = GerarTexto()
    Falhas = []
    if Primeiro != Segundo:
        Falhas.append("geracao nao deterministica")
    if "#include <" in Primeiro:
        Falhas.append("include de sistema no header")
    for Nome in PUBLICAS:
        if "#define %s forja_math_%s\n" % (Nome, Nome) not in Primeiro:
            Falhas.append("sem renome para " + Nome)
    for Flags in (FLAGS_RT, FLAGS_DRIVER):
        Falhas += ChecarCompilacao(Primeiro, Flags)
    Falhas += ChecarExportacao(Primeiro)
    if Falhas:
        print("selftest FALHOU:")
        for Falha in Falhas:
            print("  - " + Falha)
        return 1
    print("selftest OK: determinismo, sem include de sistema, renomes, gcc (-a e -b), nm, exportacao (44 globais)")
    return 0


def Main(Argumentos):
    if "--selftest" in Argumentos:
        return Selftest()
    Texto = GerarTexto()
    with open(SAIDA, "w", encoding="utf-8", newline="\n") as Arq:
        Arq.write(Texto)
    print("gravado: " + SAIDA + " (" + str(len(Texto)) + " bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(Main(sys.argv[1:]))
