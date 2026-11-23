/* forjateste.c - implementacao da biblioteca de teste do 'forja ligar' (Forja v1.8b). */
#include <stdlib.h>
#include <string.h>

#include "forjateste.h"

struct ft_contador { int32_t valor; };

ft_contador *ft_contador_criar(int32_t inicio)
{
    ft_contador *c = malloc(sizeof *c);
    if (c != NULL) c->valor = inicio;
    return c;
}

void ft_contador_somar(ft_contador *contador, int32_t n) { contador->valor += n; }
int32_t ft_contador_valor(const ft_contador *contador) { return contador->valor; }
void ft_contador_destruir(ft_contador *contador) { free(contador); }
int32_t ft_cor_valor(ft_cor cor) { return (int32_t)cor; }
int32_t ft_vec2_soma(ft_vec2 v) { return v.x + v.y; }

ft_ret ft_ret_mover(ft_ret r, int32_t dx)
{
    r.pos.x += dx;
    r.marca[3] = 9;
    return r;
}

int32_t ft_ret_area(const ft_ret *r) { return r->tam.x * r->tam.y; }
int32_t ft_valor_inteiro(ft_valor v) { return v.i; }

int32_t ft_visitar(int32_t n, ft_visitante visitante, void *ctx)
{
    int32_t soma = 0;
    for (int32_t i = 1; i <= n; i++) soma += visitante(i, ctx);
    return soma;
}

const char *ft_nome(void) { return FT_NOME; }

int32_t ft_contar_textos(const char **textos, int32_t n)
{
    int32_t soma = 0;
    for (int32_t i = 0; i < n; i++) soma += (int32_t)strlen(textos[i]);
    return soma;
}

int32_t ft_soma_tudo(int32_t n, ...) { return n; }
