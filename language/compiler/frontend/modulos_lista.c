/* modulos_lista.c - os modulos disponiveis (std embutida + vendor\ da pasta do arquivo raiz) e o modulo parecido de um import que nao existe (E1001, M6b P79).
 * A lista sai em ordem alfabetica, agrupada (std: ..., vendor: ...); o que nao cabe na causa do diagnostico (512 bytes) vira " (+N)".
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M6b P79: ModulosListarDisponiveis e ModulosParecido (sinonimos PT/EN e Levenshtein <= 2 no mesmo grupo std:/vendor:).
 * Verificacao: python compiler\construir_rapido.py && python tests\cli\verificar_cli.py --forja compiler\forja.exe
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "frontend/modulos.h"

enum { NOMES_MAX = 192, NOME_TAM = 96, VENDOR_PROFUNDIDADE = 4, DISTANCIA_MAX = 2, RESERVA_FALTA = 16 };

/* Nomes completos ("std:mem", "vendor:json/cjson"); fora = os que nao couberam no vetor. */
typedef struct {
    char nomes[NOMES_MAX][NOME_TAM];
    size_t n, fora;
} Lista;

/* Sinonimos do que a IA escreve em PT para o modulo da std (confirmados contra a tabela STD). */
typedef struct { const char *pt, *en; } Sinonimo;
static const Sinonimo SINONIMOS[] = {
    {"texto", "text"}, {"ficheiro", "file"}, {"arquivo", "file"}, {"memoria", "mem"}, {"rede", "net"}, {"tempo", "time"}, {"caminho", "path"},
};

static void Acrescentar(Lista *l, const char *nome)
{
    if (l->n >= NOMES_MAX) {
        l->fora++;
        return;
    }
    snprintf(l->nomes[l->n++], NOME_TAM, "%s", nome);
}

static void ColetarStd(Lista *l)
{
    const char *nomes[NOMES_MAX];
    size_t n = ModulosStdNomes(nomes, NOMES_MAX);
    char nome[NOME_TAM];

    for (size_t k = 0; k < n; k++) {
        snprintf(nome, sizeof nome, "std:%s", nomes[k]);
        Acrescentar(l, nome);
    }
}

#ifdef _WIN32
static bool EhFj(const char *nome)
{
    size_t n = strlen(nome);
    return n > 3 && strcmp(nome + n - 3, ".fj") == 0;
}

/* Desce a pasta 'dir' (rel = o caminho do import ate aqui, com '/' no fim) e acrescenta cada .fj como vendor:<rel><nome sem .fj>. */
static void ColetarPasta(const char *dir, const char *rel, int profundidade, Lista *l)
{
    char padrao[400], sub_dir[400], sub_rel[NOME_TAM], nome[NOME_TAM];
    WIN32_FIND_DATAA f;
    HANDLE h;

    if (profundidade > VENDOR_PROFUNDIDADE) return;
    snprintf(padrao, sizeof padrao, "%s\\*", dir);
    h = FindFirstFileA(padrao, &f);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (f.cFileName[0] == '.') continue;
        if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            snprintf(sub_dir, sizeof sub_dir, "%s\\%s", dir, f.cFileName);
            snprintf(sub_rel, sizeof sub_rel, "%s%s/", rel, f.cFileName);
            ColetarPasta(sub_dir, sub_rel, profundidade + 1, l);
        } else if (EhFj(f.cFileName)) {
            snprintf(nome, sizeof nome, "vendor:%s%.*s", rel, (int)(strlen(f.cFileName) - 3), f.cFileName);
            Acrescentar(l, nome);
        }
    } while (FindNextFileA(h, &f));
    FindClose(h);
}
#endif

/* vendor\ ao lado do arquivo raiz (pasta = Modulos.pasta). Sem Windows, so a std. */
static void ColetarVendor(const char *pasta, Lista *l)
{
#ifdef _WIN32
    char dir[400];

    snprintf(dir, sizeof dir, "%s\\vendor", pasta);
    ColetarPasta(dir, "", 0, l);
#else
    (void)pasta;
    (void)l;
#endif
}

static void ColetarNomes(const char *pasta, Lista *l)
{
    l->n = 0;
    l->fora = 0;
    ColetarStd(l);
    ColetarVendor(pasta, l);
}

