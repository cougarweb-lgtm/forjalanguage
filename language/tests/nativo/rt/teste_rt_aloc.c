/*
 * teste_rt_aloc.c - casos 15 a 22 do runtime nativo da Forja: alocacao (M2 P14a). Escolha o caso com -DCASO=15..22.
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - M6 P70: cabecalho dev de 80 B no rt nativo: o caso 20 compara distancias de Pool sem a diferenca (DiferencaPool) e o caso 19 destroi o Pool que criava (Arena/Pool vivo no fim = R0009 no nativo).
 *   0.1.0 - primeira versao: 15 heap + liberar duplo (R0008), 16 vazamento (R0009, 101), 17 frame + reset + lista do frame antigo (R0008),
 *           18 lista (cap 0, 8, 16..., 1000 anexos), 19 Arena, 20 Pool de outro dono (R0008), 21 cap negativo (R0007), 22 release (sem R0009 e sem
 *           conferencia). Os casos 15 a 21 pedem -DFORJA_DEV (oraculo e rt); o 22 e release.
 *
 * -DORACULO: usa o forja_rt.h e o CRT do gcc. Sem -DORACULO: liga com rt_win_aloc.o (entrada forja_entrada) e as declaracoes de forja_rt_nativo.h.
 * Nenhum endereco e impresso (so diferencas, alinhamentos e igualdades): oraculo e nativo tem enderecos diferentes.
 * Uso: ver verificar_rt_nativo.py
 */
#ifndef CASO
#error "compile com -DCASO=15..22"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef ORACULO
#include "../../../runtime/forja_rt.h"
#else
#include "../../../runtime/nativo/forja_rt_nativo.h"
#endif

#define ARQUIVO_TESTE "teste_aloc.fj"

static inline void ImprimirBloco(const void *p)
{
    forja_print_i64((int64_t)((uintptr_t)p % 16));
}

static inline void Preencher(void *p, int64_t n, int valor)
{
    unsigned char *b = (unsigned char *)p;
    int64_t i;

    for (i = 0; i < n; i++)
        b[i] = (unsigned char)(valor + i);
}

static inline int64_t Somar(const void *p, int64_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    int64_t soma = 0;
    int64_t i;

    for (i = 0; i < n; i++)
        soma += b[i];
    return soma;
}

static inline int64_t Diferenca(const void *a, const void *b)
{
    return (int64_t)((const unsigned char *)b - (const unsigned char *)a);
}

/* Distancia entre slots do Pool sem a diferenca de cabecalho (M6 P70): o oraculo (forja_rt.h) tem 48 B, o rt nativo em dev 80 B; a saida e a de 48 B nos dois. */
#ifdef ORACULO
#define CABECALHO_A_MAIS 0
#else
#define CABECALHO_A_MAIS 32
#endif
static inline int64_t DiferencaPool(const void *a, const void *b)
{
    int64_t d = Diferenca(a, b);

    if (d == 0)
        return 0;
    return d > 0 ? d - CABECALHO_A_MAIS : d + CABECALHO_A_MAIS;
}

#if CASO == 15
/* Heap (dev): alocar por heap e por mem_alocar, realocar para maior (conteudo preservado), liberar NULL e ponteiro do frame (nao fazem nada), contagem
 * de vivos; o 2o delete do mesmo bloco e R0008 ("o ponteiro nao veio de new(em = heap) ou ja passou por delete"), exit 101. */
