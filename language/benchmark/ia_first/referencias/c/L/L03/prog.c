#include <stdio.h>

typedef struct {
    int x;
    int y;
} Ponto;

static int mover_copia(Ponto p) {
    Ponto q = p;
    q.x += 20;
    return q.x;
}

static void mover(Ponto *p) {
    p->x += 5;
}

int main(void) {
    Ponto a = {1, 2};
    int r = mover_copia(a);
    mover(&a);
    mover(&a);
    printf("%d\n", r);
    printf("%d\n", a.x);
    printf("%d\n", a.y);
    return 0;
}
