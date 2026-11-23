/* Header de PROVA do caso 16 do verificar_ligar.py (cola C do forja ligar 0.6.0): funcao static inline, global mutavel/const/array,
 * macro-funcao (com mods\inline_cola.sig) e uma funcao comum que a lib define em inline_cola.c. */
#ifndef INLINE_COLA_H
#define INLINE_COLA_H

#include <stdint.h>

typedef struct IcPar {
    int32_t a;
    int32_t b;
} IcPar;

static inline int32_t ic_soma(int32_t a, int32_t b)
{
    return a + b;
}

static inline IcPar ic_troca(IcPar p)
{
    IcPar r = { p.b, p.a };
    return r;
}

extern int32_t ic_contador;
extern const int32_t ic_limite;
extern const char ic_nome[];

int32_t ic_incrementa(int32_t passo);

static inline int32_t ic_contador_mais(int32_t n)
{
    return ic_contador + n + ic_incrementa(0);
}

#define IC_TRIPLO(x) ((x) * 3)
#define IC_MAIOR(a, b) ((a) > (b) ? (a) : (b))
#define IC_SEM_ASSINATURA(x) ((x) + 1)

#endif
