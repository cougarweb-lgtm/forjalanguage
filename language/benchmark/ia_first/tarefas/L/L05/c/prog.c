#include <stdio.h>

typedef struct {
    int *dados;
    int tam;
} Fatia;

static int soma(Fatia s) {
    int t = 0;
    for (int i = 0; i < s.tam; i++) {
        t += s.dados[i];
    }
    return t;
}

int main(void) {
    int a[6] = {10, 20, 30, 40, 50, 60};
    Fatia meio = {&a[1], 3};
    printf("%d\n", meio.tam);
    a[2] = 0;
    printf("%d\n", soma(meio));
    Fatia tudo = {a, 6};
    printf("%d\n", soma(tudo));
    return 0;
}
