#include <stdio.h>

static void imprimir_media(const int *a, int n) {
    int total = 0;
    for (int i = 0; i < n; i++) {
        total += a[i];
    }
    printf("media: %d\n", total / n);
}

int main(void) {
    int v[3] = {4, 8, 12};
    imprimir_media(v, 3);
    imprimir_media(v, 0);
    return 0;
}
