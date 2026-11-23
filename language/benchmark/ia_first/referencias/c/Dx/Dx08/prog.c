#include <stdio.h>

typedef struct {
    int vida;
} Nave;

static void dano(Nave *n, int valor) {
    if (valor > n->vida) {
        n->vida = 0;
    } else {
        n->vida -= valor;
    }
}

int main(void) {
    Nave n = {100};
    dano(&n, 130);
    printf("%d\n", n.vida);
    return 0;
}
