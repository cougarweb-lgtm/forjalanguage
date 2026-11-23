#include <stdio.h>

typedef struct {
    int valores[3];
    int extra;
} Quadro;

static int soma(const Quadro *q) {
    int total = 0;
    for (int i = 0; i < 3; i++) {
        total += q->valores[i];
    }
    return total;
}

int main(void) {
    Quadro q = {{1, 2, 3}, 100};
    printf("%d\n", soma(&q));
    return 0;
}
