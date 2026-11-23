/*
 * forja_rt_heap.h - heap_push / heap_pop da Forja (K17): fila de prioridade (min-heap) em lista dona.
 *
 * Versao: 0.1.0
 * Changelog: 0.1.0 - K17: primeira versao. forja_heap_subir sobe o ultimo elemento (heap_push, depois do append); forja_heap_retira_topo leva o topo
 *            para o fim e desce o novo topo sobre n - 1 (heap_pop, antes de a lista encolher; o IR le o minimo em l[n - 1]). Ordem: a do sort
 *            (forja_ordc_menor de forja_rt_ordenar_chave.h) quando menor e NULL; com menor, o proc (^T, ^T) -> bool do sort_by.
 *
 * UM codigo para os dois backends: o backend C escreve este texto no .c gerado (depois de "#define FORJA_ORDENAR_API static __attribute__((unused))") so
 * quando o programa usa heap (IrPrograma.usa_heap); o runtime nativo o inclui em rt_ordenar.c (objeto rt_win_curto.o) sem definir FORJA_ORDENAR_API
 * (funcoes externas). Pre-requisito: forja_rt_ordenar_chave.h ja incluido (forja_ordc_menor) e <stdint.h>.
 * Verificacao: tests\lowering\360_* a 365_* (heap) nos dois backends.
 */
#ifndef FORJA_ORDENAR_API
#define FORJA_ORDENAR_API
#endif

/* Mesmo formato do sort_by (forja_rt_ordenar_estavel.h): recebe ponteiros para dois elementos e devolve x < y. */
typedef _Bool (*ForjaHeapMenorFn)(void *x, void *y);

/* x < y: com menor (less de heap_push_by / heap_pop_by) ou, sem ele, pela ordem do sort no tipo ORDENA_* (inteiro, real, str). */
FORJA_ORDENAR_API _Bool forja_heap_menor(ForjaHeapMenorFn menor, int64_t tipo, const unsigned char *x, const unsigned char *y)
{
    if (menor != 0)
        return menor((void *)x, (void *)y);
    return forja_ordc_menor(x, y, tipo);
}

/* Troca dois elementos de tam bytes, byte a byte (o elemento pode ser str ou struct de qualquer tamanho). */
FORJA_ORDENAR_API void forja_heap_troca(unsigned char *a, unsigned char *b, int64_t tam)
{
    for (int64_t k = 0; k < tam; k++) {
        unsigned char guardado_do_a = a[k];
        a[k] = b[k];
        b[k] = guardado_do_a;
    }
}

/* heap_push: o elemento de indice n - 1 (o que acabou de entrar pelo append) sobe enquanto for menor que o pai. n = 0 nao faz nada.
 * LIMITE: 5 parametros posicionais, a mesma forma plana de forja_ordc_posicao (K08b); o IR passa um valor por parametro. */
FORJA_ORDENAR_API void forja_heap_subir(unsigned char *base, int64_t n, int64_t tam, ForjaHeapMenorFn menor, int64_t tipo)
{
    int64_t i = n - 1;
    while (i > 0) {
        int64_t pai = (i - 1) / 2;
        if (!forja_heap_menor(menor, tipo, base + i * tam, base + pai * tam))
            return;
        forja_heap_troca(base + i * tam, base + pai * tam, tam);
        i = pai;
    }
}

/* Desce o elemento da raiz sobre os n primeiros: troca com o menor filho enquanto o filho for menor que ele.
 * LIMITE: 5 parametros posicionais (mesma razao de forja_heap_subir). */
FORJA_ORDENAR_API void forja_heap_desce(unsigned char *base, int64_t n, int64_t tam, ForjaHeapMenorFn menor, int64_t tipo)
{
    int64_t i = 0;
    for (;;) {
        int64_t filho = 2 * i + 1;
        if (filho >= n)
            return;
        if (filho + 1 < n && forja_heap_menor(menor, tipo, base + (filho + 1) * tam, base + filho * tam))
            filho++;
        if (!forja_heap_menor(menor, tipo, base + filho * tam, base + i * tam))
            return;
        forja_heap_troca(base + i * tam, base + filho * tam, tam);
        i = filho;
    }
}

/* heap_pop: o topo (indice 0) vai para o indice n - 1 e o heap de n - 1 elementos se refaz a partir da raiz. A lista ainda tem n elementos quando
 * o IR le l[n - 1]: depois disso encolhe um. n = 0 nao faz nada (o IR so chama com len > 0).
 * LIMITE: 5 parametros posicionais (mesma razao de forja_heap_subir). */
FORJA_ORDENAR_API void forja_heap_retira_topo(unsigned char *base, int64_t n, int64_t tam, ForjaHeapMenorFn menor, int64_t tipo)
{
    if (n <= 0)
        return;
    forja_heap_troca(base, base + (n - 1) * tam, tam);
    forja_heap_desce(base, n - 1, tam, menor, tipo);
}
