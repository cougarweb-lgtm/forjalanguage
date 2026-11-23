#include <stdio.h>

typedef enum { PARADO, ANDANDO, CORRENDO } Estado;

static int velocidade(Estado e) {
    switch (e) {
    case PARADO:
        return 0;
    case ANDANDO:
        return 5;
    case CORRENDO:
        return 12;
    }
    return -1;
}

static Estado proximo(Estado e) {
    switch (e) {
    case PARADO:
        return ANDANDO;
    case ANDANDO:
        return CORRENDO;
    case CORRENDO:
        return PARADO;
    }
    return PARADO;
}

int main(void) {
    Estado e = PARADO;
    int total = 0;
    for (int i = 0; i < 5; i++) {
        int v = velocidade(e);
        printf("%d\n", v);
        total += v;
        e = proximo(e);
    }
    printf("%d\n", total);
    return 0;
}
