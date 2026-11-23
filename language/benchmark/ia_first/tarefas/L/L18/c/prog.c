#include <stdio.h>

static int potencia(int base, int exp) {
    if (exp == 0) {
        return 1;
    }
    int meio = potencia(base, exp / 2);
    if (exp % 2 == 0) {
        return meio * meio;
    }
    return meio * meio * base;
}

int main(void) {
    printf("%d\n", potencia(2, 10));
    printf("%d\n", potencia(3, 5));
    printf("%d\n", potencia(7, 0));
    return 0;
}