int forja_programa(void)
{
    int64_t *a = (int64_t *)forja_heap_alocar(8 * 4, ARQUIVO_TESTE, 10);
    unsigned char *b = (unsigned char *)forja_mem_alocar(FORJA_ALOC_HEAP, 100, ARQUIVO_TESTE, 11);
    unsigned char *f = (unsigned char *)forja_frame_alocar(32, ARQUIVO_TESTE, 12);
    int64_t i;

    for (i = 0; i < 4; i++)
        a[i] = (i + 1) * 100;
    Preencher(b, 100, 1);
    forja_print_i64(forja_rt_vivos_qtd());
    a = (int64_t *)forja_heap_realocar(a, 8 * 40, ARQUIVO_TESTE, 13);
    for (i = 4; i < 40; i++)
        a[i] = i;
    forja_print_i64(a[0] + a[1] + a[2] + a[3] + a[39]);
    forja_print_i64(Somar(b, 100));
    forja_print_bool(forja_no_frame(f));
    forja_print_bool(forja_no_frame(a));
    forja_mem_liberar(NULL, ARQUIVO_TESTE, 14);
    forja_mem_liberar(f, ARQUIVO_TESTE, 15);
    forja_print_i64(forja_rt_vivos_qtd());
    forja_mem_liberar(a, ARQUIVO_TESTE, 16);
    forja_print_i64(forja_rt_vivos_qtd());
    forja_print_texto("antes do delete duplo");
    forja_mem_liberar(a, ARQUIVO_TESTE, 17);
    forja_print_texto("nao deve chegar aqui");
    return 0;
}
#elif CASO == 16
/* Vazamento (dev): 2 blocos no mesmo ponto (somam), o mesmo numero de linha em outro arquivo (ponto novo), um bloco liberado (nao conta), 14 pontos de
 * linhas diferentes (completam os 16) e 3 blocos alem do limite ("(outros)"). Sai com R0009 e exit 101; o stdout anterior sai inteiro. */
int forja_programa(void)
{
    void *liberado;
    int i;

    forja_print_texto("antes do vazamento");
    forja_heap_alocar(24, ARQUIVO_TESTE, 21);
    forja_heap_alocar(40, ARQUIVO_TESTE, 21);
    forja_heap_alocar(5, "outro.fj", 21);
    liberado = forja_heap_alocar(8, ARQUIVO_TESTE, 22);
    for (i = 0; i < 14; i++)
        forja_heap_alocar(16 + i, ARQUIVO_TESTE, 100 + i);
    forja_heap_alocar(7, ARQUIVO_TESTE, 200);
    forja_heap_alocar(9, ARQUIVO_TESTE, 201);
    forja_heap_alocar(11, ARQUIVO_TESTE, 200);
    forja_mem_liberar(liberado, ARQUIVO_TESTE, 23);
    forja_print_i64(forja_rt_vivos_qtd());
    forja_print_texto("fim do programa");
    return 0;
}
#elif CASO == 17
/* Frame (dev): alinhamento de 16, no_frame, realocar copiando, lista do frame com epoca, reset (volta ao inicio); usar a lista do quadro antigo e R0008
 * ("lista do frame usada depois do frame_reset() (quadro 1, agora 2)"), exit 101. */
int forja_programa(void)
{
    unsigned char *a = (unsigned char *)forja_frame_alocar(10, ARQUIVO_TESTE, 30);
    unsigned char *b = (unsigned char *)forja_frame_alocar(32, ARQUIVO_TESTE, 31);
    unsigned char *c = (unsigned char *)forja_frame_alocar(1, ARQUIVO_TESTE, 32);
    unsigned char *n;
    uint32_t epoca = 0;
    int64_t cap = 4;
    int64_t *lista;

    forja_print_i64(Diferenca(a, b));
    forja_print_i64(Diferenca(b, c));
    forja_print_bool(forja_no_frame(c));
    forja_print_bool(forja_no_frame(&epoca));
    Preencher(a, 10, 3);
    n = (unsigned char *)forja_frame_realocar(a, 10, 40, ARQUIVO_TESTE, 33);
    forja_print_i64(Somar(n, 10));
    lista = (int64_t *)forja_lista_criar(FORJA_ALOC_FRAME, 4, 8, &epoca, ARQUIVO_TESTE, 34);
    forja_print_i64(epoca);
    lista[0] = 11;
    lista[3] = 33;
    forja_print_i64(forja_indice_lista(3, 4, epoca, ARQUIVO_TESTE, 35));
    lista = (int64_t *)forja_lista_crescer(lista, 4, &cap, 8, FORJA_ALOC_FRAME, &epoca, ARQUIVO_TESTE, 36);
    forja_print_i64(cap);
    forja_print_i64(lista[0] + lista[3]);
    forja_frame_reset();
    a = (unsigned char *)forja_frame_alocar(5, ARQUIVO_TESTE, 37);
    forja_print_i64(Diferenca(a, n));
    forja_print_texto("depois do reset");
    forja_print_i64(forja_indice_lista(0, 8, epoca, ARQUIVO_TESTE, 38));
    forja_print_texto("nao deve chegar aqui");
    return 0;
}
#elif CASO == 18
/* Lista (dev): cap 0 (ptr nulo, epoca 0), crescer 0 -> 8 -> 16 -> ... -> 1024 em 1000 anexos (cap impresso a cada troca), conteudo e soma, cap 3 e 1
 * (crescem para 8), liberar e contagem de vivos. */
