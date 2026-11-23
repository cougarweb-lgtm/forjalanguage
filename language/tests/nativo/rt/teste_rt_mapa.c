/*
 * teste_rt_mapa.c - casos 40 a 44 do runtime nativo da Forja: map e soa dinamica (M2 P17a; os casos 26 a 30 do contrato). Escolha o caso com -DCASO=40..44.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - primeira versao: 40 map int -> int (10 mil insercoes, achar, remover, ordem densa e slots depois da remocao; chaves de 1 e 4 bytes), 41 map
 *           str -> struct (chave por conteudo, vazia, prefixos; dev), 42 soa dinamica (criar, crescer 5 vezes, colunas, frame, Arena, limites), 43 R0008
 *           de soa do frame depois do reset (dev), 44 R0008 de map do frame depois do reset (dev). Os casos 41, 43 e 44 pedem -DFORJA_DEV (oraculo e rt).
 *
 * -DORACULO: usa o forja_rt.h e o CRT do gcc. Sem -DORACULO: liga com o rt_win.o (que inclui rt_mapa.c e rt_soa.c; entrada forja_entrada) e as declaracoes de forja_rt_nativo.h.
 * A ordem densa, os slots (assinatura), cap e nslots saem impressos: o hash, a sondagem e a remocao tem de bater bit a bit com o forja_rt.h.
 * Nenhum endereco e impresso (so diferencas): oraculo e nativo tem enderecos diferentes.
 * Uso: ver verificar_rt_nativo.py
 */
#ifndef CASO
#error "compile com -DCASO=40..44"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef ORACULO
#include "../../../runtime/forja_rt.h"
#else
#include "../../../runtime/nativo/forja_rt_nativo.h"
#endif

#define ARQUIVO_TESTE "teste_mapa.fj"

static inline uint64_t Mistura(uint64_t acc, uint64_t v)
{
    return acc * 1000003ull + v + 1ull;
}

/* Assinatura de todos os slots: pega qualquer diferenca de posicao na tabela. */
static inline int64_t AssinaturaSlots(const forja_mapa *m)
{
    uint64_t acc = 17;
    uint32_t i;

    for (i = 0; i < m->nslots; i++)
        acc = Mistura(acc, m->slots[i]);
    return (int64_t)(acc >> 1);
}

static inline void ImprimirForma(const forja_mapa *m)
{
    forja_print_i64(m->len);
    forja_print_i64(m->cap);
    forja_print_i64((int64_t)m->nslots);
    forja_print_i64(AssinaturaSlots(m));
}

#if CASO == 40
static inline int64_t *Valores(const forja_mapa *m)
{
    return (int64_t *)m->valores;
}

static inline int64_t *Chaves(const forja_mapa *m)
{
    return (int64_t *)m->chaves;
}

static inline int64_t Inserir(forja_mapa *m, int64_t k, int64_t v)
{
    int64_t i = forja_mapa_inserir(m, &k, 8, false, 8, ARQUIVO_TESTE, 11);

    Valores(m)[i] = v;
    return i;
}

static inline int64_t Achar(const forja_mapa *m, int64_t k)
{
    return forja_mapa_achar(m, &k, 8, false, ARQUIVO_TESTE, 12);
}

static inline void Remover(forja_mapa *m, int64_t k)
{
    forja_mapa_remover(m, &k, 8, false, 8, ARQUIVO_TESTE, 13);
}

static inline int64_t AssinaturaDensa(const forja_mapa *m)
{
    uint64_t acc = 5;
    int64_t i;

    for (i = 0; i < m->len; i++)
        acc = Mistura(Mistura(acc, (uint64_t)Chaves(m)[i]), (uint64_t)Valores(m)[i]);
    return (int64_t)(acc >> 1);
}

static inline void ImprimirPontas(const forja_mapa *m)
{
    int64_t i;

    for (i = 0; i < 5; i++)
        forja_print_i64(Chaves(m)[i]);
    for (i = m->len - 5; i < m->len; i++)
        forja_print_i64(Chaves(m)[i]);
}

