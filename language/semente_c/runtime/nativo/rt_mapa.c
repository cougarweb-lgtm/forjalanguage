/*
 * rt_mapa.c - map do runtime nativo da Forja: forja_mapa de 56 B (M2 P17a).
 *
 * Versao: 0.1.4
 * Changelog:
 *   0.1.4 - M1: o always_inline sai para RT_SEMPRE_INLINE (rt_compat.h, unica extensao do gcc do runtime); LIMITE de parametros nas entradas de 5 (inserir e remover).
 *   0.1.3 - M1: entradas por valor forja_mapa_{achar,inserir,remover}_{k4,k8} para chave inteira de 4 ou 8 bytes (i32, u32, int, uint, enum): a chave vai no registrador
 *           e o corpo (rt_mapa_*_em) inlina com o tamanho constante; o caminho generico chama o mesmo corpo. Hash, sondagem, crescimento e swap-remove iguais.
 *   0.1.2 - chave nao-texto de 1, 2, 4 ou 8 bytes: carga (hash e igual) e copia (inserir e trocar_ultimo) por memcpy de tamanho constante em switch, em vez de memcpy/memcmp de tamanho variavel (-fno-builtin deixa cada um como chamada a libc); hash, sondagem, crescimento e layout iguais.
 *   0.1.1 - M3 F9a: rt_mapa_fechar_buraco abaixo de 6 decisoes (a condicao ciclica saiu para rt_mapa_casa_entre); comportamento igual.
 *   0.1.0 - primeira versao: portado de forja_rt.h:1682-1912 com o mesmo hash (FNV-1a para chave str, mistura de 64 bits para as demais), a mesma
 *           sondagem linear, o mesmo crescimento (cap 0..3 vira 8, depois dobra), o mesmo Algoritmo R de Knuth na remocao e o mesmo swap-remove
 *           (o ultimo par denso ocupa o lugar do removido): a ordem de iteracao e observavel e bate bit a bit com o forja_rt.h.
 *           As funcoes internas recebem um RtMapaCtx {tam_chave, tam_valor, texto, arquivo, linha} para ficar em 4 parametros; as publicas mantem as
 *           assinaturas do forja_rt.h. Dev confere a epoca do frame (R0008); release nao (rt_mapa_viva vazia).
 *
 * Bloco unico: [chaves: cap*tk][alinha 16][valores: cap*tv][alinha 16][slots: nslots*4]; slot 0 = vazio, senao indice denso + 1.
 * Vem DEPOIS de rt_aloc.c na unidade (usa rt_frame_epoca, rt_aloc_lista e RT_ALINHA, que sao dele); nao compila sozinho.
 */
#include "forja_rt_nativo.h"
#include "rt_compat.h"

#ifndef FORJA_ALOCS_MAX
#error "rt_mapa.c precisa de rt_aloc.c incluido antes"
#endif

#define RT_MAPA_SLOTS_MIN 16
#define RT_MAPA_CAP_MAX 0x40000000LL /* garante nslots e indices em uint32_t */
/* Helpers e corpos que as entradas k4/k8 inlinam com o tamanho da chave constante (sem despacho por tamanho no laco). */
#define RT_MAPA_EMBUTIDO static inline RT_SEMPRE_INLINE

typedef struct {
    int64_t tam_chave;
    int64_t tam_valor;
    bool texto;
    const char *arquivo;
    int linha;
} RtMapaCtx;

RT_MAPA_EMBUTIDO void rt_mapa_viva(const forja_mapa *m, const RtMapaCtx *c)
{
#ifdef FORJA_DEV
    forja_lista_viva(m->epoca, c->arquivo, c->linha);
#else
    (void)m;
    (void)c;
#endif
}