int forja_programa(void)
{
    uint32_t epoca = 7;
    int64_t cap = 0;
    int64_t len = 0;
    int64_t soma = 0;
    int64_t i;
    int64_t *lista = (int64_t *)forja_lista_criar(FORJA_ALOC_HEAP, 0, 8, &epoca, ARQUIVO_TESTE, 40);
    int32_t *curta;
    int64_t cap_curta = 3;

    forja_print_bool(lista == NULL);
    forja_print_i64(epoca);
    for (i = 0; i < 1000; i++) {
        if (len == cap) {
            lista = (int64_t *)forja_lista_crescer(lista, len, &cap, 8, FORJA_ALOC_HEAP, &epoca, ARQUIVO_TESTE, 41);
            forja_print_i64(cap);
        }
        lista[len++] = i * 3;
    }
    for (i = 0; i < len; i++)
        soma += lista[i];
    forja_print_i64(len);
    forja_print_i64(soma);
    forja_print_i64(lista[999]);
    curta = (int32_t *)forja_lista_criar(FORJA_ALOC_HEAP, 3, 4, &epoca, ARQUIVO_TESTE, 42);
    curta[0] = 5;
    curta[1] = 6;
    curta[2] = 7;
    curta = (int32_t *)forja_lista_crescer(curta, 3, &cap_curta, 4, FORJA_ALOC_HEAP, &epoca, ARQUIVO_TESTE, 43);
    forja_print_i64(cap_curta);
    forja_print_i64(curta[0] + curta[1] + curta[2]);
    forja_print_i64(forja_rt_vivos_qtd());
    forja_lista_liberar(lista, FORJA_ALOC_HEAP, ARQUIVO_TESTE, 44);
    forja_lista_liberar(curta, FORJA_ALOC_FRAME, ARQUIVO_TESTE, 45);
    forja_print_i64(forja_rt_vivos_qtd());
    forja_lista_liberar(curta, FORJA_ALOC_HEAP, ARQUIVO_TESTE, 46);
    forja_print_i64(forja_rt_vivos_qtd());
    return 0;
}
#elif CASO == 19
/* Arena (dev): bloco de 64 B e 3 blocos reais (40, 40 e 200 B), alinhamento, usado, conteudo intacto, limpar (o bloco novo fica, usado volta a 0),
 * destruir e reuso da entrada (geracao 2), lista que cresce na Arena e o handle de um Pool (entrada seguinte). */
