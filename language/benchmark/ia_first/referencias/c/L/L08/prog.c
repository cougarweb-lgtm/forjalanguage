#include <stdio.h>

#define NADA (-1)

static int achar(const int *a, int n, int alvo) {
    for (int i = 0; i < n; i++) {
        if (a[i] == alvo) {
            return i;
        }
    }
    return NADA;
}

static int ou_padrao(int r, int padrao) {
    return r == NADA ? padrao : r;
}

int main(void) {
    const int v[4] = {4, 8, 15, 16};
    printf("%d\n", ou_padrao(achar(v, 4, 15), 100));
    printf("%d\n", ou_padrao(achar(v, 4, 23), 100));
    int r = achar(v, 4, 16);
    if (r != NADA) {
        printf("%d\n", r);
    } else {
        printf("%d\n", 0);
    }
    return 0;
}
