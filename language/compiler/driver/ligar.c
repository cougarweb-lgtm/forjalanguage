/* ligar.c - 'forja ligar': repassa a linha de comando a "python -X utf8 <script>"; o script (gerador de foreign) entra no forja.exe por .incbin.
 * A pasta do script vem de -DFORJA_LIGAR_PASTA (barras normais, sem aspas), mesmo padrao do modulos_std.c. Sem -I e sem -s: a libclang
 * mora no site do usuario (D2). FORJA_PYTHON troca o executavel; FORJA_LIGAR_SCRIPT (so desenvolvimento) troca o script embutido por um arquivo.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - 0.21.0 F4 passo 2b: include driver/processo.h (cc.h saiu); ProcExecutarComArgumentos e ProcArquivoTemporario no lugar dos Cc*.
 *            0.1.0 - Linguagem v1.8b P7: RodarLigar (GravarScript em %TEMP%, G0101 se o Python nao iniciar).
 * Verificacao: python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/base.h"
#include "common/diag.h"
#include "driver/ligar.h"
#include "driver/processo.h"

#ifndef FORJA_LIGAR_PASTA
#error "defina -DFORJA_LIGAR_PASTA=<pasta language/tools/forja_ligar, com barras normais>"
#endif

#define FORJA_TEXTO_AUX(x) #x
#define FORJA_TEXTO(x) FORJA_TEXTO_AUX(x)

#define LIGAR_CAMINHO_MAX 600
#define LIGAR_SAIDA_USO 2
#define LIGAR_SAIDA_INTERNO 3

__asm__(
    ".section .rdata,\"dr\"\n"
    ".globl forja_ligar_py\n"
    "forja_ligar_py:\n"
    ".incbin \"" FORJA_TEXTO(FORJA_LIGAR_PASTA) "/forja_ligar.py\"\n"
    ".globl forja_ligar_py_fim\n"
    "forja_ligar_py_fim:\n"
    ".byte 0\n"
    ".text\n");

extern const char forja_ligar_py[];
extern const char forja_ligar_py_fim[];

static const char *ValorDoAmbiente(const char *nome, const char *padrao)
{
    const char *v = getenv(nome);

    return v != NULL && v[0] != '\0' ? v : padrao;
}

/* Grava o script embutido em 'caminho'. false = nao deu para gravar (nada fica no disco). */
static bool GravarScript(const char *caminho)
{
    size_t tam = (size_t)(forja_ligar_py_fim - forja_ligar_py);
    FILE *f = fopen(caminho, "wb");
    bool ok;

    if (f == NULL)
        return false;
    ok = fwrite(forja_ligar_py, 1, tam, f) == tam;
    ok = fclose(f) == 0 && ok;
    if (!ok)
        remove(caminho);
    return ok;
}

/* args = {"-X", "utf8", script, argv[0..argc-1]}. Devolve o exit do script, ou LIGAR_SAIDA_USO (G0101) se o Python nao iniciou. */
static int ExecutarPython(const char *python, const char *script, int argc, char **argv)
{
    const char **args = malloc(((size_t)argc + 3) * sizeof *args);
    char causa[LIGAR_CAMINHO_MAX];
    int codigo = 0;
    bool iniciou;

    if (args == NULL)
        FalhaInterna("ligar: memoria esgotada ao montar os argumentos do python");
    args[0] = "-X";
    args[1] = "utf8";
    args[2] = script;
    for (int k = 0; k < argc; k++)
        args[k + 3] = argv[k];
    iniciou = ProcExecutarComArgumentos(python, args, argc + 3, &codigo);
    free(args);
    if (iniciou)
        return codigo;
    snprintf(causa, sizeof causa, "python nao encontrado (tentou '%s'): instale o Python 3.10+ ou aponte FORJA_PYTHON para o python.exe",
             python);
    DiagImprimirJsonSemFonte(G0101, causa, stderr);
    return LIGAR_SAIDA_USO;
}

int RodarLigar(int argc, char **argv)
{
    const char *externo = ValorDoAmbiente("FORJA_LIGAR_SCRIPT", NULL);
    char temporario[LIGAR_CAMINHO_MAX];
    int codigo;

    if (externo != NULL)
        return ExecutarPython(ValorDoAmbiente("FORJA_PYTHON", "python"), externo, argc, argv);
    if (!ProcArquivoTemporario(temporario, sizeof temporario, "_ligar.py") || !GravarScript(temporario)) {
        fprintf(stderr, "forja: nao deu para gravar %s\n", temporario);
        return LIGAR_SAIDA_INTERNO;
    }
    codigo = ExecutarPython(ValorDoAmbiente("FORJA_PYTHON", "python"), temporario, argc, argv);
    remove(temporario);
    return codigo;
}