int forja_programa(void)
{
    uint32_t h = forja_arena_criar(64);
    unsigned char *p1 = (unsigned char *)forja_mem_alocar(h, 40, ARQUIVO_TESTE, 50);
    unsigned char *p2 = (unsigned char *)forja_mem_alocar(h, 40, ARQUIVO_TESTE, 51);
    unsigned char *p3 = (unsigned char *)forja_mem_alocar(h, 200, ARQUIVO_TESTE, 52);
    unsigned char *p4;
    uint32_t h2;
    uint32_t h3;
    uint32_t epoca = 0;
    int64_t cap = 0;
    int64_t *lista;

    forja_print_i64(h);
    ImprimirBloco(p1);
    ImprimirBloco(p2);
    ImprimirBloco(p3);
    Preencher(p1, 40, 1);
    Preencher(p2, 40, 50);
    Preencher(p3, 200, 90);
    forja_print_i64(Somar(p1, 40));
    forja_print_i64(Somar(p2, 40));
    forja_print_i64(forja_arena_usado(h));
    forja_arena_limpar(h);
    forja_print_i64(forja_arena_usado(h));
    p4 = (unsigned char *)forja_mem_alocar(h, 8, ARQUIVO_TESTE, 53);
    forja_print_i64(Diferenca(p3, p4));
    forja_print_i64(forja_arena_usado(h));
    forja_arena_destruir(h);
    h2 = forja_arena_criar(0);
    forja_print_i64(h2);
    lista = (int64_t *)forja_lista_criar(h2, 3, 8, &epoca, ARQUIVO_TESTE, 54);
    lista[0] = 1;
    lista[1] = 20;
    lista[2] = 300;
    cap = 3;
    lista = (int64_t *)forja_lista_crescer(lista, 3, &cap, 8, h2, &epoca, ARQUIVO_TESTE, 55);
    forja_print_i64(cap);
    forja_print_i64(lista[0] + lista[1] + lista[2]);
    forja_print_i64(forja_arena_usado(h2));
    h3 = forja_pool_criar(8);
    forja_print_i64(h3);
    forja_pool_destruir(h3); /* M6 P70: Arena/Pool vivo no fim virou R0009 no rt nativo; o handle impresso segue o mesmo */
    forja_arena_destruir(h2);
    return 0;
}
#elif CASO == 20
/* Pool (dev): 3 objetos de 24 B (slot de 32 + 48 B de cabecalho), vivos, devolver e realocar (LIFO devolve o mesmo slot), devolver NULL; um bloco de
 * OUTRO Pool entregue a este e R0008 ("delete(p, em = pool): o objeto nao veio deste Pool"), exit 101. */
int forja_programa(void)
{
    uint32_t h = forja_pool_criar(4);
    uint32_t g = forja_pool_criar(2);
    unsigned char *p1 = (unsigned char *)forja_mem_alocar(h, 24, ARQUIVO_TESTE, 60);
    unsigned char *p2 = (unsigned char *)forja_mem_alocar(h, 24, ARQUIVO_TESTE, 61);
    unsigned char *p3 = (unsigned char *)forja_mem_alocar(h, 24, ARQUIVO_TESTE, 62);
    unsigned char *p4;
    unsigned char *q = (unsigned char *)forja_mem_alocar(g, 16, ARQUIVO_TESTE, 63);

    forja_print_i64(DiferencaPool(p1, p2));
    forja_print_i64(DiferencaPool(p2, p3));
    ImprimirBloco(p1);
    forja_print_i64(forja_pool_vivos(h));
    forja_pool_devolver(h, p2, ARQUIVO_TESTE, 64);
    forja_pool_devolver(h, NULL, ARQUIVO_TESTE, 65);
    forja_print_i64(forja_pool_vivos(h));
    p4 = (unsigned char *)forja_mem_alocar(h, 8, ARQUIVO_TESTE, 66);
    forja_print_i64(DiferencaPool(p2, p4));
    forja_print_i64(forja_pool_vivos(h));
    forja_print_i64(forja_pool_vivos(g));
    forja_print_texto("antes do pool errado");
    forja_pool_devolver(h, q, ARQUIVO_TESTE, 67);
    forja_print_texto("nao deve chegar aqui");
    return 0;
}
#elif CASO == 21
/* R0007 (dev): make com cap negativo ("make com cap negativo (-5)"), exit 101; antes sai uma lista valida e um bloco de heap vivo (a falha cala o R0009). */
int forja_programa(void)
{
    uint32_t epoca = 0;
    int64_t *ok = (int64_t *)forja_lista_criar(FORJA_ALOC_HEAP, 2, 8, &epoca, ARQUIVO_TESTE, 70);

    ok[0] = 4;
    ok[1] = 5;
    forja_print_i64(ok[0] + ok[1]);
    forja_print_texto("antes do cap negativo");
    forja_lista_criar(FORJA_ALOC_HEAP, -5, 8, &epoca, ARQUIVO_TESTE, 71);
    forja_print_texto("nao deve chegar aqui");
    return 0;
}
#elif CASO == 22
/* Release: blocos de heap vivos no fim NAO sao relatados (exit 0, stderr vazio); delete de NULL e de ponteiro do frame nao faz nada; sem cabecalho
 * (slots de Pool de 32 B e Arena de 48 B coladas); bloco de outro Pool entregue a este NAO e conferido (so a conta de vivos muda); listas na heap
 * e na Arena; vivos sempre 0 (so dev rastreia). */
