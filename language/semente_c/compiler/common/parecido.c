/* parecido.c - "voce quis dizer": distancia Damerau-Levenshtein (OSA) e a lista dos 3 candidatos mais proximos.
 * Versao: 0.2.0
 * Changelog: 0.2.0 - IA1-b14: ParecidosVerEm (o candidato leva a posicao da declaracao); ParecidosVer chama com PARECIDO_SEM_POS.
 *            0.1.0 - Linguagem v1.11 L5: Parecido, ParecidosIniciar e ParecidosVer (B02); nome igual ao alvo (distancia 0) nao e sugestao.
 * Verificacao: construir.bat && teste_common.exe
 */
#include "common/parecido.h"

#define PARECIDO_DIST_MAX 2 /* a sugestao so aceita nomes ate essa distancia */

static uint32_t Menor(uint32_t x, uint32_t y) { return x < y ? x : y; }

/* a[i-2..i-1] e b[j-2..j-1] trocados de lugar: as duas letras vizinhas estao invertidas. */
static bool EhTransposicao(const char *a, const char *b, size_t i, size_t j)
{
    return i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1];
}

static uint32_t CelulaParecido(uint32_t m[][PARECIDO_TAM + 1], const char *a, const char *b, size_t i, size_t j)
{
    uint32_t custo = a[i - 1] == b[j - 1] ? 0 : 1;
    uint32_t v = Menor(Menor(m[i - 1][j] + 1, m[i][j - 1] + 1), m[i - 1][j - 1] + custo);
    if (EhTransposicao(a, b, i, j)) v = Menor(v, m[i - 2][j - 2] + 1);
    return v;
}

uint32_t Parecido(const char *a, size_t na, const char *b, size_t nb)
{
    uint32_t m[PARECIDO_TAM + 1][PARECIDO_TAM + 1];
    if (na > PARECIDO_TAM || nb > PARECIDO_TAM) return PARECIDO_LONGE;
    for (size_t i = 0; i <= na; i++) m[i][0] = (uint32_t)i;
    for (size_t j = 0; j <= nb; j++) m[0][j] = (uint32_t)j;
    for (size_t i = 1; i <= na; i++) {
        for (size_t j = 1; j <= nb; j++) m[i][j] = CelulaParecido(m, a, b, i, j);
    }
    return m[na][nb];
}

/* Ordem da sugestao: menor distancia primeiro; empate pelo nome (bytes, depois o mais curto). */
static int ComparaNomes(const ParecidoNome *a, const ParecidoNome *b)
{
    size_t m = a->tam < b->tam ? a->tam : b->tam;
    int c = memcmp(a->texto, b->texto, m);
    if (c != 0) return c;
    return a->tam < b->tam ? -1 : a->tam > b->tam;
}

static bool ParecidoAntes(const ParecidoNome *a, const ParecidoNome *b)
{
    if (a->distancia != b->distancia) return a->distancia < b->distancia;
    return ComparaNomes(a, b) < 0;
}

static void ParecidoInserir(Parecidos *p, ParecidoNome novo)
{
    uint32_t i = p->qtd++;
    while (i > 0 && ParecidoAntes(&novo, &p->nomes[i - 1])) {
        p->nomes[i] = p->nomes[i - 1];
        i--;
    }
    p->nomes[i] = novo;
}

void ParecidosIniciar(Parecidos *p, const char *alvo, size_t tam)
{
    size_t tercos = tam / 3;
    p->alvo = alvo;
    p->tam_alvo = tam;
    p->limite = tercos < 1 ? 1 : (tercos > PARECIDO_DIST_MAX ? PARECIDO_DIST_MAX : (uint32_t)tercos);
    p->qtd = 0;
}

void ParecidosVer(Parecidos *p, const char *nome, size_t tam)
{
    ParecidosVerEm(p, nome, tam, PARECIDO_SEM_POS);
}

void ParecidosVerEm(Parecidos *p, const char *nome, size_t tam, uint32_t pos)
{
    ParecidoNome novo = {nome, tam, Parecido(p->alvo, p->tam_alvo, nome, tam), pos};
    if (novo.distancia == 0 || novo.distancia > p->limite) return;
    if (p->qtd == PARECIDOS_MAX) {
        if (!ParecidoAntes(&novo, &p->nomes[PARECIDOS_MAX - 1])) return;
        p->qtd--;
    }
    ParecidoInserir(p, novo);
}