/* Chaves de 1 byte (u8) e de 4 bytes (negativas): o hash copia tam_chave bytes. */
static inline void ChavesCurtas(void)
{
    forja_mapa b = {0};
    forja_mapa q = {0};
    uint8_t kb;
    int32_t kq;
    int64_t i;
    int64_t indice;
    int64_t soma = 0;
    uint64_t acc = 3;

    forja_mapa_criar(&b, FORJA_ALOC_HEAP, 4, 1, 4, ARQUIVO_TESTE, 20);
    for (i = 0; i < 300; i++) {
        kb = (uint8_t)(i * 37);
        indice = forja_mapa_inserir(&b, &kb, 1, false, 4, ARQUIVO_TESTE, 21);
        ((int32_t *)b.valores)[indice] = (int32_t)i;
    }
    forja_print_i64(b.len);
    for (i = 0; i < b.len; i++)
        acc = Mistura(Mistura(acc, ((uint8_t *)b.chaves)[i]), (uint64_t)((int32_t *)b.valores)[i]);
    forja_print_i64((int64_t)(acc >> 1));
    ImprimirForma(&b);
    forja_mapa_criar(&q, FORJA_ALOC_HEAP, 0, 4, 8, ARQUIVO_TESTE, 22);
    for (i = 1; i <= 50; i++) {
        kq = (int32_t)(-i);
        indice = forja_mapa_inserir(&q, &kq, 4, false, 8, ARQUIVO_TESTE, 23);
        ((int64_t *)q.valores)[indice] = i * i;
    }
    for (i = 1; i <= 50; i += 4) {
        kq = (int32_t)(-i);
        forja_mapa_remover(&q, &kq, 4, false, 8, ARQUIVO_TESTE, 24);
    }
    for (i = 0; i < q.len; i++)
        soma = soma * 31 + ((int32_t *)q.chaves)[i];
    forja_print_i64(soma);
    ImprimirForma(&q);
    forja_mapa_liberar(&b, ARQUIVO_TESTE, 25);
    forja_mapa_liberar(&q, ARQUIVO_TESTE, 26);
}

/* map int -> int: 10 mil insercoes (cresce de 8 a 16384), achar, remover um terco, ordem densa (swap-remove) e slots (Knuth R), esvaziar e reencher. */
int forja_programa(void)
{
    forja_mapa m = {0};
    int64_t n;
    int64_t achados = 0;
    int64_t erros = 0;

    forja_mapa_criar(&m, FORJA_ALOC_HEAP, 0, 8, 8, ARQUIVO_TESTE, 10);
    for (n = 0; n < 10000; n++)
        Inserir(&m, n * 7919 + 3, n * 2);
    ImprimirForma(&m);
    forja_print_i64(Inserir(&m, 3, 77));
    forja_print_i64(Valores(&m)[0]);
    forja_print_i64(m.len);
    for (n = 0; n < 10000; n++) {
        int64_t i = Achar(&m, n * 7919 + 3);
        if (i >= 0 && Valores(&m)[i] == (n == 0 ? 77 : n * 2))
            achados++;
    }
    forja_print_i64(achados);
    forja_print_i64(Achar(&m, -5));
    forja_print_i64(AssinaturaDensa(&m));
    for (n = 0; n < 10000; n += 3)
        Remover(&m, n * 7919 + 3);
    Remover(&m, -5);
    ImprimirForma(&m);
    for (n = 0; n < 10000; n++) {
        int64_t i = Achar(&m, n * 7919 + 3);
        if ((n % 3 == 0) != (i < 0))
            erros++;
    }
    forja_print_i64(erros);
    forja_print_i64(AssinaturaDensa(&m));
    ImprimirPontas(&m);
    for (n = 1; n < 10000; n++)
        Remover(&m, n * 7919 + 3);
    ImprimirForma(&m);
    forja_print_i64(Achar(&m, 3));
    for (n = 0; n < 20; n++)
        Inserir(&m, n - 10, n);
    ImprimirForma(&m);
    ImprimirPontas(&m);
    forja_mapa_liberar(&m, ARQUIVO_TESTE, 14);
    forja_print_i64(m.cap);
    forja_print_i64(forja_rt_vivos_qtd());
    ChavesCurtas();
    return 0;
}
#elif CASO == 41
typedef struct {
    int64_t id;
    int32_t qtd;
    int32_t folga;
} Item;

static char Nomes[320][24];

static inline int64_t Tamanho(const char *s)
{
    int64_t n = 0;

    while (s[n] != '\0')
        n++;
    return n;
}

/* "item_<n>" em dst (24 bytes). */
static inline void Nome(char *dst, int64_t n)
{
    char dig[20];
    int d = 0;
    int j = 0;
    const char *pre = "item_";

    while (pre[j] != '\0') {
        dst[j] = pre[j];
        j++;
    }
    do {
        dig[d++] = (char)('0' + n % 10);
        n /= 10;
    } while (n > 0);
    while (d > 0)
        dst[j++] = dig[--d];
    dst[j] = '\0';
}