int forja_programa(void)
{
    unsigned char *a = (unsigned char *)forja_heap_alocar(100, ARQUIVO_TESTE, 80);
    unsigned char *b = (unsigned char *)forja_mem_alocar(FORJA_ALOC_HEAP, 40, ARQUIVO_TESTE, 81);
    unsigned char *f = (unsigned char *)forja_frame_alocar(16, ARQUIVO_TESTE, 82);
    uint32_t h = forja_arena_criar(256);
    uint32_t pa = forja_pool_criar(4);
    uint32_t pb = forja_pool_criar(4);
    unsigned char *a1 = (unsigned char *)forja_mem_alocar(h, 40, ARQUIVO_TESTE, 83);
    unsigned char *a2 = (unsigned char *)forja_mem_alocar(h, 40, ARQUIVO_TESTE, 84);
    unsigned char *q1 = (unsigned char *)forja_mem_alocar(pa, 24, ARQUIVO_TESTE, 85);
    unsigned char *q2 = (unsigned char *)forja_mem_alocar(pa, 24, ARQUIVO_TESTE, 86);
    unsigned char *r1 = (unsigned char *)forja_mem_alocar(pb, 24, ARQUIVO_TESTE, 87);
    uint32_t epoca = 0;
    int64_t cap = 0;
    int64_t len = 0;
    int64_t i;
    int64_t *lista = NULL;

    Preencher(a, 100, 1);
    Preencher(b, 40, 9);
    a = (unsigned char *)forja_heap_realocar(a, 4000, ARQUIVO_TESTE, 88);
    forja_print_i64(Somar(a, 100));
    forja_print_i64(Somar(b, 40));
    forja_mem_liberar(NULL, ARQUIVO_TESTE, 89);
    forja_mem_liberar(f, ARQUIVO_TESTE, 90);
    forja_print_i64(Diferenca(a1, a2));
    forja_print_i64(Diferenca(q1, q2));
    forja_print_i64(forja_arena_usado(h));
    forja_pool_devolver(pa, r1, ARQUIVO_TESTE, 91);
    forja_print_i64(forja_pool_vivos(pa));
    forja_print_i64(forja_pool_vivos(pb));
    for (i = 0; i < 100; i++) {
        if (len == cap)
            lista = (int64_t *)forja_lista_crescer(lista, len, &cap, 8, FORJA_ALOC_HEAP, &epoca, ARQUIVO_TESTE, 92);
        lista[len++] = i;
    }
    forja_print_i64(cap);
    forja_print_i64(lista[0] + lista[99]);
    forja_frame_reset();
    forja_print_bool(forja_no_frame(forja_frame_alocar(8, ARQUIVO_TESTE, 93)));
    forja_print_i64(forja_rt_vivos_qtd());
    forja_print_texto("fim sem relatorio");
    return 0;
}
#endif

#ifdef ORACULO
int main(void)
{
    return forja_programa();
}
#endif
