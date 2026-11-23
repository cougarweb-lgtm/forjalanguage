/* teste_common_atomico.c - verificacoes de unidade do Hash64 e da gravacao atomica (AtomicoVerificacoes, chamada pelo teste_common): 6 verificacoes do P22-O1a.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - Portao Z b1: VerificaSomenteLeitura (destino so-leitura: EACCES, conteudo intacto, sem .tmp nem .velho).
 *            0.1.0 - M3 P22-O1a: Hash64 de "" e "a" (vetores do FNV-1a); ArquivoGravarAtomico cria, substitui, falha em pasta inexistente com errno e, com o destino
 *                    carregado como DLL (o caso real de "em uso": o Windows recusa sobrescrever, mas deixa renomear), renomeia o antigo para <nome>.velho<pid> e grava o novo.
 * Verificacao: (em language\compiler) construir.bat && teste_common.exe
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "common/fonte.h"
#include "common/hash.h"

#define ATOMICO_TAM_PASTA 600
#define ATOMICO_TAM_CAMINHO 1100    /* pasta + nome do arquivo */
#define ATOMICO_TAM_DERIVADO 1200   /* caminho + ".tmp<pid>" ou ".velho<pid>" */

typedef struct {
    int *total;
    int *falhas;
} Contagem;

static void Confere(const Contagem *c, bool ok, const char *texto, int linha)
{
    (*c->total)++;
    if (ok)
        return;
    (*c->falhas)++;
    fprintf(stderr, "FALHOU (atomico) linha %d: %s\n", linha, texto);
}

#define ATOMICO_VERIFICA(c, cond) Confere((c), (cond), #cond, __LINE__)

/* Conteudo do arquivo igual ao texto esperado (e do tamanho dele). */
static bool ArquivoTem(const char *caminho, const char *esperado)
{
    char lido[64] = {0};
    FILE *arq = fopen(caminho, "rb");
    size_t n;

    if (arq == NULL)
        return false;
    n = fread(lido, 1, sizeof lido - 1, arq);
    fclose(arq);
    return n == strlen(esperado) && memcmp(lido, esperado, n) == 0;
}

static bool ArquivoExiste(const char *caminho)
{
    return GetFileAttributesA(caminho) != INVALID_FILE_ATTRIBUTES;
}

/* Pasta propria deste processo em %TEMP%; o teste a apaga no fim. */
static void PastaDoTeste(char *pasta, size_t tam)
{
    const char *pasta_temporaria = getenv("TEMP");

    snprintf(pasta, tam, "%s\\forja_atomico_%lu", pasta_temporaria != NULL ? pasta_temporaria : ".", (unsigned long)GetCurrentProcessId());
    CreateDirectoryA(pasta, NULL);
}

static void VerificaHash(const Contagem *c)
{
    ATOMICO_VERIFICA(c, Hash64("", 0, HASH64_SEMENTE) == UINT64_C(0xcbf29ce484222325));
    ATOMICO_VERIFICA(c, Hash64("a", 1, HASH64_SEMENTE) == UINT64_C(0xaf63dc4c8601ec8c));
}

/* Cria o arquivo (sem sobra de <nome>.tmp<pid>), substitui o conteudo e falha em pasta que nao existe com ENOENT, sem criar o destino. */
static void VerificaGravacao(const Contagem *c, const char *pasta)
{
    char destino[ATOMICO_TAM_CAMINHO], sobra[ATOMICO_TAM_DERIVADO], inexistente[ATOMICO_TAM_CAMINHO];
    int err = 0;

    snprintf(destino, sizeof destino, "%s\\saida.bin", pasta);
    snprintf(sobra, sizeof sobra, "%s.tmp%lu", destino, (unsigned long)GetCurrentProcessId());
    snprintf(inexistente, sizeof inexistente, "%s\\nao_existe\\saida.bin", pasta);
    ATOMICO_VERIFICA(c, ArquivoGravarAtomico(destino, "primeiro", 8, NULL) && ArquivoTem(destino, "primeiro") && !ArquivoExiste(sobra));
    ATOMICO_VERIFICA(c, ArquivoGravarAtomico(destino, "segundo", 7, &err) && ArquivoTem(destino, "segundo") && !ArquivoExiste(sobra));
    ATOMICO_VERIFICA(c, !ArquivoGravarAtomico(inexistente, "x", 1, &err) && err == ENOENT && !ArquivoExiste(inexistente));
    remove(destino);
}

/* Uma copia de uma DLL do sistema carregada no processo: sobrescrever e negado, renomear nao. O antigo vira <nome>.velho<pid> e o novo ocupa o nome. */
static void VerificaDestinoEmUso(const Contagem *c, const char *pasta)
{
    char dll[ATOMICO_TAM_CAMINHO], velho[ATOMICO_TAM_DERIVADO], original[ATOMICO_TAM_CAMINHO];
    const char *raiz = getenv("SystemRoot");
    HMODULE modulo;
    bool trocou;

    snprintf(dll, sizeof dll, "%s\\atomico_em_uso.dll", pasta);
    snprintf(velho, sizeof velho, "%s.velho%lu", dll, (unsigned long)GetCurrentProcessId());
    snprintf(original, sizeof original, "%s\\System32\\version.dll", raiz != NULL ? raiz : "C:\\Windows");
    modulo = CopyFileA(original, dll, FALSE) ? LoadLibraryA(dll) : NULL;
    trocou = modulo != NULL && ArquivoGravarAtomico(dll, "novo", 4, NULL);
    ATOMICO_VERIFICA(c, trocou && ArquivoTem(dll, "novo") && ArquivoExiste(velho));
    if (modulo != NULL)
        FreeLibrary(modulo);
    remove(dll);
    remove(velho);
}

/* Destino so-leitura: a gravacao recusa com EACCES, sem tocar no conteudo e sem deixar <nome>.tmp<pid> nem <nome>.velho<pid>. */
static void VerificaSomenteLeitura(const Contagem *c, const char *pasta)
{
    char destino[ATOMICO_TAM_CAMINHO], sobra[ATOMICO_TAM_DERIVADO], velho[ATOMICO_TAM_DERIVADO];
    int err = 0;
    FILE *arq;

    snprintf(destino, sizeof destino, "%s\\so_leitura.bin", pasta);
    snprintf(sobra, sizeof sobra, "%s.tmp%lu", destino, (unsigned long)GetCurrentProcessId());
    snprintf(velho, sizeof velho, "%s.velho%lu", destino, (unsigned long)GetCurrentProcessId());
    arq = fopen(destino, "wb");
    if (arq != NULL) {
        fwrite("original", 1, 8, arq);
        fclose(arq);
    }
    SetFileAttributesA(destino, FILE_ATTRIBUTE_READONLY);
    ATOMICO_VERIFICA(c, !ArquivoGravarAtomico(destino, "novo", 4, &err) && err == EACCES && ArquivoTem(destino, "original") && !ArquivoExiste(sobra) && !ArquivoExiste(velho));
    SetFileAttributesA(destino, FILE_ATTRIBUTE_NORMAL);
    remove(destino);
}

void AtomicoVerificacoes(int *total, int *falhas)
{
    Contagem c = {total, falhas};
    char pasta[ATOMICO_TAM_PASTA];

    PastaDoTeste(pasta, sizeof pasta);
    VerificaHash(&c);
    VerificaGravacao(&c, pasta);
    VerificaDestinoEmUso(&c, pasta);
    VerificaSomenteLeitura(&c, pasta);
    RemoveDirectoryA(pasta);
}