static inline forja_str Texto(const char *s)
{
    forja_str r;

    r.ptr = s;
    r.len = Tamanho(s);
    return r;
}

static inline Item *Itens(const forja_mapa *m)
{
    return (Item *)m->valores;
}

static inline int64_t Inserir(forja_mapa *m, forja_str k, int64_t id)
{
    int64_t i = forja_mapa_inserir(m, &k, (int64_t)sizeof(forja_str), true, (int64_t)sizeof(Item), ARQUIVO_TESTE, 31);

    Itens(m)[i].id = id;
    Itens(m)[i].qtd = (int32_t)(id * 3);
    Itens(m)[i].folga = 0;
    return i;
}

static inline int64_t Achar(const forja_mapa *m, forja_str k)
{
    return forja_mapa_achar(m, &k, (int64_t)sizeof(forja_str), true, ARQUIVO_TESTE, 32);
}

static inline void Remover(forja_mapa *m, forja_str k)
{
    forja_mapa_remover(m, &k, (int64_t)sizeof(forja_str), true, (int64_t)sizeof(Item), ARQUIVO_TESTE, 33);
}

static inline int64_t AssinaturaDensa(const forja_mapa *m)
{
    uint64_t acc = 9;
    int64_t i;
    int64_t j;

    for (i = 0; i < m->len; i++) {
        const forja_str *k = (const forja_str *)m->chaves + i;

        for (j = 0; j < k->len; j++)
            acc = Mistura(acc, (unsigned char)k->ptr[j]);
        acc = Mistura(Mistura(acc, (uint64_t)Itens(m)[i].id), (uint64_t)Itens(m)[i].qtd);
    }
    return (int64_t)(acc >> 1);
}

/* map str -> struct (dev): chave por conteudo (outro buffer, mesmo texto), chave vazia, prefixos, remover, ordem densa, vivos 0 no fim. */
int forja_programa(void)
{
    forja_mapa m = {0};
    char outro_buffer[24];
    int64_t n;
    int64_t achados = 0;
    int64_t erros = 0;

    /* volatile: com o tamanho constante o gcc do oraculo enxerga o ramo "nao texto" do hash (nunca executado com len 0) e barra com -Warray-bounds. */
    volatile int64_t tam_chave_str = (int64_t)sizeof(forja_str);

    forja_mapa_criar(&m, FORJA_ALOC_HEAP, 2, tam_chave_str, (int64_t)sizeof(Item), ARQUIVO_TESTE, 30);
    for (n = 0; n < 250; n++) {
        Nome(Nomes[n], n);
        Inserir(&m, Texto(Nomes[n]), n);
    }
    Inserir(&m, Texto(""), 1000);
    Inserir(&m, Texto("ab"), 1001);
    Inserir(&m, Texto("abc"), 1002);
    Inserir(&m, Texto("abd"), 1003);
    ImprimirForma(&m);
    for (n = 0; n < 250; n++) {
        int64_t i;

        Nome(outro_buffer, n);
        i = Achar(&m, Texto(outro_buffer));
        if (i >= 0 && Itens(&m)[i].id == n && Itens(&m)[i].qtd == n * 3)
            achados++;
    }
    forja_print_i64(achados);
    forja_print_i64(Itens(&m)[Achar(&m, Texto(""))].id);
    forja_print_i64(Itens(&m)[Achar(&m, Texto("abc"))].id);
    forja_print_i64(Achar(&m, Texto("abcd")));
    forja_print_i64(Achar(&m, Texto("a")));
    forja_print_i64(Inserir(&m, Texto("ab"), 2001));
    forja_print_i64(Itens(&m)[Achar(&m, Texto("ab"))].id);
    forja_print_i64(AssinaturaDensa(&m));
    for (n = 0; n < 250; n += 7) {
        Nome(outro_buffer, n);
        Remover(&m, Texto(outro_buffer));
    }
    Remover(&m, Texto(""));
    Remover(&m, Texto("nao existe"));
    ImprimirForma(&m);
    for (n = 0; n < 250; n++) {
        Nome(outro_buffer, n);
        if ((n % 7 == 0) != (Achar(&m, Texto(outro_buffer)) < 0))
            erros++;
    }
    forja_print_i64(erros);
    forja_print_i64(Achar(&m, Texto("")));
    forja_print_i64(AssinaturaDensa(&m));
    Inserir(&m, Texto("item_0"), 5000);
    Inserir(&m, Texto(""), 5001);
    forja_print_i64(m.len);
    forja_print_i64(AssinaturaDensa(&m));
    forja_print_i64(forja_rt_vivos_qtd());
    forja_mapa_liberar(&m, ARQUIVO_TESTE, 34);
    forja_print_i64(forja_rt_vivos_qtd());
    return 0;
}
#elif CASO == 42
static inline int64_t SomaColunas(void *const *col, int64_t len)
{
    int64_t soma = 0;
    int64_t i;

    for (i = 0; i < len; i++)
        soma += ((int64_t *)col[0])[i] + ((int32_t *)col[1])[i] + ((uint8_t *)col[2])[i];
    return soma;
}

