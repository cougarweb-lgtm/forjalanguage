/* forjateste.h - biblioteca C de teste do 'forja ligar' (Forja v1.8b). */
#ifndef FORJATESTE_H
#define FORJATESTE_H

#include <stdint.h>

#define FT_VERSAO 7
#define FT_ESCALA 2.5
#define FT_NOME "forjateste"
#define FT_DOBRO(x) ((x) * 2)

typedef enum ft_cor { FT_COR_VERMELHO = 1, FT_COR_VERDE = 2, FT_COR_AZUL = 4 } ft_cor;
typedef enum ft_modo { FT_MODO_LENTO, FT_MODO_RAPIDO } ft_modo;

typedef struct ft_contador ft_contador;

typedef struct ft_vec2 { int32_t x; int32_t y; } ft_vec2;
typedef struct ft_ret { ft_vec2 pos; ft_vec2 tam; int32_t id; uint8_t marca[4]; } ft_ret;
typedef union ft_valor { int32_t i; float f; uint8_t b[8]; } ft_valor;

typedef int32_t (*ft_visitante)(int32_t item, void *ctx);

ft_contador *ft_contador_criar(int32_t inicio);
void ft_contador_somar(ft_contador *contador, int32_t n);
int32_t ft_contador_valor(const ft_contador *contador);
void ft_contador_destruir(ft_contador *contador);
int32_t ft_cor_valor(ft_cor cor);
int32_t ft_vec2_soma(ft_vec2 v);
ft_ret ft_ret_mover(ft_ret r, int32_t dx);
int32_t ft_ret_area(const ft_ret *r);
int32_t ft_valor_inteiro(ft_valor v);
int32_t ft_visitar(int32_t n, ft_visitante visitante, void *ctx);
const char *ft_nome(void);
int32_t ft_contar_textos(const char **textos, int32_t n);
int32_t ft_soma_tudo(int32_t n, ...);

#endif
