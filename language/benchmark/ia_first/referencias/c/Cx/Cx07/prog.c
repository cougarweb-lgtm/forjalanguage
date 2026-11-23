#include <stdio.h>

static void repetir(int vezes, const char *texto) {
    int n = 0;
    while (n < vezes) {
        printf("%s\n", texto);
        n += 1;
    }
}

int main(void) {
    repetir(2, "oi");
    return 0;
}