RT_MAPA_EMBUTIDO uint64_t rt_mapa_mistura(uint64_t k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

/* Tamanhos de chave nao-texto com caminho tipado: o gcc expande o memcpy de tamanho constante em carga direta. */
RT_MAPA_EMBUTIDO bool rt_mapa_chave_tipada(int64_t tam)
{
    return tam == 1 || tam == 2 || tam == 4 || tam == 8;
}

/* Carga da chave de 1, 2, 4 ou 8 bytes num uint64_t zerado: a mesma conta do memcpy(&u, chave, tam) do caso geral, so que sem chamada.
 * Pre: rt_mapa_chave_tipada(tam). Duas chaves sao iguais se e so se as cargas sao iguais (a carga e injetiva nos bytes). */
RT_MAPA_EMBUTIDO uint64_t rt_mapa_carga(const void *chave, int64_t tam)
{
    uint64_t u = 0;

    switch (tam) {
    case 1:
        __builtin_memcpy(&u, chave, 1);
        break;
    case 2:
        __builtin_memcpy(&u, chave, 2);
        break;
    case 4:
        __builtin_memcpy(&u, chave, 4);
        break;
    case 8:
        __builtin_memcpy(&u, chave, 8);
        break;
    default:
        break; /* nao chega aqui: rt_mapa_chave_tipada filtra antes */
    }
    return u;
}

/* Copia de uma chave: tamanhos 1, 2, 4 e 8 com memcpy constante; os demais (chave texto, struct) seguem o memcpy geral. */
RT_MAPA_EMBUTIDO void rt_mapa_copiar_chave(void *dst, const void *src, int64_t tam)
{
    switch (tam) {
    case 1:
        __builtin_memcpy(dst, src, 1);
        return;
    case 2:
        __builtin_memcpy(dst, src, 2);
        return;
    case 4:
        __builtin_memcpy(dst, src, 4);
        return;
    case 8:
        __builtin_memcpy(dst, src, 8);
        return;
    default:
        memcpy(dst, src, (size_t)tam);
        return;
    }
}

RT_MAPA_EMBUTIDO uint64_t rt_mapa_hash(const void *chave, const RtMapaCtx *c)
{
    uint64_t h;
    uint64_t u = 0;
    int64_t i;
    const forja_str *s = (const forja_str *)chave;

    if (!c->texto) {
        if (rt_mapa_chave_tipada(c->tam_chave))
            return rt_mapa_mistura(rt_mapa_carga(chave, c->tam_chave));
        memcpy(&u, chave, (size_t)c->tam_chave);
        return rt_mapa_mistura(u);
    }
    h = 0xcbf29ce484222325ULL;
    for (i = 0; i < s->len; i++) {
        h ^= (unsigned char)s->ptr[i];
        h *= 0x100000001b3ULL;
    }
    return rt_mapa_mistura(h);
}

RT_MAPA_EMBUTIDO bool rt_mapa_igual(const void *a, const void *b, const RtMapaCtx *c)
{
    const forja_str *x = (const forja_str *)a;
    const forja_str *y = (const forja_str *)b;

    if (!c->texto) {
        if (rt_mapa_chave_tipada(c->tam_chave))
            return rt_mapa_carga(a, c->tam_chave) == rt_mapa_carga(b, c->tam_chave);
        return memcmp(a, b, (size_t)c->tam_chave) == 0;
    }
    return x->len == y->len && (x->len == 0 || memcmp(x->ptr, y->ptr, (size_t)x->len) == 0);
}

RT_MAPA_EMBUTIDO const unsigned char *rt_mapa_chave(const forja_mapa *m, int64_t i, const RtMapaCtx *c)
{
    return (const unsigned char *)m->chaves + i * c->tam_chave;
}

/* Slot da chave: o do indice denso dela (*indice >= 0) ou o primeiro vazio da sondagem (*indice = -1). Pre: nslots > 0. */
RT_MAPA_EMBUTIDO uint32_t rt_mapa_sondar(const forja_mapa *m, const void *chave, const RtMapaCtx *c, int64_t *indice)
{
    uint32_t mask = m->nslots - 1u;
    uint32_t s = (uint32_t)rt_mapa_hash(chave, c) & mask;

    for (;;) {
        uint32_t v = m->slots[s];

        if (v == 0) {
            *indice = -1;
            return s;
        }
        if (rt_mapa_igual(rt_mapa_chave(m, (int64_t)(v - 1u), c), chave, c)) {
            *indice = (int64_t)v - 1;
            return s;
        }
        s = (s + 1u) & mask;
    }
}

/* Slot "de casa" da entrada v (indice denso + 1). */
RT_MAPA_EMBUTIDO uint32_t rt_mapa_casa(const forja_mapa *m, uint32_t v, const RtMapaCtx *c)
{
    return (uint32_t)rt_mapa_hash(rt_mapa_chave(m, (int64_t)(v - 1u), c), c) & (m->nslots - 1u);
}

static void rt_mapa_refazer(forja_mapa *m, const RtMapaCtx *c)
{
    int64_t i;
    int64_t indice;

    memset(m->slots, 0, (size_t)m->nslots * 4u);
    for (i = 0; i < m->len; i++) {
        uint32_t s = rt_mapa_sondar(m, rt_mapa_chave(m, i, c), c, &indice);

        m->slots[s] = (uint32_t)i + 1u;
    }
}

static void *rt_mapa_bloco(forja_mapa *m, int64_t bytes, const RtMapaCtx *c)
{
    void *b;

    if (m->alocador == FORJA_ALOC_HEAP)
        return forja_mem_alocar(FORJA_ALOC_HEAP, bytes, c->arquivo, c->linha);
    if (m->alocador == FORJA_ALOC_FRAME) {
        b = forja_frame_alocar(bytes, c->arquivo, c->linha);
        m->epoca = rt_frame_epoca();
        return b;
    }
    return rt_aloc_lista(m->alocador, bytes, c->arquivo, c->linha);
}

static void rt_mapa_conferir_cap(const forja_mapa *m, int64_t nc, const RtMapaCtx *c)
{
    int64_t maior = c->tam_chave > c->tam_valor ? c->tam_chave : c->tam_valor;

    if (nc > INT64_MAX / 4 / maior || nc > RT_MAPA_CAP_MAX)
        forja_falha_alocacao("map passou do limite de memoria com cap", m->cap, c->arquivo, c->linha);
}

static void rt_mapa_copiar(const forja_mapa *m, unsigned char *b, int64_t off_valores, const RtMapaCtx *c)
{
    if (m->len <= 0)
        return;
    memcpy(b, m->chaves, (size_t)(m->len * c->tam_chave));
    memcpy(b + off_valores, m->valores, (size_t)(m->len * c->tam_valor));
}

/* Aloca um bloco de nc entradas, copia as len atuais, solta o bloco antigo (heap) e refaz os slots. */
static void rt_mapa_montar(forja_mapa *m, int64_t nc, const RtMapaCtx *c)
{
    int64_t ns = RT_MAPA_SLOTS_MIN;
    int64_t off_valores;
    int64_t off_slots;
    unsigned char *b;

    rt_mapa_conferir_cap(m, nc, c);
    while (ns < 2 * nc)
        ns *= 2;
    off_valores = RT_ALINHA(nc * c->tam_chave);
    off_slots = off_valores + RT_ALINHA(nc * c->tam_valor);
    b = (unsigned char *)rt_mapa_bloco(m, off_slots + ns * 4, c);
    rt_mapa_copiar(m, b, off_valores, c);
    if (m->alocador == FORJA_ALOC_HEAP)
        forja_mem_liberar(m->chaves, c->arquivo, c->linha);
    m->chaves = b;
    m->valores = b + off_valores;
    m->slots = (uint32_t *)(b + off_slots);
    m->cap = nc;
    m->nslots = (uint32_t)ns;
    rt_mapa_refazer(m, c);
}

static void rt_mapa_crescer(forja_mapa *m, const RtMapaCtx *c)
{
    if (m->alocador == 0)
        forja_falha_alocacao("insert num map sem alocador (valor zero): crie com make(map[K]V, em = heap)", 0, c->arquivo, c->linha);
    rt_mapa_montar(m, m->cap < 4 ? 8 : m->cap * 2, c);
}

void forja_mapa_criar(forja_mapa *m, uint32_t alocador, int64_t cap, int64_t tam_chave, int64_t tam_valor, const char *arquivo, int linha)
{
    RtMapaCtx c = {tam_chave, tam_valor, false, arquivo, linha};

    m->alocador = alocador;
    if (cap < 0)
        forja_falha_alocacao("make com cap negativo", cap, arquivo, linha);
    if (cap == 0)
        return;
    m->cap = 0;
    rt_mapa_montar(m, cap, &c);
}

/* Corpo do achar, comum ao caminho generico e as entradas k4/k8: indice denso da chave ou -1. Pre: a chave tem c->tam_chave bytes. */
RT_MAPA_EMBUTIDO int64_t rt_mapa_achar_em(const forja_mapa *m, const void *chave, const RtMapaCtx *c)
{
    int64_t indice;

    rt_mapa_viva(m, c);
    if (m->len == 0)
        return -1;
    rt_mapa_sondar(m, chave, c, &indice);
    return indice;
}

/* Corpo do inserir, comum aos dois caminhos: a nova entra no fim do denso; o crescimento e o mesmo de antes (so quando len == cap). */
RT_MAPA_EMBUTIDO int64_t rt_mapa_inserir_em(forja_mapa *m, const void *chave, const RtMapaCtx *c)
{
    int64_t i;
    uint32_t s;

    rt_mapa_viva(m, c);
    if (m->len == m->cap) {
        i = rt_mapa_achar_em(m, chave, c);
        if (i >= 0)
            return i;
        rt_mapa_crescer(m, c);
    }
    s = rt_mapa_sondar(m, chave, c, &i);
    if (i >= 0)
        return i;
    i = m->len;
    m->len += 1;
    rt_mapa_copiar_chave((unsigned char *)m->chaves + i * c->tam_chave, chave, c->tam_chave);
    m->slots[s] = (uint32_t)i + 1u;
    return i;
}

int64_t forja_mapa_achar(const forja_mapa *m, const void *chave, int64_t tam_chave, bool texto, const char *arquivo, int linha)
{
    RtMapaCtx c = {tam_chave, 0, texto, arquivo, linha};

    return rt_mapa_achar_em(m, chave, &c);
}

int64_t forja_mapa_inserir(forja_mapa *m, const void *chave, int64_t tam_chave, bool texto, int64_t tam_valor, const char *arquivo, int linha)
{
    RtMapaCtx c = {tam_chave, tam_valor, texto, arquivo, linha};

    return rt_mapa_inserir_em(m, chave, &c);
}

/* A casa k fica ciclicamente em (i, j]: a entrada em j nao pode ir para o buraco i. */
RT_MAPA_EMBUTIDO bool rt_mapa_casa_entre(uint32_t i, uint32_t j, uint32_t k)
{
    return i <= j ? (i < k && k <= j) : (i < k || k <= j);
}

/* Algoritmo R de Knuth: fecha o buraco do slot s deslocando para tras as entradas que nao ficam antes da casa delas. */
RT_MAPA_EMBUTIDO void rt_mapa_fechar_buraco(forja_mapa *m, uint32_t s, const RtMapaCtx *c)
{
    uint32_t mask = m->nslots - 1u;
    uint32_t i = s;
    uint32_t j = s;

    for (;;) {
        uint32_t v;
        uint32_t k;

        j = (j + 1u) & mask;
        v = m->slots[j];
        if (v == 0)
            break;
        k = rt_mapa_casa(m, v, c);
        if (rt_mapa_casa_entre(i, j, k))
            continue;
        m->slots[i] = v;
        i = j;
    }
    m->slots[i] = 0;
}

/* swap-remove: a ultima entrada densa ocupa o lugar da removida (x). */
RT_MAPA_EMBUTIDO void rt_mapa_trocar_ultimo(forja_mapa *m, int64_t x, const RtMapaCtx *c)
{
    int64_t ult = m->len - 1;
    int64_t indice_ult;
    uint32_t slot_ult;

    if (x == ult)
        return;
    slot_ult = rt_mapa_sondar(m, rt_mapa_chave(m, ult, c), c, &indice_ult);
    rt_mapa_copiar_chave((unsigned char *)m->chaves + x * c->tam_chave, (unsigned char *)m->chaves + ult * c->tam_chave, c->tam_chave);
    memcpy((unsigned char *)m->valores + x * c->tam_valor, (unsigned char *)m->valores + ult * c->tam_valor, (size_t)c->tam_valor);
    m->slots[slot_ult] = (uint32_t)x + 1u;
}

/* Corpo do remover, comum aos dois caminhos: chave ausente nao faz nada; o swap-remove e o do caminho generico. */
RT_MAPA_EMBUTIDO void rt_mapa_remover_em(forja_mapa *m, const void *chave, const RtMapaCtx *c)
{
    int64_t x;
    uint32_t s;

    rt_mapa_viva(m, c);
    if (m->len == 0)
        return;
    s = rt_mapa_sondar(m, chave, c, &x);
    if (x < 0)
        return;
    rt_mapa_fechar_buraco(m, s, c);
    rt_mapa_trocar_ultimo(m, x, c);
    m->len -= 1;
}

void forja_mapa_remover(forja_mapa *m, const void *chave, int64_t tam_chave, bool texto, int64_t tam_valor, const char *arquivo, int linha)
{
    RtMapaCtx c = {tam_chave, tam_valor, texto, arquivo, linha};

    rt_mapa_remover_em(m, chave, &c);
}

/* Entradas por valor (M1): chave inteira de 4 ou 8 bytes (i32, u32, int, uint, enum) chega no registrador, zero-estendida como a carga
 * do caminho generico (x64, little-endian). O tamanho e constante em cada entrada, entao o corpo acima inlina sem despacho. So o BIR as chama. */
int64_t forja_mapa_achar_k4(const forja_mapa *m, uint32_t chave, const char *arquivo, int linha)
{
    uint64_t k = chave;
    RtMapaCtx c = {4, 0, false, arquivo, linha};

    return rt_mapa_achar_em(m, &k, &c);
}

int64_t forja_mapa_achar_k8(const forja_mapa *m, uint64_t chave, const char *arquivo, int linha)
{
    RtMapaCtx c = {8, 0, false, arquivo, linha};

    return rt_mapa_achar_em(m, &chave, &c);
}

/* LIMITE: parametros 5 (o map, a chave, o tamanho do valor e o ponto no fonte; agrupar criaria tipo de uso unico na ponte do BIR). */
int64_t forja_mapa_inserir_k4(forja_mapa *m, uint32_t chave, int64_t tam_valor, const char *arquivo, int linha)
{
    uint64_t k = chave;
    RtMapaCtx c = {4, tam_valor, false, arquivo, linha};

    return rt_mapa_inserir_em(m, &k, &c);
}

/* LIMITE: parametros 5 (o map, a chave, o tamanho do valor e o ponto no fonte; agrupar criaria tipo de uso unico na ponte do BIR). */
int64_t forja_mapa_inserir_k8(forja_mapa *m, uint64_t chave, int64_t tam_valor, const char *arquivo, int linha)
{
    RtMapaCtx c = {8, tam_valor, false, arquivo, linha};

    return rt_mapa_inserir_em(m, &chave, &c);
}

/* LIMITE: parametros 5 (o map, a chave, o tamanho do valor e o ponto no fonte; agrupar criaria tipo de uso unico na ponte do BIR). */
void forja_mapa_remover_k4(forja_mapa *m, uint32_t chave, int64_t tam_valor, const char *arquivo, int linha)
{
    uint64_t k = chave;
    RtMapaCtx c = {4, tam_valor, false, arquivo, linha};

    rt_mapa_remover_em(m, &k, &c);
}

/* LIMITE: parametros 5 (o map, a chave, o tamanho do valor e o ponto no fonte; agrupar criaria tipo de uso unico na ponte do BIR). */
void forja_mapa_remover_k8(forja_mapa *m, uint64_t chave, int64_t tam_valor, const char *arquivo, int linha)
{
    RtMapaCtx c = {8, tam_valor, false, arquivo, linha};

    rt_mapa_remover_em(m, &chave, &c);
}

void forja_mapa_liberar(forja_mapa *m, const char *arquivo, int linha)
{
    uint32_t a = m->alocador;

    if (a == FORJA_ALOC_HEAP && m->chaves != NULL)
        forja_mem_liberar(m->chaves, arquivo, linha);
    *m = (forja_mapa){0};
    m->alocador = a;
}
