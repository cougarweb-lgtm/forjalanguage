/* lib110.h - biblioteca C de teste do 'forja ligar' 0.3.0 (Forja v1.10): alias de simbolo, macros dobraveis,
   #define repetido, acessador de union que colide com funcao e wrapper de variadica. */
#ifndef LIB110_H
#define LIB110_H

#include <stdint.h>
#include <stddef.h>

#define L110_BASE 10
#define L110_BASE 10
#define L110_DOBRO (L110_BASE * 2)
#define L110_MASCARA ((uint32_t)0xFF00u)
#define L110_TAM sizeof(int64_t)
#define L110_META (1.5)
#define L110_TUDO (~0)

typedef union l110_valor { int32_t inteiro; float real; } l110_valor;

int32_t get(int32_t indice);
int32_t insert(int32_t a, int32_t b);
int32_t has(int32_t valor);
int32_t l110_valor_inteiro(l110_valor v);
int32_t l110_log(const char *formato, ...);

#endif
