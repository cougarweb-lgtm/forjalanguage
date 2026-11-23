#include <stdio.h>

typedef struct {
    int valores[3];
    int extra;
} Quadro;

static void preencher(Quadro *q) {
    for (int i = 0; i <= 3; i++) {
        q->valores[i] = i * 10;
    }
}

int main(void) {
    Quadro q = {{0, 0, 0}, 100};
    preencher(&q);
    printf("%d\n", q.extra);
    return 0;
}