static inline void Preencher(void *const *col, int64_t de, int64_t ate)
{
    int64_t i;

    for (i = de; i < ate; i++) {
        ((int64_t *)col[0])[i] = i * 1000;
        ((int32_t *)col[1])[i] = (int32_t)(i * 7);
        ((uint8_t *)col[2])[i] = (uint8_t)i;
    }
}

static inline void ImprimirColunas(void *const *col)
{
    forja_print_i64((int64_t)((unsigned char *)col[1] - (unsigned char *)col[0]));
    forja_print_i64((int64_t)((unsigned char *)col[2] - (unsigned char *)col[1]));
}

/* soa dinamica (release): criar com cap 0 (colunas nulas) e com cap 10, crescer 5 vezes em 5 alocadores (len preservado, colunas realinhadas),
 * frame (epoca), Arena, forja_soa_total e os limites (-1). */
int forja_programa(void)
{
    static const int64_t tams[3] = {8, 4, 1};
    void *col[3];
    void *outra[3];
    uint32_t epoca = 9;
    int64_t cap = 0;
    int64_t len = 0;
    int64_t pedido;
    uint32_t h = forja_arena_criar(256);

    forja_print_i64(forja_soa_total(tams, 3, 0));
    forja_print_i64(forja_soa_total(tams, 3, 1));
    forja_print_i64(forja_soa_total(tams, 3, 10));
    forja_print_i64(forja_soa_total(tams, 3, 1000));
    forja_print_i64(forja_soa_total(tams, 3, INT64_MAX / 8));
    forja_print_i64(forja_soa_total(tams, 3, INT64_MAX));
    forja_soa_criar(col, tams, 3, 0, FORJA_ALOC_HEAP, &epoca, ARQUIVO_TESTE, 50);
    forja_print_bool(col[0] == NULL && col[1] == NULL && col[2] == NULL);
    forja_print_i64(epoca);
    for (pedido = 0; pedido < 5; pedido++) {
        forja_soa_crescer(col, tams, 3, len, &cap, FORJA_ALOC_HEAP, &epoca, ARQUIVO_TESTE, 51);
        Preencher(col, len, cap);
        len = cap;
        forja_print_i64(cap);
        ImprimirColunas(col);
        forja_print_i64(SomaColunas(col, len));
    }
    forja_soa_liberar(col, FORJA_ALOC_HEAP, ARQUIVO_TESTE, 52);
    forja_soa_criar(col, tams, 3, 10, FORJA_ALOC_HEAP, &epoca, ARQUIVO_TESTE, 53);
    Preencher(col, 0, 10);
    ImprimirColunas(col);
    forja_print_i64(SomaColunas(col, 10));
    cap = 10;
    forja_soa_crescer(col, tams, 3, 10, &cap, FORJA_ALOC_HEAP, &epoca, ARQUIVO_TESTE, 54);
    forja_print_i64(cap);
    forja_print_i64(SomaColunas(col, 10));
    forja_soa_liberar(col, FORJA_ALOC_HEAP, ARQUIVO_TESTE, 55);
    forja_soa_liberar(col, FORJA_ALOC_FRAME, ARQUIVO_TESTE, 56);
    forja_soa_criar(outra, tams, 3, 6, FORJA_ALOC_FRAME, &epoca, ARQUIVO_TESTE, 57);
    forja_print_i64(epoca);
    Preencher(outra, 0, 6);
    cap = 6;
    forja_soa_crescer(outra, tams, 3, 6, &cap, FORJA_ALOC_FRAME, &epoca, ARQUIVO_TESTE, 58);
    forja_print_i64(cap);
    forja_print_i64(SomaColunas(outra, 6));
    forja_frame_reset();
    forja_soa_criar(outra, tams, 3, 3, FORJA_ALOC_FRAME, &epoca, ARQUIVO_TESTE, 59);
    forja_print_i64(epoca);
    cap = 0;
    len = 0;
    forja_soa_criar(col, tams, 3, 0, h, &epoca, ARQUIVO_TESTE, 60);
    for (pedido = 0; pedido < 3; pedido++) {
        forja_soa_crescer(col, tams, 3, len, &cap, h, &epoca, ARQUIVO_TESTE, 61);
        Preencher(col, len, cap);
        len = cap;
    }
    forja_print_i64(cap);
    ImprimirColunas(col);
    forja_print_i64(SomaColunas(col, len));
    forja_print_i64(forja_arena_usado(h));
    forja_soa_liberar(col, h, ARQUIVO_TESTE, 62);
    forja_arena_destruir(h);
    return 0;
}
#elif CASO == 43
/* R0008 de soa do frame (dev): a soa criada e crescida no frame leva a epoca do quadro; usada depois do frame_reset() o forja_indice_lista da R0008
 * ("lista do frame usada depois do frame_reset() (quadro 1, agora 2)"), exit 101; o stdout anterior sai inteiro. */
