/* bibliotecas_nomes.c - nomes que o ligador cobre sozinho (o CRT e o kernel32) e a normalizacao que a decisao usa (P19 do M3, movido de bibliotecas.c).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M5b P39-B3: BibsCoberta, BibsNormalizar, BibsSemExtensao e BibsMinusculas saem de bibliotecas.c (800 linhas). Mesmo comportamento.
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include "nativo/obj/bibliotecas_nomes.h"

#define BIBS_TAM_NOME_NORMAL 64u

/* Os nomes que o .drectve costuma pedir e que o ligador ja cobre sozinho (o CRT e o kernel32 implicitos); sem o prefixo "lib" e sem a extensao. */
static const char *const BIBS_COBERTAS[] = {"c", "m", "msvcrt", "ucrt", "ucrtbase", "kernel32", "mingw32", "mingwex", "gcc", "moldname", "moldname-msvcrt"};

/* Minusculas, sem a extensao (.lib, .a, .dll) e sem o "lib" na frente. */
static const char *const BIBS_EXTENSOES[] = {".lib", ".a", ".dll"};

static void BibsMinusculas(const char *nome, char saida[BIBS_TAM_NOME_NORMAL])
{
    size_t n = 0;

    for (; nome[n] != '\0' && n + 1 < BIBS_TAM_NOME_NORMAL; n++)
        saida[n] = (char)tolower((unsigned char)nome[n]);
    saida[n] = '\0';
}

static void BibsSemExtensao(char *nome)
{
    char *ponto = strrchr(nome, '.');

    for (size_t k = 0; ponto != NULL && k < sizeof BIBS_EXTENSOES / sizeof BIBS_EXTENSOES[0]; k++) {
        if (strcmp(ponto, BIBS_EXTENSOES[k]) == 0)
            *ponto = '\0';
    }
}

static void BibsNormalizar(const char *nome, char saida[BIBS_TAM_NOME_NORMAL])
{
    BibsMinusculas(nome, saida);
    BibsSemExtensao(saida);
    if (strncmp(saida, "lib", 3) == 0 && saida[3] != '\0')
        memmove(saida, saida + 3, strlen(saida + 3) + 1);
}

bool BibsCoberta(const char *nome)
{
    char normal[BIBS_TAM_NOME_NORMAL];

    BibsNormalizar(nome, normal);
    for (size_t k = 0; k < sizeof BIBS_COBERTAS / sizeof BIBS_COBERTAS[0]; k++) {
        if (strcmp(normal, BIBS_COBERTAS[k]) == 0)
            return true;
    }
    return false;
}
