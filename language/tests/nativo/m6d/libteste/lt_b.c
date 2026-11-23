/*
 * lt_b.c - segunda biblioteca C de teste (M6d, P92a): a baixa cruzada (alocado em lt_a, liberado aqui) e um vazamento proprio.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - M6d P92a: primeira versao. lt_b_libera(p) = free de ponteiro de lt_a (a tabela e uma so: a baixa vale); lt_b_vaza(n) = malloc sem free.
 */
#include <stddef.h>
#include <stdlib.h>

void lt_b_libera(void *p)
{
    free(p);
}

void *lt_b_vaza(size_t n)
{
    return malloc(n);
}
