#ifndef CLONG_H
#define CLONG_H
/* 0.7.0 (M6c P84): long e unsigned long do C saem como c_long/c_ulong (o tamanho segue o alvo, nao e i32/u32 fixo). */
struct ClongDado { unsigned long tempo; long pos; int x; };
long clong_somar(long a, unsigned long b, long *saida);
unsigned long clong_pegar(void);
long *clong_ponteiro(struct ClongDado *d);
#endif
