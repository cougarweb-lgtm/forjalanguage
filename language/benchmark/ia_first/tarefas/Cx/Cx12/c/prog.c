#include <stdio.h>

typedef enum { VERMELHO, VERDE, AZUL } Cor;

static int codigo(Cor c) {
    switch (c) {
    case VERMELHO:
        return 1;
    case VERDE:
        return 2;
    }
    return 0;
}

int main(void) {
    printf("%d\n", codigo(AZUL));
    return 0;
}
