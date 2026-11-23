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

int main(void) {
    int v[3] = {5, 6, 7};
    int r = achar(v, 3, 9);
    if (r == NADA) {
        r = 0;
    }
    printf("%d\n", r + 1);
    return 0;
}