static int CompararNomes(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/* Anexa 'tam' bytes em destino se cabem com a folga da marca " (+N)"; false = nao coube. */
static bool Anexar(char *destino, size_t cap, size_t *n, const char *texto, size_t tam)
{
    if (*n + tam + RESERVA_FALTA >= cap) return false;
    memcpy(destino + *n, texto, tam);
    *n += tam;
    destino[*n] = '\0';
    return true;
}

/* Um grupo ("std: " ou "vendor: ") por prefixo; itens separados por ", " e grupos por "; ". */
static size_t Juntar(const Lista *l, char *destino, size_t cap)
{
    size_t n = 0, k = 0, cab = 0;
    const char *grupo = NULL;
    char marca[RESERVA_FALTA];

    destino[0] = '\0';
    for (k = 0; k < l->n; k++) {
        const char *nome = l->nomes[k];
        size_t cab_nome = (size_t)(strchr(nome, ':') - nome) + 1;
        bool grupo_novo = grupo == NULL || cab_nome != cab || strncmp(grupo, nome, cab_nome) != 0;

        if (grupo_novo) {
            if (!Anexar(destino, cap, &n, grupo == NULL ? "" : "; ", grupo == NULL ? 0 : 2)) break;
            if (!Anexar(destino, cap, &n, nome, cab_nome) || !Anexar(destino, cap, &n, " ", 1)) break;
            grupo = nome;
            cab = cab_nome;
        } else if (!Anexar(destino, cap, &n, ", ", 2)) {
            break;
        }
        if (!Anexar(destino, cap, &n, nome + cab_nome, strlen(nome + cab_nome))) break;
    }
    if (k < l->n || l->fora > 0) {
        snprintf(marca, sizeof marca, " (+%zu)", (l->n - k) + l->fora);
        Anexar(destino, cap, &n, marca, strlen(marca));
    }
    return n;
}

size_t ModulosListarDisponiveis(const char *pasta, char *destino, size_t cap)
{
    Lista l;

    ColetarNomes(pasta, &l);
    qsort(l.nomes, l.n, NOME_TAM, CompararNomes);
    return Juntar(&l, destino, cap);
}

/* Distancia de Levenshtein entre a e b; acima de DISTANCIA_MAX devolve DISTANCIA_MAX + 1 (com corte pelo tamanho). */
static size_t Distancia(const char *a, const char *b)
{
    size_t na = strlen(a), nb = strlen(b), ant[NOME_TAM], atu[NOME_TAM];

    if (na >= NOME_TAM || nb >= NOME_TAM) return DISTANCIA_MAX + 1;
    if (na > nb + DISTANCIA_MAX || nb > na + DISTANCIA_MAX) return DISTANCIA_MAX + 1;
    for (size_t j = 0; j <= nb; j++) ant[j] = j;
    for (size_t i = 1; i <= na; i++) {
        atu[0] = i;
        for (size_t j = 1; j <= nb; j++) {
            size_t custo = a[i - 1] == b[j - 1] ? 0 : 1, m = ant[j] + 1;

            if (atu[j - 1] + 1 < m) m = atu[j - 1] + 1;
            if (ant[j - 1] + custo < m) m = ant[j - 1] + custo;
            atu[j] = m;
        }
        memcpy(ant, atu, (nb + 1) * sizeof ant[0]);
    }
    return ant[nb] > DISTANCIA_MAX ? DISTANCIA_MAX + 1 : ant[nb];
}

static bool Existe(const Lista *l, const char *nome)
{
    for (size_t k = 0; k < l->n; k++) {
        if (strcmp(l->nomes[k], nome) == 0) return true;
    }
    return false;
}

/* Sinonimo PT -> EN do sufixo 'sufixo' no grupo 'cab' ("std:"); true se o EN existe na lista. */
static bool SinonimoExistente(const Lista *l, const char *import, size_t cab, char *destino, size_t cap)
{
    for (size_t k = 0; k < sizeof SINONIMOS / sizeof SINONIMOS[0]; k++) {
        if (strcmp(import + cab, SINONIMOS[k].pt) != 0) continue;
        snprintf(destino, cap, "%.*s%s", (int)cab, import, SINONIMOS[k].en);
        return Existe(l, destino);
    }
    return false;
}

/* Modulo com nome proximo de 'import' (mesmo grupo std:/vendor:): sinonimo primeiro, depois o de menor distancia (empate = o 1o). */
static bool MelhorParecido(const Lista *l, const char *import, size_t cab, char *destino, size_t cap)
{
    size_t melhor = DISTANCIA_MAX + 1, idx = 0;

    for (size_t k = 0; k < l->n; k++) {
        size_t d;

        if (strncmp(l->nomes[k], import, cab) != 0) continue;
        d = Distancia(l->nomes[k] + cab, import + cab);
        if (d < melhor) {
            melhor = d;
            idx = k;
        }
    }
    if (melhor > DISTANCIA_MAX) return false;
    snprintf(destino, cap, "%s", l->nomes[idx]);
    return true;
}

bool ModulosParecido(const char *pasta, const char *import, char *destino, size_t cap)
{
    Lista l;
    const char *dois = strchr(import, ':');
    size_t cab;

    if (dois == NULL) return false;
    cab = (size_t)(dois - import) + 1;
    ColetarNomes(pasta, &l);
    if (SinonimoExistente(&l, import, cab, destino, cap)) return true;
    return MelhorParecido(&l, import, cab, destino, cap);
}
