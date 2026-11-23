/* Header de PROVA do forja ligar 0.5.0 (F1c): bitfield, struct/union anonima e array flexivel. Nao e uma lib real. */
#ifndef BITFIELDS_H
#define BITFIELDS_H

#include <stdint.h>

typedef struct BitsDemo {
    uint32_t a : 1;
    uint32_t b : 3;
    int32_t c : 5;
    uint32_t d : 12;
    uint8_t e : 2;
    uint8_t f : 5;
    int32_t tag;
    union {
        int32_t ival;
        float fval;
    };
    struct {
        int16_t px;
        int16_t py;
    } pos;
    int32_t count;
    int32_t itens[];
} BitsDemo;

/* tamanho da parte fixa (sem os itens) e da alocacao com n itens */
int32_t bits_demo_tamanho(void);
/* preenche pelo lado C: a=1 b=5 c=-3 d=3000 e=2 f=21 tag=7 ival=-9 pos=(100,-200) count=n itens[i]=10*(i+1) */
void bits_demo_preencher(BitsDemo *s, int32_t n);
/* le pelo lado C o bitfield de indice i (0=a ... 5=f) como inteiro com sinal */
int32_t bits_demo_ler(const BitsDemo *s, int32_t i);
/* byte i da struct na memoria */
int32_t bits_demo_byte(const BitsDemo *s, int32_t i);

#endif