int forja_programa(void)
{
    static const int64_t tams[2] = {8, 4};
    void *col[2];
    uint32_t epoca = 0;
    int64_t cap = 4;
    int64_t *a;
    int32_t *b;

    forja_soa_criar(col, tams, 2, 4, FORJA_ALOC_FRAME, &epoca, ARQUIVO_TESTE, 70);
    a = (int64_t *)col[0];
    b = (int32_t *)col[1];
    a[0] = 5;
    a[3] = 6;
    b[1] = 7;
    forja_print_i64(epoca);
    forja_print_i64(forja_indice_lista(3, 4, epoca, ARQUIVO_TESTE, 71));
    forja_soa_crescer(col, tams, 2, 4, &cap, FORJA_ALOC_FRAME, &epoca, ARQUIVO_TESTE, 72);
    forja_print_i64(cap);
    forja_print_i64(epoca);
    forja_print_i64(((int64_t *)col[0])[0] + ((int64_t *)col[0])[3] + ((int32_t *)col[1])[1]);
    forja_print_i64(forja_indice_lista(7, 8, epoca, ARQUIVO_TESTE, 73));
    forja_frame_reset();
    forja_print_texto("depois do reset");
    forja_print_i64(forja_indice_lista(0, 8, epoca, ARQUIVO_TESTE, 74));
    forja_print_texto("nao deve chegar aqui");
    return 0;
}
#elif CASO == 44
/* R0008 de map do frame (dev): o map criado e crescido no frame leva a epoca do quadro; depois do frame_reset() achar/inserir/remover dao R0008
 * ("lista do frame usada depois do frame_reset() (quadro 1, agora 2)"), exit 101. Um map de heap e liberado antes (a falha cala o R0009). */
int forja_programa(void)
{
    forja_mapa m = {0};
    forja_mapa h = {0};
    int64_t k;
    int64_t i;

    forja_mapa_criar(&m, FORJA_ALOC_FRAME, 2, 8, 8, ARQUIVO_TESTE, 80);
    forja_mapa_criar(&h, FORJA_ALOC_HEAP, 0, 8, 8, ARQUIVO_TESTE, 81);
    forja_print_i64(m.epoca);
    forja_print_i64(h.epoca);
    for (k = 1; k <= 20; k++) {
        i = forja_mapa_inserir(&m, &k, 8, false, 8, ARQUIVO_TESTE, 82);
        ((int64_t *)m.valores)[i] = k * 11;
        i = forja_mapa_inserir(&h, &k, 8, false, 8, ARQUIVO_TESTE, 83);
        ((int64_t *)h.valores)[i] = k * 13;
    }
    ImprimirForma(&m);
    ImprimirForma(&h);
    k = 7;
    forja_print_i64(((int64_t *)m.valores)[forja_mapa_achar(&m, &k, 8, false, ARQUIVO_TESTE, 84)]);
    forja_mapa_remover(&m, &k, 8, false, 8, ARQUIVO_TESTE, 85);
    forja_print_i64(m.len);
    forja_print_i64(forja_mapa_achar(&m, &k, 8, false, ARQUIVO_TESTE, 86));
    forja_mapa_liberar(&h, ARQUIVO_TESTE, 87);
    forja_print_i64(forja_rt_vivos_qtd());
    forja_frame_reset();
    forja_print_texto("depois do reset");
    k = 3;
    forja_print_i64(forja_mapa_achar(&m, &k, 8, false, ARQUIVO_TESTE, 88));
    forja_print_texto("nao deve chegar aqui");
    return 0;
}
#endif

#ifdef ORACULO
int main(void)
{
    return forja_programa();
}
#endif
