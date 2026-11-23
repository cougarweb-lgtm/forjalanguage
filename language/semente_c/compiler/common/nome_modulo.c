/* nome_modulo.c - chave por modulo dos simbolos de topo (IA1-L8): tag do modulo, "<L><tag>_<nome>" e o nome puro de volta.
 * Versao: 0.1.0
 * Changelog: 0.1.0 - IA1-L8 P1: primeira versao.
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/nome_modulo.h"

#include <stdio.h>

static bool CaractereDeIdentificador(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/* Ultimo segmento do caminho (depois do ultimo '/' ou ':'), com o que nao e identificador trocado por '_'. Vazio vira "m"; nunca comeca por digito. */
static void TagNatural(const char *caminho, char *tag)
{
    const char *segmento = caminho;
    size_t n = 0;
    for (const char *p = caminho; *p != '\0'; p++) {
        if (*p == '/' || *p == ':') segmento = p + 1;
    }
    if (*segmento == '\0' || (*segmento >= '0' && *segmento <= '9')) tag[n++] = 'm';
    for (const char *p = segmento; *p != '\0' && n + 1 < NOME_MODULO_TAG_MAX - 12; p++) tag[n++] = CaractereDeIdentificador(*p) ? *p : '_';
    tag[n] = '\0';
}

static bool TagJaUsada(char (*tags)[NOME_MODULO_TAG_MAX], size_t ate, const char *tag)
{
    for (size_t j = 0; j < ate; j++) {
        if (strcmp(tags[j], tag) == 0) return true;
    }
    return false;
}

/* O 1o modulo da ordem fica com a tag natural; o seguinte com a mesma tag (ou com uma que um sufixo anterior ja ocupou) ganha "_<i>" ate ficar livre. */
void NomeModuloTags(const char *const *caminhos, size_t n, char (*tags)[NOME_MODULO_TAG_MAX])
{
    for (size_t i = 0; i < n; i++) {
        TagNatural(caminhos[i], tags[i]);
        while (TagJaUsada(tags, i, tags[i])) {
            size_t tam = strlen(tags[i]);
            if (tam + 12 >= NOME_MODULO_TAG_MAX) tags[i][NOME_MODULO_TAG_MAX - 13] = '\0';
            tam = strlen(tags[i]);
            snprintf(tags[i] + tam, NOME_MODULO_TAG_MAX - tam, "_%u", (unsigned)i);
        }
    }
}

size_t NomeModuloChave(const char *tag, const char *nome, size_t tam_nome, char *saida, size_t cap)
{
    int n = snprintf(saida, cap, "%u%s_%.*s", (unsigned)strlen(tag), tag, (int)tam_nome, nome);
    return n > 0 && (size_t)n < cap ? (size_t)n : 0;
}

const char *NomeModuloExibicao(const char *chave)
{
    size_t tam_tag = 0, digitos = 0;
    if (chave[0] < '1' || chave[0] > '9') return chave;
    while (chave[digitos] >= '0' && chave[digitos] <= '9') tam_tag = tam_tag * 10 + (size_t)(chave[digitos++] - '0');
    if (tam_tag == 0 || strlen(chave + digitos) < tam_tag + 1 || chave[digitos + tam_tag] != '_') return chave;
    return chave + digitos + tam_tag + 1;
}
