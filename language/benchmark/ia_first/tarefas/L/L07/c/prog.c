#include <stdio.h>

typedef enum { OK, DIVISAO_POR_ZERO, NEGATIVO } Erro;

static Erro dividir(int a, int b, int *resultado) {
    if (b == 0) {
        return DIVISAO_POR_ZERO;
    }
    if (a < 0) {
        return NEGATIVO;
    }
    *resultado = a / b;
    return OK;
}

static int dividir_ou(int a, int b, int padrao) {
    int r = 0;
    if (dividir(a, b, &r) != OK) {
        return padrao;
    }
    return r;
}

int main(void) {
    printf("%d\n", dividir_ou(10, 2, 100));
    printf("%d\n", dividir_ou(10, 0, 100));
    printf("%d\n", dividir_ou(0 - 4, 2, 200));
    int r = 0;
    if (dividir(7, 2, &r) == OK) {
        printf("%d\n", r);
    } else {
        printf("%d\n", 99);
    }
    return 0;
}
